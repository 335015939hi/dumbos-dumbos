use std::io;

use anyhow::Error;
use crossterm::event::{self, Event, KeyCode, KeyEventKind};
use ratatui::{
    DefaultTerminal,
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Style},
    widgets::{Block, Borders, Clear, Paragraph, Wrap},
};

pub fn error(terminal: &mut DefaultTerminal, error: &Error) -> io::Result<()> {
    // Include the entire error chain.
    let message = format!("{error:#}");
    let mut scroll: u16 = 0;

    loop {
        terminal.draw(|frame| {
            let popup = centered_rect(frame.area(), 80, 60);

            // Clear whatever was behind the dialog.
            frame.render_widget(Clear, popup);

            let block = Block::default()
                .title(" Error ")
                .borders(Borders::ALL)
                .border_style(Style::default().fg(Color::Red));

            let inner = block.inner(popup);
            frame.render_widget(block, popup);

            let sections = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Min(1), Constraint::Length(1)])
                .split(inner);

            // Error message.
            frame.render_widget(
                Paragraph::new(message.as_str())
                    .wrap(Wrap { trim: true })
                    .scroll((scroll, 0)),
                sections[0],
            );

            // Help text.
            frame.render_widget(
                Paragraph::new(" ↑/↓: scroll | PgUp/PgDn: page | Enter/Esc: close ")
                    .alignment(Alignment::Center)
                    .style(Style::default().fg(Color::DarkGray)),
                sections[1],
            );
        })?;

        match event::read()? {
            Event::Key(key) if matches!(key.kind, KeyEventKind::Press | KeyEventKind::Repeat) => {
                match key.code {
                    KeyCode::Enter | KeyCode::Esc => break,

                    KeyCode::Up => {
                        scroll = scroll.saturating_sub(1);
                    }
                    KeyCode::Down => {
                        scroll = scroll.saturating_add(1);
                    }
                    KeyCode::PageUp => {
                        scroll = scroll.saturating_sub(5);
                    }
                    KeyCode::PageDown => {
                        scroll = scroll.saturating_add(5);
                    }

                    _ => {}
                }
            }

            _ => {}
        }
    }

    Ok(())
}

fn centered_rect(area: Rect, percent_x: u16, percent_y: u16) -> Rect {
    let width = (u32::from(area.width) * u32::from(percent_x) / 100) as u16;
    let height = (u32::from(area.height) * u32::from(percent_y) / 100) as u16;

    let width = width.min(area.width);
    let height = height.min(area.height);

    Rect::new(
        area.x + (area.width - width) / 2,
        area.y + (area.height - height) / 2,
        width,
        height,
    )
}
