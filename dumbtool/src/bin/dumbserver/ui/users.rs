use crate::ui;
use crate::ui::dialog;
use crate::ui::menu;
use anyhow::Result;
use anyhow::bail;
use crossterm::event::KeyCode;
use dumbtool::users::DumbUser;
use ratatui::DefaultTerminal;
use ratatui::style::Modifier;
use ratatui::style::Style;
use ratatui::widgets;
use ratatui::widgets::List;
use ratatui::widgets::ListItem;

fn new_user_dialog(terminal: &mut DefaultTerminal, directory: &String) -> Result<()> {
    let mut fields: Vec<String> = vec![];
    let index_username = fields.len();
    fields.push("Username".into());
    let index_dispalyname = fields.len();
    fields.push("Display name".into());
    let data = menu::input_list(terminal, &fields)?;
    match data {
        Some(data) => {
            let username = &data[index_username];
            if username.len() == 0 {
                bail!("empty username");
            }
            let mut displayname = &data[index_dispalyname];
            if displayname.len() == 0 {
                displayname = username;
            }
            dumbtool::users::DumbUser::new(directory, username, Some(displayname))?;
        }
        None => {}
    }
    Ok(())
}
fn user_details_dialog(terminal: &mut DefaultTerminal, user: &DumbUser) -> Result<()> {
    Ok(())
}

pub fn main(terminal: &mut DefaultTerminal) -> Result<()> {
    let directory = crate::DIRECTORY.get().unwrap();
    let mut state = widgets::ListState::default().with_selected(Some(0));
    loop {
        let users = DumbUser::get_all(&directory)?;
        let mut usermenu: Vec<ListItem> = vec![];
        for user in &users {
            let user = ListItem::new(format!("{user}"));
            usermenu.push(user);
        }
        let new_user_index = usermenu.len();
        usermenu.push(ListItem::new("--Create new user--"));
        let usermenu = List::new(usermenu)
            .highlight_style(Style::default().add_modifier(Modifier::BOLD))
            .highlight_symbol("> ");
        let key = menu::navigate(terminal, &usermenu, &mut state, None)?;
        match key {
            KeyCode::Char('q') | KeyCode::Esc => {
                break;
            }
            KeyCode::Enter | KeyCode::Char(' ') => match state.selected() {
                Some(selected) => {
                    if selected == new_user_index {
                        match new_user_dialog(terminal, directory) {
                            Ok(_) => {}
                            Err(e) => {
                                dialog::error(terminal, &e)?;
                            }
                        }
                    } else {
                        let user = &users[selected];
                        let _ = user_details_dialog(terminal, user);
                    }
                }
                None => {}
            },
            _ => {}
        }
    }
    Ok(())
}
