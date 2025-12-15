"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.deploy = deploy;
const chalk_1 = __importDefault(require("chalk"));
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
const http_1 = __importDefault(require("http"));
async function deploy() {
    console.log(chalk_1.default.blue(`\n🚀 Deploying Cell to local Lattice...`));
    // 1. Read cog.json
    const configPath = path_1.default.join(process.cwd(), 'cog.json');
    if (!fs_1.default.existsSync(configPath)) {
        console.log(chalk_1.default.red(`Error: cog.json not found. Are you in a COG project?`));
        return;
    }
    const config = JSON.parse(fs_1.default.readFileSync(configPath, 'utf8'));
    // 2. Read Source
    const srcPath = path_1.default.join(process.cwd(), config.entry);
    if (!fs_1.default.existsSync(srcPath)) {
        console.log(chalk_1.default.red(`Error: Entry file ${config.entry} not found.`));
        return;
    }
    const code = fs_1.default.readFileSync(srcPath, 'utf8');
    // 3. Post to Daemon
    const payload = JSON.stringify({
        name: config.name,
        code: code
    });
    const req = http_1.default.request({
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
                console.log(chalk_1.default.green(`\n✅ Deployment Complete!`));
                console.log(chalk_1.default.gray(data));
            }
            else {
                console.log(chalk_1.default.red(`\n❌ Deployment Failed: ${res.statusCode}`));
            }
        });
    });
    req.on('error', (e) => {
        console.log(chalk_1.default.red(`\n❌ Could not connect to Lattice Daemon.`));
        console.log(chalk_1.default.gray(`Make sure you ran 'cog spin' in another terminal!`));
    });
    req.write(payload);
    req.end();
}
