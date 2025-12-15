//! Grey Language Library - Minimal Working Version
//! 
//! This crate provides the core compiler components for the Grey programming language,
//! including basic lexing, parsing, type checking, and validation.

pub mod lexer;
pub mod parser;
pub mod ast;
pub mod types;
pub mod diagnostics;
pub mod constraints;

use crate::diagnostics::{DiagnosticError, Diagnostic};

/// Parse Grey source code into an AST
pub fn parse_source(source: &str) -> Result<ast::Program, Box<dyn Diagnostic>> {
    let tokens = lexer::lex(source)?;
    parser::parse_program(&tokens)
}

/// Type check a parsed Grey program
pub fn type_check_program(program: &ast::Program) -> Result<types::TypedProgram, Box<dyn Diagnostic>> {
    let mut typechecker = types::TypeChecker::new();
    typechecker.check_program(program)
}

/// Validate a typed program against O(1) constraints
pub fn validate_program(program: &types::TypedProgram) -> Result<(), Box<dyn Diagnostic>> {
    let mut validator = constraints::O1Validator::new();
    validator.validate_program(program)
}

/// Compile pipeline: parse -> type check -> validate
pub fn compile(source: &str) -> Result<types::TypedProgram, Box<dyn Diagnostic>> {
    let program = parse_source(source)?;
    let typed_program = type_check_program(&program)?;
    validate_program(&typed_program)?;
    Ok(typed_program)
}