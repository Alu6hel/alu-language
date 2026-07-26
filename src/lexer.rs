#[derive(Debug, PartialEq)]
pub enum Token {
    Number(i64),
    Routine,
    Cap,
    Register,
    ScopeStart,
    Unsafe,
    Ident(String),
    Eof,
}

pub struct Lexer<'a> {
    input: &'a str,
    position: usize,
}

impl<'a> Lexer<'a> {
    pub fn new(input: &'a str) -> Self {
        Lexer { input, position: 0 }
    }

    pub fn next_token(&mut self) -> Token {
        self.skip_whitespace();

        if self.position >= self.input.len() {
            return Token::Eof;
        }

        let ch = self.input.as_bytes()[self.position] as char;
        
        // Slice 1: Number parsing
        if ch.is_digit(10) {
            return self.read_number();
        }

        // Slice 2: Keyword and Identifier parsing
        if ch.is_alphabetic() {
            let ident = self.read_identifier();
            return match ident.as_str() {
                "routine" => Token::Routine,
                "cap" => Token::Cap,
                "reg" => Token::Register,
                "scope" => Token::ScopeStart,
                "unsafe" => Token::Unsafe,
                _ => Token::Ident(ident),
            };
        }

        self.position += 1;
        Token::Eof // Fallback for unknown chars right now
    }

    fn skip_whitespace(&mut self) {
        while self.position < self.input.len() {
            let ch = self.input.as_bytes()[self.position] as char;
            if ch.is_whitespace() {
                self.position += 1;
            } else {
                break;
            }
        }
    }

    fn read_number(&mut self) -> Token {
        let start = self.position;
        while self.position < self.input.len() {
            let ch = self.input.as_bytes()[self.position] as char;
            if ch.is_digit(10) {
                self.position += 1;
            } else {
                break;
            }
        }
        let num_str = &self.input[start..self.position];
        let num = num_str.parse::<i64>().unwrap();
        Token::Number(num)
    }

    fn read_identifier(&mut self) -> String {
        let start = self.position;
        while self.position < self.input.len() {
            let ch = self.input.as_bytes()[self.position] as char;
            if ch.is_alphanumeric() || ch == '_' {
                self.position += 1;
            } else {
                break;
            }
        }
        self.input[start..self.position].to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_slice_1_tokens() {
        let input = "  42  1337   ";
        let mut lexer = Lexer::new(input);
        
        assert_eq!(lexer.next_token(), Token::Number(42));
        assert_eq!(lexer.next_token(), Token::Number(1337));
        assert_eq!(lexer.next_token(), Token::Eof);
    }

    #[test]
    fn test_slice_2_tokens() {
        let input = "routine cap reg scope unsafe custom_ident 42";
        let mut lexer = Lexer::new(input);
        
        assert_eq!(lexer.next_token(), Token::Routine);
        assert_eq!(lexer.next_token(), Token::Cap);
        assert_eq!(lexer.next_token(), Token::Register);
        assert_eq!(lexer.next_token(), Token::ScopeStart);
        assert_eq!(lexer.next_token(), Token::Unsafe);
        assert_eq!(lexer.next_token(), Token::Ident("custom_ident".to_string()));
        assert_eq!(lexer.next_token(), Token::Number(42));
        assert_eq!(lexer.next_token(), Token::Eof);
    }
}
