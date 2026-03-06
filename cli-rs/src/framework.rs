use crossterm::event::{Event as CrosstermEvent, EventStream};
use futures::StreamExt;
use ratatui::{Frame, layout::Rect};
use std::time::Duration;
use tokio::{sync::mpsc, time::interval};

/// Unified event type for the application
pub enum AppEvent {
    Tick,
    Input(CrosstermEvent),
    Grpc(crate::grpc::GrpcEvent),
}

/// Abstract UI Component
pub trait Component {
    fn handle_event(&mut self, event: &AppEvent) -> anyhow::Result<()>;
    fn draw(&mut self, f: &mut Frame, area: Rect);
}

pub struct TuiEngine {
    tx: mpsc::Sender<AppEvent>,
    rx: mpsc::Receiver<AppEvent>,
}

impl TuiEngine {
    pub fn new() -> Self {
        let (tx, rx) = mpsc::channel(1024);
        Self { tx, rx }
    }

    pub fn start(&self) {
        let tick_tx = self.tx.clone();
        tokio::spawn(async move {
            let mut tick_interval = interval(Duration::from_millis(16)); // ~60 FPS
            loop {
                tick_interval.tick().await;
                if tick_tx.send(AppEvent::Tick).await.is_err() {
                    break;
                }
            }
        });

        let input_tx = self.tx.clone();
        tokio::spawn(async move {
            let mut reader = EventStream::new();
            while let Some(Ok(event)) = reader.next().await {
                if input_tx.send(AppEvent::Input(event)).await.is_err() {
                    break;
                }
            }
        });
    }

    pub async fn next_event(&mut self) -> Option<AppEvent> {
        self.rx.recv().await
    }

    pub fn get_grpc_sender(&self) -> mpsc::Sender<AppEvent> {
        self.tx.clone()
    }
}
