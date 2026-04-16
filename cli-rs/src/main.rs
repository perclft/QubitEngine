use clap::Parser;
use crossterm::{
    event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode},
    execute,
    terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
};
use ratatui::{
    Terminal,
    backend::{Backend, CrosstermBackend},
};
use std::{error::Error, fs, io};

mod framework;
mod grpc;
mod ui;
mod render;
pub mod components;

use framework::{AppEvent, Component, TuiEngine};

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ActiveView {
    Simulation,
    Topology,
    Circuit,
}

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// gRPC endpoint for QubitEngine backend
    #[arg(short, long, default_value = "http://127.0.0.1:50051")]
    endpoint: String,

    /// Directory containing circuit JSON files
    #[arg(short, long, default_value = "../circuits")]
    circuits_dir: String,
}

pub struct RouterComponent {
    pub endpoint: String,
    pub circuits_dir: String,
    pub engine_tx: tokio::sync::mpsc::Sender<AppEvent>,
    pub active_view: ActiveView,
    pub circuits: Vec<String>,
    pub circuit_list_state: ratatui::widgets::ListState,
    pub sim: components::simulation::SimulationState,
    pub current_task: Option<tokio::task::JoinHandle<()>>,
    pub topology: components::topology::TopologyState,
    pub circuit: components::circuit::CircuitState,
    // Terminal size tracking for resize-only layout recalculation
    pub last_terminal_size: (u16, u16),
}

impl RouterComponent {
    fn new(
        endpoint: String,
        circuits_dir: String,
        engine_tx: tokio::sync::mpsc::Sender<AppEvent>,
    ) -> RouterComponent {
        let mut state = ratatui::widgets::ListState::default();
        state.select(Some(0));

        let mut circuits = Vec::new();
        if let Ok(entries) = fs::read_dir(&circuits_dir) {
            for entry in entries.flatten() {
                if let Ok(file_type) = entry.file_type()
                    && file_type.is_file()
                    && let Some(ext) = entry.path().extension()
                    && ext == "json"
                    && let Some(name) = entry.path().file_name()
                {
                    circuits.push(name.to_string_lossy().into_owned());
                }
            }
        }
        circuits.sort();
        if circuits.is_empty() {
            circuits.push("No circuits found.json".to_string());
        }

        RouterComponent {
            endpoint,
            circuits_dir,
            engine_tx,
            active_view: ActiveView::Simulation,
            circuits,
            circuit_list_state: state,
            sim: components::simulation::SimulationState::default(),
            current_task: None,
            topology: components::topology::TopologyState::default(),
            circuit: components::circuit::CircuitState::default(),
            last_terminal_size: (0, 0),
        }
    }

    pub fn next(&mut self) {
        let i = match self.circuit_list_state.selected() {
            Some(i) => {
                if i >= self.circuits.len() - 1 {
                    0
                } else {
                    i + 1
                }
            }
            None => 0,
        };
        self.circuit_list_state.select(Some(i));
    }

    pub fn previous(&mut self) {
        let i = match self.circuit_list_state.selected() {
            Some(i) => {
                if i == 0 {
                    self.circuits.len() - 1
                } else {
                    i - 1
                }
            }
            None => 0,
        };
        self.circuit_list_state.select(Some(i));
    }
}

impl Component for RouterComponent {
    fn draw(&mut self, f: &mut ratatui::Frame, _area: ratatui::layout::Rect) {
        ui::draw(f, self);
    }

    fn handle_event(&mut self, event: &AppEvent) -> anyhow::Result<()> {
        match event {
            AppEvent::Input(crossterm_event) => {
                // Handle terminal resize — invalidate topology cache
                if let Event::Resize(w, h) = crossterm_event {
                    self.last_terminal_size = (*w, *h);
                    self.topology.dirty = true;
                }
                if let Event::Key(key) = crossterm_event
                    && key.kind == event::KeyEventKind::Press {
                        match key.code {
                            // FSM view routing via numeric keys
                            KeyCode::Char('1') => {
                                self.active_view = ActiveView::Simulation;
                            }
                            KeyCode::Char('2') => {
                                self.active_view = ActiveView::Topology;
                            }
                            KeyCode::Char('3') => {
                                self.active_view = ActiveView::Circuit;
                            }
                            KeyCode::Tab => {
                                self.active_view = match self.active_view {
                                    ActiveView::Simulation => ActiveView::Topology,
                                    ActiveView::Topology => ActiveView::Circuit,
                                    ActiveView::Circuit => ActiveView::Simulation,
                                };
                            }
                            KeyCode::BackTab => {
                                self.active_view = match self.active_view {
                                    ActiveView::Simulation => ActiveView::Circuit,
                                    ActiveView::Topology => ActiveView::Simulation,
                                    ActiveView::Circuit => ActiveView::Topology,
                                };
                            }
                                KeyCode::Down | KeyCode::Char('j') => match self.active_view {
                                    ActiveView::Simulation => self.next(),
                                    ActiveView::Circuit
                                        if (self.circuit.scroll as usize)
                                            < self.circuit.diagram.len().saturating_sub(1) =>
                                    {
                                        self.circuit.scroll += 1;
                                    }
                                    _ => {}
                                },
                            KeyCode::Up | KeyCode::Char('k') => match self.active_view {
                                ActiveView::Simulation => self.previous(),
                                ActiveView::Circuit => {
                                    self.circuit.scroll = self.circuit.scroll.saturating_sub(1);
                                }
                                _ => {}
                            },
                            KeyCode::Enter | KeyCode::Char('\n') | KeyCode::Char('\r')
                                if !self.sim.is_executing =>
                            {
                                self.sim.is_executing = true;
                                self.sim.is_vqe = false;
                                self.sim.execution_log.clear();
                                self.sim.probabilities.clear();
                                let circuit_file = self.circuits
                                    [self.circuit_list_state.selected().unwrap_or(0)]
                                .clone();
                                let circuit_path =
                                    format!("{}/{}", self.circuits_dir, circuit_file);

                                // Build ASCII diagram for the Circuit view
                                let (diagram, name) =
                                    render::ascii_circuit::build_circuit_diagram(&circuit_path);
                                self.circuit.diagram = diagram;
                                self.circuit.name = name;
                                self.circuit.scroll = 0;

                                if let Some(task) = self.current_task.take() {
                                    task.abort();
                                }

                                let tx = self.engine_tx.clone();
                                let endpoint = self.endpoint.clone();
                                self.current_task = Some(tokio::spawn(async move {
                                    grpc::run_circuit(endpoint, circuit_path, tx).await;
                                }));
                            }
                            KeyCode::Char('v') if !self.sim.is_executing => {
                                self.sim.is_executing = true;
                                self.sim.is_vqe = true;
                                self.sim.execution_log.clear();
                                self.sim.vqe_history.clear();
                                self.sim.vqe_min_energy = f64::INFINITY;
                                self.sim.vqe_max_energy = f64::NEG_INFINITY;
                                self.sim.vqe_max_iter = 0.0;

                                if let Some(task) = self.current_task.take() {
                                    task.abort();
                                }

                                let tx = self.engine_tx.clone();
                                let endpoint = self.endpoint.clone();
                                self.current_task = Some(tokio::spawn(async move {
                                    grpc::run_vqe(endpoint, tx).await;
                                }));
                            }
                            KeyCode::Char('r') => {
                                // Re-fetch hardware topology from backend
                                let tx = self.engine_tx.clone();
                                let endpoint = self.endpoint.clone();
                                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                                self.sim.execution_log.push_back(format!(
                                    "[{}] Refreshing hardware topology...",
                                    timestamp
                                ));
                                self.sim.cap_log();
                                tokio::spawn(async move {
                                    grpc::get_topology(endpoint, tx).await;
                                });
                            }
                            KeyCode::Char('c') if self.sim.is_executing => {
                                if let Some(task) = self.current_task.take() {
                                    task.abort();
                                }
                                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                                self.sim.execution_log.push_back(format!(
                                    "[{}] Canceled active simulation",
                                    timestamp
                                ));
                                self.sim.is_executing = false;
                            }
                            _ => {}
                        }
                    }
            }
            // Broadcast gRPC events — all data handlers update regardless of active view
            AppEvent::Grpc(grpc_event) => {
                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                match grpc_event {
                    grpc::GrpcEvent::Log(msg) => {
                        self.sim.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        self.sim.cap_log();
                    }
                    grpc::GrpcEvent::Wavefunction(probs) => {
                        self.sim.probabilities = probs.clone();
                        self.sim.execution_log.push_back(format!(
                            "[{}] Wavefunction updated (top amplitudes extracted)",
                            timestamp
                        ));
                        self.sim.cap_log();
                    }
                    grpc::GrpcEvent::VqeUpdate(iteration, energy, converged) => {
                        self.sim.vqe_min_energy = self.sim.vqe_min_energy.min(*energy);
                        self.sim.vqe_max_energy = self.sim.vqe_max_energy.max(*energy);
                        self.sim.vqe_max_iter = (*iteration as f64).max(self.sim.vqe_max_iter);
                        self.sim.vqe_history.push((*iteration, *energy));
                        self.sim.execution_log.push_back(format!(
                            "[{}] VQE Iteration {}: Energy = {:.6} Hartrees{}",
                            timestamp,
                            iteration,
                            energy,
                            if *converged { " (CONVERGED)" } else { "" }
                        ));
                        self.sim.cap_log();
                    }
                    grpc::GrpcEvent::Completed(msg) | grpc::GrpcEvent::Error(msg) => {
                        self.sim.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        self.sim.cap_log();
                        self.sim.is_executing = false;
                        self.current_task = None;
                    }
                    grpc::GrpcEvent::Topology(nodes, edges) => {
                        self.topology.nodes = nodes.clone();
                        self.topology.edges = edges.clone();
                        self.topology.dirty = true;
                        self.topology.update_bounds();
                        self.sim.execution_log.push_back(format!(
                            "[{}] Loaded hardware topology diagram ({} nodes)",
                            timestamp,
                            nodes.len()
                        ));
                        self.sim.cap_log();
                    }
                }
            }
            AppEvent::Tick => {}
        }
        Ok(())
    }
}

// Import the generated gRPC bindings
pub mod api {
    tonic::include_proto!("qubit_engine");
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    let args = Args::parse();

    // Setup Terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let mut engine = TuiEngine::new();
    let root_component = RouterComponent::new(
        args.endpoint.clone(),
        args.circuits_dir,
        engine.get_grpc_sender(),
    );

    let tx = engine.get_grpc_sender();
    let endpoint = args.endpoint.clone();
    tokio::spawn(async move {
        grpc::get_topology(endpoint, tx).await;
    });

    engine.start();
    let res = run_app(&mut terminal, &mut engine, root_component).await;

    // Restore terminal
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;

    if let Err(err) = res {
        println!("{:?}", err)
    }

    Ok(())
}

async fn run_app<B: Backend>(
    terminal: &mut Terminal<B>,
    engine: &mut TuiEngine,
    mut root_component: RouterComponent,
) -> Result<(), Box<dyn Error>>
where
    <B as Backend>::Error: 'static + std::error::Error + Send + Sync,
{
    loop {
        if let Some(event) = engine.next_event().await {
            root_component.handle_event(&event)?;

            // Redraw only on tick to constrain FPS
            if matches!(event, AppEvent::Tick) {
                terminal.draw(|f| root_component.draw(f, f.area()))?;
            }

            // Check for exit
            if let AppEvent::Input(crossterm::event::Event::Key(key)) = event
                && (key.code == crossterm::event::KeyCode::Char('q')
                    || key.code == crossterm::event::KeyCode::Esc)
            {
                if let Some(task) = root_component.current_task.take() {
                    task.abort();
                }
                break;
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::sync::mpsc;
    use crossterm::event::{KeyEvent, KeyModifiers};

    #[tokio::test]
    async fn test_router_navigation() {
        let (tx, _rx) = mpsc::channel(1);
        let mut router = RouterComponent {
            endpoint: "http://localhost:50051".to_string(),
            circuits_dir: ".".to_string(),
            engine_tx: tx,
            active_view: ActiveView::Simulation,
            circuits: vec!["a.json".to_string(), "b.json".to_string()],
            circuit_list_state: ratatui::widgets::ListState::default(),
            sim: components::simulation::SimulationState::default(),
            current_task: None,
            topology: components::topology::TopologyState::default(),
            circuit: components::circuit::CircuitState::default(),
            last_terminal_size: (80, 24),
        };
        router.circuit_list_state.select(Some(0));

        router.next();
        assert_eq!(router.circuit_list_state.selected(), Some(1));

        router.next();
        assert_eq!(router.circuit_list_state.selected(), Some(0));

        router.previous();
        assert_eq!(router.circuit_list_state.selected(), Some(1));
    }

    #[tokio::test]
    async fn test_view_switching() {
        let (tx, _rx) = mpsc::channel(1);
        let mut router = RouterComponent {
            endpoint: "http://localhost:50051".to_string(),
            circuits_dir: ".".to_string(),
            engine_tx: tx,
            active_view: ActiveView::Simulation,
            circuits: vec!["a.json".to_string()],
            circuit_list_state: ratatui::widgets::ListState::default(),
            sim: components::simulation::SimulationState::default(),
            current_task: None,
            topology: components::topology::TopologyState::default(),
            circuit: components::circuit::CircuitState::default(),
            last_terminal_size: (80, 24),
        };

        // Simulate key presses for view switching via Char event handlers
        let event_topology = AppEvent::Input(crossterm::event::Event::Key(
            KeyEvent::new(KeyCode::Char('2'), KeyModifiers::empty())
        ));
        router.handle_event(&event_topology).unwrap();
        assert_eq!(router.active_view, ActiveView::Topology);

        let event_circuit = AppEvent::Input(crossterm::event::Event::Key(
            KeyEvent::new(KeyCode::Char('3'), KeyModifiers::empty())
        ));
        router.handle_event(&event_circuit).unwrap();
        assert_eq!(router.active_view, ActiveView::Circuit);
    }
}
