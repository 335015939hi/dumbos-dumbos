use crate::log;
use chrono::DateTime;
use chrono::Local;
use serde::Deserialize;
use serde::Serialize;

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

impl Event {
    pub fn dispatch(&mut self) {
        self.time = chrono::Utc::now().timestamp();
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
                s += match &self.reason {
                    Some(s) => s.as_str(),
                    None => "",
                };
                is_error = true;
            }
            EventType::Upload => {}
            EventType::UploadRejected => {}
            EventType::Error => {}
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
            event_type: EventType::Code,
            user: Some(user.clone()),
            time: -1,
            reason: Some(reason.clone()),
            extra: Some(code.clone()),
        }
    }
}
