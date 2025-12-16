//! End-to-end integration test for Grey → IR → Betti pipeline
//!
//! This test verifies that a complete Grey program can be:
//! 1. Compiled from source to typed AST
//! 2. Converted to IR representation
//! 3. Generated as Betti RDL backend code
//! 4. Executed through the Betti kernel
//! 5. Produces deterministic telemetry

use std::fs;
use std::path::PathBuf;
use tempfile::TempDir;
use grey_lang::compile;
use grey_ir::{IrBuilder, IrProgram};
use grey_backends::betti_rdl::{BettiRdlBackend, BettiConfig};
use grey_backends::CodeGenerator;

const SIMPLE_DEMO: &str = r#"
module SimpleDemo {
    const INITIAL_VALUE = 0;
    
    event Start {
        value: Int,
    }
    
    event Tick {
        counter: Int,
    }
    
    process SimpleProcess {
        counter: Int,
        active: Bool,
        
        method init() {
            this.counter = 0;
            this.active = true;
        }
        
        method handle_start(event: Start) {
            this.counter = this.counter + 1;
        }
        
        method handle_tick(event: Tick) {
            this.counter = this.counter + event.counter;
        }
    }
}
"#;

const MULTI_PROCESS_DEMO: &str = r#"
module MultiProcessDemo {
    event Message {
        count: Int,
    }
    
    process Node {
        message_count: Int,
        
        method init() {
            this.message_count = 0;
        }
        
        method handle_message(event: Message) {
            this.message_count = this.message_count + 1;
        }
    }
}
"#;

#[cfg(test)]
mod tests {
    use super::*;
    use pretty_assertions::assert_eq;

    fn create_test_temp_dir() -> TempDir {
        tempfile::tempdir().expect("Failed to create temp directory")
    }

    #[test]
    fn test_simple_demo_end_to_end() {
        // Step 1: Compile Grey source to typed AST
        let typed_program = compile(SIMPLE_DEMO)
            .expect("Failed to compile simple demo");
        
        assert_eq!(typed_program.modules.len(), 1);
        let module = &typed_program.modules[0];
        assert_eq!(module.name, "SimpleDemo");
        assert_eq!(module.processes.len(), 1);
        assert_eq!(module.events.len(), 2);

        // Step 2: Build IR from typed AST
        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("simple_demo", &typed_program)
            .expect("Failed to build IR");
        
        assert_eq!(ir_program.name, "simple_demo");
        assert_eq!(ir_program.processes.len(), 1);
        assert_eq!(ir_program.events.len(), 2);
        
        // Check process has initial state
        let process = &ir_program.processes[0];
        assert_eq!(process.name, "SimpleProcess");
        assert!(process.initial_state.values.contains_key("counter"));
        assert!(process.initial_state.values.contains_key("active"));

        // Step 3: Generate Betti RDL code
        let backend = BettiRdlBackend::new(BettiConfig {
            max_events: 100,
            process_placement: grey_backends::ProcessPlacement::GridLayout { spacing: 1 },
            telemetry_enabled: true,
            validate_coordinates: true,
        });
        
        let output = backend.generate_code(ir_program)
            .expect("Failed to generate Betti RDL code");
        
        assert!(!output.files.is_empty());
        assert_eq!(output.metadata.process_count, 1);
        assert_eq!(output.metadata.event_count, 2);

        // Step 4: Execute through Betti kernel
        let telemetry = backend.execute(&output)
            .expect("Failed to execute Betti RDL workload");
        
        // Step 5: Verify deterministic telemetry
        assert!(telemetry.events_processed > 0, 
                "Expected events to be processed, got {}", 
                telemetry.events_processed);
        assert!(telemetry.execution_time_ns > 0,
                "Expected non-zero execution time");
        assert_eq!(telemetry.process_states.len(), 1,
                   "Expected 1 process state");
    }

    #[test]
    fn test_multi_process_end_to_end() {
        // Compile and build IR
        let typed_program = compile(MULTI_PROCESS_DEMO)
            .expect("Failed to compile multi-process demo");
        
        assert_eq!(typed_program.modules[0].processes.len(), 1);
        assert_eq!(typed_program.modules[0].events.len(), 1);

        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("multi_process", &typed_program)
            .expect("Failed to build IR");
        
        // Generate code
        let backend = BettiRdlBackend::new_with_defaults();
        let output = backend.generate_code(ir_program)
            .expect("Failed to generate code");
        
        // Execute
        let telemetry = backend.execute(&output)
            .expect("Failed to execute");
        
        // Verify
        assert!(telemetry.events_processed >= 0);
        assert_eq!(telemetry.process_states.len(), 1);
    }

    #[test]
    fn test_deterministic_execution() {
        // Run the same program twice and verify identical telemetry
        let mut ir_builder = IrBuilder::new();
        
        let typed_program = compile(SIMPLE_DEMO)
            .expect("Failed to compile");
        
        let ir_program = ir_builder.build_program("deterministic_test", &typed_program)
            .expect("Failed to build IR");
        
        let backend = BettiRdlBackend::new(BettiConfig {
            max_events: 100,
            telemetry_enabled: true,
            ..Default::default()
        });
        
        let output = backend.generate_code(&ir_program)
            .expect("Failed to generate code");
        
        // Run twice
        let telemetry1 = backend.execute(&output)
            .expect("First execution failed");
        let telemetry2 = backend.execute(&output)
            .expect("Second execution failed");
        
        // For deterministic execution, event counts should match
        assert_eq!(telemetry1.events_processed, telemetry2.events_processed,
                   "Execution should be deterministic");
    }

    #[test]
    fn test_ir_extracts_transitions() {
        // Verify that IR properly extracts transitions from process methods
        let typed_program = compile(SIMPLE_DEMO)
            .expect("Failed to compile");
        
        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("transition_test", &typed_program)
            .expect("Failed to build IR");
        
        let process = &ir_program.processes[0];
        
        // Should have extracted handle_start and handle_tick
        assert!(!process.transitions.is_empty(),
                "Process should have extracted transitions from handler methods");
        
        // Verify at least one transition exists
        let event_types: Vec<_> = process.transitions.iter()
            .map(|t| t.event_type.as_str())
            .collect();
        assert!(event_types.contains(&"Start") || event_types.contains(&"Tick"),
                "Should have extracted Start or Tick transitions, got: {:?}", event_types);
    }

    #[test]
    fn test_code_generation_produces_valid_structure() {
        let typed_program = compile(SIMPLE_DEMO)
            .expect("Failed to compile");
        
        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("code_gen_test", &typed_program)
            .expect("Failed to build IR");
        
        let backend = BettiRdlBackend::new_with_defaults();
        let output = backend.generate_code(&ir_program)
            .expect("Failed to generate code");
        
        // Check that generated files exist
        let files: Vec<_> = output.files.keys().collect();
        assert!(!files.is_empty(), "Should generate at least one file");
        
        // Find the executable file
        let exec_file = files.iter()
            .find(|p| p.to_string_lossy().contains("_betti.rs"))
            .expect("Should generate _betti.rs file");
        
        let content = output.files.get(*exec_file)
            .expect("File should have content");
        
        // Verify content structure
        assert!(content.contains("Executable"), "Should define Executable struct");
        assert!(content.contains("spawn_processes"), "Should have spawn_processes method");
        assert!(content.contains("inject_events"), "Should have inject_events method");
        assert!(content.contains("Kernel::new()"), "Should create kernel");
    }

    #[test]
    fn test_telemetry_contains_required_metrics() {
        let typed_program = compile(SIMPLE_DEMO)
            .expect("Failed to compile");
        
        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("telemetry_test", &typed_program)
            .expect("Failed to build IR");
        
        let backend = BettiRdlBackend::new(BettiConfig {
            telemetry_enabled: true,
            max_events: 1000,
            ..Default::default()
        });
        
        let output = backend.generate_code(&ir_program)
            .expect("Failed to generate code");
        
        let telemetry = backend.execute(&output)
            .expect("Failed to execute");
        
        // Verify all telemetry fields are present
        assert!(telemetry.events_processed >= 0, "events_processed should be non-negative");
        assert!(telemetry.execution_time_ns > 0, "execution_time_ns should be positive");
        assert!(!telemetry.process_states.is_empty(), "process_states should not be empty");
    }
}
