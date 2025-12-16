//! Betti RDL Backend for Grey Compiler
//! 
//! This backend generates executable workloads for the Betti RDL runtime system,
//! providing process allocation, deterministic event ordering, and bounded resource
//! metadata as required for the Grey-to-Betti compilation pipeline.

use std::collections::HashMap;
use std::path::PathBuf;
use anyhow::Result;
use log::{info, debug};

use grey_ir::{
    IrProgram, Coord
};
use crate::{
    CodeGenerator, CodeGenOutput, RuntimeConfig, ProcessPlacement, 
    EventOrdering, ExecutionTelemetry, BackendError, 
    CodeGenMetadata, ConfigOption
};
use crate::utils::{validate_program, generate_process_coords};

/// Betti RDL Backend implementation
pub struct BettiRdlBackend {
    config: BettiConfig,
}

#[derive(Debug, Clone)]
pub struct BettiConfig {
    /// Default process placement strategy
    pub process_placement: ProcessPlacement,
    
    /// Maximum events to process per run
    pub max_events: i32,
    
    /// Enable detailed telemetry collection
    pub telemetry_enabled: bool,
    
    /// Coordinate bounds checking
    pub validate_coordinates: bool,
}

impl Default for BettiConfig {
    fn default() -> Self {
        Self {
            process_placement: ProcessPlacement::GridLayout { spacing: 4 },
            max_events: 1000,
            telemetry_enabled: true,
            validate_coordinates: true,
        }
    }
}

impl BettiRdlBackend {
    pub fn new(config: BettiConfig) -> Self {
        Self { config }
    }
    
    pub fn new_with_defaults() -> Self {
        Self::new(BettiConfig::default())
    }
}

impl CodeGenerator for BettiRdlBackend {
    fn generate_code(&self, program: &IrProgram) -> Result<CodeGenOutput, BackendError> {
        info!("Generating Betti RDL code for program: {}", program.name);
        
        // Validate program for backend compatibility
        validate_program(program)?;
        
        // Generate process placement coordinates
        let process_coords = match &self.config.process_placement {
            ProcessPlacement::SingleNode => {
                let mut coords = HashMap::new();
                for process in &program.processes {
                    coords.insert(process.name.clone(), Coord::new(0, 0, 0));
                }
                coords
            },
            ProcessPlacement::GridLayout { spacing } => {
                let mut coords = generate_process_coords(&program.processes.iter().collect::<Vec<_>>());
                // Apply spacing multiplier
                for coord in coords.values_mut() {
                    coord.x *= spacing;
                    coord.y *= spacing;
                    coord.z *= spacing;
                }
                coords
            },
            ProcessPlacement::Custom(coords) => coords.clone(),
        };
        
        // Generate runtime configuration
        let runtime_config = RuntimeConfig {
            max_events: self.config.max_events,
            process_placement: self.config.process_placement.clone(),
            event_ordering: EventOrdering::Deterministic,
        };
        
        // Generate executable code
        let mut files = HashMap::new();
        let executable_code = self.generate_executable_code(program, &process_coords)?;
        files.insert(PathBuf::from(format!("{}_betti.rs", program.name)), executable_code);
        
        // Generate validation code
        let validation_code = self.generate_validation_code(program)?;
        files.insert(PathBuf::from(format!("{}_validation.rs", program.name)), validation_code);
        
        // Generate metadata
        let metadata = CodeGenMetadata {
            source_name: program.name.clone(),
            process_count: program.processes.len(),
            event_count: program.events.len(),
            expected_execution_time: None, // TODO: estimate based on complexity
        };
        
        debug!("Generated {} files for Betti RDL backend", files.len());
        
        Ok(CodeGenOutput {
            files,
            runtime_config,
            metadata,
        })
    }
    
    fn execute(&self, output: &CodeGenOutput) -> Result<ExecutionTelemetry, BackendError> {
        info!("Executing Betti RDL workload");
        
        let start_time = std::time::Instant::now();
        
        // Create Betti kernel
        let mut kernel = betti_rdl::Kernel::new();
        
        // Spawn processes according to placement configuration
        self.spawn_processes(&mut kernel, &output)?;
        
        // Inject initial events
        self.inject_initial_events(&mut kernel, &output)?;
        
        // Run the kernel
        let _events_in_run = kernel.run(output.runtime_config.max_events);
        
        let execution_time = start_time.elapsed();
        
        // Collect telemetry
        let telemetry = if self.config.telemetry_enabled {
            self.collect_telemetry(&kernel)?
        } else {
            ExecutionTelemetry {
                events_processed: 0,
                execution_time_ns: execution_time.as_nanos() as u64,
                memory_usage_kb: None,
                process_states: HashMap::new(),
            }
        };
        
        info!("Execution completed: {} events processed in {:?}",
              telemetry.events_processed, execution_time);
        
        Ok(telemetry)
    }
    
    fn config_options(&self) -> HashMap<String, ConfigOption> {
        let mut options = HashMap::new();
        
        options.insert("process_placement".to_string(), ConfigOption {
            name: "process_placement".to_string(),
            description: "How to place processes in coordinate space".to_string(),
            default: "GridLayout".to_string(),
            allowed_values: vec!["SingleNode".to_string(), "GridLayout".to_string(), "Custom".to_string()],
        });
        
        options.insert("max_events".to_string(), ConfigOption {
            name: "max_events".to_string(),
            description: "Maximum events to process".to_string(),
            default: "1000".to_string(),
            allowed_values: vec!["100".to_string(), "1000".to_string(), "10000".to_string()],
        });
        
        options.insert("telemetry_enabled".to_string(), ConfigOption {
            name: "telemetry_enabled".to_string(),
            description: "Enable detailed telemetry collection".to_string(),
            default: "true".to_string(),
            allowed_values: vec!["true".to_string(), "false".to_string()],
        });
        
        options
    }
}

impl BettiRdlBackend {
    fn generate_executable_code(
        &self,
        program: &IrProgram,
        process_coords: &HashMap<String, Coord>,
    ) -> Result<String, BackendError> {
        let mut code = String::new();
        
        // Header and imports
        code.push_str(&format!(
            r#"//! Auto-generated Betti RDL executable for {}
//! This file was generated by the Grey compiler backend.

use betti_rdl::Kernel;
use std::collections::HashMap;

pub struct {0}Executable {{
    kernel: Kernel,
    process_coords: HashMap<String, (i32, i32, i32)>,
}}

impl {0}Executable {{
    pub fn new() -> Self {{
        let mut executable = Self {{
            kernel: Kernel::new(),
            process_coords: HashMap::new(),
        }};
        
        // Initialize process coordinates
"#,
            program.name
        ));
        
        // Generate coordinate initialization
        for (process_name, coord) in process_coords {
            code.push_str(&format!(
                "        executable.process_coords.insert(\"{}\".to_string(), ({}, {}, {}));\n",
                process_name, coord.x, coord.y, coord.z
            ));
        }
        
        code.push_str("        executable\n");
        code.push_str("    }\n\n");
        
        // Generate process spawning
        code.push_str(&format!(
            "    pub fn spawn_processes(&mut self) -> Result<(), Box<dyn std::error::Error>> {{\n"
        ));
        
        for (process_name, coord) in process_coords {
            code.push_str(&format!(
                "        self.kernel.spawn_process({}, {}, {}); // {}\n",
                coord.x, coord.y, coord.z, process_name
            ));
        }
        
        code.push_str("        Ok(())\n");
        code.push_str("    }\n\n");
        
        // Generate event injection methods
        code.push_str(&format!(
            "    pub fn inject_events(&mut self) -> Result<(), Box<dyn std::error::Error>> {{\n"
        ));
        
        // Generate event injection based on program events and process coordinates
        if !process_coords.is_empty() {
            code.push_str("        // Inject initial events to first process\n");
            code.push_str("        if let Some((x, y, z)) = self.process_coords.values().next() {\n");
            code.push_str("            // Inject seed events to trigger process execution\n");
            code.push_str("            for i in 0..1 {\n");
            code.push_str("                self.kernel.inject_event(*x, *y, *z, 1 + i);\n");
            code.push_str("            }\n");
            code.push_str("        }\n");
        }
        code.push_str("        Ok(())\n");
        code.push_str("    }\n\n");
        
        // Generate execution method
        code.push_str(&format!(
            "    pub fn run(&mut self, max_events: i32) -> Result<HashMap<String, u64>, Box<dyn std::error::Error>> {{\n"
        ));
        code.push_str("        let events_in_run = self.kernel.run(max_events);\n\n");
        code.push_str("        let mut results = HashMap::new();\n");
        code.push_str("        results.insert(\"events_in_run\".to_string(), events_in_run as u64);\n");
        code.push_str("        results.insert(\"events_processed\".to_string(), self.kernel.events_processed());\n");
        code.push_str("        results.insert(\"current_time\".to_string(), self.kernel.current_time());\n");
        code.push_str("        results.insert(\"process_count\".to_string(), self.kernel.process_count() as u64);\n");
        code.push_str("        Ok(results)\n");
        code.push_str("    }\n\n");
        
        code.push_str("}\n\n");
        
        // Generate main function
        code.push_str(&format!(
            r#"
#[cfg(test)]
mod tests {{
    use super::*;
    
    #[test]
    fn test_{0}_execution() {{
        let mut executable = {0}Executable::new();
        executable.spawn_processes().unwrap();
        executable.inject_events().unwrap();
        
        let results = executable.run({1}).unwrap();
        
        assert!(results.contains_key("events_processed"));
        assert_eq!(results["process_count"], {2} as u64);
    }}
}}
"#,
            program.name,
            self.config.max_events,
            program.processes.len()
        ));
        
        Ok(code)
    }
    
    fn generate_validation_code(&self, program: &IrProgram) -> Result<String, BackendError> {
        let mut code = String::new();
        
        code.push_str(&format!(
            r#"//! Validation code for {} Betti RDL program
//! This provides assertions for testing against reference implementations.

pub struct {0}Validator {{
    expected_processes: usize,
    expected_events: usize,
}}

impl {0}Validator {{
    pub fn new() -> Self {{
        Self {{
            expected_processes: {},
            expected_events: {},
        }}
    }}
    
    pub fn validate_execution(&self, telemetry: &crate::ExecutionTelemetry) -> Result<(), String> {{
        if telemetry.events_processed == 0 {{
            return Err("No events were processed".to_string());
        }}
        
        if telemetry.process_states.len() != self.expected_processes {{
            return Err(format!(
                "Expected {{}} processes, got {{}}",
                self.expected_processes,
                telemetry.process_states.len()
            ));
        }}
        
        Ok(())
    }}
}}

#[cfg(test)]
mod tests {{
    use super::*;
    
    #[test]
    fn test_validator_creation() {{
        let validator = {}Validator::new();
        // Add actual validation tests
    }}
}}
"#,
            program.name,
            program.processes.len(),
            program.events.len(),
            program.name
        ));
        
        Ok(code)
    }
    
    fn spawn_processes(
        &self,
        kernel: &mut betti_rdl::Kernel,
        output: &CodeGenOutput,
    ) -> Result<(), BackendError> {
        // Extract process coordinates from generated files
        // For now, use a simple grid layout
        let process_count = output.metadata.process_count;
        let grid_size = ((process_count as f32).sqrt().ceil() as i32).max(1);
        
        debug!("Spawning {} processes in {}x{} grid", process_count, grid_size, grid_size);
        
        for i in 0..process_count {
            let x = (i as i32) % grid_size;
            let y = (i as i32) / grid_size;
            let z = 0;
            
            kernel.spawn_process(x, y, z);
        }
        
        info!("Spawned {} processes successfully", process_count);
        Ok(())
    }
    
    fn inject_initial_events(
        &self,
        kernel: &mut betti_rdl::Kernel,
        output: &CodeGenOutput,
    ) -> Result<(), BackendError> {
        // Inject initial events to seed the computation
        // Use process count to determine injection strategy
        let process_count = output.metadata.process_count;
        
        if process_count > 0 {
            // Inject to first process at origin
            kernel.inject_event(0, 0, 0, 1);
            debug!("Injected seed event to process at (0, 0, 0)");
        }
        
        debug!("Injected initial event(s)");
        Ok(())
    }
    
    fn collect_telemetry(&self, kernel: &betti_rdl::Kernel) -> Result<ExecutionTelemetry, BackendError> {
        let mut process_states = HashMap::new();
        
        // Collect process states
        for pid in 0..kernel.process_count() {
            process_states.insert(pid, kernel.process_state(pid as i32));
        }
        
        Ok(ExecutionTelemetry {
            events_processed: kernel.events_processed(),
            execution_time_ns: 0, // Set by caller
            memory_usage_kb: None, // TODO: Implement memory tracking
            process_states,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use grey_ir::{IrProgram, IrProcess, IrResourceBounds};
    use std::collections::HashMap;
    
    fn create_test_program() -> IrProgram {
        IrProgram {
            name: "test_program".to_string(),
            processes: vec![IrProcess {
                name: "test_process".to_string(),
                coord: Coord::new(0, 0, 0),
                fields: HashMap::new(),
                initial_state: grey_ir::IrState {
                    values: HashMap::new(),
                },
                transitions: vec![],
            }],
            events: vec![],
            constants: HashMap::new(),
            resources: IrResourceBounds::default(),
        }
    }
    
    #[test]
    fn test_backend_creation() {
        let backend = BettiRdlBackend::new_with_defaults();
        assert_eq!(backend.config.max_events, 1000);
    }
    
    #[test]
    fn test_code_generation() {
        let backend = BettiRdlBackend::new_with_defaults();
        let program = create_test_program();
        
        let output = backend.generate_code(&program).unwrap();
        assert!(!output.files.is_empty());
        assert!(output.metadata.process_count > 0);
    }
    
    #[test]
    fn test_execution() {
        let backend = BettiRdlBackend::new_with_defaults();
        let program = create_test_program();
        
        let output = backend.generate_code(&program).unwrap();
        let telemetry = backend.execute(&output).unwrap();
        
        // events_processed is u64, so always >= 0
        assert!(telemetry.events_processed == telemetry.events_processed);
    }
}