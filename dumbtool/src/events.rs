use crate::log;
use crate::tables::EventsTable;
use chrono::DateTime;
use chrono::Local;
use ratatui::style::Color;
use serde::Deserialize;
use serde::Serialize;
use std::fmt;
use std::sync::Mutex;
use std::sync::OnceLock;

#[derive(Serialize, Deserialize, PartialEq, Eq)]
pub enum EventType {
    Code,
    Upload,
    CodeRejected,
    UploadRejected,
    GenericReject,
    Error,
    Info,
}

#[derive(Serialize, Deserialize)]
pub struct Event {
    pub event_type: EventType,
    pub user: Option<String>,
    pub time: i64,
    pub reason: Option<String>,
    pub extra: Option<String>,
}

pub static EVENTS_TABLE: OnceLock<Mutex<EventsTable>> = OnceLock::new();

impl fmt::Display for Event {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let time = match DateTime::from_timestamp(self.time, 0) {
            None => String::from(""),
            Some(t) => {
                let t = t.with_timezone(&Local);
                t.format("%Y-%m-%d %H:%M:%S").to_string()
            }
        };
        let mut s = match &self.user {
            Some(u) => format!("user '{u}' "),

            None => "".into(),
        };
        let tag;
        match self.event_type {
            EventType::Code => {
                tag = "Action";
                s += "used code ";
                s += match &self.extra {
                    Some(s) => s.as_str(),
                    None => "(unknown)",
                };
            }
            EventType::CodeRejected => {
                tag = "Reject";
                s += "denied code ";
                s += match &self.extra {
                    Some(s) => s.as_str(),
                    None => "(unknown)",
                };
                s += " because ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "",
                };
            }
            EventType::GenericReject => {
                tag = "Reject";
                s += format!(
                    "{} rejected becase ",
                    match &self.extra {
                        Some(a) => a.as_str(),
                        None => "unknown action",
                    }
                )
                .as_str();
                s += match &self.reason {
                    Some(r) => r.as_str(),
                    None => "unknown reason",
                }
            }
            EventType::Upload => {
                tag = "Action";
                s += "uploaded ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "unknown number of",
                };
                s += " bytes to ";
                s += format!(
                    "'{}'",
                    match &self.extra {
                        Some(s) => s.as_str(),
                        None => "unknown location",
                    }
                )
                .as_str();
            }
            EventType::UploadRejected => {
                tag = "Reject";
                s += "denied upload because ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "",
                };
            }

            EventType::Error => {
                tag = "Error";
                s += match &self.extra {
                    Some(s) => s.as_str(),
                    None => "something",
                };
                s += " failed because ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "unknown cause",
                };
            }
            EventType::Info => {
                tag = "Info";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "something happened",
                };
            }
        }
        write!(f, "[{time}][{tag}] {s}")
    }
}

impl Event {
    pub fn dispatch(&mut self) {
        self.time = chrono::Utc::now().timestamp();
        let table: &mut EventsTable = &mut EVENTS_TABLE.get().unwrap().lock().unwrap();
        match table.push(self) {
            Ok(_) => {}
            Err(e) => panic!("{e}"),
        }
        if log::get_verbosity() != 0 {
            self.print();
        }
    }
    pub fn print(&self) {
        println!("{self}");
    }

    pub fn match_type(&self, event_type: EventType) -> bool {
        self.event_type == event_type
    }
    pub fn match_user(&self, user: &String) -> bool {
        match &self.user {
            Some(u) => u == user,
            None => false,
        }
    }
    pub fn match_code(&self, is_personal: bool, code: &String) -> bool {
        let code = match is_personal {
            true => format!("{code} (personal)"),
            false => format!("{code} (global)"),
        };
        match self.event_type {
            EventType::Code | EventType::CodeRejected => match &self.extra {
                Some(c) => &code == c,
                None => false,
            },
            _ => false,
        }
    }

    pub fn color(&self) -> Color {
        match self.event_type {
            EventType::Upload | EventType::Code => Color::Cyan,
            EventType::CodeRejected | EventType::UploadRejected | EventType::GenericReject => {
                Color::Yellow
            }
            EventType::Info => Color::Green,
            EventType::Error => Color::Red,
        }
    }

    pub fn code(user: &String, code: &String, personal: bool) -> Self {
        Self {
            event_type: EventType::Code,
            user: Some(user.clone()),
            time: -1,
            reason: None,
            extra: Some(match personal {
                true => format!("{code} (personal)"),
                false => format!("{code} (global)"),
            }),
        }
    }
    pub fn reject_code(user: &String, code: &String, personal: bool, reason: &String) -> Self {
        Self {
            event_type: EventType::CodeRejected,
            user: Some(user.clone()),
            time: -1,
            reason: Some(reason.clone()),
            extra: Some(match personal {
                true => format!("{code} (personal)"),
                false => format!("{code} (global)"),
            }),
        }
    }
    pub fn reject(user: &String, action: &String, reason: &String) -> Self {
        Self {
            event_type: EventType::GenericReject,
            user: Some(user.clone()),
            time: -1,
            reason: Some(reason.clone()),
            extra: Some(action.clone()),
        }
    }
    pub fn error(action: &String, message: &String) -> Self {
        Self {
            event_type: EventType::Error,
            user: None,
            time: -1,
            reason: Some(message.clone()),
            extra: Some(action.clone()),
        }
    }
    pub fn reject_upload(user: &String, reason: &String) -> Self {
        Self {
            event_type: EventType::UploadRejected,
            user: Some(user.clone()),
            time: -1,
            reason: Some(reason.clone()),
            extra: None,
        }
    }
    pub fn upload(user: &String, dest: &String, size: usize) -> Self {
        Self {
            event_type: EventType::Upload,
            user: Some(user.clone()),
            time: -1,
            reason: Some(format!("{size}")),
            extra: Some(dest.clone()),
        }
    }
    pub fn info(msg: &String) -> Self {
        Self {
            event_type: EventType::Info,
            user: None,
            time: -1,
            reason: Some(msg.clone()),
            extra: None,
        }
    }
}
