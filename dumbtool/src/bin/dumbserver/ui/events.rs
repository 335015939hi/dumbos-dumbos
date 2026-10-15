use crate::ui::menu;
use crossterm::event::KeyCode;
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

fn events_list_append(list: &mut Vec<ListItem>, event: Event) {
    let color = match event.event_type {
        EventType::Upload | EventType::Code => Color::Cyan,
        EventType::CodeRejected | EventType::UploadRejected => Color::Yellow,
        EventType::Info => Color::Green,
        EventType::Error => Color::Red,
    };
    let event = widgets::ListItem::new(format!("{event}"));
    let event = event.style(Style::default().fg(color));
    list.insert(0, event);
}

fn events_list_reload(list: &mut Vec<ListItem>) {
    let events_table = events::EVENTS_TABLE.get().unwrap().lock().unwrap();
    let mut error: String;
    let new_len = events_table.len();
    let old_len: u64 = list.len() as u64;
    let mut new_events_list: Vec<Event> = vec![];
    for i in old_len..new_len {
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
            //also possible deadlock if we try to add this event
            None => Event::error(&"retrieving events".into(), &error),
        };
        new_events_list.push(event);
    }
    drop(events_table);
    for event in new_events_list {
        events_list_append(list, event);
    }
}

pub fn main(terminal: &mut DefaultTerminal) -> std::io::Result<()> {
    let mut state = widgets::ListState::default().with_selected(Some(0));
    let mut events_list: Vec<ListItem> = vec![];
    events_list_reload(&mut events_list);
    loop {
        let menu: widgets::List = widgets::List::new(events_list.clone());
        let menu = menu.block(
            widgets::Block::default()
                .title("Events log")
                .borders(Borders::ALL),
        );
        let menu = menu
            .highlight_style(Style::default().add_modifier(Modifier::BOLD))
            .highlight_symbol("> ");
        let key = menu::navigate(
            terminal,
            &menu,
            &mut state,
            Some(format!("{}, r reloads", menu::DEFAULT_STATUS).as_str()),
        )?;
        match key {
            KeyCode::Char('q') | KeyCode::Esc => {
                break;
            }
            KeyCode::Char('r') | KeyCode::Char('R') => {
                events_list_reload(&mut events_list);
            }

            _ => {}
        }
    }

    Ok(())
}
