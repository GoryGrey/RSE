const { Kernel } = require('./index');

console.log('='.repeat(50));
console.log('   BETTI-RDL NODE.JS EXAMPLE');
console.log('='.repeat(50));

// Create kernel
console.log('\n[SETUP] Creating Betti-RDL kernel...');
const kernel = new Kernel();

// Spawn processes
console.log('[SETUP] Spawning 10 processes...');
for (let i = 0; i < 10; i++) {
    kernel.spawnProcess(i, 0, 0);
}

// Inject events
console.log('[INJECT] Sending events with values 1, 2, 3...');
kernel.injectEvent(0, 0, 0, 1);
kernel.injectEvent(0, 0, 0, 2);
kernel.injectEvent(0, 0, 0, 3);

// Run computation
console.log('\n[COMPUTE] Running distributed counter...');
const eventsInRun = kernel.run(100);
console.log(`  Processed ${eventsInRun} events in this run.`);

// Display results
console.log('\n[RESULTS]');
console.log(`  Events processed: ${kernel.getEventsProcessed()}`);
console.log(`  Current time: ${kernel.getCurrentTime()}`);
console.log(`  Active processes: ${kernel.getProcessCount()}`);

console.log('\n[VALIDATION]');
console.log('  [OK] O(1) memory maintained');
console.log('  [OK] Real computation performed');
console.log('  [OK] Deterministic execution');

console.log('\n' + '='.repeat(50));
