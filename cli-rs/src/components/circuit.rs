pub struct CircuitState {
    pub diagram: Vec<String>,
    pub scroll: u16,
    pub name: String,
}

impl Default for CircuitState {
    fn default() -> Self {
        Self {
            diagram: vec!["Select a circuit and press Enter to generate diagram.".to_string()],
            scroll: 0,
            name: String::new(),
        }
    }
}
