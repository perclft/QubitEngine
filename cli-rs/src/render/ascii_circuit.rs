use serde::Deserialize;
use std::fs;

#[derive(Deserialize, Debug, Clone)]
pub struct CircuitFileSchema {
    pub name: String,
    pub qubits: i32,
    pub ops: Vec<OpSchema>,
}

#[derive(Deserialize, Debug, Clone)]
pub struct OpSchema {
    pub gate: String,
    pub target: u32,
    #[serde(default)]
    pub control: u32,
    #[serde(default)]
    pub control2: u32,
    #[serde(default)]
    pub angle: f64,
}

pub fn build_circuit_diagram(path: &str) -> (Vec<String>, String) {
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

    let label_width = format!("q{}: ", n - 1).len();
    let mut wires: Vec<String> = (0..n)
        .map(|i| format!("{:>width$}", format!("q{}: ", i), width = label_width))
        .collect();
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
                let max_d = *depths[lo..=hi].iter().max().unwrap_or(&0);
                for w in lo..=hi {
                    while depths[w] < max_d {
                        wires[w].push('─');
                        depths[w] += 1;
                    }
                }
                wires[ctrl].push_str("─●─");
                wires[tgt].push_str("─⊕─");
                depths[ctrl] += 3;
                depths[tgt] += 3;
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
