use crossterm::event::KeyCode;
use crossterm::event::KeyEventKind;
use ratatui::DefaultTerminal;
use ratatui::layout::Margin;
use ratatui::widgets::Scrollbar;
use ratatui::widgets::ScrollbarOrientation;
use ratatui::widgets::ScrollbarState;
use ratatui::{
    layout::{Constraint, Direction, Layout},
    style::{Color, Modifier, Style},
    widgets::{Block, Borders, List, ListItem, ListState, Paragraph},
};

pub const DEFAULT_STATUS: &str = "↑/↓ or j/k movess, Enter/Space selects, q/Esc returns";

//draw a menu once, and return after a keypress
pub fn render_once(
    terminal: &mut DefaultTerminal,
    menu: &List,
    state: &mut ListState,
    status: Option<&str>,
) -> std::io::Result<KeyCode> {
    terminal.draw(|frame| {
        let chunks = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Min(3), Constraint::Length(1)])
            .split(frame.area());

        frame.render_stateful_widget(menu, chunks[0], state);

        let scrollbar_area = chunks[0].inner(Margin {
            vertical: 1,
            horizontal: 0,
        });
        let viewport_len = scrollbar_area.height as usize;
        let item_count = menu.len();
        // Only show a scrollbar when scrolling is necessary.
        if item_count > viewport_len && viewport_len > 0 {
            let mut scrollbar_state = ScrollbarState::new(item_count - viewport_len)
                .position(state.offset())
                .viewport_content_length(viewport_len);
            frame.render_stateful_widget(
                Scrollbar::new(ScrollbarOrientation::VerticalRight),
                scrollbar_area,
                &mut scrollbar_state,
            );
        }

        match status {
            Some(status) => {
                frame.render_widget(Paragraph::new(status), chunks[1]);
            }
            None => {}
        }
    })?;
    loop {
        if let crossterm::event::Event::Key(key) = crossterm::event::read()? {
            if key.kind != KeyEventKind::Press {
                continue;
            }
            return Ok(key.code);
        }
    }
}

pub fn simple(
    terminal: &mut DefaultTerminal,
    title: &str,
    items: &Vec<String>,
    mut state: ListState,
) -> std::io::Result<ListState> {
    let menu = List::new(items.iter().map(|s| ListItem::new(s.as_str())))
        .block(Block::default().title(title).borders(Borders::ALL))
        .highlight_style(
            Style::default()
                .fg(Color::Yellow)
                .add_modifier(Modifier::BOLD),
        )
        .highlight_symbol("> ");

    loop {
        let key = navigate(terminal, &menu, &mut state, None)?;
        match key {
            KeyCode::Enter | KeyCode::Char(' ') => {
                return Ok(state);
            }
            KeyCode::Char('q') | KeyCode::Esc => {
                break Ok(state);
            }
            _ => {}
        }
    }
}

pub fn simple_menu(
    terminal: &mut DefaultTerminal,
    menu: &List,
    mut state: ListState,
) -> std::io::Result<ListState> {
    loop {
        let key = navigate(terminal, &menu, &mut state, None)?;
        match key {
            KeyCode::Enter | KeyCode::Char(' ') => {
                return Ok(state);
            }
            KeyCode::Char('q') | KeyCode::Esc => {
                return Ok(state);
            }
            _ => {}
        }
    }
}

pub fn navigate(
    terminal: &mut DefaultTerminal,
    menu: &List,
    state: &mut ListState,
    status: Option<&str>,
) -> std::io::Result<KeyCode> {
    let status = match status {
        Some(s) => s,
        None => DEFAULT_STATUS,
    };
    loop {
        let key = render_once(terminal, menu, state, Some(status))?;
        match key {
            KeyCode::Down | KeyCode::Char('j') => {
                state.select_next();
            }
            KeyCode::Up | KeyCode::Char('k') => {
                state.select_previous();
            }
            KeyCode::Enter | KeyCode::Char(' ') => {
                return Ok(key);
            }
            KeyCode::Char('q') | KeyCode::Esc => {
                state.select(None);
                break Ok(key);
            }
            _ => {
                return Ok(key);
            }
        }
    }
}
