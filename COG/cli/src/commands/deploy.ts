import chalk from 'chalk';
import fs from 'fs';
import path from 'path';
import http from 'http';

export async function deploy() {
    console.log(chalk.blue(`\n🚀 Deploying Cell to local Lattice...`));

    // 1. Read cog.json
    const configPath = path.join(process.cwd(), 'cog.json');
    if (!fs.existsSync(configPath)) {
        console.log(chalk.red(`Error: cog.json not found. Are you in a COG project?`));
        return;
    }
    const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

    // 2. Read Source
    const srcPath = path.join(process.cwd(), config.entry);
    if (!fs.existsSync(srcPath)) {
        console.log(chalk.red(`Error: Entry file ${config.entry} not found.`));
        return;
    }
    const code = fs.readFileSync(srcPath, 'utf8');

    // 3. Post to Daemon
    const payload = JSON.stringify({
        name: config.name,
        code: code
    });

    const req = http.request({
        hostname: 'localhost',
        port: 3000,
        path: '/deploy',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': payload.length
        }
    }, (res) => {
        let data = '';
        res.on('data', chunk => data += chunk);
        res.on('end', () => {
            if (res.statusCode === 200) {
                console.log(chalk.green(`\n✅ Deployment Complete!`));
                console.log(chalk.gray(data));
            } else {
                console.log(chalk.red(`\n❌ Deployment Failed: ${res.statusCode}`));
            }
        });
    });

    req.on('error', (e) => {
        console.log(chalk.red(`\n❌ Could not connect to Lattice Daemon.`));
        console.log(chalk.gray(`Make sure you ran 'cog spin' in another terminal!`));
    });

    req.write(payload);
    req.end();
}
