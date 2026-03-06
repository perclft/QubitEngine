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
use std::{collections::VecDeque, error::Error, fs, io};

mod framework;
mod grpc;
mod ui;

use framework::{AppEvent, Component, TuiEngine};

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ActiveTab {
    Simulation,
    Topology,
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

pub struct RootComponent {
    pub endpoint: String,
    pub circuits_dir: String,
    pub engine_tx: tokio::sync::mpsc::UnboundedSender<AppEvent>,
    pub active_tab: ActiveTab,
    pub circuits: Vec<String>,
    pub circuit_list_state: ratatui::widgets::ListState,
    pub is_executing: bool,
    pub is_vqe: bool,
    pub execution_log: VecDeque<String>,
    pub probabilities: Vec<(String, u64)>,
    pub vqe_history: Vec<(i32, f64)>,
    pub rx: Option<tokio::sync::mpsc::UnboundedReceiver<grpc::GrpcEvent>>,
    pub current_task: Option<tokio::task::JoinHandle<()>>,
    pub topology_nodes: Vec<(f64, f64)>,
    pub topology_edges: Vec<(usize, usize)>,
}

impl RootComponent {
    fn new(
        endpoint: String,
        circuits_dir: String,
        engine_tx: tokio::sync::mpsc::UnboundedSender<AppEvent>,
    ) -> RootComponent {
        let mut state = ratatui::widgets::ListState::default();
        state.select(Some(0));

        let mut circuits = Vec::new();
        if let Ok(entries) = fs::read_dir(&circuits_dir) {
            for entry in entries.flatten() {
                if let Ok(file_type) = entry.file_type() {
                    if file_type.is_file() {
                        if let Some(ext) = entry.path().extension() {
                            if ext == "json" {
                                if let Some(name) = entry.path().file_name() {
                                    circuits.push(name.to_string_lossy().into_owned());
                                }
                            }
                        }
                    }
                }
            }
        }
        circuits.sort();
        if circuits.is_empty() {
            circuits.push("No circuits found.json".to_string());
        }

        RootComponent {
            endpoint,
            circuits_dir,
            engine_tx,
            active_tab: ActiveTab::Simulation,
            circuits,
            circuit_list_state: state,
            is_executing: false,
            is_vqe: false,
            execution_log: VecDeque::new(),
            probabilities: vec![],
            vqe_history: vec![],
            rx: None,
            current_task: None,
            topology_nodes: vec![],
            topology_edges: vec![],
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

impl Component for RootComponent {
    fn draw(&mut self, f: &mut ratatui::Frame, _area: ratatui::layout::Rect) {
        ui::draw(f, self);
    }

    fn handle_event(&mut self, event: &AppEvent) -> anyhow::Result<()> {
        match event {
            AppEvent::Input(crossterm_event) => {
                if let Event::Key(key) = crossterm_event {
                    if key.kind == event::KeyEventKind::Press {
                        match key.code {
                            KeyCode::Tab => {
                                self.active_tab = match self.active_tab {
                                    ActiveTab::Simulation => ActiveTab::Topology,
                                    ActiveTab::Topology => ActiveTab::Simulation,
                                };
                            }
                            KeyCode::Down | KeyCode::Char('j') => self.next(),
                            KeyCode::Up | KeyCode::Char('k') => self.previous(),
                            KeyCode::Enter | KeyCode::Char('\n') | KeyCode::Char('\r') => {
                                if !self.is_executing {
                                    self.is_executing = true;
                                    self.is_vqe = false;
                                    self.execution_log.clear();
                                    self.probabilities.clear();
                                    let circuit_file = self.circuits
                                        [self.circuit_list_state.selected().unwrap_or(0)]
                                    .clone();
                                    let circuit_path =
                                        format!("{}/{}", self.circuits_dir, circuit_file);

                                    if let Some(task) = self.current_task.take() {
                                        task.abort();
                                    }

                                    let tx = self.engine_tx.clone();
                                    let endpoint = self.endpoint.clone();
                                    self.current_task = Some(tokio::spawn(async move {
                                        grpc::run_circuit(endpoint, circuit_path, tx).await;
                                    }));
                                }
                            }
                            KeyCode::Char('v') => {
                                if !self.is_executing {
                                    self.is_executing = true;
                                    self.is_vqe = true;
                                    self.execution_log.clear();
                                    self.vqe_history.clear();

                                    if let Some(task) = self.current_task.take() {
                                        task.abort();
                                    }

                                    let tx = self.engine_tx.clone();
                                    let endpoint = self.endpoint.clone();
                                    self.current_task = Some(tokio::spawn(async move {
                                        grpc::run_vqe(endpoint, tx).await;
                                    }));
                                }
                            }
                            KeyCode::Char('c') => {
                                if self.is_executing {
                                    if let Some(task) = self.current_task.take() {
                                        task.abort();
                                    }
                                    let timestamp =
                                        chrono::Local::now().format("%H:%M:%S").to_string();
                                    self.execution_log.push_back(format!(
                                        "[{}] Canceled active simulation",
                                        timestamp
                                    ));
                                    self.is_executing = false;
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
            AppEvent::Grpc(grpc_event) => {
                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                match grpc_event {
                    grpc::GrpcEvent::Log(msg) => {
                        self.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        if self.execution_log.len() > 1000 {
                            self.execution_log.drain(0..100);
                        }
                    }
                    grpc::GrpcEvent::Wavefunction(state) => {
                        let mut sorted: Vec<_> = state.clone();
                        sorted.sort_by(|a, b| {
                            let pa = a.1 * a.1 + a.2 * a.2;
                            let pb = b.1 * b.1 + b.2 * b.2;
                            pb.partial_cmp(&pa).unwrap_or(std::cmp::Ordering::Equal)
                        });

                        let mut probs = vec![];
                        // Take top 10 probabilities for charting
                        for (idx, r, i_c) in sorted.into_iter().take(10) {
                            let p = (r * r + i_c * i_c) * 100.0;
                            probs.push((format!("|{}>", idx), p as u64));
                        }
                        self.probabilities = probs;
                        self.execution_log.push_back(format!(
                            "[{}] Wavefunction updated (top amplitudes extracted)",
                            timestamp
                        ));
                        if self.execution_log.len() > 1000 {
                            self.execution_log.drain(0..100);
                        }
                    }
                    grpc::GrpcEvent::VqeUpdate(iteration, energy, converged) => {
                        self.vqe_history.push((*iteration, *energy));
                        self.execution_log.push_back(format!(
                            "[{}] VQE Iteration {}: Energy = {:.6} Hartrees{}",
                            timestamp,
                            iteration,
                            energy,
                            if *converged { " (CONVERGED)" } else { "" }
                        ));
                        if self.execution_log.len() > 1000 {
                            self.execution_log.drain(0..100);
                        }
                    }
                    grpc::GrpcEvent::Completed(msg) | grpc::GrpcEvent::Error(msg) => {
                        self.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        if self.execution_log.len() > 1000 {
                            self.execution_log.drain(0..100);
                        }
                        self.is_executing = false;
                        self.current_task = None;
                    }
                    grpc::GrpcEvent::Topology(nodes, edges) => {
                        self.topology_nodes = nodes.clone();
                        self.topology_edges = edges.clone();
                        self.execution_log.push_back(format!(
                            "[{}] Loaded hardware topology diagram ({} nodes)",
                            timestamp,
                            nodes.len()
                        ));
                        if self.execution_log.len() > 1000 {
                            self.execution_log.drain(0..100);
                        }
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
    let root_component = RootComponent::new(
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
    mut root_component: RootComponent,
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
            if let AppEvent::Input(crossterm::event::Event::Key(key)) = event {
                if key.code == crossterm::event::KeyCode::Char('q')
                    || key.code == crossterm::event::KeyCode::Esc
                {
                    if let Some(task) = root_component.current_task.take() {
                        task.abort();
                    }
                    break;
                }
            }
        }
    }
    Ok(())
}
