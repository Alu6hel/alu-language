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
const aluDebug_1 = require("./aluDebug");
const node_1 = require("vscode-languageclient/node");
let client;
function activate(context) {
    const serverOptions = {
        run: { command: "C:\\\\Users\\\\Alu\\\\source\\\\repos\\\\Alu6hel\\\\alu-language\\\\cpp_frontend\\\\alu-lsp.exe" },
        debug: { command: "C:\\\\Users\\\\Alu\\\\source\\\\repos\\\\Alu6hel\\\\alu-language\\\\cpp_frontend\\\\alu-lsp.exe" }
    };
    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'alu' }]
    };
    client = new node_1.LanguageClient('aluLanguageServer', 'ALU Language Server', serverOptions, clientOptions);
    // Register a configuration provider for 'alu-debug' debug type
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('alu-debug', new AluConfigurationProvider()));
    // Register a debug adapter descriptor factory that points to our inline AluDebugSession
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('alu-debug', new InlineDebugAdapterFactory()));
    client.start();
}
class AluConfigurationProvider {
    resolveDebugConfiguration(folder, config, token) {
        // if launch.json is missing or empty
        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor && editor.document.languageId === 'alu') {
                config.type = 'alu-debug';
                config.name = 'Launch';
                config.request = 'launch';
                config.program = '${file}';
                config.stopOnEntry = true;
            }
        }
        if (!config.program) {
            return vscode.window.showInformationMessage("Cannot find a program to debug").then(_ => {
                return undefined;
            });
        }
        return config;
    }
}
class InlineDebugAdapterFactory {
    createDebugAdapterDescriptor(_session) {
        return new vscode.DebugAdapterInlineImplementation(new aluDebug_1.AluDebugSession());
    }
}
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
//# sourceMappingURL=extension.js.map