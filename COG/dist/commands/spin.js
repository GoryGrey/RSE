"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.spin = spin;
const chalk_1 = __importDefault(require("chalk"));
const fastify_1 = __importDefault(require("fastify"));
// This is the "Daemon" that wraps the C++ Metal Kernel
// In phase 1, it runs inside the CLI process for simplicity.
// Later, 'cog spin' will spawn this in the background.
async function spin(options) {
    const port = parseInt(options.port);
    console.log(chalk_1.default.cyan(`\n🌀 Spining up Betti-RDL Lattice on port ${port}...`));
    const server = (0, fastify_1.default)();
    // Mock Kernel State
    const activeCells = new Map();
    server.get('/', async () => {
        return { status: 'running', cells: activeCells.size, mode: 'O(1)' };
    });
    server.post('/deploy', async (request, reply) => {
        const payload = request.body;
        console.log(chalk_1.default.yellow(`\n[KERNEL] Receiving Deployment: ${payload.name}`));
        console.log(chalk_1.default.gray(`[KERNEL] Injecting into Lattice...`));
        // Simulating O(1) Loading
        activeCells.set(payload.name, { status: 'active', code: payload.code.length + ' bytes' });
        console.log(chalk_1.default.green(`[KERNEL] Deployed successfully to Cell ID: ${Math.floor(Math.random() * 10000)}`));
        return { success: true, cell_id: '0x123' };
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
