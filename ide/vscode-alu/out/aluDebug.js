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
exports.AluDebugSession = void 0;
const debugadapter_1 = require("@vscode/debugadapter");
const fs = __importStar(require("fs"));
const path_1 = require("path");
class AluDebugSession extends debugadapter_1.LoggingDebugSession {
    _targetFile = '';
    _sourceLines = [];
    _currentLine = 0;
    _breakpoints = new Map();
    constructor() {
        super("alu-debug.txt");
        this.setDebuggerLinesStartAt1(true);
        this.setDebuggerColumnsStartAt1(true);
    }
    initializeRequest(response, args) {
        response.body = response.body || {};
        response.body.supportsConfigurationDoneRequest = true;
        response.body.supportsEvaluateForHovers = false;
        response.body.supportsStepBack = false;
        response.body.supportsDataBreakpoints = false;
        response.body.supportsCompletionsRequest = false;
        this.sendResponse(response);
        this.sendEvent(new debugadapter_1.InitializedEvent());
    }
    configurationDoneRequest(response, args) {
        super.configurationDoneRequest(response, args);
        // Could start execution here
    }
    launchRequest(response, args) {
        this._targetFile = args.program;
        try {
            const content = fs.readFileSync(this._targetFile, 'utf8');
            this._sourceLines = content.split(/\r?\n/);
        }
        catch (e) {
            this.sendErrorResponse(response, 1, `Cannot read program file: ${this._targetFile}`);
            return;
        }
        this.sendResponse(response);
        this.sendEvent(new debugadapter_1.OutputEvent(`[AluDebugger] Launched program: ${this._targetFile}\n`));
        if (args.stopOnEntry) {
            this._currentLine = 1;
            this.sendEvent(new debugadapter_1.StoppedEvent('entry', 1));
        }
        else {
            this.continueRequest(response, { threadId: 1 });
        }
    }
    setBreakPointsRequest(response, args) {
        const path = args.source.path;
        const clientLines = args.breakpoints || [];
        this._breakpoints.set(path, clientLines.map(b => b.line));
        const breakpoints = clientLines.map(l => {
            const bp = new debugadapter_1.Breakpoint(true, l.line);
            bp.id = l.line;
            return bp;
        });
        response.body = { breakpoints };
        this.sendResponse(response);
    }
    threadsRequest(response) {
        response.body = {
            threads: [
                new debugadapter_1.Thread(1, "thread 1")
            ]
        };
        this.sendResponse(response);
    }
    stackTraceRequest(response, args) {
        const frame = new debugadapter_1.StackFrame(1, "main", new debugadapter_1.Source((0, path_1.basename)(this._targetFile), this._targetFile), this._currentLine, 0);
        response.body = {
            stackFrames: [frame],
            totalFrames: 1
        };
        this.sendResponse(response);
    }
    scopesRequest(response, args) {
        response.body = {
            scopes: [
                new debugadapter_1.Scope("Locals", 1, false)
            ]
        };
        this.sendResponse(response);
    }
    variablesRequest(response, args) {
        const variables = [
            {
                name: "simulated_ptr",
                type: "ptr<u8>",
                value: "0x00A4FF00",
                variablesReference: 0
            }
        ];
        response.body = { variables };
        this.sendResponse(response);
    }
    continueRequest(response, args) {
        this.sendResponse(response);
        // Very basic simulation of stepping to the next breakpoint
        const lines = this._breakpoints.get(this._targetFile) || [];
        for (let i = this._currentLine; i <= this._sourceLines.length; i++) {
            if (lines.indexOf(i) !== -1) {
                this._currentLine = i;
                this.sendEvent(new debugadapter_1.StoppedEvent('breakpoint', 1));
                return;
            }
        }
        this.sendEvent(new debugadapter_1.TerminatedEvent());
    }
    nextRequest(response, args) {
        this.sendResponse(response);
        this._currentLine++;
        if (this._currentLine > this._sourceLines.length) {
            this.sendEvent(new debugadapter_1.TerminatedEvent());
        }
        else {
            this.sendEvent(new debugadapter_1.StoppedEvent('step', 1));
        }
    }
}
exports.AluDebugSession = AluDebugSession;
//# sourceMappingURL=aluDebug.js.map