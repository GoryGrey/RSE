//! Minimal lexer for the Grey programming language
//! 
//! This module provides basic tokenization of Grey source code.

use crate::diagnostics::{DiagnosticError, Diagnostic};

/// All possible tokens in Grey
#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Identifier(String),
    Integer(i64),
    String(String),
    Module,
    Process,
    Event,
    Const,
    Fn,
    Let,
    If,
    Else,
    While,
    For,
    Return,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Semicolon,
    Colon,
    Assign,
    Plus,
    Minus,
    Star,
    Slash,
    LessThan,
    GreaterThan,
    Equals,
    NotEquals,
    Arrow,
    Dot,
    At,
    CoordLiteral,
    Eof,
}

/// Spanned token with source location information
#[derive(Debug, Clone, PartialEq)]
pub struct SpannedToken {
    pub token: Token,
    pub span: (usize, usize), // (start, end) byte positions
}

/// Main lexing function
pub fn lex(source: &str) -> Result<Vec<SpannedToken>, Box<dyn Diagnostic>> {
    let mut tokens = Vec::new();
    let chars: Vec<char> = source.chars().collect();
    let mut pos = 0;
    
    while pos < chars.len() {
        let c = chars[pos];
        
        match c {
            // Whitespace
            ' ' | '\t' | '\n' | '\r' => {
                pos += 1;
            }
            // Comments
            '/' if pos + 1 < chars.len() && chars[pos + 1] == '/' => {
                // Skip to end of line
                while pos < chars.len() && chars[pos] != '\n' {
                    pos += 1;
                }
            }
            // Identifiers and keywords
            'a'..='z' | 'A'..='Z' | '_' => {
                let start = pos;
                while pos < chars.len() && (chars[pos].is_ascii_alphanumeric() || chars[pos] == '_') {
                    pos += 1;
                }
                let identifier = chars[start..pos].iter().collect::<String>();
                
                // Check for keywords
                let token = match identifier.as_str() {
                    "module" => Token::Module,
                    "process" => Token::Process,
                    "event" => Token::Event,
                    "fn" => Token::Fn,
                    "let" => Token::Let,
                    "if" => Token::If,
                    "else" => Token::Else,
                    "while" => Token::While,
                    "for" => Token::For,
                    "return" => Token::Return,
                    _ => Token::Identifier(identifier),
                };
                
                tokens.push(SpannedToken {
                    token,
                    span: (start, pos),
                });
            }
            // Integer literals
            '0'..='9' => {
                let start = pos;
                while pos < chars.len() && chars[pos].is_ascii_digit() {
                    pos += 1;
                }
                let num_str = chars[start..pos].iter().collect::<String>();
                
                if let Ok(num) = num_str.parse::<i64>() {
                    tokens.push(SpannedToken {
                        token: Token::Integer(num),
                        span: (start, pos),
                    });
                } else {
                    return Err(Box::new(DiagnosticError::general(
                        &format!("Invalid integer: {}", num_str),
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
            }
            // String literals
            '"' => {
                let start = pos;
                pos += 1; // Skip opening quote
                let mut string_content = String::new();
                
                while pos < chars.len() && chars[pos] != '"' {
                    if chars[pos] == '\\' && pos + 1 < chars.len() {
                        // Handle escape sequences
                        pos += 1;
                        match chars[pos] {
                            'n' => string_content.push('\n'),
                            't' => string_content.push('\t'),
                            'r' => string_content.push('\r'),
                            '\\' => string_content.push('\\'),
                            '"' => string_content.push('"'),
                            _ => string_content.push(chars[pos]),
                        }
                    } else {
                        string_content.push(chars[pos]);
                    }
                    pos += 1;
                }
                
                if pos >= chars.len() {
                    return Err(Box::new(DiagnosticError::general(
                        "Unterminated string literal",
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
                
                pos += 1; // Skip closing quote
                
                tokens.push(SpannedToken {
                    token: Token::String(string_content),
                    span: (start, pos),
                });
            }
            // Coordinate literal (e.g. "<1, 2>") or '<' operator
            '<' => {
                let start = pos;

                // Try to lex a coordinate literal: '<' ... '>' with only digits, minus, commas, and
                // whitespace inside. If it doesn't match, fall back to a LessThan token.
                let mut i = pos + 1;
                while i < chars.len() && chars[i].is_ascii_whitespace() {
                    i += 1;
                }

                let mut saw_digit = false;
                while i < chars.len() {
                    let ch = chars[i];
                    if ch.is_ascii_digit() {
                        saw_digit = true;
                        i += 1;
                        continue;
                    }
                    if ch == '-' || ch == ',' || ch.is_ascii_whitespace() {
                        i += 1;
                        continue;
                    }
                    if ch == '>' && saw_digit {
                        pos = i + 1;
                        tokens.push(SpannedToken {
                            token: Token::CoordLiteral,
                            span: (start, pos),
                        });
                        break;
                    }

                    tokens.push(SpannedToken {
                        token: Token::LessThan,
                        span: (pos, pos + 1),
                    });
                    pos += 1;
                    break;
                }

                if i >= chars.len() {
                    tokens.push(SpannedToken {
                        token: Token::LessThan,
                        span: (pos, pos + 1),
                    });
                    pos += 1;
                }
            }
            // Single character tokens
            '(' => {
                tokens.push(SpannedToken {
                    token: Token::LParen,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            ')' => {
                tokens.push(SpannedToken {
                    token: Token::RParen,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '{' => {
                tokens.push(SpannedToken {
                    token: Token::LBrace,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '}' => {
                tokens.push(SpannedToken {
                    token: Token::RBrace,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '[' => {
                tokens.push(SpannedToken {
                    token: Token::LBracket,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            ']' => {
                tokens.push(SpannedToken {
                    token: Token::RBracket,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            ',' => {
                tokens.push(SpannedToken {
                    token: Token::Comma,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            ';' => {
                tokens.push(SpannedToken {
                    token: Token::Semicolon,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            ':' => {
                tokens.push(SpannedToken {
                    token: Token::Colon,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '=' => {
                if pos + 1 < chars.len() && chars[pos + 1] == '>' {
                    tokens.push(SpannedToken {
                        token: Token::Arrow,
                        span: (pos, pos + 2),
                    });
                    pos += 2;
                } else {
                    tokens.push(SpannedToken {
                        token: Token::Assign,
                        span: (pos, pos + 1),
                    });
                    pos += 1;
                }
            }
            '+' => {
                tokens.push(SpannedToken {
                    token: Token::Plus,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '-' => {
                tokens.push(SpannedToken {
                    token: Token::Minus,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '*' => {
                tokens.push(SpannedToken {
                    token: Token::Star,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '/' => {
                tokens.push(SpannedToken {
                    token: Token::Slash,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '>' => {
                tokens.push(SpannedToken {
                    token: Token::GreaterThan,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '.' => {
                tokens.push(SpannedToken {
                    token: Token::Dot,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            '@' => {
                tokens.push(SpannedToken {
                    token: Token::At,
                    span: (pos, pos + 1),
                });
                pos += 1;
            }
            // Unknown character
            _ => {
                return Err(Box::new(DiagnosticError::general(
                    &format!("Unexpected character: {}", c),
                    crate::diagnostics::SourceLocation::dummy(),
                )));
            }
        }
    }
    
    // Add EOF token
    tokens.push(SpannedToken {
        token: Token::Eof,
        span: (chars.len(), chars.len()),
    });
    
    Ok(tokens)
}