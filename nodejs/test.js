const { Kernel } = require('./index');
const assert = require('assert');

console.log('Running Node.js Bindings Tests...');

const kernel = new Kernel();
kernel.spawnProcess(0, 0, 0);

// Test 1: Return value of run()
console.log('Test 1: run() returns event count');
// Inject at x=9 so it doesn't propagate (logic: if next_x < 10 propagate)
// 9 -> 10 (stop)
kernel.injectEvent(9, 0, 0, 1);
kernel.injectEvent(9, 0, 0, 2);
const processed = kernel.run(10);
assert.strictEqual(processed, 2);
// getEventsProcessed returns a number now (impl changed to return number)
assert.strictEqual(Number(kernel.getEventsProcessed()), 2);
console.log('  PASS');

// Test 2: Max events limit
console.log('Test 2: run() respects max_events');
for (let i = 0; i < 10; i++) {
    kernel.injectEvent(9, 0, 0, i);
}
const limited = kernel.run(5);
assert.strictEqual(limited, 5);
assert.strictEqual(Number(kernel.getEventsProcessed()), 2 + 5);

const remaining = kernel.run(10);
assert.strictEqual(remaining, 5);
assert.strictEqual(Number(kernel.getEventsProcessed()), 2 + 10);
console.log('  PASS');

console.log('All tests passed!');
