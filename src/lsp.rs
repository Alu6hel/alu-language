use serde_json::{json, Value};
use std::io::{self, Read, Write};

pub fn run_lsp_server() {
    let mut stdin = io::stdin();
    
    loop {
        // Read headers
        let mut headers = String::new();
        loop {
            let mut byte = [0; 1];
            if stdin.read_exact(&mut byte).is_err() { return; }
            let b = byte[0] as char;
            headers.push(b);
            if headers.ends_with("\r\n\r\n") { break; }
        }

        // Parse Content-Length
        let mut content_length = 0;
        for line in headers.split("\r\n") {
            if line.starts_with("Content-Length:") {
                let parts: Vec<&str> = line.split(':').collect();
                if parts.len() == 2 {
                    content_length = parts[1].trim().parse::<usize>().unwrap_or(0);
                }
            }
        }

        if content_length == 0 { continue; }

        // Read payload
        let mut payload = vec![0; content_length];
        if stdin.read_exact(&mut payload).is_err() { return; }
        
        let payload_str = String::from_utf8_lossy(&payload);
        if let Ok(msg) = serde_json::from_str::<Value>(&payload_str) {
            handle_message(msg);
        }
    }
}

fn handle_message(msg: Value) {
    if let Some(method) = msg.get("method").and_then(|m| m.as_str()) {
        if method == "initialize" {
            let id = msg.get("id").unwrap_or(&json!(null));
            let response = json!({
                "jsonrpc": "2.0",
                "id": id,
                "result": {
                    "capabilities": {
                        "textDocumentSync": 1 // Full sync
                    }
                }
            });
            send_response(&response);
        } else if method == "textDocument/didOpen" || method == "textDocument/didChange" {
            let params = msg.get("params");
            let uri = if method == "textDocument/didOpen" {
                params.and_then(|p| p.get("textDocument")).and_then(|td| td.get("uri")).and_then(|u| u.as_str())
            } else {
                params.and_then(|p| p.get("textDocument")).and_then(|td| td.get("uri")).and_then(|u| u.as_str())
            };
            
            let text = if method == "textDocument/didOpen" {
                params.and_then(|p| p.get("textDocument")).and_then(|td| td.get("text")).and_then(|t| t.as_str())
            } else {
                params.and_then(|p| p.get("contentChanges")).and_then(|cc| cc.get(0)).and_then(|c| c.get("text")).and_then(|t| t.as_str())
            };

            if let (Some(uri), Some(text)) = (uri, text) {
                run_diagnostics(uri, text);
            }
        }
    }
}

fn run_diagnostics(uri: &str, text: &str) {
    let mut lexer = crate::lexer::Lexer::new(text);
    let mut tokens = Vec::new();
    loop {
        let token = lexer.next_token();
        if token == crate::lexer::Token::EOF { break; }
        tokens.push(token);
    }
    
    let mut parser = crate::ast::Parser::new(tokens);
    let ast = parser.parse();
    let verifier = crate::verifier::Verifier::new();
    
    let mut diagnostics = vec![];
    
    if let Err(e) = verifier.verify_ast(&ast) {
        // Simple search for the offending line if possible, fallback to line 0
        let mut error_line = 0;
        let mut error_char_start = 0;
        let mut error_char_end = 100;
        
        // Very basic heuristic: extract the number or text from the error string and find it
        if let Some(idx) = text.find("-") {
            // Find the line number of this offset
            let prefix = &text[..idx];
            error_line = prefix.lines().count().saturating_sub(1);
            if let Some(last_line) = prefix.lines().last() {
                error_char_start = last_line.len();
                error_char_end = error_char_start + 10;
            }
        }

        diagnostics.push(json!({
            "range": {
                "start": {"line": error_line, "character": error_char_start},
                "end": {"line": error_line, "character": error_char_end}
            },
            "severity": 1, // Error
            "source": "ALU Verifier",
            "message": e
        }));
    }

    let response = json!({
        "jsonrpc": "2.0",
        "method": "textDocument/publishDiagnostics",
        "params": {
            "uri": uri,
            "diagnostics": diagnostics
        }
    });
    send_response(&response);
}

fn send_response(response: &Value) {
    let response_str = response.to_string();
    let rpc = format!("Content-Length: {}\r\n\r\n{}", response_str.len(), response_str);
    print!("{}", rpc);
    io::stdout().flush().unwrap();
}
