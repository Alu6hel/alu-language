use serde::Serialize;

#[derive(Serialize)]
pub struct DiagnosticError {
    pub file: String,
    pub line: usize,
    pub column: usize,
    pub severity: String,
    pub message: String,
}

pub struct LspServer;

impl LspServer {
    /// ST-05: IDE Diagnostics Engine for Google Antigravity
    /// Streams JSON capability errors directly to the editor client.
    pub fn stream_error(error: &DiagnosticError) {
        let json = serde_json::to_string(error).unwrap();
        // Stream to Antigravity LSP stdout
        println!("Content-Length: {}\r\n\r\n{}", json.len(), json);
    }
}
