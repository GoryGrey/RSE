//! Minimal diagnostic system for Grey compiler
//! 
//! This module provides basic error reporting for the Grey language compiler.

use serde::{Deserialize, Serialize};
use std::fmt;

/// Main diagnostic error type
#[derive(Debug, thiserror::Error)]
pub enum DiagnosticError {
    #[error("Compiler error: {message}")]
    General {
        message: String,
        location: SourceLocation,
    },
}

/// Diagnostic trait for compile errors
pub trait Diagnostic: std::error::Error + fmt::Display {
    fn message(&self) -> &str;
    fn location(&self) -> &SourceLocation;
}

impl Diagnostic for DiagnosticError {
    fn message(&self) -> &str {
        match self {
            DiagnosticError::General { message, .. } => message,
        }
    }
    
    fn location(&self) -> &SourceLocation {
        match self {
            DiagnosticError::General { location, .. } => location,
        }
    }
}

impl DiagnosticError {
    /// Create a general error
    pub fn general(message: &str, location: SourceLocation) -> Self {
        Self::General {
            message: message.to_string(),
            location,
        }
    }
}

/// Source location information for diagnostics
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SourceLocation {
    pub line: usize,
    pub column: usize,
    pub span: (usize, usize), // (start, end) byte positions
}

impl SourceLocation {
    /// Create a new source location
    pub fn new(line: usize, column: usize, span: (usize, usize)) -> Self {
        Self { line, column, span }
    }
    
    /// Create a dummy location for errors without source context
    pub fn dummy() -> Self {
        Self {
            line: 0,
            column: 0,
            span: (0, 0),
        }
    }
}

impl fmt::Display for SourceLocation {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "line {}, column {}", self.line, self.column)
    }
}