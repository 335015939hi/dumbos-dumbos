use crate::ui::menu;
use anyhow::Result;
use ratatui::DefaultTerminal;
use ratatui::widgets::ListState;

pub fn main() {
    match ratatui::run(app) {
        Ok(_) => {}
        Err(e) => {
            panic!("{e}");
        }
    }
}

fn app(terminal: &mut DefaultTerminal) -> Result<()> {
    let mut state = ListState::default().with_selected(Some(0));
    loop {
        state = menu::simple(
            terminal,
            "DumbOS Server",
            &vec![
                "Manage codes".into(),
                "Manage users".into(),
                "Events log".into(),
                "Shutdown server".into(),
            ],
            state,
        )?;
        match state.selected() {
            Some(selected) => match selected {
                //shutdown option
                3 => break,
                //manage codes option
                0 => {}
                //manage users
                1 => {
                    crate::ui::users::main(terminal)?;
                }
                //events log
                2 => {
                    crate::ui::events::main(terminal)?;
                }
                //anything else
                _ => {}
            },
            //go back
            None => {}
        }
    }
    Ok(())
}
