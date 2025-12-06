import { SymbolicAgent } from './types';

export class ToroidalSpace {
    width: number;
    height: number;
    depth: number;
    grid: Map<string, SymbolicAgent>; // Sparse storage key "x,y,z"

    constructor(width = 16, height = 16, depth = 16) {
        this.width = width;
        this.height = height;
        this.depth = depth;
        this.grid = new Map();
    }

    // Wrap coordinate to [0, dim)
    wrap(v: number, max: number): number {
        return ((v % max) + max) % max;
    }

    // Get 3D coordinate key
    key(x: number, y: number, z: number): string {
        return `${this.wrap(x, this.width)},${this.wrap(y, this.height)},${this.wrap(z, this.depth)}`;
    }

    addAgent(agent: SymbolicAgent): void {
        const k = this.key(agent.x, agent.y, agent.z);
        this.grid.set(k, agent);
    }

    getAgentAt(x: number, y: number, z: number): SymbolicAgent | undefined {
        return this.grid.get(this.key(x, y, z));
    }

    moveAgent(agent: SymbolicAgent, dx: number, dy: number, dz: number): void {
        const oldKey = this.key(agent.x, agent.y, agent.z);
        this.grid.delete(oldKey);

        agent.x = this.wrap(agent.x + dx, this.width);
        agent.y = this.wrap(agent.y + dy, this.height);
        agent.z = this.wrap(agent.z + dz, this.depth);

        const newKey = this.key(agent.x, agent.y, agent.z);
        this.grid.set(newKey, agent); // Note: Collisions currently overwrite (simplified physics)
    }

    // Von Neumann Neighborhood (6 neighbors in 3D)
    getNeighbors(x: number, y: number, z: number): (SymbolicAgent | undefined)[] {
        const neighbors = [];
        const dirs = [
            [1, 0, 0], [-1, 0, 0],
            [0, 1, 0], [0, -1, 0],
            [0, 0, 1], [0, 0, -1]
        ];

        for (const [dx, dy, dz] of dirs) {
            neighbors.push(this.getAgentAt(x + dx, y + dy, z + dz));
        }
        return neighbors;
    }
}
