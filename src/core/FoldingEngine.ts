import { ToroidalSpace } from './ToroidalSpace';
import { SymbolicAgent } from './types';

// Logic for the "Black Hole" attraction Force
export class FoldingEngine {
    space: ToroidalSpace;
    active: boolean;
    centerOfGravity: { x: number, y: number, z: number };

    constructor(space: ToroidalSpace) {
        this.space = space;
        this.active = false;
        this.centerOfGravity = { x: 16, y: 16, z: 16 };
    }

    // Ingest text and convert to symbolic agents
    ingestText(text: string) {
        console.log("Ingesting text for folding:", text.length, "chars");
        this.space.grid.clear(); // Reset universe for demo

        // Calculate "Cloud" distribution
        // We want the text to appear as a dispersed cloud that will collapse
        const count = text.length;
        const radius = 12;

        for (let i = 0; i < count; i++) {
            // Spherical distribution
            const phi = Math.acos(-1 + (2 * i) / count);
            const theta = Math.sqrt(count * Math.PI) * phi;

            const x = this.centerOfGravity.x + radius * Math.cos(theta) * Math.sin(phi);
            const y = this.centerOfGravity.y + radius * Math.sin(theta) * Math.sin(phi);
            const z = this.centerOfGravity.z + radius * Math.cos(phi);

            const agent: SymbolicAgent = {
                id: crypto.randomUUID(),
                symbol: text[i],
                x: this.space.wrap(x, this.space.width),
                y: this.space.wrap(y, this.space.height),
                z: this.space.wrap(z, this.space.depth),
                memory: [],
                entropy: 1.0,
                age: 0
            };
            this.space.addAgent(agent);
        }
    }

    // Apply folding physics (Step)
    // Returns true if folding is complete (collapsed)
    step(): boolean {
        if (!this.active) return false;

        let totalDistance = 0;
        const agents = Array.from(this.space.grid.values());

        // Simple gravity: Move towards Center
        for (const agent of agents) {
            const dx = this.centerOfGravity.x - agent.x;
            const dy = this.centerOfGravity.y - agent.y;
            const dz = this.centerOfGravity.z - agent.z;

            // Move 5% closer
            this.space.moveAgent(agent, dx * 0.05, dy * 0.05, dz * 0.05);

            totalDistance += Math.abs(dx) + Math.abs(dy) + Math.abs(dz);
        }

        // Check if fully collapsed (Singularity)
        return totalDistance < (agents.length * 0.5);
    }
}
