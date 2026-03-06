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

use crate::{ActiveTab, RootComponent};

pub fn draw(f: &mut Frame, app: &mut RootComponent) {
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

    match app.active_tab {
        ActiveTab::Simulation => {
            let body_chunks = Layout::default()
                .direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(20), Constraint::Percentage(80)].as_ref())
                .split(chunks[2]);

            draw_circuit_list(f, app, body_chunks[0]);
            draw_execution_view(f, app, body_chunks[1]);
        }
        ActiveTab::Topology => {
            draw_topology_view(f, app, chunks[2]);
        }
    }

    draw_footer(f, chunks[3]);
}

fn draw_header(f: &mut Frame, app: &RootComponent, area: Rect) {
    let title = Paragraph::new(Line::from(vec![
        Span::styled(
            " QubitEngine",
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        ),
        Span::raw(format!(" | R&D Terminal [{}]", app.endpoint)),
    ]))
    .block(
        Block::default()
            .borders(Borders::ALL)
            .style(Style::default().fg(Color::Blue)),
    );
    f.render_widget(title, area);
}

fn draw_tabs(f: &mut Frame, app: &RootComponent, area: Rect) {
    let titles = vec![
        Line::from(Span::styled(
            " Simulation [1] ",
            Style::default().fg(Color::Green),
        )),
        Line::from(Span::styled(
            " Topology [2] ",
            Style::default().fg(Color::Green),
        )),
    ];

    let active_index = match app.active_tab {
        ActiveTab::Simulation => 0,
        ActiveTab::Topology => 1,
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

fn draw_circuit_list(f: &mut Frame, app: &mut RootComponent, area: Rect) {
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

fn draw_execution_view(f: &mut Frame, app: &mut RootComponent, area: Rect) {
    let title = if app.is_executing {
        if app.is_vqe {
            " VQE Convergence (H2 Molecule) "
        } else {
            " Execution View (Streaming...) "
        }
    } else {
        " Execution View "
    };

    if app.is_vqe {
        if app.vqe_history.is_empty() {
            let content = app.execution_log.join("\n");
            let p =
                Paragraph::new(content).block(Block::default().title(title).borders(Borders::ALL));
            f.render_widget(p, area);
        } else {
            let data: Vec<(f64, f64)> = app
                .vqe_history
                .iter()
                .map(|(i, e)| (*i as f64, *e))
                .collect();

            let min_energy = data.iter().map(|(_, e)| *e).fold(f64::INFINITY, f64::min);
            let max_energy = data
                .iter()
                .map(|(_, e)| *e)
                .fold(f64::NEG_INFINITY, f64::max);
            let max_iter = data.last().map(|(i, _)| *i).unwrap_or(100.0);

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
            let recent: Vec<_> = app.vqe_history.iter().rev().take(max_points).collect();
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
    } else {
        if app.probabilities.is_empty() {
            let content = if app.execution_log.is_empty() {
                if app.is_executing {
                    "Connecting...".to_string()
                } else {
                    "Select a circuit and press Enter to execute.\nPress 'v' to run VQE optimization on H2.\nPress 'q' to quit.".to_string()
                }
            } else {
                app.execution_log.join("\n")
            };
            let p =
                Paragraph::new(content).block(Block::default().title(title).borders(Borders::ALL));
            f.render_widget(p, area);
        } else {
            let data: Vec<(&str, u64)> = app
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
}

fn draw_topology_view(f: &mut Frame, _app: &mut RootComponent, area: Rect) {
    let canvas = Canvas::default()
        .block(
            Block::default()
                .title(" CPU/QPU Hardware Topology (Heavy-Hex) ")
                .borders(Borders::ALL),
        )
        .marker(symbols::Marker::Braille)
        .paint(|ctx| {
            // Draw a mock 3x3 heavy-hex lattice layout
            let mut qubits = vec![];

            // Qubit coordinates grid mapping
            let spacing = 20.0;
            for row in 0..3 {
                for col in 0..3 {
                    let x = 10.0 + (col as f64) * spacing;
                    let y = 10.0 + (row as f64) * spacing;
                    qubits.push((x, y));
                }
            }

            // Draw couplers (lines)
            ctx.draw(&CanvasLine {
                x1: qubits[0].0,
                y1: qubits[0].1,
                x2: qubits[1].0,
                y2: qubits[1].1,
                color: Color::Gray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[1].0,
                y1: qubits[1].1,
                x2: qubits[2].0,
                y2: qubits[2].1,
                color: Color::Gray,
            });

            ctx.draw(&CanvasLine {
                x1: qubits[3].0,
                y1: qubits[3].1,
                x2: qubits[4].0,
                y2: qubits[4].1,
                color: Color::Gray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[4].0,
                y1: qubits[4].1,
                x2: qubits[5].0,
                y2: qubits[5].1,
                color: Color::Gray,
            });

            ctx.draw(&CanvasLine {
                x1: qubits[6].0,
                y1: qubits[6].1,
                x2: qubits[7].0,
                y2: qubits[7].1,
                color: Color::Gray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[7].0,
                y1: qubits[7].1,
                x2: qubits[8].0,
                y2: qubits[8].1,
                color: Color::Gray,
            });

            // Vertical connections (Couplers)
            ctx.draw(&CanvasLine {
                x1: qubits[0].0,
                y1: qubits[0].1,
                x2: qubits[3].0,
                y2: qubits[3].1,
                color: Color::DarkGray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[2].0,
                y1: qubits[2].1,
                x2: qubits[5].0,
                y2: qubits[5].1,
                color: Color::DarkGray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[3].0,
                y1: qubits[3].1,
                x2: qubits[6].0,
                y2: qubits[6].1,
                color: Color::DarkGray,
            });
            ctx.draw(&CanvasLine {
                x1: qubits[5].0,
                y1: qubits[5].1,
                x2: qubits[8].0,
                y2: qubits[8].1,
                color: Color::DarkGray,
            });

            // Draw physical qubits as circles
            for (i, &(x, y)) in qubits.iter().enumerate() {
                ctx.draw(&Circle {
                    x,
                    y,
                    radius: 2.0,
                    color: Color::Cyan, // Hardware Active
                });

                // Active highlighting concept could dynamically use app state later.
                ctx.print(x + 3.0, y - 1.0, format!("Q{}", i));
            }
        })
        .x_bounds([0.0, 80.0])
        .y_bounds([0.0, 80.0]);

    f.render_widget(canvas, area);
}

fn draw_footer(f: &mut Frame, area: Rect) {
    let footer =
        Paragraph::new(" Navigate: ↑/↓ | Switch View: Tab/Shift+Tab | Execute: Enter | Quit: q ")
            .block(Block::default().borders(Borders::ALL));
    f.render_widget(footer, area);
}
