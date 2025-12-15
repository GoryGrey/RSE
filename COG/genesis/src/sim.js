
// Project Genesis: 1 Million Agent Simulation
// Running on Betti-RDL Kernel (O(1))

// State Format: [x, y, z, energy, r, g, b]
const WORLD_SIZE = 100;

function random(min, max) {
    return Math.random() * (max - min) + min;
}

export function onEvent(event) {
    // 1. If INIT, spawn the swarm
    if (event.type === 'INIT') {
        console.log("Genesis: Big Bang initiated.");
        for (let i = 0; i < 1000; i++) { // Start with seed population
            emit('SPAWN', {
                x: random(-WORLD_SIZE, WORLD_SIZE),
                y: random(-WORLD_SIZE, WORLD_SIZE),
                z: random(-WORLD_SIZE, WORLD_SIZE),
                energy: 100,
                color: { r: 0, g: 1, b: 0 }
            });
        }
        return;
    }

    // 2. If TICK, update agent
    if (event.type === 'TICK') {
        const agent = event.payload;

        // Simple Brain: Move towards Origin ( Gravity )
        agent.x -= agent.x * 0.01;
        agent.y -= agent.y * 0.01;
        agent.z -= agent.z * 0.01;

        // Burn Energy
        agent.energy -= 0.1;

        // Die if energy < 0
        if (agent.energy <= 0) return; // Cell death (O(1) reclamation)

        // Reproduce if energy > 120
        if (agent.energy > 120) {
            agent.energy -= 60;
            emit('SPAWN', { ...agent, x: agent.x + 1 }); // Mitosis
        }

        // Emit new state
        emit('UPDATE', agent);
    }
}
