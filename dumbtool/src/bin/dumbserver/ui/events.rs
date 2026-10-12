use crate::server;
use crate::ui::menu;
use dumbtool::events::Event;
use ratatui::DefaultTerminal;
use ratatui::widgets::ListState;

fn main(terminal: &mut DefaultTerminal) -> std::io::Result<()> {
    let mut state = ListState::default().with_selected(Some(0));
    let events: Vec<Event> = vec![];
    Ok(())
}
