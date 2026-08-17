"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const connection = (0, node_1.createConnection)(node_1.ProposedFeatures.all);
const documents = new node_1.TextDocuments(vscode_languageserver_textdocument_1.TextDocument);
connection.onInitialize((params) => {
    return {
        capabilities: {
            textDocumentSync: node_1.TextDocumentSyncKind.Incremental,
        }
    };
});
documents.onDidChangeContent(change => {
    validateTextDocument(change.document);
});
async function validateTextDocument(textDocument) {
    const text = textDocument.getText();
    const diagnostics = [];
    // Simple heuristic LSP for unhandled 'try' without 'catch'
    const tryRegex = /\btry\b\s*{[\s\S]*?}/g;
    const catchRegex = /\bcatch\b\s*\(/;
    let match;
    while ((match = tryRegex.exec(text)) !== null) {
        const tryBlock = match[0];
        const endOfTry = match.index + tryBlock.length;
        const followingText = text.substring(endOfTry, endOfTry + 20); // Look ahead for catch
        if (!catchRegex.test(followingText)) {
            const diagnostic = {
                severity: node_1.DiagnosticSeverity.Error,
                range: {
                    start: textDocument.positionAt(match.index),
                    end: textDocument.positionAt(match.index + 3) // length of 'try'
                },
                message: "Unhandled exception: 'try' block is missing a corresponding 'catch' block.",
                source: 'alu'
            };
            diagnostics.push(diagnostic);
        }
    }
    connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map