//! Minimal parser for Grey programs
//!
//! This module provides a basic recursive descent parser for Grey source code.

use crate::ast::*;
use crate::diagnostics::{Diagnostic, DiagnosticError};
use crate::lexer::{SpannedToken, Token};

/// Parser implementation
pub struct Parser<'a> {
    tokens: &'a [SpannedToken],
    current: usize,
}

impl<'a> Parser<'a> {
    /// Create a new parser with the given token stream
    pub fn new(tokens: &'a [SpannedToken]) -> Self {
        Self { tokens, current: 0 }
    }

    /// Parse the complete program
    pub fn parse_program(mut self) -> Result<Program, Box<dyn Diagnostic>> {
        let mut modules = Vec::new();

        while !self.is_at_end() {
            if self.check(&Token::Eof) {
                break;
            }

            modules.push(self.parse_module()?);
        }

        Ok(Program { modules })
    }

    fn parse_module(&mut self) -> Result<Module, Box<dyn Diagnostic>> {
        self.consume(&Token::Module, "Expected 'module'")?;
        let name = self.consume_identifier("Expected module name")?;
        self.consume(&Token::LBrace, "Expected '{' after module name")?;

        let mut constants = Vec::new();
        let mut processes = Vec::new();
        let mut events = Vec::new();

        while !self.check(&Token::RBrace) && !self.is_at_end() {
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

        self.consume(&Token::RBrace, "Expected '}' to close module")?;

        Ok(Module {
            name,
            constants,
            processes,
            events,
        })
    }

    fn parse_constant(&mut self) -> Result<ConstantDeclaration, Box<dyn Diagnostic>> {
        self.consume(&Token::Const, "Expected 'const'")?;
        let name = self.consume_identifier("Expected constant name")?;
        self.consume(&Token::Assign, "Expected '=' after constant name")?;
        let value = self.parse_expression()?;
        self.consume(&Token::Semicolon, "Expected ';' after constant")?;

        Ok(ConstantDeclaration { name, value })
    }

    fn parse_process(&mut self) -> Result<ProcessDefinition, Box<dyn Diagnostic>> {
        self.consume(&Token::Process, "Expected 'process'")?;
        let name = self.consume_identifier("Expected process name")?;
        self.consume(&Token::LBrace, "Expected '{' after process name")?;

        let mut fields = Vec::new();
        let mut methods = Vec::new();

        while !self.check(&Token::RBrace) && !self.is_at_end() {
            match &self.peek().token {
                Token::Fn => methods.push(self.parse_method()?),
                Token::Identifier(_) => {
                    if self.peek_n(1).map(|t| &t.token) == Some(&Token::Colon) {
                        fields.push(self.parse_field_declaration()?);
                        self.consume_optional_field_separator();
                    } else {
                        return Err(Box::new(DiagnosticError::general(
                            "Expected field declaration or method definition",
                            crate::diagnostics::SourceLocation::dummy(),
                        )));
                    }
                }
                Token::Comma | Token::Semicolon => {
                    self.advance();
                }
                _ => {
                    return Err(Box::new(DiagnosticError::general(
                        "Expected field declaration or method definition",
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
            }
        }

        self.consume(&Token::RBrace, "Expected '}' to close process")?;

        Ok(ProcessDefinition { name, fields, methods })
    }

    fn parse_event(&mut self) -> Result<EventDefinition, Box<dyn Diagnostic>> {
        self.consume(&Token::Event, "Expected 'event'")?;
        let name = self.consume_identifier("Expected event name")?;
        self.consume(&Token::LBrace, "Expected '{' after event name")?;

        let mut fields = Vec::new();

        while !self.check(&Token::RBrace) && !self.is_at_end() {
            match &self.peek().token {
                Token::Identifier(_) => {
                    fields.push(self.parse_field_declaration()?);
                    self.consume_optional_field_separator();
                }
                Token::Comma | Token::Semicolon => {
                    self.advance();
                }
                _ => {
                    return Err(Box::new(DiagnosticError::general(
                        "Expected field declaration in event",
                        crate::diagnostics::SourceLocation::dummy(),
                    )));
                }
            }
        }

        self.consume(&Token::RBrace, "Expected '}' to close event")?;

        Ok(EventDefinition { name, fields })
    }

    fn parse_field_declaration(&mut self) -> Result<FieldDeclaration, Box<dyn Diagnostic>> {
        let name = self.consume_identifier("Expected field name")?;
        self.consume(&Token::Colon, "Expected ':' after field name")?;
        let field_type = self.parse_type()?;

        Ok(FieldDeclaration { name, field_type })
    }

    fn consume_optional_field_separator(&mut self) {
        if self.check(&Token::Comma) || self.check(&Token::Semicolon) {
            self.advance();
        }
    }

    fn parse_method(&mut self) -> Result<FunctionDefinition, Box<dyn Diagnostic>> {
        self.consume(&Token::Fn, "Expected 'fn' or 'method'")?;
        let name = self.consume_identifier("Expected method name")?;
        self.consume(&Token::LParen, "Expected '(' after method name")?;
        let parameters = self.parse_parameter_list()?;
        self.consume(&Token::RParen, "Expected ')' after parameter list")?;

        let return_type = if self.consume_if(&Token::Arrow) {
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

    fn parse_parameter_list(&mut self) -> Result<Vec<FunctionParameter>, Box<dyn Diagnostic>> {
        let mut parameters = Vec::new();

        if self.check(&Token::RParen) {
            return Ok(parameters);
        }

        loop {
            let name = self.consume_identifier("Expected parameter name")?;
            self.consume(&Token::Colon, "Expected ':' after parameter name")?;
            let param_type = self.parse_type()?;
            parameters.push(FunctionParameter { name, param_type });

            if self.consume_if(&Token::Comma) {
                if self.check(&Token::RParen) {
                    break;
                }
                continue;
            }

            break;
        }

        Ok(parameters)
    }

    fn parse_type(&mut self) -> Result<Type, Box<dyn Diagnostic>> {
        match &self.peek().token {
            Token::Identifier(name) => {
                let name = name.clone();
                self.advance();

                Ok(match name.as_str() {
                    "Int" | "int" => Type::Int,
                    "String" | "string" => Type::String,
                    "Bool" | "bool" => Type::Bool,
                    "Coord" | "coord" => Type::Coord,
                    _ => Type::Named(name),
                })
            }
            Token::CoordLiteral => {
                self.advance();
                Ok(Type::Coord)
            }
            _ => Err(Box::new(DiagnosticError::general(
                "Expected type specification",
                crate::diagnostics::SourceLocation::dummy(),
            ))),
        }
    }

    fn parse_block_expression(&mut self) -> Result<BlockExpression, Box<dyn Diagnostic>> {
        self.consume(&Token::LBrace, "Expected '{' to start block")?;

        let mut statements = Vec::new();

        while !self.check(&Token::RBrace) && !self.is_at_end() {
            statements.push(self.parse_statement()?);
        }

        self.consume(&Token::RBrace, "Expected '}' to close block")?;

        Ok(BlockExpression {
            statements,
            result: None,
        })
    }

    fn parse_statement(&mut self) -> Result<Statement, Box<dyn Diagnostic>> {
        match &self.peek().token {
            Token::Let => {
                self.advance();
                let pattern = Pattern::Identifier(self.consume_identifier("Expected pattern")?);
                self.consume(&Token::Assign, "Expected '=' after pattern")?;
                let value = self.parse_expression()?;
                self.consume(&Token::Semicolon, "Expected ';' after statement")?;
                Ok(Statement::Let { pattern, value })
            }
            Token::Return => {
                self.advance();
                let value = if !self.check(&Token::Semicolon) {
                    Some(self.parse_expression()?)
                } else {
                    None
                };
                self.consume(&Token::Semicolon, "Expected ';' after return statement")?;
                Ok(Statement::Return(value))
            }
            Token::If => {
                let merged = self.parse_if_statement_to_statements()?;
                Ok(Statement::Expression(Expression::Block { statements: merged }))
            }
            _ => {
                if let Some(stmt) = self.try_parse_assignment_statement()? {
                    return Ok(stmt);
                }

                let expr = self.parse_expression()?;
                self.consume(&Token::Semicolon, "Expected ';' after expression statement")?;
                Ok(Statement::Expression(expr))
            }
        }
    }

    fn parse_if_statement_to_statements(&mut self) -> Result<Vec<Statement>, Box<dyn Diagnostic>> {
        self.consume(&Token::If, "Expected 'if'")?;
        self.consume(&Token::LParen, "Expected '(' after 'if'")?;
        let _condition = self.parse_expression()?;
        self.consume(&Token::RParen, "Expected ')' after if condition")?;

        let then_block = self.parse_block_expression()?;
        let mut statements = then_block.statements;

        if self.consume_if(&Token::Else) {
            if self.check(&Token::If) {
                let else_branch = self.parse_if_statement_to_statements()?;
                statements.extend(else_branch);
            } else {
                let else_block = self.parse_block_expression()?;
                statements.extend(else_block.statements);
            }
        }

        Ok(statements)
    }

    fn try_parse_assignment_statement(&mut self) -> Result<Option<Statement>, Box<dyn Diagnostic>> {
        // this.field = expr;
        if let Some(Token::Identifier(name)) = self.peek_n(0).map(|t| &t.token) {
            if name == "this"
                && matches!(self.peek_n(1).map(|t| &t.token), Some(Token::Dot))
                && matches!(self.peek_n(2).map(|t| &t.token), Some(Token::Identifier(_)))
                && matches!(self.peek_n(3).map(|t| &t.token), Some(Token::Assign))
            {
                self.advance(); // this
                self.consume(&Token::Dot, "Expected '.' after 'this'")?;
                let field = self.consume_identifier("Expected field name")?;
                self.consume(&Token::Assign, "Expected '=' after field")?;
                let value = self.parse_expression()?;
                self.consume(&Token::Semicolon, "Expected ';' after assignment")?;
                return Ok(Some(Statement::Let {
                    pattern: Pattern::Identifier(field),
                    value,
                }));
            }
        }

        // name = expr;
        if matches!(self.peek().token, Token::Identifier(_))
            && matches!(self.peek_n(1).map(|t| &t.token), Some(Token::Assign))
        {
            let name = self.consume_identifier("Expected assignment target")?;
            self.consume(&Token::Assign, "Expected '=' after assignment target")?;
            let value = self.parse_expression()?;
            self.consume(&Token::Semicolon, "Expected ';' after assignment")?;
            return Ok(Some(Statement::Let {
                pattern: Pattern::Identifier(name),
                value,
            }));
        }

        Ok(None)
    }

    fn parse_expression(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        self.parse_term()
    }

    fn parse_term(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        let mut expr = self.parse_factor()?;

        while self.check(&Token::Plus) || self.check(&Token::Minus) {
            if self.consume_if(&Token::Plus) {
                let right = self.parse_factor()?;
                expr = Expression::Add {
                    left: Box::new(expr),
                    right: Box::new(right),
                };
            } else if self.consume_if(&Token::Minus) {
                let right = self.parse_factor()?;
                expr = Expression::Subtract {
                    left: Box::new(expr),
                    right: Box::new(right),
                };
            }
        }

        Ok(expr)
    }

    fn parse_factor(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        let mut expr = self.parse_unary()?;

        while self.check(&Token::Star) || self.check(&Token::Slash) {
            if self.consume_if(&Token::Star) {
                let right = self.parse_unary()?;
                expr = Expression::Multiply {
                    left: Box::new(expr),
                    right: Box::new(right),
                };
            } else if self.consume_if(&Token::Slash) {
                let right = self.parse_unary()?;
                expr = Expression::Divide {
                    left: Box::new(expr),
                    right: Box::new(right),
                };
            }
        }

        Ok(expr)
    }

    fn parse_unary(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        if self.consume_if(&Token::Bang) {
            // Minimal semantics: parse and discard the '!' operator.
            return self.parse_unary();
        }

        if self.consume_if(&Token::Minus) {
            let expr = self.parse_unary()?;
            return Ok(match expr {
                Expression::Integer(i) => Expression::Integer(-i),
                other => Expression::Subtract {
                    left: Box::new(Expression::Integer(0)),
                    right: Box::new(other),
                },
            });
        }

        self.parse_primary()
    }

    fn parse_primary(&mut self) -> Result<Expression, Box<dyn Diagnostic>> {
        match &self.peek().token {
            Token::Integer(value) => {
                let value = *value;
                self.advance();
                Ok(Expression::Integer(value))
            }
            Token::Boolean(value) => {
                let value = *value;
                self.advance();
                Ok(Expression::Boolean(value))
            }
            Token::String(value) => {
                let value = value.clone();
                self.advance();
                Ok(Expression::String(value))
            }
            Token::Identifier(name) => {
                let mut identifier = name.clone();
                self.advance();

                // this.<field> lowers to identifier "<field>" for now.
                if identifier == "this" && self.consume_if(&Token::Dot) {
                    identifier = self.consume_identifier("Expected field name")?;
                }

                let mut expr = Expression::Identifier(identifier);

                // Call expression
                if self.consume_if(&Token::LParen) {
                    let arguments = self.parse_expression_list(&Token::RParen)?;
                    expr = Expression::Call {
                        function: Box::new(expr),
                        arguments,
                    };
                }

                Ok(expr)
            }
            Token::CoordLiteral => {
                self.advance();
                Ok(Expression::CoordLiteral)
            }
            Token::LParen => {
                self.advance();
                let expr = self.parse_expression()?;
                self.consume(&Token::RParen, "Expected ')' after expression")?;
                Ok(expr)
            }
            _ => Err(Box::new(DiagnosticError::general(
                "Expected expression",
                crate::diagnostics::SourceLocation::dummy(),
            ))),
        }
    }

    fn parse_expression_list(&mut self, end_token: &Token) -> Result<Vec<Expression>, Box<dyn Diagnostic>> {
        let mut expressions = Vec::new();

        if !self.check(end_token) {
            loop {
                expressions.push(self.parse_expression()?);

                if self.consume_if(&Token::Comma) {
                    if self.check(end_token) {
                        break;
                    }
                    continue;
                }

                break;
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
            Err(Box::new(DiagnosticError::general(
                message,
                crate::diagnostics::SourceLocation::dummy(),
            )))
        }
    }

    fn consume(&mut self, expected: &Token, message: &str) -> Result<(), Box<dyn Diagnostic>> {
        if self.check(expected) {
            self.advance();
            Ok(())
        } else {
            Err(Box::new(DiagnosticError::general(
                message,
                crate::diagnostics::SourceLocation::dummy(),
            )))
        }
    }

    fn consume_if(&mut self, expected: &Token) -> bool {
        if self.check(expected) {
            self.advance();
            true
        } else {
            false
        }
    }

    fn advance(&mut self) -> &SpannedToken {
        if !self.is_at_end() {
            self.current += 1;
        }
        self.previous()
    }

    fn check(&self, expected: &Token) -> bool {
        if self.current >= self.tokens.len() {
            return matches!(expected, Token::Eof);
        }

        if matches!(self.tokens[self.current].token, Token::Eof) {
            return matches!(expected, Token::Eof);
        }

        std::mem::discriminant(&self.peek().token) == std::mem::discriminant(expected)
    }

    fn peek(&self) -> &SpannedToken {
        &self.tokens[self.current]
    }

    fn peek_n(&self, offset: usize) -> Option<&SpannedToken> {
        self.tokens.get(self.current + offset)
    }

    fn previous(&self) -> &SpannedToken {
        &self.tokens[self.current - 1]
    }

    fn is_at_end(&self) -> bool {
        self.current >= self.tokens.len() || matches!(self.tokens[self.current].token, Token::Eof)
    }
}

/// Main parsing function
pub fn parse_program(tokens: &[SpannedToken]) -> Result<Program, Box<dyn Diagnostic>> {
    Parser::new(tokens).parse_program()
}
