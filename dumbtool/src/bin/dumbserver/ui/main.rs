use ratatui::DefaultTerminal;
use ratatui::Frame;

pub fn main() {
    match ratatui::run(app) {
        Ok(_) => {}
        Err(e) => {
            panic!("{e}");
        }
    }
}

fn app(terminal: &mut DefaultTerminal) -> std::io::Result<()> {
    loop {
        terminal.draw(render)?;
        if crossterm::event::read()?.is_key_press() {
            break Ok(());
        }
    }
}

fn render(frame: &mut Frame) {
    frame.render_widget("hello world", frame.area());
}
