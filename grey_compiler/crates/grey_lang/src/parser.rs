//! Minimal parser for Grey programs
//! 
//! This module provides a basic recursive descent parser for Grey source code.

use crate::lexer::{Token, SpannedToken};
use crate::ast::*;
use crate::diagnostics::{DiagnosticError, Diagnostic};

/// Parser implementation
pub struct Parser<'a> {
    tokens: &'a [SpannedToken],
    current: usize,
}

impl<'a> Parser<'a> {
    /// Create a new parser with the given token stream
    pub fn new(tokens: &'a [SpannedToken]) -> Self {
        Self {
            tokens,
            current: 0,
        }
    }
    
    /// Parse the complete program
    pub fn parse_program(mut self) -> Result<Program, Box<dyn Diagnostic>> {
        let mut modules = Vec::new();
        
        while !self.is_at_end() && !self.check(Token::Eof) {
            match self.parse_module() {
                Ok(module) => modules.push(module),
                Err(e) => return Err(e),
            }
        }
        
        Ok(Program { modules })
    }
    
    /// Parse a module
    fn parse_module(&mut self) -> Result<Module, Box<dyn Diagnostic>> {
        self.consume(Token::Module, "Expected 'module'")?;
        let name = self.consume_identifier("Expected module name")?;
        self.consume(Token::LBrace, "Expected '{' after module name")?;
        
        let mut constants = Vec::new();
        let mut processes = Vec::new();
        let mut events = Vec::new();
        
        while !self.check(Token::RBrace) && !self.is_at_end() {
            match &self.peek().token {
                Token::Const => constants.push(self.parse_constant()?),
                Token::Process => processes.push(self.parse_process()?),
                Token::Event => events.push(self.parse_event()?),
                _ => {
                    return Err(Box::new(DiagnosticError::general(
                        "Expected constant, process, or event definition",
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
            }
        }
        
        self.consume(Token::RBrace, "Expected '}' to close module")?;
        
        Ok(Module {
            name,
            constants,
            processes,
            events,
        })
    }
    
    /// Parse a constant declaration
    fn parse_constant(&mut self) -> Result<ConstantDeclaration, Box<dyn Diagnostic>> {
        self.consume(Token::Const, "Expected 'const'")?;
        let name = self.consume_identifier("Expected constant name")?;
        self.consume(Token::Assign, "Expected '=' after constant name")?;
        let value = self.parse_expression()?;
        self.consume(Token::Semicolon, "Expected ';' after constant")?;
        
        Ok(ConstantDeclaration { name, value })
    }
    
    /// Parse a process definition
    fn parse_process(&mut self) -> Result<ProcessDefinition, Box<dyn Diagnostic>> {
        self.consume(Token::Process, "Expected 'process'")?;
        let name = self.consume_identifier("Expected process name")?;
        self.consume(Token::LBrace, "Expected '{' after process name")?;
        
        let mut fields = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(Token::RBrace) && !self.is_at_end() {
            match &self.peek().token {
                Token::Identifier(_) => {
                    // Look ahead to see if this is a field or method
                    if self.peek_at(1).token == Token::Colon {
                        fields.push(self.parse_field()?);
                    } else {
                        methods.push(self.parse_method()?);
                    }
                }
                Token::Fn => methods.push(self.parse_method()?),
                _ => {
                    return Err(Box::new(DiagnosticError::general(
                        "Expected field or method definition",
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
            }
        }
        
        self.consume(Token::RBrace, "Expected '}' to close process")?;
        
        Ok(ProcessDefinition { name, fields, methods })
    }
    
    /// Parse a field declaration
    fn parse_field(&mut self) -> Result<FieldDeclaration, Box<dyn Diagnostic>> {
        let name = self.consume_identifier("Expected field name")?;
        self.consume(Token::Colon, "Expected ':' after field name")?;
        let field_type = self.parse_type()?;
        self.consume(Token::Semicolon, "Expected ';' after field declaration")?;
        
        Ok(FieldDeclaration { name, field_type })
    }
    
    /// Parse an event definition
    fn parse_event(&mut self) -> Result<EventDefinition, Box<dyn Diagnostic>> {
        self.consume(Token::Event, "Expected 'event'")?;
        let name = self.consume_identifier("Expected event name")?;
        self.consume(Token::LBrace, "Expected '{' after event name")?;
        
        let mut fields = Vec::new();
        
        while !self.check(Token::RBrace) && !self.is_at_end() {
            if matches!(self.peek().token, Token::Identifier(_)) {
                fields.push(self.parse_field()?);
            } else {
                return Err(Box::new(DiagnosticError::general(
                    "Expected field declaration in event",
                    crate::diagnostics::SourceLocation::dummy(),
                )));
            }
        }
        
        self.consume(Token::RBrace, "Expected '}' to close event")?;
        
        Ok(EventDefinition { name, fields })
    }
    
    /// Parse a method definition
    fn parse_method(&mut self) -> Result<FunctionDefinition, Box<dyn Diagnostic>> {
        self.consume(Token::Fn, "Expected 'fn'")?;
        let name = self.consume_identifier("Expected method name")?;
        self.consume(Token::LParen, "Expected '(' after method name")?;
        let parameters = self.parse_parameter_list()?;
        self.consume(Token::RParen, "Expected ')' after parameter list")?;
        
        let return_type = if self.consume(Token::Arrow, "Expected return type or '{'").is_ok() {
            Some(self.parse_type()?)
        } else {
            None
        };
        
        let body = self.parse_block_expression()?;
        
        Ok(FunctionDefinition {
            name,
            parameters,
            return_type,
            body,
        })
    }
    
    /// Parse parameter list
    fn parse_parameter_list(&mut self) -> Result<Vec<FunctionParameter>, Box<dyn Diagnostic>> {
        let mut parameters = Vec::new();
        
        if !self.check(Token::RParen) {
            loop {
                let name = self.consume_identifier("Expected parameter name")?;
                self.consume(Token::Colon, "Expected ':' after parameter name")?;
                let param_type = self.parse_type()?;
                
                parameters.push(FunctionParameter { name, param_type });
                
                if !self.consume(Token::Comma, "Expected ',' between parameters").is_ok() {
                    break;
                }
            }
        }
        
        Ok(parameters)
    }
    
    /// Parse a type
    fn parse_type(&mut self) -> Result<Type, Box<dyn Diagnostic>> {
        let token = &self.peek().token;
        match token {
            Token::Identifier(name) => {
                let name_clone = name.clone();
                self.advance();
                Ok(Type::Named(name_clone))
            }
            Token::CoordLiteral => {
                self.advance();
                Ok(Type::Coord)
            }
            _ => {
                return Err(Box::new(DiagnosticError::general(
                    "Expected type specification",
                    crate::diagnostics::SourceLocation::dummy(),
                )));
            }
        }
    }
    
    /// Parse a block expression
    fn parse_block_expression(&mut self) -> Result<BlockExpression, Box<dyn Diagnostic>> {
        self.consume(Token::LBrace, "Expected '{' to start block")?;
        
        let mut statements = Vec::new();
        
        while !self.check(Token::RBrace) && !self.is_at_end() {
            statements.push(self.parse_statement()?);
        }
        
        self.consume(Token::RBrace, "Expected '}' to close block")?;
        
        Ok(BlockExpression {
            statements,
            result: None,
        })
    }
    
    /// Parse a statement
    fn parse_statement(&mut self) -> Result<Statement, Box<dyn Diagnostic>> {
        match &self.peek().token {
            Token::Let => {
                self.advance();
                let pattern = Pattern::Identifier(self.consume_identifier("Expected pattern")?);
                self.consume(Token::Assign, "Expected '=' after pattern")?;
                let value = self.parse_expression()?;
                self.consume(Token::Semicolon, "Expected ';' after statement")?;
                Ok(Statement::Let { pattern, value })
            }
            Token::Return => {
                self.advance();
                let value = if !self.check(Token::Semicolon) {
                    Some(self.parse_expression()?)
                } else {
                    None
                };
                self.consume(Token::Semicolon, "Expected ';' after return statement")?;
                Ok(Statement::Return(value))
            }
            _ => {
                let expr = self.parse_expression()?;
                self.consume(Token::Semicolon, "Expected ';' after expression statement")?;
                Ok(Statement::Expression(expr))
            }
        }
    }
    
    /// Parse an expression
    fn parse_expression(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        let token = &self.peek().token;
        match token {
            Token::Integer(value) => {
                let value_copy = *value;
                self.advance();
                Ok(Expression::Integer(value_copy))
            }
            Token::String(value) => {
                let value_copy = value.clone();
                self.advance();
                Ok(Expression::String(value_copy))
            }
            Token::Identifier(name) => {
                let identifier = name.clone();
                self.advance();
                
                // Check for function call
                if self.consume(Token::LParen, "Expected '(' for function call").is_ok() {
                    let arguments = self.parse_expression_list(Token::RParen)?;
                    Ok(Expression::Call {
                        function: Box::new(Expression::Identifier(identifier)),
                        arguments,
                    })
                } else {
                    Ok(Expression::Identifier(identifier))
                }
            }
            Token::CoordLiteral => {
                self.advance();
                Ok(Expression::CoordLiteral)
            }
            _ => {
                return Err(Box::new(DiagnosticError::general(
                    "Expected expression",
                    crate::diagnostics::SourceLocation::dummy(),
                )));
            }
        }
    }
    
    /// Parse expression list
    fn parse_expression_list(&mut self, end_token: Token) -> Result<Vec<Expression>, Box<dyn Diagnostic>> {
        let mut expressions = Vec::new();
        
        if !self.check(end_token.clone()) {
            loop {
                expressions.push(self.parse_expression()?);
                if !self.consume(Token::Comma, "Expected ',' between expressions").is_ok() {
                    break;
                }
            }
        }
        
        self.consume(end_token, "Expected closing delimiter")?;
        Ok(expressions)
    }
    
    // === Utility Methods ===
    
    fn consume_identifier(&mut self, message: &str) -> Result<String, Box<dyn Diagnostic>> {
        if let Token::Identifier(name) = &self.peek().token {
            let name = name.clone();
            self.advance();
            Ok(name)
        } else {
            Err(Box::new(DiagnosticError::general(message, crate::diagnostics::SourceLocation::dummy())))
        }
    }
    
    fn consume(&mut self, expected: Token, message: &str) -> Result<(), Box<dyn Diagnostic>> {
        if self.check(expected.clone()) {
            self.advance();
            Ok(())
        } else {
            Err(Box::new(DiagnosticError::general(message, crate::diagnostics::SourceLocation::dummy())))
        }
    }
    
    fn advance(&mut self) -> &SpannedToken {
        if !self.is_at_end() {
            self.current += 1;
        }
        self.previous()
    }
    
    fn check(&self, token: Token) -> bool {
        if self.is_at_end() {
            false
        } else {
            std::mem::discriminant(&self.peek().token) == std::mem::discriminant(&token)
        }
    }
    
    fn peek(&self) -> &SpannedToken {
        &self.tokens[self.current]
    }
    
    fn peek_at(&self, offset: usize) -> &SpannedToken {
        &self.tokens[self.current + offset]
    }
    
    fn previous(&self) -> &SpannedToken {
        &self.tokens[self.current - 1]
    }
    
    fn is_at_end(&self) -> bool {
        self.current >= self.tokens.len() || self.check(Token::Eof)
    }
}

/// Main parsing function
pub fn parse_program(tokens: &[SpannedToken]) -> Result<Program, Box<dyn Diagnostic>> {
    Parser::new(tokens).parse_program()
}