# Grey Compiler Frontend

A Rust-based compiler frontend for the Grey programming language, implementing O(1) memory guarantees and deterministic execution on a 32³ toroidal computational substrate.

## Overview

The Grey compiler frontend provides a complete compilation pipeline from source code to validated typed programs, ensuring adherence to the language's spatial and temporal constraints.

### Key Features

- **Lexical Analysis**: UTF-8 aware tokenization with comprehensive error recovery
- **Parsing**: Recursive descent parser generating rich AST representations
- **Type System**: Static type checking with ownership and reference semantics
- **O(1) Validation**: Static analysis ensuring bounded memory usage and computation
- **Developer Tools**: CLI with check, format, and REPL functionality

## Architecture

```
grey_compiler/
├── Cargo.toml                    # Workspace root
├── crates/
│   ├── grey_lang/                # Core language library
│   │   ├── src/
│   │   │   ├── lexer.rs          # Lexical analysis
│   │   │   ├── parser.rs         # Parsing & AST construction
│   │   │   ├── ast.rs            # Abstract Syntax Tree definitions
│   │   │   ├── types.rs          # Type system & checking
│   │   │   ├── constraints.rs    # O(1) constraint validation
│   │   │   ├── diagnostics.rs    # Error reporting & diagnostics
│   │   │   └── lib.rs            # Main library interface
│   │   └── Cargo.toml
│   └── greyc_cli/                # Command-line interface
│       ├── src/
│       │   └── main.rs           # CLI implementation
│       └── Cargo.toml
├── tests/                        # Integration tests
└── README.md                     # This file
```

## Getting Started

### Prerequisites

- Rust 1.70+ (2021 edition)
- Cargo package manager

### Building

```bash
# Build the entire workspace
cd grey_compiler
cargo build

# Build just the library
cargo build -p grey_lang

# Build the CLI
cargo build -p greyc_cli
```

### Testing

```bash
# Run all tests
cargo test

# Run tests for specific crate
cargo test -p grey_lang
cargo test -p greyc_cli

# Run with verbose output
cargo test -v
```

## Usage Examples

### Command Line Interface

#### Check a Grey source file:
```bash
cargo run --bin greyc check example.grey
```

#### Format a source file:
```bash
cargo run --bin greyc format input.grey --output formatted.grey
```

#### Start the REPL:
```bash
cargo run --bin greyc repl
```

### Programmatic Usage

#### Parse and compile Grey source:
```rust
use grey_lang::compile;

fn main() -> Result<(), grey_lang::diagnostics::Diagnostic> {
    let source = r#"
        module hello_world {
            process Greeter {
                greeting: string;
                
                fn new() -> Greeter {
                    return Greeter {
                        greeting: "Hello, World!"
                    };
                }
                
                fn greet() -> string {
                    return self.greeting;
                }
            }
        }
    "#;
    
    let typed_program = compile(source)?;
    println!("✅ Compilation successful!");
    
    Ok(())
}
```

#### Individual compilation phases:
```rust
use grey_lang::{parse_source, type_check_program, validate_program};

fn compile_phases(source: &str) -> Result<(), grey_lang::diagnostics::Diagnostic> {
    // Phase 1: Parse
    let program = parse_source(source)?;
    
    // Phase 2: Type check
    let typed_program = type_check_program(&program)?;
    
    // Phase 3: Validate O(1) constraints
    validate_program(&typed_program)?;
    
    println!("✅ All phases completed successfully!");
    Ok(())
}
```

## Language Features Supported

### ✅ Implemented

- **Module System**: Module declarations with dependencies
- **Process Definitions**: Process types with fields and methods
- **Event System**: Event types and handlers
- **Type System**: Primitive types, references, generics
- **Expression Parsing**: Arithmetic, boolean, control flow
- **Basic Type Checking**: Type inference and validation
- **O(1) Memory Analysis**: Spatial constraint validation
- **Ownership Rules**: Single ownership enforcement
- **CLI Tools**: Check, format, REPL

### 🚧 In Progress

- **Advanced Type Inference**: Generic type instantiation
- **Loop Bound Analysis**: Unbounded loop detection
- **Recursion Validation**: Termination analysis
- **Code Formatting**: AST-based pretty printing
- **Backend Integration**: Code generation for multiple targets

### 📋 Planned

- **Language Server Protocol**: IDE integration
- **Advanced Optimizations**: Spatial and temporal optimizations
- **WASM Backend**: WebAssembly compilation target
- **C++ Backend**: Native code generation
- **LLVM Backend**: Cross-platform compilation

## Example Programs

### Basic Process Definition

```grey
module counter_demo {
    process Counter {
        count: int,
        max: int,
        
        fn new(initial: int, limit: int) -> Counter {
            return Counter {
                count: initial,
                max: limit
            };
        }
        
        fn increment() -> bool {
            if self.count < self.max {
                self.count = self.count + 1;
                return true;
            }
            return false;
        }
        
        fn get_count() -> int {
            return self.count;
        }
    }
}
```

### Event-Driven Communication

```grey
module message_demo {
    event Message {
        from: coord,
        to: coord,
        payload: string,
        timestamp: timestamp
    }
    
    process Messenger {
        position: coord,
        inbox: queue<Message, 100>,
        
        fn new(pos: coord) -> Messenger {
            return Messenger {
                position: pos,
                inbox: queue::new(100)
            };
        }
        
        handle Message(msg: Message) {
            if msg.to == self.position {
                self.inbox.push(msg);
                self.process_message(msg);
            }
        }
        
        fn process_message(msg: Message) {
            // Handle received message
            let response = format!("Received: {}", msg.payload);
            emit Message {
                from: self.position,
                to: msg.from,
                payload: response,
                timestamp: now()
            } to msg.from;
        }
    }
}
```

### O(1) Memory Pattern

```grey
module toroidal_demo {
    const GRID_SIZE = 32;
    
    process GridNode {
        coord: coord,
        neighbors: [27] of ProcessRef<GridNode>,
        state: GridState,
        
        fn new(x: int, y: int, z: int) -> GridNode {
            let position = coord { x: x, y: y, z: z };
            return GridNode {
                coord: position,
                neighbors: [27] of ProcessRef::null(),
                state: GridState::Idle
            };
        }
        
        fn initialize_neighbors() {
            // O(1) neighbor access - fixed 27 neighbors in toroidal space
            let mut idx = 0;
            for dx in -1..=1 {
                for dy in -1..=1 {
                    for dz in -1..=1 {
                        if dx != 0 || dy != 0 || dz != 0 {
                            let neighbor_coord = wrap_coord(
                                self.coord.x + dx,
                                self.coord.y + dy, 
                                self.coord.z + dz
                            );
                            self.neighbors[idx] = lookup_process(neighbor_coord);
                            idx = idx + 1;
                        }
                    }
                }
            }
        }
    }
}
```

## O(1) Constraint Validation

The compiler validates the following constraints:

### Spatial Constraints
- **Maximum Processes**: 32³ = 32,768 processes
- **Memory Usage**: Bounded by toroidal space capacity
- **Collection Sizes**: All collections must have compile-time bounds

### Temporal Constraints
- **Loop Bounds**: All loops must have finite iteration bounds
- **Recursion**: Recursive calls must be bounded by base cases
- **Event Processing**: Event queues must have bounded size

### Ownership Rules
- **Single Ownership**: Each process owned by exactly one coordinate
- **No Shared Mutable State**: Processes communicate via events only
- **Move Semantics**: Ownership transfer is explicit

## Error Messages

The compiler provides rich diagnostic messages with source locations:

```
❌ Compilation failed:
E004: Validation error: Unbounded loop detected
  --> example.grey:25:15
   |
25 | while true {
   |           ^^^^
   |
   = help: Consider using toroidal transformation or bounded loops
```

## Development

### Adding New Features

1. **Lexer Updates**: Modify `lexer.rs` for new tokens
2. **Parser Extensions**: Update `parser.rs` for new syntax
3. **Type System**: Enhance `types.rs` for new type features
4. **Validation**: Extend `constraints.rs` for new O(1) rules

### Testing Strategy

- **Unit Tests**: Individual component testing
- **Integration Tests**: End-to-end compilation pipeline
- **Property Tests**: Invariant preservation
- **Performance Tests**: O(1) constraint verification

### Code Style

- Follow Rust standard formatting (`cargo fmt`)
- Use semantic naming and comprehensive documentation
- Maintain backwards compatibility for public APIs
- Add tests for all public functions

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make changes with tests
4. Run the test suite: `cargo test`
5. Format code: `cargo fmt`
6. Submit a pull request

## License

This project is part of the Grey language ecosystem. See the main project license for details.

## Resources

- [Grey Language Specification](docs/grey_language_spec.md)
- [Grey Compiler Architecture](docs/grey_compiler_architecture.md)
- [Betti-RDL Runtime Documentation](src/cpp_kernel/README.md)
- [Design Principles](../The%20General%20Theory%20of%20Recursive%20Symbolic.md)

---

**Status**: Frontend scaffolding complete. Ready for backend integration and advanced validation features.