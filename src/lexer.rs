#[derive(Debug, PartialEq, Clone)]
pub enum Token {
    Keyword(String),
    Identifier(String),
    Number(u64),
    StringLiteral(String),
    Operator(String),
    Punctuation(String),
    EOF,
}

pub struct Lexer<'a> {
    input: std::str::Chars<'a>,
    current_char: Option<char>,
}

impl<'a> Lexer<'a> {
    pub fn new(input: &'a str) -> Self {
        let mut lexer = Lexer {
            input: input.chars(),
            current_char: None,
        };
        lexer.advance();
        lexer
    }

    fn advance(&mut self) {
        self.current_char = self.input.next();
    }

    fn skip_whitespace(&mut self) {
        while let Some(c) = self.current_char {
            if c.is_whitespace() {
                self.advance();
            } else {
                break;
            }
        }
    }

    pub fn next_token(&mut self) -> Token {
        self.skip_whitespace();

        if let Some(c) = self.current_char {
            match c {
                'a'..='z' | 'A'..='Z' | '_' | '@' => self.read_identifier_or_keyword(),
                '0'..='9' => self.read_number(),
                '"' => self.read_string(),
                '{' | '}' | '(' | ')' | '[' | ']' | ',' | ';' => {
                    self.advance();
                    Token::Punctuation(c.to_string())
                }
                ':' => {
                    self.advance();
                    if self.current_char == Some(':') {
                        self.advance();
                        Token::Operator("::".to_string())
                    } else {
                        Token::Punctuation(":".to_string())
                    }
                }
                '-' => {
                    self.advance();
                    if self.current_char == Some('>') {
                        self.advance();
                        Token::Operator("->".to_string())
                    } else {
                        Token::Operator("-".to_string())
                    }
                }
                '=' | '+' | '*' | '/' | '<' | '>' => {
                    self.advance();
                    Token::Operator(c.to_string())
                }
                _ => {
                    self.advance();
                    Token::Operator(c.to_string())
                }
            }
        } else {
            Token::EOF
        }
    }

    fn read_identifier_or_keyword(&mut self) -> Token {
        let mut result = String::new();
        while let Some(c) = self.current_char {
            if c.is_alphanumeric() || c == '_' || c == '@' {
                result.push(c);
                self.advance();
            } else {
                break;
            }
        }

        match result.as_str() {
            "import" | "routine" | "extern" | "struct" | "pub" | "const" | "namespace" | "@requires" | "@ensures" | "return" | "reg" | "while" | "if" | "else" | "true" | "false" => {
                Token::Keyword(result)
            }
            _ => Token::Identifier(result),
        }
    }

    fn read_number(&mut self) -> Token {
        let mut result = String::new();
        while let Some(c) = self.current_char {
            if c.is_ascii_hexdigit() || c == 'x' {
                result.push(c);
                self.advance();
            } else {
                break;
            }
        }
        
        let value = if result.starts_with("0x") {
            u64::from_str_radix(&result[2..], 16).unwrap_or(0)
        } else {
            result.parse::<u64>().unwrap_or(0)
        };
        
        Token::Number(value)
    }

    fn read_string(&mut self) -> Token {
        self.advance(); // Skip initial quote
        let mut result = String::new();
        while let Some(c) = self.current_char {
            if c == '"' {
                self.advance(); // Skip ending quote
                break;
            }
            result.push(c);
            self.advance();
        }
        Token::StringLiteral(result)
    }
}
