#!/usr/bin/env node
import { Command } from 'commander';
import chalk from 'chalk';
import { spin } from './commands/spin';
import { deploy } from './commands/deploy';
import { init } from './commands/init';

const program = new Command();

program
    .name('cog')
    .description('COG: The Personal World Engine\nTurn your machine into a Betti-RDL Hyper-Cluster.')
    .version('0.1.0');

program
    .command('init')
    .description('Initialize a new COG project')
    .argument('<name>', 'Name of the world to create')
    .action((name) => {
        init(name);
    });

program
    .command('spin')
    .description('Spin up the local Lattice Daemon')
    .option('-p, --port <number>', 'Port to run on', '3000')
    .action((options) => {
        spin(options);
    });

program
    .command('deploy')
    .description('Deploy current directory to the running Lattice')
    .action(() => {
        deploy();
    });

program.parse();
