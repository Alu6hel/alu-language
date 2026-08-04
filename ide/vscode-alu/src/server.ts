import {
    createConnection,
    TextDocuments,
    Diagnostic,
    DiagnosticSeverity,
    ProposedFeatures,
    InitializeParams,
    TextDocumentSyncKind
} from 'vscode-languageserver/node';

import {
    TextDocument
} from 'vscode-languageserver-textdocument';

const connection = createConnection(ProposedFeatures.all);
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

connection.onInitialize((params: InitializeParams) => {
    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
        }
    };
});

documents.onDidChangeContent(change => {
    validateTextDocument(change.document);
});

async function validateTextDocument(textDocument: TextDocument): Promise<void> {
    const text = textDocument.getText();
    const diagnostics: Diagnostic[] = [];

    // Simple heuristic LSP for unhandled 'try' without 'catch'
    const tryRegex = /\btry\b\s*{[\s\S]*?}/g;
    const catchRegex = /\bcatch\b\s*\(/;

    let match;
    while ((match = tryRegex.exec(text)) !== null) {
        const tryBlock = match[0];
        const endOfTry = match.index + tryBlock.length;
        const followingText = text.substring(endOfTry, endOfTry + 20); // Look ahead for catch
        
        if (!catchRegex.test(followingText)) {
            const diagnostic: Diagnostic = {
                severity: DiagnosticSeverity.Error,
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
