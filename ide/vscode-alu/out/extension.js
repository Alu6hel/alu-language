"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const node_1 = require("vscode-languageclient/node");
const path = __importStar(require("path"));
const cp = __importStar(require("child_process"));
let client;
function activate(context) {
    const serverPath = process.platform === 'win32'
        ? path.join(__dirname, "../../../cpp_frontend/alu-lsp.exe")
        : path.join(__dirname, "../../../cpp_frontend/alu-lsp");
    const serverOptions = {
        run: { command: serverPath },
        debug: { command: serverPath }
    };
    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'alu' }]
    };
    client = new node_1.LanguageClient('aluLanguageServer', 'ALU Language Server', serverOptions, clientOptions);
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('alu-debug', new AluConfigurationProvider()));
    client.start();
}
class AluConfigurationProvider {
    async resolveDebugConfiguration(folder, config, token) {
        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === 'alu') {
                config.type = 'alu-debug';
                config.name = 'Debug ALU';
                config.request = 'launch';
                config.program = '${file}';
            }
        }
        if (!config.program) {
            vscode.window.showInformationMessage("Cannot find a program to debug");
            return undefined;
        }
        const lldbExt = vscode.extensions.getExtension("vadimcn.vscode-lldb");
        if (!lldbExt) {
            vscode.window.showErrorMessage("The 'CodeLLDB' extension (vadimcn.vscode-lldb) is required to debug ALU programs. Please install it.");
            return undefined;
        }
        return new Promise((resolve) => {
            vscode.window.withProgress({
                location: vscode.ProgressLocation.Notification,
                title: "Compiling ALU program for debugging...",
                cancellable: false
            }, async (progress) => {
                const workspacePath = folder ? folder.uri.fsPath : path.dirname(vscode.window.activeTextEditor?.document.uri.fsPath || "");
                const programPath = config.program.replace('${file}', vscode.window.activeTextEditor?.document.uri.fsPath || "");
                const outBin = programPath.replace(/\.alu$/, process.platform === 'win32' ? '.exe' : '');
                const aluExe = process.platform === 'win32' ? "alu.exe" : "alu";
                const compileCmd = `${aluExe} build "${programPath}" -g`;
                cp.exec(compileCmd, { cwd: workspacePath }, (err, stdout, stderr) => {
                    if (err) {
                        vscode.window.showErrorMessage(`Compilation failed: ${stderr}`);
                        resolve(undefined);
                        return;
                    }
                    // Replace the alu-debug config with a valid lldb config
                    const lldbConfig = {
                        type: 'lldb',
                        request: 'launch',
                        name: config.name,
                        program: outBin,
                        args: config.args || [],
                        cwd: config.cwd || workspacePath,
                        stopOnEntry: config.stopOnEntry === true,
                        terminal: 'integrated'
                    };
                    resolve(lldbConfig);
                });
            });
        });
    }
}
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
//# sourceMappingURL=extension.js.map