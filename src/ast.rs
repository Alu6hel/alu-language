use crate::lexer::Token;

#[derive(Debug, Clone)]
pub enum ASTNode {
    Import { path: String },
    Routine { name: String, requires: Vec<String>, ensures: Vec<String>, body: Vec<ASTNode> },
    Print { message: String },
    VariableDecl { name: String, value: Box<ASTNode> },
    Alloc { size: String },
    Loop { body: Vec<ASTNode> },
    Condition { check: String, body: Vec<ASTNode> },
    Return { value: String },
    WriteJSON { file: String, status: String },
    Spawn { thread_id: String, body: Vec<ASTNode> },
    Lock { mutex_name: String },
    Unknown { text: String },
}

pub struct Parser {
    tokens: Vec<Token>,
    position: usize,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, position: 0 }
    }

    fn peek(&self) -> Option<&Token> {
        self.tokens.get(self.position)
    }

    fn advance(&mut self) -> Option<&Token> {
        let token = self.tokens.get(self.position);
        self.position += 1;
        token
    }

    fn expect_ident(&mut self) -> String {
        match self.advance() {
            Some(Token::Identifier(id)) => id.clone(),
            _ => "unknown".to_string(),
        }
    }

    fn expect_string(&mut self) -> String {
        match self.advance() {
            Some(Token::StringLiteral(s)) => s.clone(),
            _ => "unknown".to_string(),
        }
    }

    fn match_punct(&mut self, punct: &str) -> bool {
        if let Some(Token::Punctuation(p)) = self.peek() {
            if p == punct {
                self.advance();
                return true;
            }
        }
        false
    }
    
    fn match_op(&mut self, op: &str) -> bool {
        if let Some(Token::Operator(o)) = self.peek() {
            if o == op {
                self.advance();
                return true;
            }
        }
        false
    }

    pub fn parse(&mut self) -> Vec<ASTNode> {
        let mut ast = Vec::new();
        while self.position < self.tokens.len() {
            if let Some(token) = self.peek() {
                match token {
                    Token::EOF => break,
                    Token::Keyword(kw) if kw == "import" => {
                        self.advance();
                        let mut path = self.expect_ident();
                        while self.match_op("::") {
                            path.push_str("::");
                            path.push_str(&self.expect_ident());
                        }
                        ast.push(ASTNode::Import { path });
                    }
                    Token::Keyword(kw) if kw == "routine" => {
                        ast.push(self.parse_routine());
                    }
                    _ => {
                        self.advance(); // Skip unknown top-level for now
                    }
                }
            } else {
                break;
            }
        }
        ast
    }

    fn parse_routine(&mut self) -> ASTNode {
        self.advance(); // skip 'routine'
        let name = self.expect_ident();
        self.match_punct("(");
        while let Some(t) = self.peek() {
            match t {
                Token::Punctuation(p) if p == ")" => {
                    self.advance();
                    break;
                },
                _ => { self.advance(); }
            }
        }
        
        let mut requires = Vec::new();
        let mut ensures = Vec::new();
        
        // Check for @requires / @ensures
        while let Some(Token::Keyword(kw)) = self.peek() {
            if kw == "@requires" {
                self.advance();
                self.match_punct("(");
                requires.push(self.parse_expression_string());
                self.match_punct(")");
            } else if kw == "@ensures" {
                self.advance();
                self.match_punct("(");
                ensures.push(self.parse_expression_string());
                self.match_punct(")");
            } else {
                break;
            }
        }

        let body = self.parse_block();
        ASTNode::Routine { name, requires, ensures, body }
    }

    fn parse_block(&mut self) -> Vec<ASTNode> {
        let mut body = Vec::new();
        self.match_punct("{");
        while let Some(token) = self.peek() {
            match token {
                Token::Punctuation(p) if p == "}" => {
                    self.advance();
                    break;
                }
                Token::EOF => break,
                Token::Identifier(id) => {
                    let id_cloned = id.clone();
                    if id_cloned == "print" {
                        self.advance();
                        self.match_punct("(");
                        let msg = self.expect_string();
                        self.match_punct(")");
                        body.push(ASTNode::Print { message: msg });
                    } else if id_cloned == "write_status" {
                        self.advance();
                        self.match_punct("(");
                        let file = self.expect_string();
                        self.match_punct(",");
                        let status = self.expect_string();
                        self.match_punct(")");
                        body.push(ASTNode::WriteJSON { file, status });
                    } else {
                        // generic function call parsing (mock for unknown)
                        let expr = self.parse_expression_string();
                        body.push(ASTNode::Unknown { text: expr });
                    }
                }
                Token::Keyword(kw) => {
                    let kw_cloned = kw.clone();
                    if kw_cloned == "reg" {
                        self.advance();
                        let name = self.expect_ident();
                        self.match_op("=");
                        let expr = self.parse_expression();
                        body.push(ASTNode::VariableDecl { name, value: Box::new(expr) });
                    } else if kw_cloned == "while" {
                        self.advance();
                        self.match_punct("(");
                        self.parse_expression_string(); // skip condition for now
                        self.match_punct(")");
                        let loop_body = self.parse_block();
                        body.push(ASTNode::Loop { body: loop_body });
                    } else if kw_cloned == "if" {
                        self.advance();
                        self.match_punct("(");
                        let check = self.parse_expression_string();
                        self.match_punct(")");
                        let if_body = self.parse_block();
                        body.push(ASTNode::Condition { check, body: if_body });
                    } else {
                        self.advance();
                    }
                }
                _ => {
                    self.advance();
                }
            }
        }
        body
    }
    
    fn parse_expression(&mut self) -> ASTNode {
        if let Some(Token::Identifier(id)) = self.peek() {
            if id == "alloc" {
                self.advance();
                self.match_punct("(");
                
                let mut size = String::new();
                if self.match_op("-") {
                    size.push('-');
                }
                
                if let Some(Token::Number(n)) = self.peek() {
                    size.push_str(&n.to_string());
                    self.advance();
                } else {
                    size.push_str("0");
                }
                
                self.match_punct(")");
                return ASTNode::Alloc { size };
            } else {
                let id_clone = id.clone();
                // Check if it's a function call
                let mut is_call = false;
                if self.tokens.len() > self.position + 1 {
                    if let Token::Punctuation(p) = &self.tokens[self.position + 1] {
                        if p == "(" {
                            is_call = true;
                        }
                    }
                }
                
                if is_call {
                    self.advance(); // consume id
                    self.match_punct("(");
                    self.match_punct(")");
                    return ASTNode::Unknown { text: format!("CALL {}", id_clone) };
                }
            }
        }
        let expr_str = self.parse_expression_string();
        ASTNode::Unknown { text: expr_str }
    }
    
    fn parse_expression_string(&mut self) -> String {
        // Just consume tokens until we hit a punctuation that closes it or a newline
        let mut res = String::new();
        while let Some(t) = self.peek() {
            match t {
                Token::Punctuation(p) if p == ")" || p == "}" || p == "," => break,
                Token::Identifier(id) => { res.push_str(id); self.advance(); },
                Token::Keyword(kw) => { res.push_str(kw); self.advance(); },
                Token::Operator(op) => { res.push_str(op); self.advance(); },
                Token::Number(n) => { res.push_str(&n.to_string()); self.advance(); },
                Token::StringLiteral(s) => { res.push('"'); res.push_str(s); res.push('"'); self.advance(); },
                Token::Punctuation(p) => { res.push_str(p); self.advance(); },
                Token::EOF => break,
            }
            res.push(' ');
        }
        res.trim().to_string()
    }
}
