# Betti-RDL Rust

Space-Time Native Computation Runtime for Rust

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
betti-rdl = "1.0"
```

## Quick Start

```rust
use betti_rdl::Kernel;

fn main() {
    let mut kernel = Kernel::new();

    // Spawn processes
    for i in 0..10 {
        kernel.spawn_process(i, 0, 0);
    }

    // Inject event
    kernel.inject_event(0, 0, 0, 1);

    // Run
    kernel.run(100);

    println!("Processed: {} events", kernel.events_processed());
    // Memory: O(1)
}
```

## Features

- **Zero-cost abstractions**: Thin wrapper over C++ kernel
- **Thread-safe**: `Send + Sync` implementation
- **Type-safe**: Rust's type system prevents misuse
- **No runtime overhead**: Direct FFI calls

## API Documentation

Run `cargo doc --open` to view full API documentation.

## License

MIT
