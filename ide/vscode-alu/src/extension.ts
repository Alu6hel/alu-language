import * as vscode from 'vscode';
import * as net from 'net';
import { AluDebugSession } from './aluDebug';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';

import * as path from 'path';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    const serverPath = process.platform === 'win32' 
        ? path.join(__dirname, "../../../cpp_frontend/alu-lsp.exe")
        : path.join(__dirname, "../../../cpp_frontend/alu-lsp");
    
    const serverOptions: ServerOptions = {
        run: { command: serverPath },
        debug: { command: serverPath }
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'alu' }]
    };

    client = new LanguageClient(
        'aluLanguageServer',
        'ALU Language Server',
        serverOptions,
        clientOptions
    );

    // Register a configuration provider for 'alu-debug' debug type
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('alu-debug', new AluConfigurationProvider()));

    // Register a debug adapter descriptor factory that points to our inline AluDebugSession
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('alu-debug', new InlineDebugAdapterFactory()));

    client.start();
}

class AluConfigurationProvider implements vscode.DebugConfigurationProvider {
    resolveDebugConfiguration(folder: vscode.WorkspaceFolder | undefined, config: vscode.DebugConfiguration, token?: vscode.CancellationToken): vscode.ProviderResult<vscode.DebugConfiguration> {
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

class InlineDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {
    createDebugAdapterDescriptor(_session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        return new vscode.DebugAdapterInlineImplementation(new AluDebugSession());
    }
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
