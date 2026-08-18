import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';
import * as path from 'path';
import * as cp from 'child_process';

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

    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('alu-debug', new AluConfigurationProvider()));

    client.start();
}

class AluConfigurationProvider implements vscode.DebugConfigurationProvider {
    async resolveDebugConfiguration(folder: vscode.WorkspaceFolder | undefined, config: vscode.DebugConfiguration, token?: vscode.CancellationToken): Promise<vscode.DebugConfiguration | undefined> {
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
                    const lldbConfig: vscode.DebugConfiguration = {
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

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
