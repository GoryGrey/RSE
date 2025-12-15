//! Grey Compiler Backends
//! 
//! Backend system for code generation targeting different runtime systems.
//! Each backend implements the CodeGenerator trait to convert IR to executable code.

use anyhow::Result;
use grey_ir::{IrProgram, IrError};
use std::path::PathBuf;
use std::collections::HashMap;

/// Output artifacts from code generation
#[derive(Debug)]
pub struct CodeGenOutput {
    /// Generated source files
    pub files: HashMap<PathBuf, String>,
    
    /// Runtime execution configuration
    pub runtime_config: RuntimeConfig,
    
    /// Metadata for validation
    pub metadata: CodeGenMetadata,
}

/// Runtime execution configuration
#[derive(Debug, Clone)]
pub struct RuntimeConfig {
    /// Maximum events to process
    pub max_events: i32,
    
    /// Process placement hint
    pub process_placement: ProcessPlacement,
    
    /// Event ordering configuration
    pub event_ordering: EventOrdering,
}

/// How to place processes in coordinate space
#[derive(Debug, Clone)]
pub enum ProcessPlacement {
    /// Single node at origin
    SingleNode,
    
    /// Grid layout based on process count
    GridLayout { spacing: i32 },
    
    /// Custom coordinate mapping
    Custom(HashMap<String, grey_ir::Coord>),
}

/// Event ordering guarantees
#[derive(Debug, Clone)]
pub enum EventOrdering {
    /// FIFO ordering within same timestamp
    Fifo,
    
    /// Deterministic tie-breaking by coordinates
    Deterministic,
}

/// Metadata for validation and debugging
#[derive(Debug)]
pub struct CodeGenMetadata {
    pub source_name: String,
    pub process_count: usize,
    pub event_count: usize,
    pub expected_execution_time: Option<u64>,
}

/// Backend-specific error types
#[derive(Debug, thiserror::Error)]
pub enum BackendError {
    #[error("IR error: {0}")]
    IrError(#[from] IrError),
    
    #[error("Code generation failed: {0}")]
    CodegenFailed(String),
    
    #[error("Runtime execution failed: {0}")]
    RuntimeError(String),
    
    #[error("Validation failed: {0}")]
    ValidationError(String),
}

/// Trait for all backend code generators
pub trait CodeGenerator {
    /// Generate code from IR program
    fn generate_code(&self, program: &IrProgram) -> Result<CodeGenOutput, BackendError>;
    
    /// Execute the generated code and return telemetry
    fn execute(&self, output: &CodeGenOutput) -> Result<ExecutionTelemetry, BackendError>;
    
    /// Get backend-specific configuration options
    fn config_options(&self) -> HashMap<String, ConfigOption>;
}

/// Telemetry from runtime execution
#[derive(Debug)]
pub struct ExecutionTelemetry {
    pub events_processed: u64,
    pub execution_time_ns: u64,
    pub memory_usage_kb: Option<u64>,
    pub process_states: HashMap<usize, i32>,
}

/// Configuration option for backends
#[derive(Debug, Clone)]
pub struct ConfigOption {
    pub name: String,
    pub description: String,
    pub default: String,
    pub allowed_values: Vec<String>,
}

impl Default for RuntimeConfig {
    fn default() -> Self {
        Self {
            max_events: 10000,
            process_placement: ProcessPlacement::SingleNode,
            event_ordering: EventOrdering::Deterministic,
        }
    }
}

/// Utility functions for backend implementations
pub mod utils {
    use grey_ir::{IrProgram, IrProcess, IrEvent};
    use std::collections::HashMap;
    use crate::BackendError;
    
    /// Validate IR program for backend compatibility
    pub fn validate_program(program: &IrProgram) -> Result<(), BackendError> {
        // Check coordinate bounds
        for process in &program.processes {
            if !process.coord.is_valid() {
                return Err(BackendError::ValidationError(format!(
                    "Process {} has invalid coordinate: {:?}",
                    process.name, process.coord
                )));
            }
        }
        
        // Check resource bounds
        if program.processes.len() > program.resources.max_processes {
            return Err(BackendError::ValidationError(format!(
                "Too many processes: {} > {}",
                program.processes.len(),
                program.resources.max_processes
            )));
        }
        
        Ok(())
    }
    
    /// Generate deterministic coordinate assignment
    pub fn generate_process_coords(processes: &[&IrProcess]) -> HashMap<String, grey_ir::Coord> {
        let mut coords = HashMap::new();
        let grid_size = ((processes.len() as f32).sqrt().ceil() as i32).max(1);
        
        for (i, process) in processes.iter().enumerate() {
            let x = (i as i32) % grid_size;
            let y = (i as i32) / grid_size;
            let z = 0; // 2D layout for now
            
            coords.insert(process.name.clone(), grey_ir::Coord::new(x, y, z));
        }
        
        coords
    }
    
    /// Extract event definitions for validation
    pub fn get_event_map(program: &IrProgram) -> HashMap<String, &IrEvent> {
        program.events.iter().map(|e| (e.name.clone(), e)).collect()
    }
    
    /// Extract process definitions for validation
    pub fn get_process_map(program: &IrProgram) -> HashMap<String, &IrProcess> {
        program.processes.iter().map(|p| (p.name.clone(), p)).collect()
    }
}

/// Betti RDL backend implementation
pub mod betti_rdl;
}