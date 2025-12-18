---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Grey Universal Compiler Architecture

**Version 1.0**  
**Phase 2 Implementation Blueprint**

## Table of Contents
1. [Executive Summary](#1-executive-summary)
2. [System Overview](#2-system-overview)
3. [Compiler Pipeline Architecture](#3-compiler-pipeline-architecture)
4. [Module Boundaries & Directory Layout](#4-module-boundaries--directory-layout)
5. [Data Flow & IR Design](#5-data-flow--ir-design)
6. [O(1) Validator Design](#6-o1-validator-design)
7. [Backend Architecture](#7-backend-architecture)
8. [Testing & CI Strategy](#8-testing--ci-strategy)
9. [Developer Tooling Integration](#9-developer-tooling-integration)
10. [Operational Concerns](#10-operational-concerns)

---

## 1. Executive Summary

The Grey universal compiler translates high-level Grey programs into multiple target backends while maintaining **O(1) spatial guarantees** and **deterministic execution** properties. The compiler implements a multi-stage pipeline that preserves semantic correctness across Betti-RDL, WASM, C++, and LLVM IR backends.

### Key Design Principles
- **Semantic Preservation**: Each transformation preserves O(1) memory guarantees and deterministic scheduling
- **Multi-Backend Support**: Shared frontend and optimization passes feed specialized backend emitters
- **Compile-Time Validation**: Static analysis ensures bounded memory usage before code generation
- **Developer Experience**: Integrated tooling (LSP, formatter, REPL) maintains consistency across targets

### Target Backends
1. **Betti-RDL Primary**: Direct mapping to existing C++ runtime (`src/cpp_kernel`)
2. **WASM**: Web deployment with memory budgeting constraints
3. **C++**: System programming with manual memory management
4. **LLVM IR**: Cross-platform optimization and native code generation

---

## 2. System Overview

```mermaid
graph TD
    A[Grey Source Code] --> B[Frontend<br/>Lexer + Parser]
    B --> C[AST + Symbol Table]
    C --> D[Type System<br/>& Constraint Resolution]
    D --> E[O(1) Validator]
    E --> F[Optimizer<br/>Passes]
    F --> G[Canonical IR]
    G --> H[Betti-RDL<br/>Emitter]
    G --> I[WASM<br/>Emitter]
    G --> J[C++<br/>Emitter]
    G --> K[LLVM IR<br/>Emitter]
    
    subgraph "Development Tools"
        L[LSP Server]
        M[Formatter]
        N[REPL]
        O[Type Checker CLI]
    end
    
    C --> L
    M --> B
    N --> G
    O --> D
```

### Core Guarantees Maintained
- **Spatial Bounds**: Compile-time verification of O(1) memory usage
- **Temporal Determinism**: Preserved scheduling order (timestamp, dst_node, src_node, payload)
- **Event Semantics**: RDL event model consistency across all backends
- **Concurrency Safety**: Thread-safe event injection patterns

---

## 3. Compiler Pipeline Architecture

### 3.1 Frontend Stages

#### Lexer/Tokenizer
- **Input**: Grey source code (UTF-8 text)
- **Output**: Token stream with position metadata
- **Responsibilities**:
  - Unicode-aware lexical analysis
  - Comment and whitespace handling
  - Error recovery for malformed input
  - Line/column tracking for diagnostics

#### Parser
- **Input**: Token stream
- **Output**: Parse tree + symbol table
- **Responsibilities**:
  - LL(1) or recursive descent parsing
  - Syntax error reporting with recovery
  - Abstract Syntax Tree construction
  - Initial symbol resolution (imports, modules)

### 3.2 Semantic Analysis Stages

#### Type System & Constraint Resolution
- **Input**: AST + symbol table
- **Output**: Typed AST + constraint graph
- **Responsibilities**:
  - Static type checking
  - Generic type instantiation
  - Trait/impl resolution
  - Lifetime analysis for ownership

#### O(1) Validator
- **Input**: Typed AST
- **Output**: Validation report + corrected AST
- **Responsibilities**:
  - Loop bound analysis
  - Memory budget verification
  - Region constraint checking
  - Resource usage profiling

### 3.3 Optimization & Code Generation

#### Optimizer Passes
- **Input**: Validated AST
- **Output**: Optimized AST
- **Responsibilities**:
  - Dead code elimination
  - Constant folding
  - Event consolidation
  - Spatial optimization (toroidal mapping)

#### Backend Emitters
- **Input**: Canonical IR
- **Output**: Target-specific code
- **Responsibilities**:
  - IR-to-target translation
  - Target-specific optimizations
  - Code generation for runtime interfaces
  - Link-time artifact generation

---

## 4. Module Boundaries & Directory Layout

### 4.1 Rust Workspace Structure

```
grey_compiler/
├── Cargo.toml                 # Workspace root
├── greyc_cli/                 # Main compiler binary
├── grey_lsp_server/           # Language server
├── grey_tools/                # Developer utilities
│
├── crates/
│   ├── grey_lexer/           # Lexical analysis
│   ├── grey_parser/          # Parsing + AST
│   ├── grey_typecheck/       # Type system
│   ├── grey_validator/       # O(1) memory validation
│   ├── grey_optimizer/       # Optimization passes
│   ├── grey_ir/              # Canonical IR definition
│   │
│   ├── backends/
│   │   ├── grey_backend_betti/    # Betti-RDL primary
│   │   ├── grey_backend_wasm/      # WebAssembly
│   │   ├── grey_backend_cpp/       # C++ generation
│   │   └── grey_backend_llvm/      # LLVM IR
│   │
│   └── tools/
│       ├── grey_formatter/    # Code formatting
│       ├── grey_repl/         # Interactive REPL
│       └── grey_typecheck_cli/ # Standalone type checker
```

### 4.2 Module Responsibilities

#### Frontend Modules
- **grey_lexer**: UTF-8 text → token streams
- **grey_parser**: Tokens → AST + symbol resolution
- **grey_typecheck**: AST → typed AST + constraints

#### Analysis Modules  
- **grey_validator**: Memory budget verification
- **grey_optimizer**: AST optimization passes

#### IR & Backend Modules
- **grey_ir**: Canonical intermediate representation
- **backends/***: Target-specific code generation

#### Tooling Modules
- **grey_formatter**: AST → formatted source
- **grey_repl**: Interactive compilation/execution
- **grey_typecheck_cli**: Standalone type checking

### 4.3 Data Exchange Between Stages

| Stage | Input | Output | Key Artifacts |
|-------|-------|--------|---------------|
| Lexer | Source text | Tokens | Token + position |
| Parser | Tokens | AST + symbols | Parse tree + scope |
| Typecheck | AST | Typed AST | Type annotations |
| Validate | Typed AST | Validated AST | Constraint graph |
| Optimize | Validated AST | Optimized AST | IR representation |
| Backend | IR | Target code | Executable artifacts |

---

## 5. Data Flow & IR Design

### 5.1 Canonical IR Structure

The Grey Canonical IR (GIR) represents programs as dataflow graphs with explicit memory semantics:

```rust
// Simplified GIR representation
pub struct Program {
    pub modules: Vec<Module>,
    pub entry_point: FunctionId,
    pub memory_budget: MemoryBudget,
    pub event_topology: ToroidalConfig,
}

pub struct Module {
    pub functions: Vec<Function>,
    pub processes: Vec<Process>,
    pub events: Vec<EventType>,
}

pub struct Function {
    pub id: FunctionId,
    pub signature: FunctionSig,
    pub body: BasicBlock,
    pub spatial_bounds: SpatialConstraints,
}

pub struct EventType {
    pub payload_type: Type,
    pub spatial_requirements: SpatialBounds,
    pub temporal_constraints: TimeBounds,
}
```

### 5.2 IR Invariants

1. **Spatial Boundedness**: All loops have computable bounds
2. **Event Determinism**: Event ordering preserved in IR structure
3. **Memory Isolation**: No aliasing between spatial regions
4. **Time Constraints**: All temporal operations have explicit bounds

### 5.3 Transformation Pipeline

```mermaid
graph LR
    A[Parse Tree] --> B[AST + Symbols]
    B --> C[Typed AST]
    C --> D[Constraint Graph]
    D --> E[O(1) Validation]
    E --> F[Optimized AST]
    F --> G[GIR]
    G --> H[Target IR]
    H --> I[Backend Code]
    
    subgraph "Validation Checks"
        D --> D1[Loop Bound Analysis]
        D --> D2[Memory Budget Check]
        D --> D3[Region Validation]
        E --> E1[Semantic Verification]
    end
```

---

## 6. O(1) Validator Design

### 6.1 Static Analysis Components

#### Loop Bound Analysis
```rust
pub struct LoopBoundAnalyzer {
    // Analyzes all loops to ensure bounded iteration
    // Rejects unbounded recursion without toroidal transformation
}

impl LoopBoundAnalyzer {
    pub fn analyze_function(&self, func: &Function) -> Result<ValidationReport> {
        // 1. Extract loop structures from AST
        let loops = self.extract_loops(&func.body);
        
        // 2. Analyze iteration bounds
        for loop in loops {
            let bound = self.compute_bound(&loop)?;
            if !bound.is_finite() {
                return Err(ValidationError::UnboundedLoop(loop.id));
            }
        }
        
        // 3. Verify toroidal transformation for unbounded logic
        self.verify_toroidal_transform(&func.body)?;
        
        Ok(ValidationReport::success())
    }
}
```

#### Memory Budget Verification
```rust
pub struct MemoryBudgetAnalyzer {
    // Ensures spatial memory usage stays within 32³ limits
    // Tracks region allocations and deallocations
}

impl MemoryBudgetAnalyzer {
    pub fn verify_memory_budget(&self, program: &Program) -> Result<BudgetReport> {
        let mut region_tracker = RegionTracker::new();
        
        // 1. Analyze all region allocations
        for func in program.functions() {
            let allocations = self.analyze_allocations(func)?;
            region_tracker.add_allocations(allocations)?;
        }
        
        // 2. Verify spatial bounds
        if region_tracker.total_cells() > TOROIDAL_SPACE_CELLS {
            return Err(ValidationError::MemoryExceeded(
                region_tracker.total_cells(),
                TOROIDAL_SPACE_CELLS
            ));
        }
        
        Ok(region_tracker.generate_report())
    }
}
```

#### Resource Typing
```rust
pub struct ResourceTypeChecker {
    // Validates resource usage patterns
    // Ensures no shared mutable state violations
}

pub enum ResourceType {
    Process(ProcessId),
    Event(EventId), 
    SpatialRegion(SpatialBounds),
    TemporalConstraint(TimeBounds),
}
```

### 6.2 Diagnostic Integration

#### Error Reporting
```rust
pub struct ValidationDiagnostics {
    pub errors: Vec<ValidationError>,
    pub warnings: Vec<ValidationWarning>,
    pub suggestions: Vec<DiagnosticSuggestion>,
}

pub enum ValidationError {
    UnboundedLoop { loop_id: LoopId, location: SourceLocation },
    MemoryExceeded { used: usize, limit: usize, location: SourceLocation },
    InvalidToroidalTransform { reason: String, location: SourceLocation },
    ResourceLeak { resource: ResourceId, location: SourceLocation },
}
```

#### Developer Tooling Integration
- **IDE Integration**: Real-time validation in editor
- **CI Integration**: Fail builds on validation errors
- **REPL Integration**: Interactive validation feedback
- **Formatter Integration**: Preserve validation annotations

### 6.3 Validation Rules

| Rule Category | Constraint | Detection Method |
|---------------|------------|------------------|
| Spatial Bounds | Max 32³ cells | Abstract interpretation |
| Loop Bounds | Finite iterations | Induction variable analysis |
| Event Ordering | Deterministic timestamps | Dependency graph analysis |
| Memory Isolation | No aliasing between regions | Escape analysis |
| Resource Lifetime | Proper cleanup | Lifetime verification |

---

## 7. Backend Architecture

### 7.1 Betti-RDL Backend (Primary)

#### Mapping Strategy
```cpp
// GIR to Betti-RDL mapping
struct BettiRDLEmitter {
    // Direct mapping to existing C++ runtime
    // Preserves O(1) guarantees through runtime integration
};

struct RDLMapping {
    // Grey Function → C++ Process
    FunctionId → ProcessType { execute() }
    
    // Grey Event → RDL Event  
    EventType → RDLEvent { timestamp, payload }
    
    // Spatial Region → Toroidal Space Cell
    SpatialBounds → GridCoord { x, y, z }
    
    // Deterministic Ordering → Canonical Event Queue
    EventOrder → PriorityQueue { timestamp, dst, src, payload }
};
```

#### Validation Integration
- **Compile-Time**: Static O(1) analysis ensures runtime safety
- **Runtime**: Betti kernel validates spatial constraints
- **Equivalence**: Grey program semantics = Betti execution trace

### 7.2 WebAssembly Backend

#### Memory Budget Constraints
```wasm
;; WASM memory allocation with budget enforcement
(module
    (memory (export "memory") 1)  ;; 64KB pages, budget-controlled
    (func (export "allocate_region") (param $size i32) (result i32)
        ;; Check against global memory budget
        ;; Reject if exceeds toroidal space limits
    )
)
```

#### WASM Constraints
- **Memory Limits**: Enforce 32³ spatial constraints in linear memory
- **Deterministic Execution**: WASM instruction ordering preserves event semantics
- **Time Modeling**: Logical timestamps mapped to WASM execution steps

### 7.3 C++ Backend

#### Memory Management Integration
```cpp
// C++ code generation with manual memory control
class GreyProcess {
private:
    std::array<ProcessState, TOROIDAL_CELLS> space;
    
public:
    // Compile-time spatial bounds enforcement
    void execute_event(const RDLEvent& event) {
        // O(1) memory usage guaranteed by design
        auto& cell = space[event.dst_cell];
        cell.process(event.payload);
    }
};
```

#### C++ Specific Features
- **RAII Integration**: Automatic resource cleanup within spatial bounds
- **Template Optimization**: Compile-time spatial constraints
- **Concurrency**: Thread-safe event injection patterns

### 7.4 LLVM IR Backend

#### Optimization Pipeline
```rust
pub struct LLVMBackend {
    // Leverage LLVM optimization passes
    // Maintain O(1) guarantees through IR annotations
}

impl LLVMBackend {
    pub fn generate_llvm_ir(&self, gir: &GreyIR) -> LLVMModule {
        // 1. Map GIR to LLVM IR with spatial annotations
        // 2. Apply LLVM optimization passes
        // 3. Verify O(1) constraints in optimized IR
        // 4. Generate target-specific code
    }
}
```

#### LLVM Integration
- **Cross-Platform**: Single IR for multiple target architectures
- **Optimization**: Aggressive optimization while preserving semantics
- **Verification**: Formal verification of O(1) properties

### 7.5 Backend Comparison

| Backend | Strengths | Constraints | Use Cases |
|---------|-----------|-------------|-----------|
| Betti-RDL | Native O(1) guarantees | C++ runtime required | High-performance compute |
| WASM | Web deployment | Memory budget limits | Browser applications |
| C++ | Full system access | Manual memory management | Systems programming |
| LLVM IR | Cross-platform | Requires LLVM toolchain | Native compilation |

---

## 8. Testing & CI Strategy

### 8.1 Testing Pyramid

#### Unit Tests
```rust
//grey_compiler/crates/grey_validator/tests/loop_bounds.rs
#[test]
fn test_recursive_factorial_o1_validation() {
    let source = r#"
        process factorial(n: u64) -> u64 {
            if n <= 1 { 1 }
            else { n * factorial(n - 1) }
        }
    "#;
    
    let result = validate_program(source);
    assert!(result.is_ok()); // Should pass with toroidal transformation
}

#[test] 
fn test_unbounded_loop_rejection() {
    let source = r#"
        process infinite_loop() {
            while true {
                // This should fail validation
                process_event();
            }
        }
    "#;
    
    let result = validate_program(source);
    assert!(matches!(result, Err(ValidationError::UnboundedLoop)));
}
```

#### Integration Tests
```rust
//grey_compiler/tests/backend_equivalence.rs
#[test]
fn test_betti_wasm_equivalence() {
    let source = build_test_program();
    
    let betti_result = compile_to_betti(&source);
    let wasm_result = compile_to_wasm(&source);
    
    assert_eq!(betti_result.events(), wasm_result.events());
    assert_eq!(betti_result.memory_usage(), wasm_result.memory_usage());
}
```

### 8.2 Golden File Testing

#### Expected Outputs
```
tests/golden/
├── basic_programs/
│   ├── factorial.grey → factorial_betti.cpp
│   ├── factorial.grey → factorial_wasm.wat
│   ├── factorial.grey → factorial.ll
│   └── factorial.grey → factorial.cpp
├── optimization_tests/
│   ├── dead_code_elimination/
│   ├── constant_folding/
│   └── spatial_optimization/
└── validation_tests/
    ├── valid_memory_budgets/
    ├── invalid_unbounded_loops/
    └── resource_leaks/
```

### 8.3 Integration with Existing Demos

#### Regression Testing
```bash
# Compare Grey compiler output to hand-written C++ demos
./greyc tests/demos/self_healing_city.grey --backend betti
./tests/verify_equivalence.sh generated_output.cpp demos/BettiRDLDemo.cpp
```

#### Performance Benchmarks
```rust
//grey_compiler/benches/compiler_performance.rs
fn benchmark_compilation(bench: &mut Bencher) {
    let source = read_large_test_program();
    
    bench.iter(|| {
        let result = compile_program(&source);
        assert!(result.is_ok());
    });
}
```

### 8.4 CI Pipeline

```yaml
# .github/workflows/grey_compiler.yml
name: Grey Compiler CI

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        backend: [betti, wasm, cpp, llvm]
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup Rust
      uses: actions-rs/toolchain@v1
      with:
        toolchain: stable
    
    - name: Run Unit Tests
      run: cargo test --lib
    
    - name: Run Integration Tests  
      run: cargo test --test integration
    
    - name: Golden File Tests
      run: cargo test --test golden_files
      
    - name: Backend Validation
      run: ./tests/validate_backends.sh
      
    - name: Performance Regression
      run: cargo bench

  fuzz:
    # Continuous fuzzing for parser and lexer
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Fuzz Parser
      run: cargo fuzz run parser
    
  miri:
    # Memory safety verification
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: MIRI Testing
      run: cargo +nightly miri test
```

---

## 9. Developer Tooling Integration

### 9.1 Language Server Protocol (LSP)

#### Server Implementation
```rust
//grey_lsp_server/src/server.rs
pub struct GreyLanguageServer {
    compilation_db: CompilationDatabase,
    validator: O1Validator,
    formatter: GreyFormatter,
}

impl LanguageServer for GreyLanguageServer {
    fn completion(&self, params: CompletionParams) -> Result<CompletionList> {
        // Context-aware completions based on AST
        // Include type hints and spatial constraint info
    }
    
    fn diagnostic(&self, params: DocumentDiagnosticParams) -> Result<DocumentDiagnosticReport> {
        // Real-time O(1) validation feedback
        let diagnostics = self.validator.validate_file(¶ms.text_document.uri)?;
        Ok(diagnostic_report(diagnostics))
    }
    
    fn code_action(&self, params: CodeActionParams) -> Result<Option<CodeAction>> {
        // Suggest toroidal transformations for unbounded loops
        // Offer memory budget optimizations
    }
}
```

#### LSP Features
- **Auto-completion**: Type-aware suggestions with spatial constraints
- **Real-time Diagnostics**: O(1) validation feedback
- **Code Actions**: Automated fixes for common validation errors
- **Hover Info**: Memory usage and spatial bounds information

### 9.2 Code Formatter

#### Formatting Rules
```rust
//grey_formatter/src/rules.rs
pub struct GreyFormatter {
    // Preserves semantic meaning while formatting
    // Maintains validation annotations
}

impl GreyFormatter {
    pub fn format_source(&self, source: &str) -> Result<String> {
        let ast = self.parse(source)?;
        let formatted = self.apply_formatting_rules(ast)?;
        self.restore_validation_annotations(formatted)
    }
}
```

### 9.3 Interactive REPL

#### REPL Features
```rust
//grey_repl/src/repl.rs
pub struct GreyREPL {
    compiler: GreyCompiler,
    validator: O1Validator,
}

impl GreyREPL {
    pub fn evaluate(&mut self, input: &str) -> REPLResult {
        // 1. Parse and validate input
        let ast = self.compiler.parse(input)?;
        let validation = self.validator.validate(&ast)?;
        
        // 2. Execute with immediate feedback
        match self.compile_and_run(&ast) {
            Ok(result) => REPLResult::Success(result),
            Err(e) => REPLResult::Error(e.with_context(&validation)),
        }
    }
}
```

#### REPL Commands
- `:type <expr>` - Show expression type
- `:memory` - Display memory usage analysis  
- `:validate` - Run O(1) validation
- `:optimize` - Show optimization suggestions
- `:backend <target>` - Switch compilation target

### 9.4 Standalone Type Checker

#### CLI Interface
```bash
# Check O(1) constraints without full compilation
greyc check --validate-memory-bounds program.grey

# Output validation report
Program: program.grey
✓ Loop bounds: All loops have finite bounds
✓ Memory budget: 2.1KB / 32KB (6.6%)
✓ Spatial constraints: Valid toroidal mapping
✓ Event determinism: Preserved

Warnings:
- Consider optimizing nested loops (lines 15-23)
```

---

## 10. Operational Concerns

### 10.1 Incremental Compilation

#### Change Detection
```rust
pub struct IncrementalCompiler {
    dependency_graph: DependencyGraph,
    cache: CompilationCache,
}

impl IncrementalCompiler {
    pub fn compile_changes(&mut self, changes: FileChanges) -> IncrementalResult {
        // 1. Determine affected modules
        let affected = self.dependency_graph.affected_by(&changes);
        
        // 2. Re-compile minimal set
        for module in affected {
            self.recompile_module(module)?;
        }
        
        // 3. Update caches
        self.cache.update(&affected);
        
        Ok(IncrementalResult::success())
    }
}
```

#### Build Graph Optimization
- **Parallel Compilation**: Independent modules compile concurrently
- **Cache Warming**: Pre-compiled dependencies
- **Incremental Linking**: Only changed components re-linked

### 10.2 Caching Strategy

#### Multi-Level Caching
```rust
pub struct CompilationCache {
    // L1: Parse trees (memory cache)
    parse_cache: Arc<Mutex<LruCache<FileId, ParseTree>>>,
    
    // L2: Type-checked ASTs (disk cache) 
    typecheck_cache: Arc<DiskCache<FileId, TypedAST>>,
    
    // L3: Optimized IR (persistent cache)
    ir_cache: Arc<PersistentCache<FileId, OptimizedIR>>,
}

impl CompilationCache {
    pub fn get_cached_result(&self, file_id: FileId) -> Option<CompilationStage> {
        // Try L1, L2, L3 caches
        // Validate cache freshness
        // Return best available result
    }
}
```

### 10.3 Package Distribution

#### Rust Crate Publishing
```toml
# crates/grey_compiler/Cargo.toml
[package]
name = "grey-compiler"
version = "0.1.0"
edition = "2021"

[dependencies]
grey-lexer = { path = "../grey_lexer" }
grey-parser = { path = "../grey_parser" }
grey-validator = { path = "../grey_validator" }
# ... other dependencies
```

#### Binary Distribution
```bash
# Install CLI tools
cargo install greyc-cli
cargo install grey-lsp-server  
cargo install grey-tools

# Package for distribution
./scripts/package_release.sh v0.1.0
# Creates: greyc-v0.1.0-x86_64-unknown-linux-gnu.tar.gz
```

### 10.4 CI Integration

#### Developer Workflow Integration
```yaml
# .pre-commit-config.yaml
repos:
- repo: local
  hooks:
  - id: grey-check
    name: Grey O(1) Validation
    entry: greyc check
    language: system
    files: \.grey$
    
  - id: grey-format
    name: Grey Formatter  
    entry: greyc format --check
    language: system
    files: \.grey$
```

#### IDE Plugin Integration
- **VSCode Extension**: Language support + debugging
- **IntelliJ Plugin**: Full-featured IDE integration
- **Vim/Emacs**: LSP client configuration

### 10.5 Performance Monitoring

#### Compilation Metrics
```rust
pub struct CompilationMetrics {
    pub parse_time: Duration,
    pub typecheck_time: Duration, 
    pub validation_time: Duration,
    pub optimization_time: Duration,
    pub codegen_time: Duration,
    pub memory_usage: MemoryStats,
}

impl CompilationMetrics {
    pub fn report_performance(&self) {
        if self.parse_time > TIMEOUT_PARSE {
            warn!("Slow parsing detected: {:?}", self.parse_time);
        }
        if self.validation_time > TIMEOUT_VALIDATION {
            warn!("Validation taking too long: {:?}", self.validation_time);
        }
    }
}
```

#### Resource Usage Tracking
- **Memory**: Track compilation memory usage
- **CPU**: Profile compilation bottlenecks  
- **Disk**: Cache performance metrics
- **Network**: Dependency download monitoring

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-4)
- [ ] Set up Rust workspace structure
- [ ] Implement lexer and basic parser
- [ ] Create symbol table and AST structures
- [ ] Build minimal test infrastructure

### Phase 2: Type System (Weeks 5-8)
- [ ] Implement core type checker
- [ ] Add constraint resolution
- [ ] Build O(1) validator framework
- [ ] Create validation test suite

### Phase 3: Backend Infrastructure (Weeks 9-12)
- [ ] Design Canonical IR format
- [ ] Implement Betti-RDL backend
- [ ] Add WASM backend
- [ ] Create optimization passes

### Phase 4: Tooling (Weeks 13-16)
- [ ] Build LSP server
- [ ] Implement code formatter
- [ ] Create REPL interface
- [ ] Add CLI tools

### Phase 5: Integration & Optimization (Weeks 17-20)
- [ ] Complete C++ and LLVM backends
- [ ] Performance optimization
- [ ] Full test coverage
- [ ] Documentation and examples

---

## Conclusion

This architecture blueprint provides a comprehensive foundation for implementing the Grey universal compiler while maintaining the core O(1) guarantees and deterministic execution properties. The multi-backend approach enables broad deployment scenarios while the integrated validation system ensures safety and correctness at compile time.

The modular design supports incremental development and testing, while the developer tooling integration provides a modern programming experience. The focus on semantic preservation across backends ensures that Grey programs maintain their fundamental properties regardless of the target platform.

**Key Success Metrics:**
- ✅ All backends preserve O(1) spatial complexity
- ✅ Deterministic execution maintained across targets  
- ✅ Compile-time validation catches all memory violations
- ✅ Developer tooling provides real-time feedback
- ✅ Test coverage ensures backward compatibility

This blueprint serves as the implementation guide for Phase 2 development, with each component designed to integrate seamlessly into the broader Betti-RDL ecosystem.