use crossterm::{
    event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode},
    execute,
    terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
};
use ratatui::{
    Terminal,
    backend::{Backend, CrosstermBackend},
};
use std::{error::Error, io};

mod grpc;
mod ui;

pub struct App {
    pub circuits: Vec<String>,
    pub circuit_list_state: ratatui::widgets::ListState,
    pub is_executing: bool,
    pub is_vqe: bool,
    pub execution_log: Vec<String>,
    pub probabilities: Vec<(String, u64)>,
    pub vqe_history: Vec<(i32, f64)>,
    pub rx: Option<tokio::sync::mpsc::Receiver<grpc::GrpcEvent>>,
}

impl App {
    fn new() -> App {
        let mut state = ratatui::widgets::ListState::default();
        state.select(Some(0));
        App {
            circuits: vec![
                "bell.json".to_string(),
                "stream_test.json".to_string(),
                "test_circuit.json".to_string(),
            ],
            circuit_list_state: state,
            is_executing: false,
            is_vqe: false,
            execution_log: vec![],
            probabilities: vec![],
            vqe_history: vec![],
            rx: None,
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

// Import the generated gRPC bindings
pub mod api {
    tonic::include_proto!("qubit_engine");
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    // Setup Terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let mut app = App::new();
    let res = run_app(&mut terminal, &mut app);

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

fn run_app<B: Backend>(terminal: &mut Terminal<B>, app: &mut App) -> Result<(), Box<dyn Error>>
where
    <B as Backend>::Error: 'static,
{
    loop {
        terminal.draw(|f| ui::draw(f, app))?;

        // Update state from gRPC channel
        if let Some(ref mut rx) = app.rx {
            while let Ok(event) = rx.try_recv() {
                match event {
                    grpc::GrpcEvent::Log(msg) => app.execution_log.push(msg),
                    grpc::GrpcEvent::Wavefunction(state) => {
                        let mut sorted: Vec<_> = state.into_iter().collect();
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
                        app.probabilities = probs;
                        app.execution_log
                            .push(format!("Wavefunction updated (top amplitudes extracted)"));
                    }
                    grpc::GrpcEvent::VqeUpdate(iteration, energy, converged) => {
                        app.vqe_history.push((iteration, energy));
                        app.execution_log.push(format!(
                            "VQE Iteration {}: Energy = {:.6} Hartrees{}",
                            iteration,
                            energy,
                            if converged { " (CONVERGED)" } else { "" }
                        ));
                    }
                    grpc::GrpcEvent::Completed(msg) | grpc::GrpcEvent::Error(msg) => {
                        app.execution_log.push(msg);
                        app.is_executing = false;
                    }
                }
            }
        }

        if event::poll(std::time::Duration::from_millis(50))? {
            if let Event::Key(key) = event::read()? {
                if key.kind == event::KeyEventKind::Press {
                    match key.code {
                        KeyCode::Char('q') | KeyCode::Esc => return Ok(()),
                        KeyCode::Down | KeyCode::Char('j') => app.next(),
                        KeyCode::Up | KeyCode::Char('k') => app.previous(),
                        KeyCode::Enter | KeyCode::Char('\n') | KeyCode::Char('\r') => {
                            if !app.is_executing {
                                app.is_executing = true;
                                app.is_vqe = false;
                                app.execution_log.clear();
                                app.probabilities.clear();
                                let circuit_file = app.circuits
                                    [app.circuit_list_state.selected().unwrap_or(0)]
                                .clone();
                                let circuit_path = format!("../circuits/{}", circuit_file);

                                let (tx, rx) = tokio::sync::mpsc::channel(100);
                                app.rx = Some(rx);

                                tokio::spawn(async move {
                                    grpc::run_circuit(
                                        "http://127.0.0.1:50051".to_string(),
                                        circuit_path,
                                        tx,
                                    )
                                    .await;
                                });
                            }
                        }
                        KeyCode::Char('v') => {
                            if !app.is_executing {
                                app.is_executing = true;
                                app.is_vqe = true;
                                app.execution_log.clear();
                                app.vqe_history.clear();

                                let (tx, rx) = tokio::sync::mpsc::channel(100);
                                app.rx = Some(rx);

                                tokio::spawn(async move {
                                    grpc::run_vqe("http://127.0.0.1:50051".to_string(), tx).await;
                                });
                            }
                        }
                        _ => {}
                    }
                }
            }
        }
    }
}
