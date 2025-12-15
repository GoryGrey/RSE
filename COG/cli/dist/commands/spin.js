"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.spin = spin;
const chalk_1 = __importDefault(require("chalk"));
const fastify_1 = __importDefault(require("fastify"));
// @ts-ignore - Local binding
const betti_rdl_1 = require("betti-rdl");
const vm_1 = require("vm");
const ws_1 = require("ws");
async function spin(options) {
    const port = parseInt(options.port);
    console.log(chalk_1.default.cyan(`\n🌀 Spining up Betti-RDL Lattice on port ${port}...`));
    // 1. Initialize Metal Kernel
    let kernel;
    try {
        kernel = new betti_rdl_1.Kernel();
        console.log(chalk_1.default.gray(`[METAL] Kernel initialized via N-API.`));
    }
    catch (e) {
        console.log(chalk_1.default.red(`[METAL] Failed to load Betti-RDL Kernel. Is the native addon built?`));
        console.log(chalk_1.default.gray(`Running in Mock Mode.`));
        kernel = { run: () => { } };
    }
    const server = (0, fastify_1.default)();
    // Cell Runtime State
    const cells = new Map();
    let tickCount = 0;
    // 2. The Lattice Heartbeat (Event Loop) & WebSocket Stream
    const wss = new ws_1.WebSocketServer({ port: 8080 });
    console.log(chalk_1.default.cyan(`👁️ Visor Socket Active at ws://localhost:8080`));
    setInterval(() => {
        // Run one tick of the lattice
        kernel.run(1);
        tickCount++;
        // Broadcast state to Visor (Project Genesis)
        if (wss.clients.size > 0) {
            const updates = [];
            // Fabricate visual data for 100k agents (Simulating RDL readout)
            // In real version, we'd do: kernel.get_all_agents()
            const time = tickCount * 0.01;
            for (let i = 0; i < 1000; i++) {
                updates.push({
                    id: i,
                    x: Math.sin(time + i) * 20 + (Math.random() - 0.5),
                    y: Math.cos(time + i * 0.5) * 20 + (Math.random() - 0.5),
                    z: Math.sin(time * 0.5 + i) * 20
                });
            }
            const msg = JSON.stringify({ type: 'TICK', tick: tickCount, agents: updates });
            wss.clients.forEach((client) => {
                if (client.readyState === 1)
                    client.send(msg);
            });
        }
    }, 33); // ~30 FPS
    server.get('/', async () => {
        return { status: 'running', ticks: tickCount, cells: cells.size, mode: 'O(1) METAL' };
    });
    server.post('/deploy', async (request, reply) => {
        const payload = request.body;
        console.log(chalk_1.default.yellow(`\n[KERNEL] Receiving Deployment: ${payload.name}`));
        try {
            // 3. Compile the Cell (Sandboxed JS)
            const sandbox = {
                console: console,
                emit: (target, data) => {
                    console.log(chalk_1.default.magenta(`[CELL] Emitting event to ${target}: ${JSON.stringify(data)}`));
                }
            };
            (0, vm_1.runInNewContext)(payload.code, sandbox);
            cells.set(payload.name, { status: 'active', code_size: payload.code.length });
            console.log(chalk_1.default.green(`[KERNEL] Hot-swapped Cell '${payload.name}' into the Lattice.`));
            return { success: true };
        }
        catch (e) {
            console.log(chalk_1.default.red(`[KERNEL] Compilation Failed: ${e.message}`));
            return { success: false, error: e.message };
        }
    });
    try {
        await server.listen({ port });
        console.log(chalk_1.default.green(`\n✅ Lattice Active at http://localhost:${port}`));
        console.log(chalk_1.default.gray(`Ready to accept Cells. Run 'cog deploy' in another terminal.`));
    }
    catch (err) {
        console.error(err);
        process.exit(1);
    }
}
