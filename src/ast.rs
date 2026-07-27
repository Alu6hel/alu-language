use crate::lexer::{Token, Lexer};

#[derive(Debug)]
pub enum AstNode {
    Program(Vec<AstNode>),
    Import { path: Vec<String> },
    Routine { name: String, params: Vec<String>, body: Vec<AstNode> },
    ExternRoutine { name: String, ret_type: String },
    StructDef { name: String, fields: Vec<(String, String)> },
    ConstDef { name: String, value: u64 },
    Requires { expr: String },
    Ensures { expr: String },
}

pub struct Parser<'a> {
    lexer: Lexer<'a>,
    current_token: Token,
}

impl<'a> Parser<'a> {
    pub fn new(mut lexer: Lexer<'a>) -> Self {
        let current_token = lexer.next_token();
        Parser { lexer, current_token }
    }

    fn advance(&mut self) {
        self.current_token = self.lexer.next_token();
    }

    pub fn parse_program(&mut self) -> AstNode {
        let mut statements = Vec::new();
        while self.current_token != Token::EOF {
            if let Some(stmt) = self.parse_statement() {
                statements.push(stmt);
            } else {
                self.advance();
            }
        }
        AstNode::Program(statements)
    }

    fn parse_statement(&mut self) -> Option<AstNode> {
        match &self.current_token {
            Token::Keyword(kw) => match kw.as_str() {
                "import" => self.parse_import(),
                "routine" => self.parse_routine(),
                "extern" => self.parse_extern(),
                "struct" | "pub" => {
                    self.advance(); 
                    if let Token::Keyword(k) = &self.current_token {
                        if k == "struct" {
                            return self.parse_struct();
                        }
                    }
                    None
                }
                "@requires" => self.parse_requires(),
                "@ensures" => self.parse_ensures(),
                _ => {
                    self.advance();
                    None
                }
            },
            _ => {
                self.advance();
                None
            }
        }
    }

    fn parse_import(&mut self) -> Option<AstNode> {
        self.advance(); // consume 'import'
        let mut path = Vec::new();
        while let Token::Identifier(id) = &self.current_token {
            path.push(id.clone());
            self.advance();
            if self.current_token == Token::Operator("::".to_string()) {
                self.advance();
            } else {
                break;
            }
        }
        Some(AstNode::Import { path })
    }

    fn parse_routine(&mut self) -> Option<AstNode> {
        self.advance(); // consume 'routine'
        if let Token::Identifier(name) = &self.current_token {
            let routine_name = name.clone();
            self.advance();
            // Skip params and body for minimal stub
            return Some(AstNode::Routine { name: routine_name, params: vec![], body: vec![] });
        }
        None
    }

    fn parse_extern(&mut self) -> Option<AstNode> {
        self.advance(); // consume 'extern'
        // Skip calling convention
        if let Token::StringLiteral(_) = &self.current_token {
            self.advance();
        }
        if let Token::Keyword(kw) = &self.current_token {
            if kw == "routine" {
                self.advance();
                if let Token::Identifier(name) = &self.current_token {
                    let routine_name = name.clone();
                    self.advance();
                    return Some(AstNode::ExternRoutine { name: routine_name, ret_type: "i32".to_string() });
                }
            }
        }
        None
    }

    fn parse_struct(&mut self) -> Option<AstNode> {
        self.advance(); // consume 'struct'
        if let Token::Identifier(name) = &self.current_token {
            let struct_name = name.clone();
            self.advance();
            return Some(AstNode::StructDef { name: struct_name, fields: vec![] });
        }
        None
    }

    fn parse_requires(&mut self) -> Option<AstNode> {
        self.advance(); // consume '@requires'
        Some(AstNode::Requires { expr: "stub".to_string() })
    }

    fn parse_ensures(&mut self) -> Option<AstNode> {
        self.advance(); // consume '@ensures'
        Some(AstNode::Ensures { expr: "stub".to_string() })
    }
}
