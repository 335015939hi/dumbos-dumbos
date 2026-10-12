use crate::log;
use crate::tables::EventsTable;
use chrono::DateTime;
use chrono::Local;
use clap::builder::Str;
use serde::Deserialize;
use serde::Serialize;
use std::sync::Mutex;
use std::sync::OnceLock;

#[derive(Serialize, Deserialize)]
pub enum EventType {
    Code,
    Upload,
    CodeRejected,
    UploadRejected,
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

impl Event {
    pub fn dispatch(&mut self) {
        self.time = chrono::Utc::now().timestamp();
        let table: &mut EventsTable = &mut EVENTS_TABLE.get().unwrap().lock().unwrap();
        match table.push(self) {
            Ok(_) => {}
            Err(e) => panic!("{e}"),
        }
        self.print();
    }
    pub fn print(&self) {
        let mut s = match DateTime::from_timestamp(self.time, 0) {
            None => String::from(""),
            Some(t) => {
                let t = t.with_timezone(&Local);
                t.format("[%Y-%m-%d %H:%M:%S] ").to_string()
            }
        };
        let user_str;
        s += match &self.user {
            Some(u) => {
                user_str = format!("user '{u}' ");
                user_str.as_str()
            }
            None => "",
        };
        let mut is_error = false;
        match self.event_type {
            EventType::Code => {
                s += "used code ";
                s += match &self.extra {
                    Some(s) => s.as_str(),
                    None => "(unknown)",
                };
            }
            EventType::CodeRejected => {
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
                is_error = true;
            }
            EventType::Upload => {
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
                s += "denied upload because ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "",
                };
                is_error = true;
            }

            EventType::Error => {
                s += match &self.extra {
                    Some(s) => s.as_str(),
                    None => "something",
                };
                s += " failed because ";
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "unknown cause",
                };
                is_error = true;
            }
            EventType::Info => {}
        }
        match is_error {
            true => log::error(s.as_str()),
            false => log::log(s.as_str()),
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
    pub fn reject_code(user: &String, code: &String, reason: &String) -> Self {
        Self {
            event_type: EventType::CodeRejected,
            user: Some(user.clone()),
            time: -1,
            reason: Some(reason.clone()),
            extra: Some(code.clone()),
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
}
