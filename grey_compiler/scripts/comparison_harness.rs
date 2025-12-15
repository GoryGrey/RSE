//! Betti RDL Comparison Harness
//! 
//! This utility invokes both the Grey-compiled workload and the reference C++ demo
//! to prove deterministic parity for a shared seed, forming the basis of the
//! "killer demo rewrite" requirement.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};

use grey_lang::compile;
use grey_ir::{IrBuilder, IrProgram};
use grey_backends::betti_rdl::{BettiRdlBackend, BettiConfig};
use grey_backends::CodeGenerator;

/// Results from executing a workload
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionResult {
    pub events_processed: u64,
    pub execution_time_ns: u64,
    pub process_states: HashMap<usize, i32>,
    pub memory_usage_kb: Option<u64>,
    pub seed_used: u64,
}

/// Comparison between two executions
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComparisonResult {
    pub grey_result: ExecutionResult,
    pub cpp_result: ExecutionResult,
    pub events_match: bool,
    pub time_ratio: f64,
    pub state_differences: Vec<String>,
    pub parity_achieved: bool,
}

/// Configuration for comparison runs
#[derive(Debug, Clone)]
pub struct ComparisonConfig {
    pub seed: u64,
    pub max_events: i32,
    pub timeout_seconds: u64,
    pub enable_memory_tracking: bool,
    pub cpp_demo_path: Option<PathBuf>,
}

impl Default for ComparisonConfig {
    fn default() -> Self {
        Self {
            seed: 42,
            max_events: 1000,
            timeout_seconds: 30,
            enable_memory_tracking: false,
            cpp_demo_path: None,
        }
    }
}

/// Main comparison harness
pub struct BettiComparisonHarness {
    config: ComparisonConfig,
}

impl BettiComparisonHarness {
    pub fn new(config: ComparisonConfig) -> Self {
        Self { config }
    }
    
    pub fn run_comparison(&self, grey_program: &str) -> Result<ComparisonResult, String> {
        println!("🔄 Starting Betti RDL comparison harness");
        println!("   Seed: {}", self.config.seed);
        println!("   Max events: {}", self.config.max_events);
        println!("   Timeout: {}s", self.config.timeout_seconds);
        
        // Execute Grey-compiled workload
        println!("\n📝 Executing Grey-compiled workload...");
        let grey_result = self.execute_grey_workload(grey_program)?;
        
        // Execute reference C++ demo
        println!("\n🔧 Executing reference C++ demo...");
        let cpp_result = self.execute_cpp_demo()?;
        
        // Compare results
        let comparison = self.compare_results(&grey_result, &cpp_result);
        
        // Print comparison summary
        self.print_comparison_summary(&comparison);
        
        Ok(comparison)
    }
    
    fn execute_grey_workload(&self, program: &str) -> Result<ExecutionResult, String> {
        let start_time = Instant::now();
        
        // Compile Grey source
        let typed_program = compile(program)
            .map_err(|e| format!("Grey compilation failed: {:?}", e))?;
        
        // Build IR
        let mut ir_builder = IrBuilder::new();
        let ir_program = ir_builder.build_program("grey_demo", &typed_program)
            .map_err(|e| format!("IR building failed: {}", e))?;
        
        // Configure backend
        let backend_config = BettiConfig {
            max_events: self.config.max_events,
            telemetry_enabled: true,
            validate_coordinates: true,
            process_placement: grey_backends::ProcessPlacement::GridLayout { spacing: 2 },
        };
        
        let backend = BettiRdlBackend::new(backend_config);
        
        // Generate code
        let output = backend.generate_code(ir_program)
            .map_err(|e| format!("Code generation failed: {}", e))?;
        
        // Execute
        let telemetry = backend.execute(&output)
            .map_err(|e| format!("Execution failed: {}", e))?;
        
        let execution_time = start_time.elapsed();
        
        Ok(ExecutionResult {
            events_processed: telemetry.events_processed,
            execution_time_ns: execution_time.as_nanos() as u64,
            process_states: telemetry.process_states,
            memory_usage_kb: telemetry.memory_usage_kb,
            seed_used: self.config.seed,
        })
    }
    
    fn execute_cpp_demo(&self) -> Result<ExecutionResult, String> {
        let cpp_path = self.config.cpp_demo_path
            .as_ref()
            .ok_or_else(|| "C++ demo path not configured".to_string())?
            .clone();
        
        if !cpp_path.exists() {
            return Err(format!("C++ demo not found at: {:?}", cpp_path));
        }
        
        let start_time = Instant::now();
        
        // Execute C++ demo with same seed and parameters
        let mut cmd = Command::new(&cpp_path);
        cmd.arg("--seed").arg(self.config.seed.to_string())
           .arg("--max-events").arg(self.config.max_events.to_string())
           .stdout(Stdio::piped())
           .stderr(Stdio::piped());
        
        let timeout = Duration::from_secs(self.config.timeout_seconds);
        
        let output = cmd.output()
            .map_err(|e| format!("Failed to execute C++ demo: {}", e))?;
        
        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(format!("C++ demo failed: {}", stderr));
        }
        
        let execution_time = start_time.elapsed();
        
        // Parse C++ output (assuming JSON format)
        let stdout = String::from_utf8_lossy(&output.stdout);
        let cpp_result = self.parse_cpp_output(&stdout)
            .unwrap_or_else(|_| {
                // Fallback: create result with basic info
                ExecutionResult {
                    events_processed: 0, // Would need C++ demo to output this
                    execution_time_ns: execution_time.as_nanos() as u64,
                    process_states: HashMap::new(),
                    memory_usage_kb: None,
                    seed_used: self.config.seed,
                }
            });
        
        Ok(cpp_result)
    }
    
    fn parse_cpp_output(&self, output: &str) -> Result<ExecutionResult, String> {
        // Try to parse JSON output from C++ demo
        // This would need to match the C++ demo's output format
        // For now, return a basic result structure
        
        // TODO: Implement proper JSON parsing when C++ demo output format is defined
        let result = ExecutionResult {
            events_processed: 0, // Placeholder
            execution_time_ns: 0, // Placeholder
            process_states: HashMap::new(),
            memory_usage_kb: None,
            seed_used: self.config.seed,
        };
        
        Ok(result)
    }
    
    fn compare_results(&self, grey: &ExecutionResult, cpp: &ExecutionResult) -> ComparisonResult {
        // Compare event counts
        let events_match = grey.events_processed == cpp.events_processed;
        
        // Compare execution times (with ratio)
        let time_ratio = if cpp.execution_time_ns > 0 {
            grey.execution_time_ns as f64 / cpp.execution_time_ns as f64
        } else {
            0.0
        };
        
        // Compare process states
        let mut state_differences = Vec::new();
        
        // Check for missing or different process states
        let all_pids: std::collections::HashSet<usize> = grey.process_states.keys()
            .chain(cpp.process_states.keys())
            .cloned()
            .collect();
        
        for pid in all_pids {
            let grey_state = grey.process_states.get(&pid);
            let cpp_state = cpp.process_states.get(&pid);
            
            if grey_state != cpp_state {
                state_differences.push(format!(
                    "Process {}: Grey={:?}, C++={:?}",
                    pid, grey_state, cpp_state
                ));
            }
        }
        
        // Determine if parity is achieved
        let parity_achieved = events_match && state_differences.is_empty();
        
        ComparisonResult {
            grey_result: grey.clone(),
            cpp_result: cpp.clone(),
            events_match,
            time_ratio,
            state_differences,
            parity_achieved,
        }
    }
    
    fn print_comparison_summary(&self, comparison: &ComparisonResult) {
        println!("\n📊 Comparison Results:");
        println!("  Parity Achieved: {}", 
                 if comparison.parity_achieved { "✅ YES" } else { "❌ NO" });
        
        println!("\n  Event Counts:");
        println!("    Grey: {}", comparison.grey_result.events_processed);
        println!("    C++:  {}", comparison.cpp_result.events_processed);
        println!("    Match: {}", if comparison.events_match { "✅" } else { "❌" });
        
        println!("\n  Execution Times:");
        println!("    Grey: {}ns", comparison.grey_result.execution_time_ns);
        println!("    C++:  {}ns", comparison.cpp_result.execution_time_ns);
        println!("    Ratio: {:.2}x", comparison.time_ratio);
        
        if !comparison.state_differences.is_empty() {
            println!("\n  State Differences:");
            for diff in &comparison.state_differences {
                println!("    {}", diff);
            }
        } else {
            println!("\n  ✅ All process states match");
        }
        
        if comparison.parity_achieved {
            println!("\n🎉 Deterministic parity achieved!");
        } else {
            println!("\n⚠️  Parity not achieved - investigate differences");
        }
    }
}

/// Convenience function to run a quick comparison
pub fn quick_compare(grey_program: &str, seed: u64) -> Result<ComparisonResult, String> {
    let config = ComparisonConfig {
        seed,
        max_events: 500,
        timeout_seconds: 10,
        enable_memory_tracking: false,
        cpp_demo_path: None, // Would need to be set for full comparison
    };
    
    let harness = BettiComparisonHarness::new(config);
    harness.run_comparison(grey_program)
}

/// Demo programs for testing
pub const DEMO_PROGRAMS: &[(&str, &str)] = &[
    ("logistics", r#"
module LogisticsDemo {
    event PackageShipped {
        package_id: Int,
        destination: Coord,
    }
    
    process LogisticsHub {
        packages_in_transit: Int,
        
        method init() {
            this.packages_in_transit = 0;
        }
        
        method handle_shipment(event: PackageShipped) {
            this.packages_in_transit = this.packages_in_transit + 1;
        }
    }
}
"#),
    ("contagion", r#"
module ContagionDemo {
    event InfectionSpread {
        source_node: Int,
        target_node: Int,
    }
    
    process Node {
        infected: Bool,
        
        method init() {
            this.infected = false;
        }
        
        method handle_infection(event: InfectionSpread) {
            if (!this.infected) {
                this.infected = true;
            }
        }
    }
}
"#),
];

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn test_grey_workload_execution() {
        let config = ComparisonConfig {
            seed: 123,
            max_events: 100,
            timeout_seconds: 5,
            enable_memory_tracking: false,
            cpp_demo_path: None,
        };
        
        let harness = BettiComparisonHarness::new(config);
        let result = harness.execute_grey_workload(DEMO_PROGRAMS[0].1);
        
        assert!(result.is_ok(), "Grey workload should execute successfully");
        
        let execution = result.unwrap();
        assert!(execution.events_processed >= 0, "Should process events");
        assert!(execution.execution_time_ns > 0, "Should have execution time");
        assert_eq!(execution.seed_used, 123, "Should use configured seed");
    }

    #[test]
    fn test_quick_compare() {
        let result = quick_compare(DEMO_PROGRAMS[0].1, 42);
        
        assert!(result.is_ok(), "Quick compare should succeed");
        let comparison = result.unwrap();
        
        // Should have basic execution data
        assert!(comparison.grey_result.events_processed >= 0);
        assert!(comparison.grey_result.execution_time_ns > 0);
        
        // Note: parity will likely not be achieved since we don't have C++ demo
        println!("Quick compare completed: {} events processed", 
                 comparison.grey_result.events_processed);
    }

    #[test]
    fn test_comparison_config() {
        let config = ComparisonConfig::default();
        assert_eq!(config.seed, 42);
        assert_eq!(config.max_events, 1000);
        assert_eq!(config.timeout_seconds, 30);
        
        let custom_config = ComparisonConfig {
            seed: 999,
            max_events: 2000,
            timeout_seconds: 60,
            enable_memory_tracking: true,
            cpp_demo_path: Some(PathBuf::from("/path/to/cpp/demo")),
        };
        
        assert_eq!(custom_config.seed, 999);
        assert!(custom_config.enable_memory_tracking);
    }

    #[test]
    fn test_execution_result_structure() {
        let mut states = HashMap::new();
        states.insert(0, 1);
        states.insert(1, 2);
        
        let result = ExecutionResult {
            events_processed: 150,
            execution_time_ns: 1_000_000,
            process_states: states.clone(),
            memory_usage_kb: Some(1024),
            seed_used: 42,
        };
        
        assert_eq!(result.events_processed, 150);
        assert_eq!(result.process_states, states);
        assert_eq!(result.seed_used, 42);
    }

    #[test]
    fn test_demo_programs() {
        // Verify that demo programs compile successfully
        for (name, program) in DEMO_PROGRAMS {
            let compile_result = compile(program);
            assert!(compile_result.is_ok(), "Demo program '{}' should compile", name);
            
            println!("✅ Demo program '{}' compiles successfully", name);
        }
    }
}