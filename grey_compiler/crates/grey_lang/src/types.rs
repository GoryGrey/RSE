//! Minimal type system for Grey
//! 
//! This module provides basic type checking for Grey programs.

use crate::ast::*;
use crate::diagnostics::{Diagnostic, DiagnosticError};

/// Typed program with all types resolved
#[derive(Debug, Clone, PartialEq)]
pub struct TypedProgram {
    pub modules: Vec<TypedModule>,
}

/// Typed module
#[derive(Debug, Clone, PartialEq)]
pub struct TypedModule {
    pub name: String,
    pub constants: Vec<TypedConstantDeclaration>,
    pub processes: Vec<TypedProcessDefinition>,
    pub events: Vec<TypedEventDefinition>,
}

/// Typed constant declaration
#[derive(Debug, Clone, PartialEq)]
pub struct TypedConstantDeclaration {
    pub name: String,
    pub value: TypedExpression,
}

/// Typed process definition
#[derive(Debug, Clone, PartialEq)]
pub struct TypedProcessDefinition {
    pub name: String,
    pub fields: Vec<TypedFieldDeclaration>,
    pub methods: Vec<TypedFunctionDefinition>,
}

/// Typed field declaration
#[derive(Debug, Clone, PartialEq)]
pub struct TypedFieldDeclaration {
    pub name: String,
    pub field_type: Type,
}

/// Typed event definition
#[derive(Debug, Clone, PartialEq)]
pub struct TypedEventDefinition {
    pub name: String,
    pub fields: Vec<TypedFieldDeclaration>,
}

/// Typed function definition
#[derive(Debug, Clone, PartialEq)]
pub struct TypedFunctionDefinition {
    pub name: String,
    pub parameters: Vec<TypedFunctionParameter>,
    pub return_type: Type,
    pub body: TypedBlockExpression,
}

/// Typed function parameter
#[derive(Debug, Clone, PartialEq)]
pub struct TypedFunctionParameter {
    pub name: String,
    pub param_type: Type,
}

/// Typed expression with resolved types
#[derive(Debug, Clone, PartialEq)]
pub struct TypedExpression {
    pub expression: Expression,
    pub type_: Type,
}

/// Typed block expression
#[derive(Debug, Clone, PartialEq)]
pub struct TypedBlockExpression {
    pub statements: Vec<TypedStatement>,
    pub result: Option<Box<TypedExpression>>,
    pub type_: Type,
}

/// Typed statement
#[derive(Debug, Clone, PartialEq)]
pub enum TypedStatement {
    Expression(TypedExpression),
    Let {
        pattern: Pattern,
        value: TypedExpression,
    },
    Return(Option<TypedExpression>),
}

/// Type representation for the type system
#[derive(Debug, Clone, PartialEq, Hash, Eq)]
pub enum Type {
    Int,
    String,
    Bool,
    Coord,
    Named(String),
    Unit,
}

impl Type {
    /// Get the type name as a string
    pub fn type_name(&self) -> String {
        match self {
            Type::Int => "int".to_string(),
            Type::String => "string".to_string(),
            Type::Bool => "bool".to_string(),
            Type::Coord => "coord".to_string(),
            Type::Named(name) => name.clone(),
            Type::Unit => "()".to_string(),
        }
    }
}

/// Type checking context
pub struct TypeChecker {
    /// Errors encountered during type checking
    errors: Vec<Box<dyn Diagnostic>>,
}

impl TypeChecker {
    /// Create a new type checker
    pub fn new() -> Self {
        Self {
            errors: Vec::new(),
        }
    }
    
    /// Type check a complete program
    pub fn check_program(&mut self, program: &Program) -> Result<TypedProgram, Box<dyn Diagnostic>> {
        // Clear previous errors
        self.errors.clear();
        
        // Type check each module
        let mut typed_modules = Vec::new();
        for module in &program.modules {
            let typed_module = self.check_module(module)?;
            typed_modules.push(typed_module);
        }
        
        if !self.errors.is_empty() {
            return Err(self.errors.remove(0));
        }
        
        Ok(TypedProgram {
            modules: typed_modules,
        })
    }
    
    /// Type check a module
    fn check_module(&mut self, module: &Module) -> Result<TypedModule, Box<dyn Diagnostic>> {
        // Type check constants
        let mut typed_constants = Vec::new();
        for constant in &module.constants {
            let typed_constant = self.check_constant(constant)?;
            typed_constants.push(typed_constant);
        }
        
        // Type check events
        let mut typed_events = Vec::new();
        for event in &module.events {
            let typed_event = self.check_event(event)?;
            typed_events.push(typed_event);
        }
        
        // Type check processes
        let mut typed_processes = Vec::new();
        for process in &module.processes {
            let typed_process = self.check_process(process)?;
            typed_processes.push(typed_process);
        }
        
        Ok(TypedModule {
            name: module.name.clone(),
            constants: typed_constants,
            processes: typed_processes,
            events: typed_events,
        })
    }
    
    /// Type check a constant declaration
    fn check_constant(&mut self, constant: &ConstantDeclaration) -> Result<TypedConstantDeclaration, Box<dyn Diagnostic>> {
        let value_type = self.check_expression(&constant.value)?;
        
        Ok(TypedConstantDeclaration {
            name: constant.name.clone(),
            value: value_type,
        })
    }
    
    /// Type check a process definition
    fn check_process(&mut self, process: &ProcessDefinition) -> Result<TypedProcessDefinition, Box<dyn Diagnostic>> {
        // Type check fields
        let mut typed_fields = Vec::new();
        for field in &process.fields {
            typed_fields.push(TypedFieldDeclaration {
                name: field.name.clone(),
                field_type: self.convert_ast_type(&field.field_type)?,
            });
        }
        
        // Type check methods
        let mut typed_methods = Vec::new();
        for method in &process.methods {
            let typed_method = self.check_function_definition(method)?;
            typed_methods.push(typed_method);
        }
        
        Ok(TypedProcessDefinition {
            name: process.name.clone(),
            fields: typed_fields,
            methods: typed_methods,
        })
    }
    
    /// Type check an event definition
    fn check_event(&mut self, event: &EventDefinition) -> Result<TypedEventDefinition, Box<dyn Diagnostic>> {
        // Type check fields
        let mut typed_fields = Vec::new();
        for field in &event.fields {
            typed_fields.push(TypedFieldDeclaration {
                name: field.name.clone(),
                field_type: self.convert_ast_type(&field.field_type)?,
            });
        }
        
        Ok(TypedEventDefinition {
            name: event.name.clone(),
            fields: typed_fields,
        })
    }
    
    /// Type check a function definition
    fn check_function_definition(&mut self, function: &FunctionDefinition) -> Result<TypedFunctionDefinition, Box<dyn Diagnostic>> {
        // Type check parameters
        let mut typed_parameters = Vec::new();
        for param in &function.parameters {
            let converted_type = self.convert_ast_type(param.param_type)?;
            let typed_param = TypedFunctionParameter {
                name: param.name.clone(),
                param_type: converted_type,
            };
            typed_parameters.push(typed_param);
        }
        
        // Type check return type
        let return_type = if let Some(ref ret_type) = function.return_type {
            self.convert_ast_type(ret_type)?
        } else {
            Type::Unit
        };
        
        // Type check body
        let body_type = self.check_block_expression(&function.body)?;
        
        Ok(TypedFunctionDefinition {
            name: function.name.clone(),
            parameters: typed_parameters,
            return_type,
            body: body_type,
        })
    }
    
    /// Type check a block expression
    fn check_block_expression(&mut self, block: &BlockExpression) -> Result<TypedBlockExpression, Box<dyn Diagnostic>> {
        // Type check statements
        let mut typed_statements = Vec::new();
        
        for statement in &block.statements {
            let typed_statement = self.check_statement(statement)?;
            typed_statements.push(typed_statement);
        }
        
        // Type check result expression
        let result_type = if let Some(ref result) = block.result {
            self.check_expression(result)?
        } else {
            TypedExpression {
                expression: Expression::Block { statements: vec![] },
                type_: Type::Unit,
            }
        };
        
        Ok(TypedBlockExpression {
            statements: typed_statements,
            result: Some(Box::new(result_type.clone())),
            type_: result_type.type_.clone(),
        })
    }
    
    /// Type check a statement
    fn check_statement(&mut self, statement: &Statement) -> Result<TypedStatement, Box<dyn Diagnostic>> {
        match statement {
            Statement::Expression(expression) => {
                let typed_expr = self.check_expression(expression)?;
                Ok(TypedStatement::Expression(typed_expr))
            }
            Statement::Let { pattern, value } => {
                let typed_value = self.check_expression(value)?;
                Ok(TypedStatement::Let {
                    pattern: pattern.clone(),
                    value: typed_value,
                })
            }
            Statement::Return(value) => {
                let typed_value = if let Some(ref val) = value {
                    Some(self.check_expression(val)?)
                } else {
                    None
                };
                Ok(TypedStatement::Return(typed_value))
            }
        }
    }
    
    /// Type check an expression
    fn check_expression(&mut self, expression: &Expression) -> Result<TypedExpression, Box<dyn Diagnostic>> {
        match expression {
            Expression::Integer(_value) => {
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Int,
                })
            }
            Expression::String(_value) => {
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::String,
                })
            }
            Expression::Identifier(_name) => {
                // For now, assume identifiers have Unit type
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Unit,
                })
            }
            Expression::CoordLiteral => {
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Coord,
                })
            }
            Expression::Call { .. } => {
                // For now, assume function calls return Unit type
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Unit,
                })
            }
            Expression::Block { .. } => {
                // For now, assume blocks return Unit type
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Unit,
                })
            }
            Expression::Add { .. } => {
                // For now, assume addition returns Unit type
                Ok(TypedExpression {
                    expression: expression.clone(),
                    type_: Type::Unit,
                })
            }
        }
    }
    
    /// Convert AST type to type system type
    fn convert_ast_type(&self, ast_type: &crate::ast::Type) -> Result<Type, Box<dyn Diagnostic>> {
        match ast_type {
            crate::ast::Type::Int => Ok(Type::Int),
            crate::ast::Type::String => Ok(Type::String),
            crate::ast::Type::Bool => Ok(Type::Bool),
            crate::ast::Type::Coord => Ok(Type::Coord),
            crate::ast::Type::Named(name) => Ok(Type::Named(name.clone())),
        }
    }
}

impl Default for TypeChecker {
    fn default() -> Self {
        Self::new()
    }
}