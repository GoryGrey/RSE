"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.init = init;
const chalk_1 = __importDefault(require("chalk"));
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
function init(name) {
    console.log(chalk_1.default.blue(`Initializing new world: ${name}...`));
    const projectPath = path_1.default.join(process.cwd(), name);
    if (fs_1.default.existsSync(projectPath)) {
        console.log(chalk_1.default.red(`Error: Directory ${name} already exists.`));
        return;
    }
    fs_1.default.mkdirSync(projectPath);
    // Create default cog.json
    const config = {
        name: name,
        version: "1.0.0",
        entry: "src/main.js"
    };
    fs_1.default.writeFileSync(path_1.default.join(projectPath, 'cog.json'), JSON.stringify(config, null, 2));
    // Create src dir
    fs_1.default.mkdirSync(path_1.default.join(projectPath, 'src'));
    fs_1.default.writeFileSync(path_1.default.join(projectPath, 'src', 'main.js'), `
// Welcome to your COG Cell
// This code runs on the Betti-RDL Lattice

export function onEvent(event) {
    console.log("Received event:", event);
    return { type: "emit", target: "neighbor", payload: "Hello World" };
}
`);
    console.log(chalk_1.default.green(`\nWorld '${name}' created successfully!`));
    console.log(`\nNext steps:\n  cd ${name}\n  cog spin\n  cog deploy`);
}
