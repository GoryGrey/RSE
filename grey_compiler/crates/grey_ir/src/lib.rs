//! Grey Intermediate Representation (IR)
//! 
//! Canonical representation for Grey programs suitable for backend code generation.
//! This IR abstracts away language-specific constructs and focuses on the core
//! computational model of processes, events, and state transitions.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use thiserror::Error;

/// Result type for IR operations
pub type Result<T> = std::result::Result<T, IrError>;

/// IR-specific error types
#[derive(Error, Debug)]
pub enum IrError {
    #[error("Type mismatch: {0}")]
    TypeMismatch(String),
    
    #[error("Process not found: {0}")]
    ProcessNotFound(String),
    
    #[error("Event not found: {0}")]
    EventNotFound(String),
    
    #[error("Invalid coordinate: {0}")]
    InvalidCoordinate(String),
    
    #[error("Resource constraint violation: {0}")]
    ResourceConstraint(String),
}

/// 3D coordinate for process placement
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct Coord {
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

impl Coord {
    /// Create a new coordinate
    pub fn new(x: i32, y: i32, z: i32) -> Self {
        Self { x, y, z }
    }
    
    /// Check if coordinate is within valid bounds (0-31 for each dimension)
    pub fn is_valid(&self) -> bool {
        (0..=31).contains(&self.x) && (0..=31).contains(&self.y) && (0..=31).contains(&self.z)
    }
}

/// Top-level IR program
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrProgram {
    pub name: String,
    pub processes: Vec<IrProcess>,
    pub events: Vec<IrEvent>,
    pub constants: HashMap<String, IrValue>,
    pub resources: IrResourceBounds,
}

/// Process definition in IR
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrProcess {
    pub name: String,
    pub coord: Coord,
    pub fields: HashMap<String, IrType>,
    pub initial_state: IrState,
    pub transitions: Vec<IrTransition>,
}

/// Event definition in IR
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrEvent {
    pub name: String,
    pub fields: HashMap<String, IrType>,
}

/// Process state representation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrState {
    pub values: HashMap<String, IrValue>,
}

/// State transition from event handling
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrTransition {
    pub event_type: String,
    pub condition: Option<IrExpression>,
    pub actions: Vec<IrAction>,
}

/// Action performed during state transition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IrAction {
    UpdateField {
        field: String,
        value: IrExpression,
    },
    SendEvent {
        event_type: String,
        target: Coord,
        fields: HashMap<String, IrExpression>,
    },
    SpawnProcess {
        process_type: String,
        coord: Coord,
        initial_state: IrState,
    },
}

/// IR expressions
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IrExpression {
    Constant(IrValue),
    FieldAccess(String),
    Arithmetic {
        op: IrArithmeticOp,
        left: Box<IrExpression>,
        right: Box<IrExpression>,
    },
    Comparison {
        op: IrComparisonOp,
        left: Box<IrExpression>,
        right: Box<IrExpression>,
    },
}

/// Arithmetic operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IrArithmeticOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
}

/// Comparison operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IrComparisonOp {
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
}

/// IR values
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IrValue {
    Integer(i64),
    String(String),
    Boolean(bool),
    Coord(Coord),
}

/// IR types
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum IrType {
    Int,
    String,
    Bool,
    Coord,
}

/// Resource bounds for O(1) memory validation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IrResourceBounds {
    pub max_processes: usize,
    pub max_events_per_tick: usize,
    pub max_coordinate_value: i32,
}

impl Default for IrResourceBounds {
    fn default() -> Self {
        Self {
            max_processes: 1024,
            max_events_per_tick: 10000,
            max_coordinate_value: 31,
        }
    }
}

/// IR Builder for constructing programs from typed AST
pub struct IrBuilder {
    programs: HashMap<String, IrProgram>,
}

impl IrBuilder {
    pub fn new() -> Self {
        Self {
            programs: HashMap::new(),
        }
    }
    
    /// Build IR from a typed Grey program
    pub fn build_program(
        &mut self,
        name: &str,
        typed_program: &grey_lang::types::TypedProgram,
    ) -> Result<&IrProgram> {
        let mut program = IrProgram {
            name: name.to_string(),
            processes: Vec::new(),
            events: Vec::new(),
            constants: HashMap::new(),
            resources: IrResourceBounds::default(),
        };
        
        // Build events first
        for module in &typed_program.modules {
            for event in &module.events {
                let ir_event = self.build_event(event)?;
                program.events.push(ir_event);
            }
            
            // Build processes
            for process in &module.processes {
                let ir_process = self.build_process(process)?;
                program.processes.push(ir_process);
            }
            
            // Build constants
            for constant in &module.constants {
                let value = self.build_constant(&constant.value)?;
                program.constants.insert(constant.name.clone(), value);
            }
        }
        
        self.programs.insert(name.to_string(), program);
        Ok(self.programs.get(name).unwrap())
    }
    
    fn build_event(&self, event: &grey_lang::types::TypedEventDefinition) -> Result<IrEvent> {
        let mut fields = HashMap::new();
        for field in &event.fields {
            let ir_type = self.convert_type(&field.field_type)?;
            fields.insert(field.name.clone(), ir_type);
        }
        
        Ok(IrEvent {
            name: event.name.clone(),
            fields,
        })
    }
    
    fn build_process(&self, process: &grey_lang::types::TypedProcessDefinition) -> Result<IrProcess> {
        let mut fields = HashMap::new();
        for field in &process.fields {
            let ir_type = self.convert_type(&field.field_type)?;
            fields.insert(field.name.clone(), ir_type);
        }
        
        // Extract initial state from init() method
        let initial_state = self.extract_initial_state(&process.methods, &fields)?;
        
        // Extract transitions from handler methods
        let transitions = self.extract_transitions(&process.methods)?;
        
        Ok(IrProcess {
            name: process.name.clone(),
            coord: Coord::new(0, 0, 0), // Default placement; can be overridden by backend
            fields,
            initial_state,
            transitions,
        })
    }
    
    fn extract_initial_state(&self, methods: &[grey_lang::types::TypedFunctionDefinition], fields: &HashMap<String, IrType>) -> Result<IrState> {
        let mut values = HashMap::new();
        
        // Look for init() method
        if let Some(init_method) = methods.iter().find(|m| m.name == "init") {
            // Extract initial values from init method body
            for statement in &init_method.body.statements {
                if let grey_lang::types::TypedStatement::Let { pattern, value } = statement {
                    match pattern {
                        grey_lang::ast::Pattern::Identifier(field_name) => {
                            let ir_value = self.expression_to_value(&value.expression)?;
                            values.insert(field_name.clone(), ir_value);
                        }
                    }
                }
            }
        }
        
        // Initialize missing fields with sensible defaults
        for (field_name, field_type) in fields {
            if !values.contains_key(field_name) {
                let default_value = match field_type {
                    IrType::Int => IrValue::Integer(0),
                    IrType::String => IrValue::String(String::new()),
                    IrType::Bool => IrValue::Boolean(false),
                    IrType::Coord => IrValue::Coord(Coord::new(0, 0, 0)),
                };
                values.insert(field_name.clone(), default_value);
            }
        }
        
        Ok(IrState { values })
    }
    
    fn extract_transitions(&self, methods: &[grey_lang::types::TypedFunctionDefinition]) -> Result<Vec<IrTransition>> {
        let mut transitions = Vec::new();
        
        for method in methods {
            // Handler methods typically start with "handle_"
            if method.name.starts_with("handle_") {
                // Extract event type from method name (e.g., handle_shipment -> Shipment)
                let event_name_lower = method.name.strip_prefix("handle_").unwrap_or("");
                let event_type = if !event_name_lower.is_empty() {
                    // Capitalize first letter
                    let mut chars = event_name_lower.chars();
                    match chars.next() {
                        None => continue,
                        Some(first) => {
                            first.to_uppercase().collect::<String>() + chars.as_str()
                        }
                    }
                } else {
                    continue;
                };
                
                // Extract actions from method body
                let actions = self.extract_actions(&method.body.statements)?;
                
                transitions.push(IrTransition {
                    event_type,
                    condition: None,
                    actions,
                });
            }
        }
        
        Ok(transitions)
    }
    
    fn extract_actions(&self, statements: &[grey_lang::types::TypedStatement]) -> Result<Vec<IrAction>> {
        let mut actions = Vec::new();
        
        for statement in statements {
            if let grey_lang::types::TypedStatement::Let { pattern, value } = statement {
                match pattern {
                    grey_lang::ast::Pattern::Identifier(field_name) => {
                        let expr = self.expression_to_ir_expression(&value.expression)?;
                        actions.push(IrAction::UpdateField {
                            field: field_name.clone(),
                            value: expr,
                        });
                    }
                }
            }
        }
        
        Ok(actions)
    }
    
    fn expression_to_value(&self, expr: &grey_lang::ast::Expression) -> Result<IrValue> {
        match expr {
            grey_lang::ast::Expression::Integer(i) => Ok(IrValue::Integer(*i)),
            grey_lang::ast::Expression::String(s) => Ok(IrValue::String(s.clone())),
            grey_lang::ast::Expression::CoordLiteral => Ok(IrValue::Coord(Coord::new(0, 0, 0))),
            _ => Ok(IrValue::Integer(0)), // Default for unrecognized expressions
        }
    }
    
    fn expression_to_ir_expression(&self, expr: &grey_lang::ast::Expression) -> Result<IrExpression> {
        match expr {
            grey_lang::ast::Expression::Integer(i) => {
                Ok(IrExpression::Constant(IrValue::Integer(*i)))
            }
            grey_lang::ast::Expression::String(s) => {
                Ok(IrExpression::Constant(IrValue::String(s.clone())))
            }
            grey_lang::ast::Expression::Identifier(name) => {
                Ok(IrExpression::FieldAccess(name.clone()))
            }
            grey_lang::ast::Expression::Add { left, right } => {
                Ok(IrExpression::Arithmetic {
                    op: IrArithmeticOp::Add,
                    left: Box::new(self.expression_to_ir_expression(left)?),
                    right: Box::new(self.expression_to_ir_expression(right)?),
                })
            }
            grey_lang::ast::Expression::CoordLiteral => {
                Ok(IrExpression::Constant(IrValue::Coord(Coord::new(0, 0, 0))))
            }
            _ => Ok(IrExpression::Constant(IrValue::Integer(0))),
        }
    }
    
    fn build_constant(&self, expr: &grey_lang::types::TypedExpression) -> Result<IrValue> {
        match &expr.expression {
            grey_lang::ast::Expression::Integer(i) => Ok(IrValue::Integer(*i)),
            grey_lang::ast::Expression::String(s) => Ok(IrValue::String(s.clone())),
            _ => Err(IrError::TypeMismatch("Unsupported constant type".to_string())),
        }
    }
    
    fn convert_type(&self, ty: &grey_lang::types::Type) -> Result<IrType> {
        match ty {
            grey_lang::types::Type::Int => Ok(IrType::Int),
            grey_lang::types::Type::String => Ok(IrType::String),
            grey_lang::types::Type::Bool => Ok(IrType::Bool),
            grey_lang::types::Type::Coord => Ok(IrType::Coord),
            _ => Err(IrError::TypeMismatch(format!("Unsupported type: {:?}", ty))),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_coord_validation() {
        let valid = Coord::new(15, 0, 31);
        assert!(valid.is_valid());
        
        let invalid = Coord::new(-1, 32, 15);
        assert!(!invalid.is_valid());
    }
    
    #[test]
    fn test_ir_builder() {
        let builder = IrBuilder::new();
        // Basic builder construction test
        assert_eq!(builder.programs.len(), 0);
    }
}