#!/usr/bin/env node
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const commander_1 = require("commander");
const spin_1 = require("./commands/spin");
const deploy_1 = require("./commands/deploy");
const init_1 = require("./commands/init");
const program = new commander_1.Command();
program
    .name('cog')
    .description('COG: The Personal World Engine\nTurn your machine into a Betti-RDL Hyper-Cluster.')
    .version('0.1.0');
program
    .command('init')
    .description('Initialize a new COG project')
    .argument('<name>', 'Name of the world to create')
    .action((name) => {
    (0, init_1.init)(name);
});
program
    .command('spin')
    .description('Spin up the local Lattice Daemon')
    .option('-p, --port <number>', 'Port to run on', '3000')
    .action((options) => {
    (0, spin_1.spin)(options);
});
program
    .command('deploy')
    .description('Deploy current directory to the running Lattice')
    .action(() => {
    (0, deploy_1.deploy)();
});
program.parse();
