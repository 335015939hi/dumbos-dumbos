use crate::server;
use crate::ui::menu;
use dumbtool::events;
use dumbtool::events::Event;
use dumbtool::events::EventType;
use ratatui::DefaultTerminal;
use ratatui::style::Color;
use ratatui::style::Modifier;
use ratatui::style::Style;
use ratatui::widgets;
use ratatui::widgets::Borders;
use ratatui::widgets::ListItem;

pub fn main(terminal: &mut DefaultTerminal) -> std::io::Result<()> {
    let mut state = widgets::ListState::default().with_selected(Some(0));
    loop {
        let events_table = events::EVENTS_TABLE.get().unwrap().lock().unwrap();
        let mut error: String;
        let mut events_list: Vec<ListItem> = vec![];
        let len = events_table.len();
        for i in (0..len).rev() {
            error = format!("event {i} missing");
            let event = match events_table.get(i) {
                Ok(v) => v,
                Err(e) => {
                    error = e.to_string();
                    None
                }
            };
            let event = match event {
                Some(v) => v,
                //there's already an error with the event table. no need to complicate things by
                //dispatching more events
                None => Event::error(&"retrieving events".into(), &error),
            };
            let color = match event.event_type {
                EventType::Upload | EventType::Code => Color::Cyan,
                EventType::CodeRejected | EventType::UploadRejected => Color::Yellow,
                EventType::Info => Color::Green,
                EventType::Error => Color::Red,
            };
            let event = widgets::ListItem::new(format!("{event}"));
            let event = event.style(Style::default().fg(color));
            events_list.push(event);
        }
        drop(events_table);

        let menu: widgets::List = widgets::List::new(events_list);
        let menu = menu.block(
            widgets::Block::default()
                .title("Events log")
                .borders(Borders::ALL),
        );
        let menu = menu
            .highlight_style(Style::default().add_modifier(Modifier::BOLD))
            .highlight_symbol("> ");
        state = menu::simple_menu(terminal, &menu, state)?;
        match state.selected() {
            Some(_) => {}
            None => break,
        }
    }

    Ok(())
}
