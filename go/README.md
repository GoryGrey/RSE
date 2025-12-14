# Betti-RDL Go

Space-Time Native Computation Runtime for Go

## Installation

```bash
go get github.com/betti-labs/betti-rdl
```

## Quick Start

```go
package main

import (
    "fmt"
    "github.com/betti-labs/betti-rdl"
)

func main() {
    kernel := bettirdl.NewKernel()
    defer kernel.Close()

    // Spawn processes
    for i := 0; i < 10; i++ {
        kernel.SpawnProcess(i, 0, 0)
    }

    // Inject event
    kernel.InjectEvent(0, 0, 0, 1)

    // Run
    kernel.Run(100)

    fmt.Printf("Processed: %d events\n", kernel.EventsProcessed())
    // Memory: O(1)
}
```

## Features

- **Idiomatic Go API**: Follows Go conventions
- **Resource management**: Automatic cleanup with `defer`
- **CGo bindings**: Direct C++ integration
- **Thread-safe**: Safe for concurrent use

## Documentation

See [GoDoc](https://pkg.go.dev/github.com/betti-labs/betti-rdl) for full API documentation.

## License

MIT
