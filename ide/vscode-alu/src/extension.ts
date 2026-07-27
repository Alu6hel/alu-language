import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions, ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    const serverExecutable = 'alu'; // Assumes alu.exe is in PATH
    const serverOptions: ServerOptions = {
        run: { command: serverExecutable, args: ['lsp'] },
        debug: { command: serverExecutable, args: ['lsp'] }
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

    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
