use std::collections::VecDeque;

pub struct SimulationState {
    pub is_executing: bool,
    pub is_vqe: bool,
    pub execution_log: VecDeque<String>,
    pub probabilities: Vec<(String, u64)>,
    pub vqe_history: Vec<(i32, f64)>,
    pub vqe_min_energy: f64,
    pub vqe_max_energy: f64,
    pub vqe_max_iter: f64,
}

impl Default for SimulationState {
    fn default() -> Self {
        Self {
            is_executing: false,
            is_vqe: false,
            execution_log: VecDeque::new(),
            probabilities: vec![],
            vqe_history: vec![],
            vqe_min_energy: f64::INFINITY,
            vqe_max_energy: f64::NEG_INFINITY,
            vqe_max_iter: 0.0,
        }
    }
}

impl SimulationState {
    pub fn cap_log(&mut self) {
        while self.execution_log.len() > 1000 {
            self.execution_log.pop_front();
        }
    }
}
