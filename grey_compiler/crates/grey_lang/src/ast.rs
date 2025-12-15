//! Minimal Abstract Syntax Tree for Grey programs
//! 
//! This module defines the basic AST structures for Grey programs.

/// Source location information
#[derive(Debug, Clone, PartialEq)]
pub struct SourceLocation {
    pub line: usize,
    pub column: usize,
    pub span: (usize, usize), // byte positions in source
}

/// Top-level program structure
#[derive(Debug, Clone, PartialEq)]
pub struct Program {
    pub modules: Vec<Module>,
}

/// Module definition
#[derive(Debug, Clone, PartialEq)]
pub struct Module {
    pub name: String,
    pub constants: Vec<ConstantDeclaration>,
    pub processes: Vec<ProcessDefinition>,
    pub events: Vec<EventDefinition>,
}

/// Constant declaration
#[derive(Debug, Clone, PartialEq)]
pub struct ConstantDeclaration {
    pub name: String,
    pub value: Expression,
}

/// Process definition
#[derive(Debug, Clone, PartialEq)]
pub struct ProcessDefinition {
    pub name: String,
    pub fields: Vec<FieldDeclaration>,
    pub methods: Vec<FunctionDefinition>,
}

/// Field declaration in process
#[derive(Debug, Clone, PartialEq)]
pub struct FieldDeclaration {
    pub name: String,
    pub field_type: Type,
}

/// Event definition
#[derive(Debug, Clone, PartialEq)]
pub struct EventDefinition {
    pub name: String,
    pub fields: Vec<FieldDeclaration>,
}

/// Function definition
#[derive(Debug, Clone, PartialEq)]
pub struct FunctionDefinition {
    pub name: String,
    pub parameters: Vec<FunctionParameter>,
    pub return_type: Option<Type>,
    pub body: BlockExpression,
}

/// Function parameter
#[derive(Debug, Clone, PartialEq)]
pub struct FunctionParameter {
    pub name: String,
    pub param_type: Type,
}

/// Expressions
#[derive(Debug, Clone, PartialEq)]
pub enum Expression {
    Integer(i64),
    String(String),
    Identifier(String),
    CoordLiteral,
    
    Add {
        left: Box<Expression>,
        right: Box<Expression>,
    },
    
    Call {
        function: Box<Expression>,
        arguments: Vec<Expression>,
    },
    
    Block {
        statements: Vec<Statement>,
    },
}

/// Statements
#[derive(Debug, Clone, PartialEq)]
pub enum Statement {
    Expression(Expression),
    Let {
        pattern: Pattern,
        value: Expression,
    },
    Return(Option<Expression>),
}

/// Patterns for destructuring
#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    Identifier(String),
}

/// Block expression
#[derive(Debug, Clone, PartialEq)]
pub struct BlockExpression {
    pub statements: Vec<Statement>,
    pub result: Option<Box<Expression>>,
}

/// Type representations
#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Int,
    String,
    Bool,
    Coord,
    Named(String),
}