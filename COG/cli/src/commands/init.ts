import chalk from 'chalk';
import fs from 'fs';
import path from 'path';

export function init(name: string) {
    console.log(chalk.blue(`Initializing new world: ${name}...`));

    const projectPath = path.join(process.cwd(), name);
    if (fs.existsSync(projectPath)) {
        console.log(chalk.red(`Error: Directory ${name} already exists.`));
        return;
    }

    fs.mkdirSync(projectPath);

    // Create default cog.json
    const config = {
        name: name,
        version: "1.0.0",
        entry: "src/main.js"
    };
    fs.writeFileSync(path.join(projectPath, 'cog.json'), JSON.stringify(config, null, 2));

    // Create src dir
    fs.mkdirSync(path.join(projectPath, 'src'));
    fs.writeFileSync(path.join(projectPath, 'src', 'main.js'), `
// Welcome to your COG Cell
// This code runs on the Betti-RDL Lattice

export function onEvent(event) {
    console.log("Received event:", event);
    return { type: "emit", target: "neighbor", payload: "Hello World" };
}
`);

    console.log(chalk.green(`\nWorld '${name}' created successfully!`));
    console.log(`\nNext steps:\n  cd ${name}\n  cog spin\n  cog deploy`);
}
