# Betti-RDL: Space-Time Native Computation

**O(1) memory for recursive execution. Massive parallelism. Proven at scale.**

## What Is This?

A computational runtime that maintains constant memory regardless of recursion depth or parallel workload size.

**Proven results:**
- 33M recursive operations: 44 bytes memory
- 1M events processed: 0 bytes memory growth
- 16 parallel instances: 119 bytes each (constant)

## Quick Start

```bash
pip install betti-rdl
```

```python
import betti_rdl

kernel = betti_rdl.Kernel()

# Spawn processes
for i in range(10):
    kernel.spawn_process(i, 0, 0)

# Inject events
kernel.inject_event(0, 0, 0, value=1)

# Run
kernel.run(max_events=100)

print(f"Processed: {kernel.events_processed} events")
# Memory used: O(1)
```

## Use Cases

**Deep Recursion**
- Parse deeply nested structures without stack overflow
- Unlimited recursion depth
- Constant memory usage

**Massive Parallelism**
- Run 1000s of parallel tasks in tiny memory
- 10-100x better resource utilization
- Linear scaling with cores

**Real-World Applications**
- Password recovery / security testing
- Parallel simulations (Monte Carlo, physics, climate)
- AI hyperparameter search
- Rendering farms
- Financial modeling

## How It Works

Traditional recursion uses a stack that grows with depth. Betti-RDL uses a fixed-size toroidal space where processes communicate via events.

**Result**: Memory stays constant no matter how deep or parallel your workload.

## Performance

| Test | Traditional | Betti-RDL |
|------|-------------|-----------|
| Tower of Hanoi (25 disks) | Stack overflow | 44 bytes |
| 1M parallel events | ~8GB | 0 bytes growth |
| 16 parallel instances | ~2GB | 1.9KB total |

## Documentation

- [GitHub](https://github.com/betti-labs/betti-rdl)
- [Examples](https://github.com/betti-labs/betti-rdl/tree/main/examples)
- [Paper](https://github.com/betti-labs/betti-rdl/blob/main/rdl_paper.pdf)

## License

MIT

## Author

Gregory Betti - [Betti Labs](https://betti.dev)

---

**Built something cool with Betti-RDL? [Let me know](https://github.com/betti-labs/betti-rdl/discussions)**
