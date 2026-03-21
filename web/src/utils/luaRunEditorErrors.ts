import type { OnMount } from "@monaco-editor/react";

type MonacoEditor = Parameters<OnMount>[0];
type MonacoNs = Parameters<OnMount>[1];

const MARKER_OWNER = "warlock-lua-run";

/** Best-effort parse of Lua/sol error strings like `[string "..."]:12: message` or `stdin:1: msg`. */
export function parseLuaErrorLine(raw: string): { line: number; message: string } | null {
  const s = raw.trim().split("\n")[0] ?? "";
  let m = /\]:(\d+):\s*(.*)$/.exec(s);
  if (m) {
    const line = Number.parseInt(m[1], 10);
    if (line > 0) return { line, message: (m[2] ?? "").trim() || s };
  }
  m = /^([^:\n]+):(\d+):\s*(.+)$/.exec(s);
  if (m) {
    const line = Number.parseInt(m[2], 10);
    if (line > 0) return { line, message: (m[3] ?? "").trim() };
  }
  return null;
}

/** Requires the `monaco` namespace from the editor `onMount` callback (second argument). */
export function setLuaRunErrorMarkers(
  ed: MonacoEditor,
  monaco: MonacoNs,
  errorText: string | undefined,
): void {
  const model = ed.getModel();
  if (!model) return;
  if (!errorText?.trim()) {
    monaco.editor.setModelMarkers(model, MARKER_OWNER, []);
    return;
  }
  const parsed = parseLuaErrorLine(errorText);
  const lineCount = model.getLineCount();
  let line = 1;
  let msg = errorText.trim();
  if (parsed) {
    line = Math.min(Math.max(1, parsed.line), lineCount);
    msg = parsed.message || msg;
  }
  const maxCol = model.getLineMaxColumn(line);
  monaco.editor.setModelMarkers(model, MARKER_OWNER, [
    {
      startLineNumber: line,
      startColumn: 1,
      endLineNumber: line,
      endColumn: maxCol,
      message: msg,
      severity: monaco.MarkerSeverity.Error,
    },
  ]);
  ed.revealLineInCenter(line);
}

export function clearLuaRunMarkersWithMonaco(ed: MonacoEditor, monaco: MonacoNs): void {
  const model = ed.getModel();
  if (model) monaco.editor.setModelMarkers(model, MARKER_OWNER, []);
}
