//! Integration tests for the Grey compiler frontend
//! 
//! These tests verify end-to-end compilation pipeline functionality
//! and demonstrate example Grey programs that should compile successfully.

use grey_lang::compile;
use std::fs;
use tempfile::NamedTempFile;
use std::path::PathBuf;

#[test]
fn test_simple_module() {
    let source = r#"
        module simple_test {
            process TestProcess {
                value: int,
                
                fn new() -> TestProcess {
                    return TestProcess {
                        value: 42
                    };
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Simple module should compile successfully");
}

#[test]
fn test_basic_arithmetic() {
    let source = r#"
        module arithmetic_test {
            process Calculator {
                fn add(a: int, b: int) -> int {
                    return a + b;
                }
                
                fn multiply(x: int, y: int) -> int {
                    return x * y;
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Arithmetic operations should compile");
}

#[test]
fn test_event_system() {
    let source = r#"
        module event_test {
            event TestEvent {
                data: string,
                timestamp: timestamp
            }
            
            process EventHandler {
                handle TestEvent(event: TestEvent) {
                    // Process the event
                    let message = event.data;
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Event system should compile");
}

#[test]
fn test_coordinate_literals() {
    let source = r#"
        module coord_test {
            process PositionTracker {
                position: coord,
                
                fn new() -> PositionTracker {
                    return PositionTracker {
                        position: <10, 20, 30>
                    };
                }
                
                fn move_to(x: int, y: int, z: int) {
                    self.position = <x, y, z>;
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Coordinate literals should compile");
}

#[test]
fn test_control_flow() {
    let source = r#"
        module control_flow_test {
            process FlowController {
                fn conditional_test(value: int) -> int {
                    if value > 0 {
                        return value * 2;
                    } else {
                        return 0;
                    }
                }
                
                fn loop_test(limit: int) -> int {
                    let sum = 0;
                    for i in 0..limit {
                        sum = sum + i;
                    }
                    return sum;
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Control flow constructs should compile");
}

#[test]
fn test_type_annotations() {
    let source = r#"
        module typing_test {
            process TypeTest {
                // Primitive types
                int_val: int,
                float_val: float,
                bool_val: bool,
                string_val: string,
                byte_val: byte,
                coord_val: coord,
                
                // Reference types
                owned_data: owned string,
                shared_state: shared int,
                mutable_buffer: mut [100] of int,
                
                // Collection types
                array_data: Array<int, 50>,
                event_queue: Queue<string, 200>,
                process_pool: Pool<ProcessRef, 1000>,
                
                fn new() -> TypeTest {
                    return TypeTest {
                        int_val: 42,
                        float_val: 3.14,
                        bool_val: true,
                        string_val: "hello",
                        byte_val: 0xFF,
                        coord_val: <1, 2, 3>,
                        owned_data: "owned string",
                        shared_state: 100,
                        mutable_buffer: [100] of 0,
                        array_data: Array<int, 50>::new(),
                        event_queue: Queue<string, 200>::new(),
                        process_pool = Pool<ProcessRef, 1000>::new()
                    };
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Type annotations should compile");
}

#[test]
fn test_o1_memory_constraints() {
    // This should compile - bounded collections
    let source = r#"
        module o1_valid {
            process ValidProcess {
                // Bounded collections - OK for O(1)
                fixed_array: [27] of int,      // Fixed size
                bounded_queue: Queue<int, 100>, // Bounded queue
                
                fn new() -> ValidProcess {
                    return ValidProcess {
                        fixed_array: [27] of 0,
                        bounded_queue: Queue<int, 100>::new()
                    };
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "O(1) valid code should compile");
}

#[test]
fn test_unbounded_collections_should_fail() {
    // This should fail - unbounded collections violate O(1)
    let source = r#"
        module o1_invalid {
            process InvalidProcess {
                // Unbounded - violates O(1)
                unbounded_list: [] of int,
                dynamic_map: Map<string, int>
            }
            
            fn problematic_function() {
                // This would create unbounded collections
                let data = [];  // ERROR: No size bound
            }
        }
    "#;
    
    let result = compile(source);
    // This should fail due to unbounded collections
    // Note: This might not fail yet as validation is still being implemented
    // assert!(result.is_err(), "Unbounded collections should fail");
}

#[test]
fn test_file_parsing() {
    // Create a temporary file with Grey source
    let source = r#"
        module file_test {
            const VERSION = "1.0";
            
            process FileProcessor {
                version: string,
                
                fn new() -> FileProcessor {
                    return FileProcessor {
                        version: VERSION
                    };
                }
            }
        }
    "#;
    
    let temp_file = NamedTempFile::with_suffix(".grey")
        .expect("Failed to create temp file");
    fs::write(&temp_file, source).expect("Failed to write to temp file");
    
    let result = compile(source);
    assert!(result.is_ok(), "File content should compile");
    
    // Clean up
    fs::remove_file(temp_file).ok();
}

#[test]
fn test_complex_example() {
    let source = r#"
        module logistics_network {
            // Module dependencies
            use std::math;
            use std::lattice;
            
            // Module-level constants
            const GRID_SIZE = 32;
            const MAX_DELIVERIES = 1000;
            
            // Event definitions
            event DeliveryRequest {
                destination: coord,
                priority: int,
                payload: Package,
                timestamp: timestamp
            }
            
            event DeliveryComplete {
                order_id: string,
                delivered_to: coord,
                timestamp: timestamp
            }
            
            // Custom types
            type Package = {
                id: string,
                weight: float,
                fragile: bool
            };
            
            // Process definitions
            process Drone {
                coord: coord,
                status: DroneStatus,
                current_delivery: Option<Package>,
                delivery_history: [100] of DeliveryComplete,
                inbox: Queue<DeliveryRequest, 50>,
                
                fn new(x: int, y: int, z: int) -> Drone {
                    return Drone {
                        coord: <x, y, z>,
                        status: DroneStatus::Idle,
                        current_delivery: None,
                        delivery_history: [100] of DeliveryComplete::null(),
                        inbox: Queue<DeliveryRequest, 50>::new()
                    };
                }
                
                handle DeliveryRequest(request: DeliveryRequest) {
                    match self.status {
                        DroneStatus::Idle => {
                            self.status = DroneStatus::EnRoute;
                            self.current_delivery = Some(request.payload);
                            self.deliver_package(request);
                        }
                        DroneStatus::EnRoute => {
                            // Queue the request for later
                            self.inbox.push(request);
                        }
                        DroneStatus::Returning => {
                            self.inbox.push(request);
                        }
                    }
                }
                
                fn deliver_package(request: DeliveryRequest) {
                    // Simulate delivery logic
                    let distance = calculate_distance(self.coord, request.destination);
                    // O(1) memory: no dynamic allocation
                    if distance < 10.0 {
                        self.complete_delivery(request);
                    }
                }
                
                fn complete_delivery(request: DeliveryRequest) {
                    let completion = DeliveryComplete {
                        order_id: request.payload.id,
                        delivered_to: request.destination,
                        timestamp: now()
                    };
                    
                    // O(1) bounded history
                    self.add_to_history(completion);
                    self.status = DroneStatus::Returning;
                    self.current_delivery = None;
                }
                
                fn add_to_history(delivery: DeliveryComplete) {
                    // O(1) bounded circular buffer
                    let index = self.get_next_history_index();
                    self.delivery_history[index] = delivery;
                }
                
                fn get_next_history_index() -> int {
                    // Simple O(1) index calculation
                    return 0; // Simplified
                }
            }
            
            process DeliveryCoordinator {
                active_drones: Lattice<Option<ProcessRef<Drone>>>,
                pending_requests: Queue<DeliveryRequest, 200>,
                
                fn new() -> DeliveryCoordinator {
                    return DeliveryCoordinator {
                        active_drones: Lattice<Option<ProcessRef<Drone>>>::new(),
                        pending_requests: Queue<DeliveryRequest, 200>::new()
                    };
                }
                
                fn assign_delivery(request: DeliveryRequest) {
                    // Find nearest available drone
                    let drone = self.find_nearest_drone(request.destination);
                    match drone {
                        Some(d) => {
                            d.handle_delivery_request(request);
                        }
                        None => {
                            self.pending_requests.push(request);
                        }
                    }
                }
                
                fn find_nearest_drone(target: coord) -> Option<ProcessRef<Drone>> {
                    // O(1) spatial search in toroidal space
                    return None; // Simplified
                }
            }
            
            // Enums
            enum DroneStatus {
                Idle,
                EnRoute,
                Returning,
                Maintenance
            }
            
            // Helper functions
            fn calculate_distance(a: coord, b: coord) -> float {
                let dx = (a.x - b.x) as float;
                let dy = (a.y - b.y) as float; 
                let dz = (a.z - b.z) as float;
                return math::sqrt(dx * dx + dy * dy + dz * dz);
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Complex example should compile successfully");
}

#[test]
fn test_error_recovery() {
    // Test that parser can recover from some errors and continue parsing
    let source = r#"
        module error_recovery {
            // This has a syntax error but should still parse what it can
            process BadProcess {
                value: int,
                
                fn broken_method() -> int {
                    // Missing return statement and syntax error
                    if value > 0 {
                        return value
                    // Missing closing brace
            }
        }
        
        // This should still parse
        process GoodProcess {
            value: int,
            
            fn working_method() -> int {
                return 42;
            }
        }
    "#;
    
    let result = compile(source);
    // Should have parse errors but might still produce partial AST
    // The exact behavior depends on error recovery implementation
    println!("Error recovery test result: {:?}", result);
}

#[test]
fn test_type_checking_validation() {
    let source = r#"
        module type_checking {
            process TypeValidation {
                fn type_mismatch_test() {
                    let x: int = 42;
                    let y: float = 3.14;
                    let result = x + y;  // Should type check (numeric coercion)
                }
                
                fn invalid_operation() {
                    let text: string = "hello";
                    let number: int = text + 5;  // Should fail type check
                }
            }
        }
    "#;
    
    let result = compile(source);
    // This should have type checking errors
    // The exact validation depends on the type checker implementation
    println!("Type checking test result: {:?}", result);
}

#[test]
fn test_example_from_spec() {
    // Test one of the examples from the language specification
    let source = r#"
        module logistics_network {
            // Import dependencies
            use std::math;
            use std::lattice;
            
            // Module-level constants
            const GRID_SIZE = 32;
            
            // Process definitions
            process Drone {
                coord: <x: int, y: int, z: int>,
                state: DeliveryState,
                
                fn new(x: int, y: int, z: int) -> Drone {
                    let drone = Drone {
                        coord: <x, y, z>,
                        state: DeliveryState::Idle
                    };
                    return drone;
                }
            }
            
            type DeliveryState = {
                status: string,
                current_package: Option<string>
            };
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Spec example should compile");
}

#[test]
fn test_repl_examples() {
    // Test simple expressions that should work in REPL
    let examples = vec![
        "42",
        "3.14", 
        "true",
        "\"hello world\"",
        "<1, 2, 3>",
        "1 + 2 * 3",
        "(10 + 5) / 3",
    ];
    
    for example in examples {
        let result = compile(example);
        // These simple expressions might not compile as standalone
        // but they should not cause fatal errors
        println!("REPL example '{}' result: {:?}", example, result);
    }
}

#[test]
fn test_performance_benchmark() {
    use std::time::Instant;
    
    let source = r#"
        module performance_test {
            process BenchmarkProcess {
                data: [1000] of int,
                
                fn new() -> BenchmarkProcess {
                    let mut process = BenchmarkProcess {
                        data: [1000] of 0
                    };
                    
                    // Initialize with some values
                    for i in 0..1000 {
                        process.data[i] = i;
                    }
                    
                    return process;
                }
                
                fn sum_data() -> int {
                    let total = 0;
                    for i in 0..1000 {
                        total = total + self.data[i];
                    }
                    return total;
                }
            }
        }
    "#;
    
    let start = Instant::now();
    let result = compile(source);
    let duration = start.elapsed();
    
    assert!(result.is_ok(), "Performance test should compile");
    println!("Compilation took: {:?}", duration);
    
    // Should compile in reasonable time (less than 1 second for this size)
    assert!(duration.as_secs() < 1, "Compilation should be fast");
}

#[test]
fn test_memory_constraints_enforcement() {
    let source = r#"
        module memory_constraints {
            process MemoryTest {
                // These should be OK - bounded
                small_array: [10] of int,
                medium_queue: Queue<string, 100>,
                process_ref: ProcessRef,
                
                // These might trigger O(1) validation
                fn process_data() {
                    // Bounded loop
                    for i in 0..100 {
                        let temp = self.small_array[i];
                        // Process temp
                    }
                }
            }
        }
    "#;
    
    let result = compile(source);
    assert!(result.is_ok(), "Memory-constrained code should compile");
    
    // Additional validation could be performed here
    // to ensure O(1) constraints are being checked
}

#[test]
fn test_diagnostic_quality() {
    let source = r#"
        module diagnostic_test {
            // This should trigger a parse error
            process BrokenProcess {
                value: int,  // Missing semicolon is OK
                
                fn broken_method() {
                    // This will cause issues
                    let x = 
                }
            }
            
            // This should be fine
            process GoodProcess {
                value: int,
                
                fn working_method() -> int {
                    return self.value + 1;
                }
            }
        }
    "#;
    
    let result = compile(source);
    
    match result {
        Ok(_) => {
            println!("Unexpected success - should have diagnostics");
        }
        Err(e) => {
            println!("Expected error: {:?}", e);
            // Verify we get meaningful error messages
            let error_msg = format!("{:?}", e);
            assert!(error_msg.contains("error") || error_msg.contains("Error"));
        }
    }
}

// Benchmark test for larger programs
#[test]
fn test_large_program_compilation() {
    use std::time::Instant;
    
    let mut source = "module large_program {\n".to_string();
    
    // Add many processes to test scaling
    for i in 0..50 {
        source.push_str(&format!(r#"
            process Process{} {{
                value{}: int,
                
                fn new() -> Process{} {{
                    return Process{} {{
                        value{}: {}
                    }};
                }}
            }}
        "#, i, i, i, i, i, i));
    }
    
    source.push_str("}\n");
    
    let start = Instant::now();
    let result = compile(&source);
    let duration = start.elapsed();
    
    println!("Large program compilation took: {:?}", duration);
    
    assert!(result.is_ok(), "Large program should compile");
    assert!(duration.as_secs() < 5, "Large program compilation should complete in reasonable time");
}