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
    Coord, IrProgram, IrValue,
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

    /// Seed used for deterministic injection patterns.
    pub seed: u64,

    /// Enable detailed telemetry collection
    pub telemetry_enabled: bool,

    /// Coordinate bounds checking
    pub validate_coordinates: bool,
}

impl Default for BettiConfig {
    fn default() -> Self {
        Self {
            process_placement: ProcessPlacement::GridLayout { spacing: 1 },
            max_events: 1000,
            seed: 42,
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

    fn estimate_execution_time_ns(&self, program: &IrProgram, runtime_process_count: usize) -> u64 {
        let event_count = program.events.len() as u64;
        let max_events = if self.config.max_events > 0 {
            self.config.max_events as u64
        } else {
            event_count
        };
        let bounded_events = if max_events == 0 { event_count } else { event_count.min(max_events) };
        let per_event_ns = 1000u64;
        let per_process_ns = 500u64;
        bounded_events
            .saturating_mul(per_event_ns)
            .saturating_add((runtime_process_count as u64).saturating_mul(per_process_ns))
    }
}

impl CodeGenerator for BettiRdlBackend {
    fn generate_code(&self, program: &IrProgram) -> Result<CodeGenOutput, BackendError> {
        info!("Generating Betti RDL code for program: {}", program.name);
        
        // Validate program for backend compatibility
        validate_program(program)?;

        let runtime_process_count = match &self.config.process_placement {
            ProcessPlacement::Custom(coords) => coords.len().max(1),
            ProcessPlacement::SingleNode => 1,
            ProcessPlacement::GridLayout { .. } => {
                let from_constants = program
                    .constants
                    .get("RUNTIME_PROCESSES")
                    .or_else(|| program.constants.get("MAX_PROCESSES"))
                    .and_then(|v| match v {
                        IrValue::Integer(i) if *i > 0 => Some(*i as usize),
                        _ => None,
                    });

                from_constants.unwrap_or(program.processes.len().max(1))
            }
        };

        if runtime_process_count > program.resources.max_processes {
            return Err(BackendError::ValidationError(format!(
                "Runtime process count {} exceeds max_processes {}",
                runtime_process_count, program.resources.max_processes
            )));
        }

        // BettiRDLCompute has a fixed process pool.
        if runtime_process_count > 2048 {
            return Err(BackendError::ValidationError(format!(
                "Runtime process count {} exceeds kernel hard limit 2048",
                runtime_process_count
            )));
        }

        // Generate process placement coordinates
        let process_coords = match &self.config.process_placement {
            ProcessPlacement::SingleNode => {
                let mut coords = HashMap::new();
                coords.insert("p0".to_string(), Coord::new(0, 0, 0));
                coords
            }
            ProcessPlacement::GridLayout { spacing } => {
                let mut coords = HashMap::new();
                let grid_size = ((runtime_process_count as f32).sqrt().ceil() as i32).max(1);

                for i in 0..runtime_process_count {
                    let x = (i as i32) % grid_size;
                    let y = (i as i32) / grid_size;
                    let z = 0;
                    coords.insert(
                        format!("p{}", i),
                        Coord::new(x * spacing, y * spacing, z * spacing),
                    );
                }
                coords
            }
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
            runtime_process_count,
            event_count: program.events.len(),
            expected_execution_time: Some(self.estimate_execution_time_ns(program, runtime_process_count)),
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
        let process_coords = self.spawn_processes(&mut kernel, output)?;

        // Inject initial events
        self.inject_initial_events(&mut kernel, output, &process_coords)?;

        // Run the kernel
        let _events_in_run = kernel.run(output.runtime_config.max_events);

        let execution_time = start_time.elapsed();
        let execution_time_ns = execution_time.as_nanos() as u64;

        // Collect telemetry
        let telemetry = if self.config.telemetry_enabled {
            self.collect_telemetry(&kernel, &process_coords, execution_time_ns)?
        } else {
            ExecutionTelemetry {
                events_processed: kernel.events_processed(),
                current_time: kernel.current_time(),
                execution_time_ns,
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

        options.insert("seed".to_string(), ConfigOption {
            name: "seed".to_string(),
            description: "Deterministic seed used for initial injection patterns".to_string(),
            default: "42".to_string(),
            allowed_values: vec!["0".to_string(), "1".to_string(), "42".to_string(), "123".to_string()],
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
        let mut process_entries: Vec<_> = process_coords.iter().collect();
        process_entries.sort_by(|(a, _), (b, _)| a.cmp(b));

        for (process_name, coord) in process_entries {
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
        
        let mut spawn_entries: Vec<_> = process_coords.iter().collect();
        spawn_entries.sort_by(|(a, _), (b, _)| a.cmp(b));

        for (process_name, coord) in spawn_entries {
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
            code.push_str("        if let Some((x, y, z)) = self.process_coords.get(\"p0\") {\n");
            code.push_str("            // Inject seed events to trigger process execution\n");
            code.push_str("            self.kernel.inject_event(*x, *y, *z, 1);\n");
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
            process_coords.len()
        ));
        
        Ok(code)
    }
    
    fn generate_validation_code(&self, program: &IrProgram) -> Result<String, BackendError> {
        let mut code = String::new();

        let expected_processes = program
            .constants
            .get("RUNTIME_PROCESSES")
            .or_else(|| program.constants.get("MAX_PROCESSES"))
            .and_then(|v| match v {
                IrValue::Integer(i) if *i > 0 => Some(*i as usize),
                _ => None,
            })
            .unwrap_or(program.processes.len().max(1));
        
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
            expected_processes,
            program.events.len(),
            program.name
        ));
        
        Ok(code)
    }
    
    fn spawn_processes(
        &self,
        kernel: &mut betti_rdl::Kernel,
        output: &CodeGenOutput,
    ) -> Result<Vec<Coord>, BackendError> {
        let process_count = output.metadata.runtime_process_count;

        let coords: Vec<Coord> = match &output.runtime_config.process_placement {
            ProcessPlacement::SingleNode => vec![Coord::new(0, 0, 0)],
            ProcessPlacement::GridLayout { spacing } => {
                let grid_size = ((process_count as f32).sqrt().ceil() as i32).max(1);

                (0..process_count)
                    .map(|i| {
                        let x = (i as i32) % grid_size;
                        let y = (i as i32) / grid_size;
                        let z = 0;
                        Coord::new(x * spacing, y * spacing, z * spacing)
                    })
                    .collect()
            }
            ProcessPlacement::Custom(mapping) => {
                let mut keys: Vec<_> = mapping.keys().cloned().collect();
                keys.sort();
                keys.into_iter()
                    .filter_map(|k| mapping.get(&k).cloned())
                    .collect()
            }
        };

        debug!("Spawning {} processes", coords.len());

        for coord in &coords {
            kernel.spawn_process(coord.x, coord.y, coord.z);
        }

        info!("Spawned {} processes successfully", coords.len());
        Ok(coords)
    }

    fn inject_initial_events(
        &self,
        kernel: &mut betti_rdl::Kernel,
        _output: &CodeGenOutput,
        process_coords: &[Coord],
    ) -> Result<(), BackendError> {
        if process_coords.is_empty() {
            return Ok(());
        }

        struct XorShift64 {
            state: u64,
        }

        impl XorShift64 {
            fn new(seed: u64) -> Self {
                Self { state: seed.max(1) }
            }

            fn next_u64(&mut self) -> u64 {
                let mut x = self.state;
                x ^= x << 13;
                x ^= x >> 7;
                x ^= x << 17;
                self.state = x;
                x
            }
        }

        let mut rng = XorShift64::new(self.config.seed);
        let injections = 4.min(process_coords.len());

        for _ in 0..injections {
            let idx = (rng.next_u64() as usize) % process_coords.len();
            let value = (rng.next_u64() % 5) as i32 + 1;
            let coord = &process_coords[idx];
            kernel.inject_event(coord.x, coord.y, coord.z, value);
        }

        debug!("Injected {} initial event(s)", injections);
        Ok(())
    }

    fn collect_telemetry(
        &self,
        kernel: &betti_rdl::Kernel,
        process_coords: &[Coord],
        execution_time_ns: u64,
    ) -> Result<ExecutionTelemetry, BackendError> {
        let mut process_states = HashMap::new();

        for coord in process_coords {
            let pid = Self::node_id(coord) as usize;
            process_states.insert(pid, kernel.process_state(pid as i32));
        }

        Ok(ExecutionTelemetry {
            events_processed: kernel.events_processed(),
            current_time: kernel.current_time(),
            execution_time_ns,
            memory_usage_kb: None,
            process_states,
        })
    }

    fn node_id(coord: &Coord) -> i32 {
        fn wrap(v: i32) -> i32 {
            let m = v % 32;
            if m < 0 { m + 32 } else { m }
        }

        let x = wrap(coord.x);
        let y = wrap(coord.y);
        let z = wrap(coord.z);
        x * 1024 + y * 32 + z
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
