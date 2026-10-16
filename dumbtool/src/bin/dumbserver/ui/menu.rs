use anyhow::Result;
use crossterm::event;
use crossterm::event::Event;
use crossterm::event::KeyCode;
use crossterm::event::KeyEventKind;
use ratatui::DefaultTerminal;
use ratatui::Frame;
use ratatui::layout::Alignment;
use ratatui::layout::Margin;
use ratatui::layout::Position;
use ratatui::widgets::Scrollbar;
use ratatui::widgets::ScrollbarOrientation;
use ratatui::widgets::ScrollbarState;
use ratatui::{
    layout::{Constraint, Direction, Layout},
    style::{Color, Modifier, Style},
    widgets::{Block, Borders, List, ListItem, ListState, Paragraph},
};

use tui_input::{Input, backend::crossterm::EventHandler};

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

pub fn input_list(
    terminal: &mut DefaultTerminal,
    fields: &Vec<String>,
) -> Result<Option<Vec<String>>> {
    let mut input_fields: Vec<Input> = (0..fields.len()).map(|_| Input::default()).collect();

    // Input fields followed by Submit and Cancel.
    let mut selected: usize = 0;
    let control_count = fields.len() + 2;

    loop {
        terminal.draw(|frame| {
            input_list_draw(frame, &input_fields, fields, selected);
        })?;

        let event = event::read()?;

        if let Event::Key(key) = &event {
            if !matches!(key.kind, KeyEventKind::Press | KeyEventKind::Repeat) {
                continue;
            }

            match key.code {
                KeyCode::Esc => {
                    return Ok(None);
                }

                KeyCode::Up | KeyCode::BackTab => {
                    selected = (selected + control_count - 1) % control_count;
                }

                KeyCode::Down | KeyCode::Tab => {
                    selected = (selected + 1) % control_count;
                }

                KeyCode::Enter => {
                    if selected < fields.len() {
                        // Advance to the next field or button.
                        selected = (selected + 1) % control_count;
                    } else if selected == fields.len() {
                        // Submit.
                        return Ok(Some(
                            input_fields
                                .iter()
                                .map(|input| input.value().to_owned())
                                .collect(),
                        ));
                    } else {
                        // Cancel.
                        return Ok(None);
                    }
                }

                _ if selected < fields.len() => {
                    let _ = input_fields[selected].handle_event(&event);
                }

                _ => {}
            }
        }
    }
}

fn input_list_draw(
    frame: &mut Frame<'_>,
    input_fields: &[Input],
    fields: &[String],
    selected: usize,
) {
    let mut constraints = vec![Constraint::Length(3); input_fields.len()];

    constraints.push(Constraint::Length(3)); // Buttons
    constraints.push(Constraint::Length(1)); // Help text

    let areas = Layout::default()
        .direction(Direction::Vertical)
        .constraints(constraints)
        .split(frame.area());

    // Draw input fields.
    for (i, (input, area)) in input_fields.iter().zip(areas.iter()).enumerate() {
        let focused = i == selected;
        let inner_width = area.width.saturating_sub(2) as usize;
        let scroll = input.visual_scroll(inner_width);

        let border_style = if focused {
            Style::default().fg(Color::Yellow)
        } else {
            Style::default()
        };

        frame.render_widget(
            Paragraph::new(input.value())
                .scroll((0, scroll.min(u16::MAX as usize) as u16))
                .block(
                    Block::default()
                        .borders(Borders::ALL)
                        .border_style(border_style)
                        .title(format!(" {} ", fields[i])),
                ),
            *area,
        );

        // Position the cursor in the focused input.
        if focused && inner_width > 0 && area.height >= 3 {
            let cursor_x = input
                .visual_cursor()
                .saturating_sub(scroll)
                .min(inner_width - 1);

            frame.set_cursor_position(Position::new(area.x + 1 + cursor_x as u16, area.y + 1));
        }
    }

    // Split the button row into two buttons.
    let button_areas = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(areas[input_fields.len()]);

    let labels = [" Submit ", " Cancel "];

    for i in 0..2 {
        let focused = selected == input_fields.len() + i;

        let style = if focused {
            Style::default().fg(Color::Yellow)
        } else {
            Style::default()
        };

        let border_style = if focused {
            Style::default().fg(Color::Yellow)
        } else {
            Style::default()
        };

        frame.render_widget(
            Paragraph::new(labels[i])
                .alignment(Alignment::Center)
                .style(style)
                .block(
                    Block::default()
                        .borders(Borders::ALL)
                        .border_style(border_style),
                ),
            button_areas[i],
        );
    }

    frame.render_widget(
        Paragraph::new("Up/Down or Tab: navigate | Enter: select | Esc: cancel"),
        areas[input_fields.len() + 1],
    );
}
