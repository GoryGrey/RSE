# Grey Compiler Backend System

This document describes the backend infrastructure for the Grey programming language compiler, with specific focus on the Betti RDL backend for executing Grey programs on the Betti runtime system.

## Overview

The Grey compiler uses a multi-stage architecture:

1. **Parsing**: Grey source code → AST
2. **Type Checking**: AST → Typed AST
3. **IR Generation**: Typed AST → Intermediate Representation (IR)
4. **Code Generation**: IR → Backend-specific output
5. **Execution**: Runtime-specific execution and telemetry

## Architecture

### Workspace Structure

```
grey_compiler/
├── crates/
│   ├── grey_lang/          # Core language processing
│   ├── grey_ir/            # Intermediate representation
│   ├── grey_backends/      # Backend infrastructure
│   └── greyc_cli/          # Command-line interface
├── tests/                  # Integration tests
└── scripts/                # Utility scripts and harnesses
```

### Core Components

#### 1. IR (Intermediate Representation)

The `grey_ir` crate provides a canonical intermediate representation suitable for backend code generation:

- **Program Structure**: Top-level organization with processes, events, and constants
- **Process Model**: Grid-based spatial computation with 3D coordinates
- **Event System**: Typed event definitions with field declarations
- **Resource Bounds**: O(1) memory constraints validation
- **Serialization**: JSON support for cross-platform compatibility

Key types:
- `IrProgram`: Top-level program structure
- `IrProcess`: Process definitions with coordinates and state
- `IrEvent`: Event type definitions
- `IrTransition`: State machine transitions
- `Coord`: 3D coordinate system (0-31 per dimension)

#### 2. Backend Infrastructure

The `grey_backends` crate provides a trait-based backend system:

- **CodeGenerator Trait**: Standard interface for all backends
- **Output Management**: File generation, runtime configuration, metadata
- **Execution Framework**: Telemetry collection and validation
- **Utilities**: Process placement, coordinate validation, resource checking

#### 3. Betti RDL Backend

The Betti RDL backend (`grey_backends::betti_rdl`) implements the CodeGenerator trait to:

- Generate executable Rust code using the `betti-rdl` FFI crate
- Configure process placement in 2D/3D coordinate space
- Handle deterministic event ordering
- Collect runtime telemetry (events processed, execution time, process states)
- Validate resource constraints

## Usage

### Command-Line Interface

#### Basic Compilation and Execution

```bash
# Compile Grey source to Betti RDL and run
greyc emit-betti program.grey --run

# Compile without execution
greyc emit-betti program.grey

# Configure execution parameters
greyc emit-betti program.grey --run --max-events 5000 --telemetry
```

#### CLI Options

- `--run`: Execute the generated Betti RDL workload
- `--max-events N`: Maximum events to process (default: 1000)
- `--seed N`: Deterministic seed used for initial event injection (default: 42)
- `--telemetry`: Enable detailed telemetry output

### Programmatic Usage

#### Compile and Execute a Grey Program

```rust
use grey_lang::compile;
use grey_ir::{IrBuilder, IrProgram};
use grey_backends::betti_rdl::{BettiRdlBackend, BettiConfig};
use grey_backends::CodeGenerator;

// 1. Compile Grey source
let source = r#"
    module Demo {
        event Message {
            text: String,
        }
        
        process Node {
            message_count: Int,
            method init() {
                this.message_count = 0;
            }
        }
    }
"#;

let typed_program = compile(source)?;

// 2. Build IR
let mut ir_builder = IrBuilder::new();
let ir_program = ir_builder.build_program("demo", &typed_program)?;

// 3. Configure backend
let backend = BettiRdlBackend::new(BettiConfig {
    max_events: 1000,
    telemetry_enabled: true,
    ..Default::default()
});

// 4. Generate code
let output = backend.generate_code(ir_program)?;

// 5. Execute
let telemetry = backend.execute(&output)?;

println!("Processed {} events", telemetry.events_processed);
```

#### Custom Backend Configuration

```rust
use grey_backends::{ProcessPlacement, EventOrdering};

let config = BettiConfig {
    process_placement: ProcessPlacement::GridLayout { spacing: 4 },
    max_events: 5000,
    telemetry_enabled: true,
    validate_coordinates: true,
};
```

## Integration Testing

### Betti Integration Tests

Located in `grey_compiler/tests/betti_integration.rs`, these tests provide:

- **Compilation Validation**: Ensure Grey programs compile successfully
- **IR Building**: Verify IR construction from typed AST
- **Code Generation**: Validate generated Betti RDL code structure
- **Execution Testing**: Run workloads and collect telemetry
- **Configuration Testing**: Verify backend configuration options

Run with:
```bash
cd grey_compiler
cargo test betti_integration
```

### Demo Programs

The integration tests include reduced versions of key demo programs:

- **Logistics Demo**: Package shipping and delivery tracking
- **Contagion Demo**: Disease spread simulation

These demonstrate the core process/event model and provide validation targets.

## Killer Demo (Grey): SIR Epidemic

A production-like Grey demo lives at:

- `grey_compiler/examples/sir_demo.grey`

It defines multiple event types, bounded per-process state, and a `RUNTIME_PROCESSES` constant that the Betti backend uses to spawn many process instances (still O(1) memory in the Betti lattice).

### Run via the CLI

```bash
cd grey_compiler

# Compile to the Betti backend and execute immediately
cargo run -p greyc_cli --bin greyc -- emit-betti examples/sir_demo.grey \
  --run --max-events 1000 --seed 42 --telemetry
```

Expected behavior:
- deterministic `events_processed` and `current_time` for the same `--seed`
- process-state snapshot keyed by Betti PID (toroidal node id)

## Comparison Harness (Grey vs C++)

To prove deterministic parity, we ship a harness as a real Cargo crate:

- Harness crate: `grey_compiler/crates/grey_harness`
- C++ reference executable: `src/cpp_kernel/demos/scale_demos/grey_sir_reference.cpp` (built via CMake)

### Run the harness

```bash
cd grey_compiler
cargo run -p grey_harness --bin grey_compare_sir -- --max-events 1000 --seed 42 --spacing 1
```

The harness will:
1. compile the Grey demo
2. run it through the Betti backend
3. build + run the C++ reference
4. compare `events_processed`, `current_time`, and the per-process state snapshot

### Integration test

The end-to-end harness test is marked `#[ignore]` (it builds C++ via CMake):

```bash
cd grey_compiler
cargo test -p grey_harness -- --ignored
```

## Extending the Backend System

### Adding New Backends

1. **Implement CodeGenerator Trait**:

```rust
use grey_backends::{CodeGenerator, CodeGenOutput, BackendError};

pub struct MyBackend {
    // Configuration
}

impl CodeGenerator for MyBackend {
    fn generate_code(&self, program: &IrProgram) -> Result<CodeGenOutput, BackendError> {
        // Generate backend-specific code
    }
    
    fn execute(&self, output: &CodeGenOutput) -> Result<ExecutionTelemetry, BackendError> {
        // Execute generated code and collect telemetry
    }
    
    fn config_options(&self) -> HashMap<String, ConfigOption> {
        // Define configuration options
    }
}
```

2. **Add to CLI**:

Update `greyc_cli/src/main.rs` to include the new backend option.

3. **Add Tests**: Create integration tests for the new backend.

### IR Extensions

To extend the IR with new features:

1. **Add Types**: Define new IR types in `grey_ir/src/lib.rs`
2. **Update Builder**: Extend `IrBuilder` to construct the new IR constructs
3. **Validate**: Add validation logic in backend utilities
4. **Test**: Add comprehensive tests

## Best Practices

### Process Placement

- Use **GridLayout** for most applications (good spatial distribution)
- Use **SingleNode** for simple single-process programs
- Use **Custom** for specific spatial requirements

### Resource Management

- Monitor **event counts** to detect runaway workloads
- Track **execution time** for performance regression detection
- Use **telemetry enabled** for development, disabled for production

### Deterministic Testing

- Always use **explicit seeds** for reproducible tests
- Compare **process states** for functional correctness
- Monitor **memory usage** for resource leak detection

## Limitations and Caveats

### Current Limitations

1. **Limited Event Injection**: Initial events must be injected programmatically
2. **Basic State Management**: Process state is limited to simple field updates
3. **Coordinate Bounds**: Fixed to 0-31 range per dimension
4. **Single Runtime**: Currently only Betti RDL backend is implemented

### Performance Considerations

- **Grid Layout**: Use appropriate spacing to avoid coordinate collisions
- **Event Batching**: Process events in batches for better throughput
- **Memory Tracking**: Enable only when needed to avoid overhead

### Future Enhancements

- **Multi-runtime Support**: Add backends for other runtime systems
- **Advanced State Machines**: Support complex state transition logic
- **Distributed Execution**: Support for multi-node deployments
- **Real-time Telemetry**: Streaming telemetry for long-running workloads

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure all dependencies are properly linked
2. **Coordinate Validation**: Check that process coordinates are within bounds
3. **Resource Constraints**: Verify processes/events don't exceed limits
4. **Execution Timeouts**: Increase timeout or reduce max_events

### Debug Mode

Enable detailed logging:
```bash
RUST_LOG=debug greyc emit-betti program.grey --run
```

### Telemetry Analysis

When execution completes, examine:
- **Events Processed**: Should be > 0 for active workloads
- **Execution Time**: Indicates computational complexity
- **Process States**: Final state of all processes
- **Memory Usage**: Resource consumption tracking

## API Reference

### Key Types

- `IrProgram`: Program structure with processes, events, constants
- `BettiRdlBackend`: Main backend for Betti RDL execution
- `ExecutionTelemetry`: Runtime execution metrics
- `ComparisonResult`: Parity comparison results

### Configuration

- `BettiConfig`: Backend configuration options
- `ComparisonConfig`: Harness configuration for parity testing
- `RuntimeConfig`: Runtime execution parameters

### CLI Commands

- `greyc emit-betti`: Compile Grey to Betti RDL format
- `greyc check`: Validate Grey source without compilation
- `greyc repl`: Interactive Grey evaluation

For detailed API documentation, see the individual crate documentation with `cargo doc --open`.