pub enum EventType {
    Code,
    Upload,
    CodeRejected,
    UploadRejected,
}

pub struct Event {
    eventType: EventType,
    user: Option<String>,
    time: u64,
    reason: Option<String>,
    extra: Option<String>,
}
