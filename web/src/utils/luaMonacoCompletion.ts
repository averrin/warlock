import type { OnMount } from "@monaco-editor/react";
import type { RpcClient } from "../rpc/client";

type MonacoNs = Parameters<OnMount>[1];

type CompletionModel = {
  getLineContent: (lineNumber: number) => string;
};

type CompletionPosition = {
  lineNumber: number;
  column: number;
};

/** Monaco positions are 1-based. */
export function parseLuaCompletionContext(
  line: string,
  column: number,
): { path: string; prefix: string } {
  const before = line.slice(0, Math.max(0, column - 1));
  const trimmed = before.trimEnd();
  const lastDot = trimmed.lastIndexOf(".");
  if (lastDot === -1) {
    const m = trimmed.match(/(?:^|[\s,(\[{])([a-zA-Z_]\w*)$/);
    return { path: "", prefix: m ? m[1] : "" };
  }
  const pathPart = trimmed.slice(0, lastDot);
  const afterDot = trimmed.slice(lastDot + 1);
  const chain = pathPart.match(/([a-zA-Z_][\w.]*)$/);
  const path = chain ? chain[1] : "";
  const prefixMatch = afterDot.match(/^(\w*)/);
  const prefix = prefixMatch ? prefixMatch[1] : "";
  return { path, prefix };
}

let requestSeq = 0;

export function registerLuaStateCompletionProvider(
  monaco: MonacoNs,
  rpcClient: RpcClient,
  getFrameId: () => number | null,
): { dispose: () => void } {
  const disposable = monaco.languages.registerCompletionItemProvider("lua", {
    triggerCharacters: ["."],
    provideCompletionItems: async (model: CompletionModel, position: CompletionPosition) => {
      const frameId = getFrameId();
      if (frameId == null) {
        return { suggestions: [] };
      }
      const line = model.getLineContent(position.lineNumber);
      const { path, prefix } = parseLuaCompletionContext(line, position.column);
      const seq = ++requestSeq;
      try {
        const data = await rpcClient.call<{ keys: string[] }>("code.completion", {
          frame_id: frameId,
          path,
        });
        if (seq !== requestSeq) {
          return { suggestions: [] };
        }
        const keys = data.keys ?? [];
        const filtered =
          prefix === ""
            ? keys
            : keys.filter((k) => k.toLowerCase().startsWith(prefix.toLowerCase()));
        return {
          suggestions: filtered.map((k) => ({
            label: k,
            kind: monaco.languages.CompletionItemKind.Field,
            insertText: k,
            range: {
              startLineNumber: position.lineNumber,
              endLineNumber: position.lineNumber,
              startColumn: position.column - prefix.length,
              endColumn: position.column,
            },
          })),
        };
      } catch {
        return { suggestions: [] };
      }
    },
  });
  // Monaco's IDisposable.dispose must not be extracted unbound (breaks `this`); wrap for callers.
  return { dispose: () => disposable.dispose() };
}
