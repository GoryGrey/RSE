# Betti-RDL Node.js

Space-Time Native Computation Runtime for Node.js

## Installation

```bash
npm install betti-rdl
```

## Quick Start

```javascript
const { Kernel } = require('betti-rdl');

const kernel = new Kernel();

// Spawn processes
for (let i = 0; i < 10; i++) {
    kernel.spawnProcess(i, 0, 0);
}

// Inject events
kernel.injectEvent(0, 0, 0, 1);

// Run
kernel.run(100);

console.log(`Processed: ${kernel.getEventsProcessed()} events`);
// Memory: O(1)
```

## TypeScript Support

Full TypeScript definitions included:

```typescript
import { Kernel } from 'betti-rdl';

const kernel = new Kernel();
kernel.spawnProcess(0, 0, 0);
```

## API

See [index.d.ts](index.d.ts) for full API documentation.

## License

MIT
