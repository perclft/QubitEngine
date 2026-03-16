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
use serde::Deserialize;
use std::{collections::VecDeque, error::Error, fs, io};

mod framework;
mod grpc;
mod ui;

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

// --- Circuit JSON structures for ASCII rendering ---
#[derive(Deserialize, Debug, Clone)]
struct CircuitFileSchema {
    name: String,
    qubits: i32,
    ops: Vec<OpSchema>,
}

#[derive(Deserialize, Debug, Clone)]
struct OpSchema {
    gate: String,
    target: u32,
    #[serde(default)]
    control: u32,
    #[serde(default)]
    control2: u32,
    #[serde(default)]
    angle: f64,
}

pub struct RouterComponent {
    pub endpoint: String,
    pub circuits_dir: String,
    pub engine_tx: tokio::sync::mpsc::Sender<AppEvent>,
    pub active_view: ActiveView,
    pub circuits: Vec<String>,
    pub circuit_list_state: ratatui::widgets::ListState,
    pub is_executing: bool,
    pub is_vqe: bool,
    pub execution_log: VecDeque<String>,
    pub probabilities: Vec<(String, u64)>,
    pub vqe_history: Vec<(i32, f64)>,
    pub vqe_min_energy: f64,
    pub vqe_max_energy: f64,
    pub vqe_max_iter: f64,
    pub current_task: Option<tokio::task::JoinHandle<()>>,
    // Topology state with cached bounds and pre-computed labels
    pub topology_nodes: Vec<(f64, f64)>,
    pub topology_edges: Vec<(usize, usize)>,
    pub topo_min_x: f64,
    pub topo_max_x: f64,
    pub topo_min_y: f64,
    pub topo_max_y: f64,
    pub topo_dirty: bool,
    pub topo_label_cache: Vec<(f64, f64, String)>,
    // Circuit diagram state
    pub circuit_diagram: Vec<String>,
    pub circuit_scroll: u16,
    pub circuit_name: String,
    // Terminal size tracking for resize-only layout recalculation
    pub last_terminal_size: (u16, u16),
}

/// O(1) amortized log cap using VecDeque ring buffer semantics.
fn cap_log(log: &mut VecDeque<String>) {
    while log.len() > 1000 {
        log.pop_front();
    }
}

/// Build ASCII circuit diagram from a circuit JSON file.
fn build_circuit_diagram(path: &str) -> (Vec<String>, String) {
    let data = match fs::read_to_string(path) {
        Ok(d) => d,
        Err(_) => {
            return (
                vec!["Error: Could not read circuit file.".to_string()],
                String::new(),
            );
        }
    };
    let circuit: CircuitFileSchema = match serde_json::from_str(&data) {
        Ok(c) => c,
        Err(e) => return (vec![format!("Error parsing JSON: {}", e)], String::new()),
    };

    let n = circuit.qubits as usize;
    if n == 0 {
        return (vec!["Empty circuit (0 qubits).".to_string()], circuit.name);
    }

    // Each wire is a String we progressively extend
    let label_width = format!("q{}: ", n - 1).len();
    let mut wires: Vec<String> = (0..n)
        .map(|i| format!("{:>width$}", format!("q{}: ", i), width = label_width))
        .collect();
    // Track the current column depth of each wire
    let mut depths: Vec<usize> = wires.iter().map(|w| w.len()).collect();

    for op in &circuit.ops {
        let gate = op.gate.to_uppercase();
        match gate.as_str() {
            "CNOT" => {
                let ctrl = op.control as usize;
                let tgt = op.target as usize;
                if ctrl >= n || tgt >= n {
                    continue;
                }
                let lo = ctrl.min(tgt);
                let hi = ctrl.max(tgt);
                // Pad all wires in range to same depth
                let max_d = *depths[lo..=hi].iter().max().unwrap_or(&0);
                for w in lo..=hi {
                    while depths[w] < max_d {
                        wires[w].push('─');
                        depths[w] += 1;
                    }
                }
                // Place control dot and target symbol
                wires[ctrl].push_str("─●─");
                wires[tgt].push_str("─⊕─");
                depths[ctrl] += 3;
                depths[tgt] += 3;
                // Fill intermediate wires with vertical connectors
                for w in (lo + 1)..hi {
                    if w != ctrl && w != tgt {
                        wires[w].push_str("─│─");
                        depths[w] += 3;
                    }
                }
            }
            "TOFFOLI" | "CCNOT" => {
                let ctrl1 = op.control as usize;
                let ctrl2 = op.control2 as usize;
                let tgt = op.target as usize;
                let lo = ctrl1.min(ctrl2).min(tgt);
                let hi = ctrl1.max(ctrl2).max(tgt);
                if hi >= n {
                    continue;
                }
                let max_d = *depths[lo..=hi].iter().max().unwrap_or(&0);
                for w in lo..=hi {
                    while depths[w] < max_d {
                        wires[w].push('─');
                        depths[w] += 1;
                    }
                }
                wires[ctrl1].push_str("─●─");
                wires[ctrl2].push_str("─●─");
                wires[tgt].push_str("─⊕─");
                depths[ctrl1] += 3;
                depths[ctrl2] += 3;
                depths[tgt] += 3;
                for w in (lo + 1)..hi {
                    if w != ctrl1 && w != ctrl2 && w != tgt {
                        wires[w].push_str("─│─");
                        depths[w] += 3;
                    }
                }
            }
            _ => {
                // Single-qubit gate (H, X, S, T, RY, RZ, M)
                let tgt = op.target as usize;
                if tgt >= n {
                    continue;
                }
                let label = if gate == "RY" || gate == "RZ" {
                    format!("{}({:.1})", gate, op.angle)
                } else {
                    gate.clone()
                };
                let token = format!("─[{}]─", label);
                wires[tgt].push_str(&token);
                depths[tgt] += token.len();
            }
        }
    }

    // Pad all wires to equal final length with trailing dashes
    let max_depth = depths.iter().copied().max().unwrap_or(0);
    for (i, wire) in wires.iter_mut().enumerate() {
        while depths[i] < max_depth {
            wire.push('─');
            depths[i] += 1;
        }
    }

    let mut lines = Vec::new();
    lines.push(format!(
        "Circuit: {} ({} qubits, {} gates)",
        circuit.name,
        n,
        circuit.ops.len()
    ));
    lines.push(String::new());
    for wire in wires {
        lines.push(wire);
    }

    (lines, circuit.name)
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

        RouterComponent {
            endpoint,
            circuits_dir,
            engine_tx,
            active_view: ActiveView::Simulation,
            circuits,
            circuit_list_state: state,
            is_executing: false,
            is_vqe: false,
            execution_log: VecDeque::new(),
            probabilities: vec![],
            vqe_history: vec![],
            vqe_min_energy: f64::INFINITY,
            vqe_max_energy: f64::NEG_INFINITY,
            vqe_max_iter: 0.0,
            current_task: None,
            topology_nodes: vec![],
            topology_edges: vec![],
            topo_min_x: 0.0,
            topo_max_x: 80.0,
            topo_min_y: 0.0,
            topo_max_y: 80.0,
            topo_dirty: true,
            topo_label_cache: vec![],
            circuit_diagram: vec![
                "Select a circuit and press Enter to generate diagram.".to_string(),
            ],
            circuit_scroll: 0,
            circuit_name: String::new(),
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

    fn update_topology_bounds(&mut self) {
        if self.topology_nodes.is_empty() {
            return;
        }
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;
        for &(x, y) in &self.topology_nodes {
            min_x = min_x.min(x);
            max_x = max_x.max(x);
            min_y = min_y.min(y);
            max_y = max_y.max(y);
        }
        let pad_x = (max_x - min_x).max(0.1) * 0.15;
        let pad_y = (max_y - min_y).max(0.1) * 0.15;
        self.topo_min_x = min_x - pad_x;
        self.topo_max_x = max_x + pad_x;
        self.topo_min_y = min_y - pad_y;
        self.topo_max_y = max_y + pad_y;
        // Pre-compute label strings to avoid format!() allocations per frame
        self.topo_label_cache = self
            .topology_nodes
            .iter()
            .enumerate()
            .map(|(i, &(x, y))| (x + 3.0, y - 1.0, format!("Q{}", i)))
            .collect();
        self.topo_dirty = false;
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
                    self.topo_dirty = true;
                }
                if let Event::Key(key) = crossterm_event {
                    if key.kind == event::KeyEventKind::Press {
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
                                ActiveView::Circuit => {
                                    if (self.circuit_scroll as usize)
                                        < self.circuit_diagram.len().saturating_sub(1)
                                    {
                                        self.circuit_scroll += 1;
                                    }
                                }
                                _ => {}
                            },
                            KeyCode::Up | KeyCode::Char('k') => match self.active_view {
                                ActiveView::Simulation => self.previous(),
                                ActiveView::Circuit => {
                                    self.circuit_scroll = self.circuit_scroll.saturating_sub(1);
                                }
                                _ => {}
                            },
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

                                    // Build ASCII diagram for the Circuit view
                                    let (diagram, name) = build_circuit_diagram(&circuit_path);
                                    self.circuit_diagram = diagram;
                                    self.circuit_name = name;
                                    self.circuit_scroll = 0;

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
                                    self.vqe_min_energy = f64::INFINITY;
                                    self.vqe_max_energy = f64::NEG_INFINITY;
                                    self.vqe_max_iter = 0.0;

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
                            KeyCode::Char('r') => {
                                // Re-fetch hardware topology from backend
                                let tx = self.engine_tx.clone();
                                let endpoint = self.endpoint.clone();
                                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                                self.execution_log.push_back(format!(
                                    "[{}] Refreshing hardware topology...",
                                    timestamp
                                ));
                                cap_log(&mut self.execution_log);
                                tokio::spawn(async move {
                                    grpc::get_topology(endpoint, tx).await;
                                });
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
            // Broadcast gRPC events — all data handlers update regardless of active view
            AppEvent::Grpc(grpc_event) => {
                let timestamp = chrono::Local::now().format("%H:%M:%S").to_string();
                match grpc_event {
                    grpc::GrpcEvent::Log(msg) => {
                        self.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        cap_log(&mut self.execution_log);
                    }
                    grpc::GrpcEvent::Wavefunction(probs) => {
                        self.probabilities = probs.clone();
                        self.execution_log.push_back(format!(
                            "[{}] Wavefunction updated (top amplitudes extracted)",
                            timestamp
                        ));
                        cap_log(&mut self.execution_log);
                    }
                    grpc::GrpcEvent::VqeUpdate(iteration, energy, converged) => {
                        self.vqe_min_energy = self.vqe_min_energy.min(*energy);
                        self.vqe_max_energy = self.vqe_max_energy.max(*energy);
                        self.vqe_max_iter = (*iteration as f64).max(self.vqe_max_iter);
                        self.vqe_history.push((*iteration, *energy));
                        self.execution_log.push_back(format!(
                            "[{}] VQE Iteration {}: Energy = {:.6} Hartrees{}",
                            timestamp,
                            iteration,
                            energy,
                            if *converged { " (CONVERGED)" } else { "" }
                        ));
                        cap_log(&mut self.execution_log);
                    }
                    grpc::GrpcEvent::Completed(msg) | grpc::GrpcEvent::Error(msg) => {
                        self.execution_log
                            .push_back(format!("[{}] {}", timestamp, msg));
                        cap_log(&mut self.execution_log);
                        self.is_executing = false;
                        self.current_task = None;
                    }
                    grpc::GrpcEvent::Topology(nodes, edges) => {
                        self.topology_nodes = nodes.clone();
                        self.topology_edges = edges.clone();
                        self.topo_dirty = true;
                        self.update_topology_bounds();
                        self.execution_log.push_back(format!(
                            "[{}] Loaded hardware topology diagram ({} nodes)",
                            timestamp,
                            nodes.len()
                        ));
                        cap_log(&mut self.execution_log);
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
