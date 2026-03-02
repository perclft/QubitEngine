use ratatui::{
    Frame,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    symbols,
    text::{Line, Span},
    widgets::{Axis, BarChart, Block, Borders, Chart, Dataset, List, ListItem, Paragraph},
};

use crate::App;

pub fn draw(f: &mut Frame, app: &mut App) {
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .margin(1)
        .constraints(
            [
                Constraint::Length(3), // Header
                Constraint::Min(10),   // Main content
                Constraint::Length(3), // Footer
            ]
            .as_ref(),
        )
        .split(f.area());

    draw_header(f, chunks[0]);

    // Split main content into Left (Circuits) and Right (Visualization)
    let body_chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(20), Constraint::Percentage(80)].as_ref())
        .split(chunks[1]);

    draw_circuit_list(f, app, body_chunks[0]);
    draw_execution_view(f, app, body_chunks[1]);

    draw_footer(f, chunks[2]);
}

fn draw_header(f: &mut Frame, area: Rect) {
    let title = Paragraph::new(Line::from(vec![
        Span::styled(
            " QubitEngine",
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        ),
        Span::raw(" | R&D Terminal"),
    ]))
    .block(
        Block::default()
            .borders(Borders::ALL)
            .style(Style::default().fg(Color::Blue)),
    );
    f.render_widget(title, area);
}

fn draw_circuit_list(f: &mut Frame, app: &mut App, area: Rect) {
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

fn draw_execution_view(f: &mut Frame, app: &mut App, area: Rect) {
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
            f.render_widget(chart, area);
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

fn draw_footer(f: &mut Frame, area: Rect) {
    let footer = Paragraph::new(" Navigate: ↑/↓ | Execute: Enter | Quit: q ")
        .block(Block::default().borders(Borders::ALL));
    f.render_widget(footer, area);
}
