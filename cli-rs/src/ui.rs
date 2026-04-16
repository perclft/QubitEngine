use ratatui::{
    Frame,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    symbols,
    text::{Line, Span},
    widgets::{
        Axis, BarChart, Block, Borders, Chart, Dataset, Gauge, List, ListItem, Paragraph,
        Sparkline, Tabs,
        canvas::{Canvas, Circle, Line as CanvasLine},
    },
};

use crate::{ActiveView, RouterComponent};

pub fn draw(f: &mut Frame, app: &mut RouterComponent) {
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .margin(1)
        .constraints(
            [
                Constraint::Length(3), // Header
                Constraint::Length(3), // Tabs
                Constraint::Min(10),   // Main content
                Constraint::Length(3), // Footer
            ]
            .as_ref(),
        )
        .split(f.area());

    draw_header(f, app, chunks[0]);
    draw_tabs(f, app, chunks[1]);

    // FSM view dispatch
    match app.active_view {
        ActiveView::Simulation => {
            let body_chunks = Layout::default()
                .direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(20), Constraint::Percentage(80)].as_ref())
                .split(chunks[2]);

            draw_circuit_list(f, app, body_chunks[0]);
            draw_execution_view(f, app, body_chunks[1]);
        }
        ActiveView::Topology => {
            draw_topology_view(f, app, chunks[2]);
        }
        ActiveView::Circuit => {
            draw_circuit_diagram_view(f, app, chunks[2]);
        }
    }

    draw_footer(f, chunks[3]);
}

fn draw_header(f: &mut Frame, app: &RouterComponent, area: Rect) {
    let status = if app.sim.is_executing {
        if app.sim.is_vqe {
            " ● VQE Running"
        } else {
            " ● Simulating"
        }
    } else {
        " ○ Idle"
    };

    let title = Paragraph::new(Line::from(vec![
        Span::styled(
            " QubitEngine",
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        ),
        Span::raw(format!(" | R&D Terminal [{}]", app.endpoint)),
        Span::styled(
            status,
            Style::default().fg(if app.sim.is_executing {
                Color::Green
            } else {
                Color::DarkGray
            }),
        ),
    ]))
    .block(
        Block::default()
            .borders(Borders::ALL)
            .style(Style::default().fg(Color::Blue)),
    );
    f.render_widget(title, area);
}

fn draw_tabs(f: &mut Frame, app: &RouterComponent, area: Rect) {
    let titles = vec![
        Line::from(Span::styled(
            " Simulation [1] ",
            Style::default().fg(Color::Green),
        )),
        Line::from(Span::styled(
            " Topology [2] ",
            Style::default().fg(Color::Green),
        )),
        Line::from(Span::styled(
            " Circuit [3] ",
            Style::default().fg(Color::Green),
        )),
    ];

    let active_index = match app.active_view {
        ActiveView::Simulation => 0,
        ActiveView::Topology => 1,
        ActiveView::Circuit => 2,
    };

    let tabs = Tabs::new(titles)
        .block(Block::default().borders(Borders::ALL).title(" Views "))
        .select(active_index)
        .highlight_style(
            Style::default()
                .add_modifier(Modifier::BOLD)
                .bg(Color::Cyan)
                .fg(Color::Black),
        );

    f.render_widget(tabs, area);
}

fn draw_circuit_list(f: &mut Frame, app: &mut RouterComponent, area: Rect) {
    let items: Vec<ListItem> = app
        .circuits
        .iter()
        .map(|c| ListItem::new(c.as_str()))
        .collect();

    let list = List::new(items)
        .block(Block::default().title(" Circuits ").borders(Borders::ALL))
        .highlight_style(
            Style::default()
                .fg(Color::Yellow)
                .add_modifier(Modifier::BOLD),
        )
        .highlight_symbol(">> ");

    f.render_stateful_widget(list, area, &mut app.circuit_list_state);
}

fn draw_execution_view(f: &mut Frame, app: &mut RouterComponent, area: Rect) {
    let title = if app.sim.is_executing {
        if app.sim.is_vqe {
            " VQE Convergence (H2 Molecule) "
        } else {
            " Execution View (Streaming...) "
        }
    } else {
        " Execution View "
    };

    if app.sim.is_vqe {
        if app.sim.vqe_history.is_empty() {
            let log_vec: Vec<String> = app.sim.execution_log.iter().cloned().collect();
            let content = log_vec.join("\n");
            let p =
                Paragraph::new(content).block(Block::default().title(title).borders(Borders::ALL));
            f.render_widget(p, area);
        } else {
            let data: Vec<(f64, f64)> = app
                .sim
                .vqe_history
                .iter()
                .map(|(i, e)| (*i as f64, *e))
                .collect();

            // Use cached bounds — O(1) reads instead of O(N) folds per frame
            let min_energy = app.sim.vqe_min_energy;
            let max_energy = app.sim.vqe_max_energy;
            let max_iter = app.sim.vqe_max_iter.max(1.0);

            let vqe_chunks = Layout::default()
                .direction(Direction::Vertical)
                .margin(0)
                .constraints(
                    [
                        Constraint::Length(3),      // Gauge
                        Constraint::Percentage(70), // Line Chart
                        Constraint::Percentage(30), // Sparkline
                    ]
                    .as_ref(),
                )
                .split(area);

            // 1. Progress Gauge
            let current_iter = max_iter as u16;
            let gauge = Gauge::default()
                .block(
                    Block::default()
                        .title(" VQE Progress ")
                        .borders(Borders::ALL),
                )
                .gauge_style(
                    Style::default()
                        .fg(Color::Magenta)
                        .bg(Color::Black)
                        .add_modifier(Modifier::BOLD),
                )
                .percent(current_iter.clamp(0, 100));
            f.render_widget(gauge, vqe_chunks[0]);

            // 2. Main Convergence Chart
            let datasets = vec![
                Dataset::default()
                    .name("Energy (Hartrees)")
                    .marker(symbols::Marker::Braille)
                    .style(Style::default().fg(Color::Green))
                    .data(&data),
            ];

            let chart = Chart::new(datasets)
                .block(Block::default().title(title).borders(Borders::ALL))
                .x_axis(
                    Axis::default()
                        .title("Iteration")
                        .style(Style::default().fg(Color::Gray))
                        .bounds([0.0, max_iter]),
                )
                .y_axis(
                    Axis::default()
                        .title("Energy")
                        .style(Style::default().fg(Color::Gray))
                        .bounds([min_energy - 0.1, max_energy + 0.1])
                        .labels(vec![
                            Span::raw(format!("{:.2}", min_energy - 0.1)),
                            Span::raw(format!("{:.2}", max_energy + 0.1)),
                        ]),
                );
            f.render_widget(chart, vqe_chunks[1]);

            // 3. High-Frequency Sparkline
            let max_points = vqe_chunks[2].width.saturating_sub(2) as usize;
            let recent: Vec<_> = app.sim.vqe_history.iter().rev().take(max_points).collect();
            let spark_data: Vec<_> = recent.into_iter().rev().map(|(_, e)| *e).collect();

            // Normalize for Sparkline (0-100 u64)
            let s_min = spark_data.iter().copied().fold(f64::INFINITY, f64::min);
            let s_max = spark_data.iter().copied().fold(f64::NEG_INFINITY, f64::max);

            let spark_u64: Vec<u64> = spark_data
                .iter()
                .map(|&e| {
                    if s_max - s_min < 1e-6 {
                        50
                    } else {
                        ((e - s_min) / (s_max - s_min) * 100.0) as u64
                    }
                })
                .collect();

            let sparkline = Sparkline::default()
                .block(
                    Block::default()
                        .title(" Energy Variance ")
                        .borders(Borders::ALL),
                )
                .data(&spark_u64)
                .style(Style::default().fg(Color::Yellow));

            f.render_widget(sparkline, vqe_chunks[2]);
        }
    } else if app.sim.probabilities.is_empty() {
        let content = if app.sim.execution_log.is_empty() {
            if app.sim.is_executing {
                "Connecting...".to_string()
            } else {
                "Select a circuit and press Enter to execute.\nPress 'v' to run VQE optimization on H2.\nPress 'c' to cancel.\nPress 'q' to quit.".to_string()
            }
        } else {
            let log_vec: Vec<String> = app.sim.execution_log.iter().cloned().collect();
            log_vec.join("\n")
        };
        let p = Paragraph::new(content).block(Block::default().title(title).borders(Borders::ALL));
        f.render_widget(p, area);
    } else {
        let data: Vec<(&str, u64)> = app
            .sim
            .probabilities
            .iter()
            .map(|(k, v)| (k.as_str(), *v))
            .collect();

        let barchart = BarChart::default()
            .block(Block::default().title(title).borders(Borders::ALL))
            .data(&data)
            .bar_width(9)
            .bar_gap(1)
            .bar_style(Style::default().fg(Color::Cyan))
            .value_style(
                Style::default()
                    .bg(Color::Cyan)
                    .fg(Color::Black)
                    .add_modifier(Modifier::BOLD),
            )
            .max(100);

        f.render_widget(barchart, area);
    }
}

fn draw_topology_view(f: &mut Frame, app: &mut RouterComponent, area: Rect) {
    // Clone topology data for the 'static paint() closure
    let edges: Vec<(usize, usize)> = app.topology.edges.clone();
    let nodes: Vec<(f64, f64)> = app.topology.nodes.clone();
    let labels: Vec<(f64, f64, String)> = app.topology.label_cache.clone();
    let x_bounds = [app.topology.min_x, app.topology.max_x];
    let y_bounds = [app.topology.min_y, app.topology.max_y];

    let canvas = Canvas::default()
        .block(
            Block::default()
                .title(" CPU/QPU Hardware Topology (Heavy-Hex) ")
                .borders(Borders::ALL),
        )
        .marker(symbols::Marker::Braille)
        .paint(move |ctx| {
            // Draw edges
            for &(n1, n2) in &edges {
                if let (Some(&(x1, y1)), Some(&(x2, y2))) = (nodes.get(n1), nodes.get(n2)) {
                    ctx.draw(&CanvasLine {
                        x1,
                        y1,
                        x2,
                        y2,
                        color: Color::Gray,
                    });
                }
            }
            // Draw physical qubits as circles
            for &(x, y) in &nodes {
                ctx.draw(&Circle {
                    x,
                    y,
                    radius: 2.0,
                    color: Color::Cyan,
                });
            }
            // Pre-computed labels — zero format!() allocations per frame
            for (lx, ly, label) in &labels {
                ctx.print(*lx, *ly, label.clone());
            }
        })
        .x_bounds(x_bounds)
        .y_bounds(y_bounds);

    f.render_widget(canvas, area);
}

fn draw_circuit_diagram_view(f: &mut Frame, app: &mut RouterComponent, area: Rect) {
    let title = if app.circuit.name.is_empty() {
        " Circuit Diagram ".to_string()
    } else {
        format!(" Circuit Diagram — {} ", app.circuit.name)
    };

    // Viewport slicing: only pass the visible lines to the layout engine
    // This bounds rendering to O(visible_height) regardless of total circuit depth
    let visible_height = area.height.saturating_sub(2) as usize; // minus top/bottom borders
    let start = app.circuit.scroll as usize;
    let end = (start + visible_height).min(app.circuit.diagram.len());
    let visible_lines = if start < app.circuit.diagram.len() {
        &app.circuit.diagram[start..end]
    } else {
        &[]
    };
    let content = visible_lines.join("\n");

    let paragraph = Paragraph::new(content)
        .block(Block::default().title(title).borders(Borders::ALL))
        .style(Style::default().fg(Color::White));

    f.render_widget(paragraph, area);
}

fn draw_footer(f: &mut Frame, area: Rect) {
    let footer = Paragraph::new(
        " Views: 1/2/3 | Navigate: ↑/↓ | Tab/Shift+Tab | Execute: Enter | VQE: v | Reload Topo: r | Cancel: c | Quit: q ",
    )
    .block(Block::default().borders(Borders::ALL));
    f.render_widget(footer, area);
}
