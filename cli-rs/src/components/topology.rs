pub struct TopologyState {
    pub nodes: Vec<(f64, f64)>,
    pub edges: Vec<(usize, usize)>,
    pub min_x: f64,
    pub max_x: f64,
    pub min_y: f64,
    pub max_y: f64,
    pub dirty: bool,
    pub label_cache: Vec<(f64, f64, String)>,
}

impl Default for TopologyState {
    fn default() -> Self {
        Self {
            nodes: vec![],
            edges: vec![],
            min_x: 0.0,
            max_x: 80.0,
            min_y: 0.0,
            max_y: 80.0,
            dirty: true,
            label_cache: vec![],
        }
    }
}

impl TopologyState {
    pub fn update_bounds(&mut self) {
        if self.nodes.is_empty() {
            return;
        }
        let mut min_x = f64::INFINITY;
        let mut max_x = f64::NEG_INFINITY;
        let mut min_y = f64::INFINITY;
        let mut max_y = f64::NEG_INFINITY;
        for &(x, y) in &self.nodes {
            min_x = min_x.min(x);
            max_x = max_x.max(x);
            min_y = min_y.min(y);
            max_y = max_y.max(y);
        }
        let pad_x = (max_x - min_x).max(0.1) * 0.15;
        let pad_y = (max_y - min_y).max(0.1) * 0.15;
        self.min_x = min_x - pad_x;
        self.max_x = max_x + pad_x;
        self.min_y = min_y - pad_y;
        self.max_y = max_y + pad_y;
        
        let span_x = (max_x - min_x).max(1.0);
        let span_y = (max_y - min_y).max(1.0);
        let offset_x = span_x * 0.03;
        let offset_y = span_y * -0.015;

        self.label_cache = self
            .nodes
            .iter()
            .enumerate()
            .map(|(i, &(x, y))| (x + offset_x, y + offset_y, format!("Q{}", i)))
            .collect();
        self.dirty = false;
    }
}
