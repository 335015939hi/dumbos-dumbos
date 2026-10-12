use crossterm::event::KeyCode;
use crossterm::event::KeyEventKind;
use ratatui::DefaultTerminal;
use ratatui::{
    layout::{Constraint, Direction, Layout},
    style::{Color, Modifier, Style},
    widgets::{Block, Borders, List, ListItem, ListState, Paragraph},
};

pub fn main(
    terminal: &mut DefaultTerminal,
    title: &str,
    items: &Vec<String>,
    mut state: ListState,
) -> std::io::Result<ListState> {
    let status = String::from(
        "Use ↑/↓ or j/k to navigate, Enter or Space to select, q or Escape to go back",
    );

    loop {
        terminal.draw(|frame| {
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Min(3), Constraint::Length(1)])
                .split(frame.area());

            let menu = List::new(items.iter().map(|s| ListItem::new(s.as_str())))
                .block(Block::default().title(title).borders(Borders::ALL))
                .highlight_style(
                    Style::default()
                        .fg(Color::Yellow)
                        .add_modifier(Modifier::BOLD),
                )
                .highlight_symbol(" > ");

            frame.render_stateful_widget(menu, chunks[0], &mut state);

            frame.render_widget(Paragraph::new(status.as_str()), chunks[1]);
        })?;

        if let crossterm::event::Event::Key(key) = crossterm::event::read()? {
            if key.kind != KeyEventKind::Press {
                continue;
            }

            match key.code {
                KeyCode::Down | KeyCode::Char('j') => {
                    state.select_next();
                }

                KeyCode::Up | KeyCode::Char('k') => {
                    state.select_previous();
                }

                KeyCode::Enter | KeyCode::Char(' ') => {
                    return Ok(state);
                }

                KeyCode::Char('q') | KeyCode::Esc => {
                    state.select(None);
                    break Ok(state);
                }

                _ => {}
            }
        }
    }
}
