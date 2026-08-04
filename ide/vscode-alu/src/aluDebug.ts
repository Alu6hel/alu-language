import {
    Logger, logger,
    LoggingDebugSession,
    InitializedEvent, TerminatedEvent, StoppedEvent, BreakpointEvent, OutputEvent,
    ProgressStartEvent, ProgressUpdateEvent, ProgressEndEvent,
    Thread, StackFrame, Scope, Source, Handles, Breakpoint
} from '@vscode/debugadapter';
import { DebugProtocol } from '@vscode/debugprotocol';
import * as fs from 'fs';
import { basename } from 'path';

export interface LaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {
    program: string;
    stopOnEntry?: boolean;
}

export class AluDebugSession extends LoggingDebugSession {
    private _targetFile = '';
    private _sourceLines: string[] = [];
    private _currentLine = 0;
    private _breakpoints = new Map<string, number[]>();

    public constructor() {
        super("alu-debug.txt");
        this.setDebuggerLinesStartAt1(true);
        this.setDebuggerColumnsStartAt1(true);
    }

    protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {
        response.body = response.body || {};
        response.body.supportsConfigurationDoneRequest = true;
        response.body.supportsEvaluateForHovers = false;
        response.body.supportsStepBack = false;
        response.body.supportsDataBreakpoints = false;
        response.body.supportsCompletionsRequest = false;

        this.sendResponse(response);
        this.sendEvent(new InitializedEvent());
    }

    protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
        super.configurationDoneRequest(response, args);
        // Could start execution here
    }

    protected launchRequest(response: DebugProtocol.LaunchResponse, args: LaunchRequestArguments): void {
        this._targetFile = args.program;
        
        try {
            const content = fs.readFileSync(this._targetFile, 'utf8');
            this._sourceLines = content.split(/\r?\n/);
        } catch (e) {
            this.sendErrorResponse(response, 1, `Cannot read program file: ${this._targetFile}`);
            return;
        }

        this.sendResponse(response);
        this.sendEvent(new OutputEvent(`[AluDebugger] Launched program: ${this._targetFile}\n`));

        if (args.stopOnEntry) {
            this._currentLine = 1;
            this.sendEvent(new StoppedEvent('entry', 1));
        } else {
            this.continueRequest(<DebugProtocol.ContinueResponse>response, { threadId: 1 });
        }
    }

    protected setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments): void {
        const path = args.source.path as string;
        const clientLines = args.breakpoints || [];
        
        this._breakpoints.set(path, clientLines.map(b => b.line));

        const breakpoints = clientLines.map(l => {
            const bp = <DebugProtocol.Breakpoint> new Breakpoint(true, l.line);
            bp.id = l.line;
            return bp;
        });

        response.body = { breakpoints };
        this.sendResponse(response);
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = {
            threads: [
                new Thread(1, "thread 1")
            ]
        };
        this.sendResponse(response);
    }

    protected stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): void {
        const frame = new StackFrame(1, "main", new Source(basename(this._targetFile), this._targetFile), this._currentLine, 0);
        response.body = {
            stackFrames: [frame],
            totalFrames: 1
        };
        this.sendResponse(response);
    }

    protected scopesRequest(response: DebugProtocol.ScopesResponse, args: DebugProtocol.ScopesArguments): void {
        response.body = {
            scopes: [
                new Scope("Locals", 1, false)
            ]
        };
        this.sendResponse(response);
    }

    protected variablesRequest(response: DebugProtocol.VariablesResponse, args: DebugProtocol.VariablesArguments): void {
        const variables: DebugProtocol.Variable[] = [
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

    protected continueRequest(response: DebugProtocol.ContinueResponse, args: DebugProtocol.ContinueArguments): void {
        this.sendResponse(response);
        
        // Very basic simulation of stepping to the next breakpoint
        const lines = this._breakpoints.get(this._targetFile) || [];
        for (let i = this._currentLine; i <= this._sourceLines.length; i++) {
            if (lines.indexOf(i) !== -1) {
                this._currentLine = i;
                this.sendEvent(new StoppedEvent('breakpoint', 1));
                return;
            }
        }
        
        this.sendEvent(new TerminatedEvent());
    }

    protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
        this.sendResponse(response);
        this._currentLine++;
        if (this._currentLine > this._sourceLines.length) {
            this.sendEvent(new TerminatedEvent());
        } else {
            this.sendEvent(new StoppedEvent('step', 1));
        }
    }
}
