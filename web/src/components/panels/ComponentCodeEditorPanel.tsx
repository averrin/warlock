import { Suspense, lazy, useState, useEffect, useRef, useMemo, useCallback } from "react";
import type { OnMount } from "@monaco-editor/react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { Badge } from "../ui";
import { btnBase } from "../ui/styles";
import {
  clearLuaRunMarkersWithMonaco,
  setLuaRunErrorMarkers,
} from "../../utils/luaRunEditorErrors";
import { registerLuaStateCompletionProvider } from "../../utils/luaMonacoCompletion";
import { ComponentControls } from "../frame/ComponentControls";

type MonacoEditor = Parameters<OnMount>[0];
type MonacoNs = Parameters<OnMount>[1];

const Editor = lazy(() => import("@monaco-editor/react"));

export interface ComponentCodeEditorParams {
  /** "attribute" — edit a component's code attribute; "source" — view/edit a source template */
  mode: "attribute" | "source";
  // attribute mode
  frameId?: number;
  componentId?: number;
  attrKey?: string;
  initialCode?: string;
  // source mode
  sourceName?: string;
}

type Props = ComponentCodeEditorParams & { rpcClient: RpcClient };

/** Logical component type from `type` field or `attributes.type` (matches server serializeComponent). */
function getComponentLogicalType(c: ComponentDTO): string {
  if (c.type) return c.type;
  const raw = c.attributes?.type;
  if (raw && typeof raw === "object" && "base_value" in raw) {
    const bv = (raw as { base_value: unknown }).base_value;
    if (typeof bv === "string") return bv;
  }
  return "";
}

const toolbarStyle: React.CSSProperties = {
  display: "flex",
  alignItems: "center",
  gap: 6,
  padding: "5px 8px",
  borderBottom: "1px solid #1e293b",
  background: "#0f172a",
  flexShrink: 0,
};

const saveBtn: React.CSSProperties = {
  ...btnBase,
  background: "#1e3a5f",
  borderColor: "#3b82f6",
  color: "#93c5fd",
};

const runBtn: React.CSSProperties = {
  ...btnBase,
  background: "#14532d",
  borderColor: "#22c55e",
  color: "#86efac",
};

const fileBtn: React.CSSProperties = {
  ...btnBase,
  background: "#1a1a2e",
  borderColor: "#6366f1",
  color: "#a5b4fc",
};

const disabledBtn: React.CSSProperties = {
  ...btnBase,
  opacity: 0.4,
  cursor: "not-allowed",
};

export function ComponentCodeEditorPanel({
  mode,
  frameId,
  componentId,
  attrKey = "code",
  initialCode = "",
  sourceName,
  rpcClient,
}: Props) {
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);
  const executeCoreUpdate = useGameStore((s) => s.executeCoreUpdate);
  const sourcesSupported = isFeatureSupported(capabilityMethods.codeEditor);
  const defEditorSupported = isFeatureSupported(capabilityMethods.codeDefEditor);
  const executeSupported = isFeatureSupported(capabilityMethods.codeExecute);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const frames = useGameStore((s) => s.frames);

  const coreCodeContext = useMemo(() => {
    if (mode !== "attribute" || attrKey !== "code" || frameId == null || componentId == null) {
      return null;
    }
    const frame = frames.find((f) => f.id === frameId);
    const comp = frame?.components?.find((c) => c.id === componentId);
    if (!comp || getComponentLogicalType(comp) !== "Core") return null;
    return { frameId, componentId, state: comp.state };
  }, [mode, attrKey, frameId, componentId, frames]);

  const [code, setCode] = useState(initialCode);
  const [status, setStatus] = useState("");
  const [statusOk, setStatusOk] = useState(true);
  const [runConsole, setRunConsole] = useState<string[]>([]);
  const [loading, setLoading] = useState(mode === "source");
  const editorRef = useRef<MonacoEditor | null>(null);
  const monacoRef = useRef<MonacoNs | null>(null);
  const completionFrameRef = useRef<number | null>(null);
  completionFrameRef.current =
    mode === "attribute" ? (frameId ?? null) : (selectedFrameId ?? null);
  const luaCompletionDisposeRef = useRef<(() => void) | null>(null);

  useEffect(
    () => () => {
      luaCompletionDisposeRef.current?.();
      luaCompletionDisposeRef.current = null;
    },
    [],
  );

  const isReadOnly = mode === "source" && !(defEditorSupported && sourcesSupported);
  const canSave = mode === "attribute" && frameId != null && componentId != null;
  const canRun = mode === "attribute" && frameId != null && executeSupported;
  const canSaveSource = mode === "source" && defEditorSupported && sourcesSupported && !!sourceName;
  const fileInputRef = useRef<HTMLInputElement | null>(null);

  const suggestedFileName = useMemo(() => {
    const parts: string[] = [];
    if (frameId != null) {
      const frame = frames.find((f) => f.id === frameId);
      if (frame?.name) parts.push(frame.name);
      parts.push(String(frameId));
    }
    if (componentId != null) {
      const frame = frames.find((f) => f.id === frameId);
      const comp = frame?.components?.find((c) => c.id === componentId);
      if (comp?.name) parts.push(comp.name);
    }
    parts.push(attrKey);
    return parts.join("-").replace(/[^a-zA-Z0-9_-]/g, "_") + ".lua";
  }, [frameId, componentId, attrKey, frames]);

  const handleSaveToFile = useCallback(() => {
    const blob = new Blob([code], { type: "text/x-lua" });

    // Try File System Access API first (Chromium browsers)
    if ("showSaveFilePicker" in window) {
      void (async () => {
        try {
          const handle = await (window as unknown as { showSaveFilePicker: (opts: unknown) => Promise<FileSystemFileHandle> }).showSaveFilePicker({
            suggestedName: suggestedFileName,
            types: [{ description: "Lua files", accept: { "text/x-lua": [".lua"] } }],
          });
          const writable = await handle.createWritable();
          await writable.write(blob);
          await writable.close();
          setStatus("Exported to file"); setStatusOk(true);
        } catch (err: unknown) {
          if (err instanceof DOMException && err.name === "AbortError") return;
          setStatus("File export failed"); setStatusOk(false);
        }
      })();
      return;
    }

    // Fallback: download via anchor
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = suggestedFileName;
    a.click();
    URL.revokeObjectURL(url);
    setStatus("Exported to file"); setStatusOk(true);
  }, [code, suggestedFileName]);

  const handleLoadFromFile = useCallback(() => {
    // Try File System Access API first (Chromium browsers)
    if ("showOpenFilePicker" in window) {
      void (async () => {
        try {
          const [handle] = await (window as unknown as { showOpenFilePicker: (opts: unknown) => Promise<FileSystemFileHandle[]> }).showOpenFilePicker({
            types: [{ description: "Lua files", accept: { "text/x-lua": [".lua"] } }],
            multiple: false,
          });
          const file = await handle.getFile();
          const text = await file.text();
          setCode(text);
          setStatus("Loaded from file"); setStatusOk(true);
        } catch (err: unknown) {
          if (err instanceof DOMException && err.name === "AbortError") return;
          setStatus("File load failed"); setStatusOk(false);
        }
      })();
      return;
    }

    // Fallback: hidden file input
    fileInputRef.current?.click();
  }, []);

  const handleFileInputChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    void file.text().then((text) => {
      setCode(text);
      setStatus("Loaded from file"); setStatusOk(true);
    }).catch(() => {
      setStatus("File load failed"); setStatusOk(false);
    });
    // Reset so the same file can be re-selected
    e.target.value = "";
  }, []);

  // Source mode: load from code.sources by name
  useEffect(() => {
    if (mode !== "source" || !sourceName) {
      setLoading(false);
      return;
    }
    if (!sourcesSupported) {
      setLoading(false);
      return;
    }
    setLoading(true);
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => {
        const sources = data.sources ?? {};
        const found =
          sources[sourceName] ??
          Object.entries(sources).find(([k]) => k.toLowerCase() === sourceName.toLowerCase())?.[1] ??
          Object.entries(sources).find(([k]) => k.toLowerCase().includes(sourceName.toLowerCase()))?.[1] ??
          `-- Source "${sourceName}" not found`;
        setCode(found);
      })
      .catch(() => setCode(`-- Failed to load source "${sourceName}"`))
      .finally(() => setLoading(false));
  }, [mode, sourceName, rpcClient, sourcesSupported]);

  const handleSave = () => {
    if (mode === "attribute") {
      if (!canSave) return;
      void updateComponentAttribute(rpcClient, frameId!, componentId!, attrKey, code)
        .then(() => { setStatus("Saved ✓"); setStatusOk(true); })
        .catch(() => { setStatus("Save failed ✗"); setStatusOk(false); });
      return;
    }

    if (mode === "source") {
      if (!canSaveSource) return;
      const name = sourceName!;
      void rpcClient
        .call("code.update_source", { source_name: name, code })
        .then(() => { setStatus("Saved ✓"); setStatusOk(true); })
        .catch((error: unknown) => {
          const message = error instanceof Error ? error.message : String(error);
          setStatus(`Save failed ✗ (${message})`);
          setStatusOk(false);
        });
    }
  };

  const clearRunErrors = () => {
    const ed = editorRef.current;
    const monaco = monacoRef.current;
    if (ed && monaco) clearLuaRunMarkersWithMonaco(ed, monaco);
    setRunConsole([]);
    setStatus("");
  };

  const handleRun = () => {
    if (frameId == null) return;
    const ed0 = editorRef.current;
    const monaco0 = monacoRef.current;
    if (ed0 && monaco0) clearLuaRunMarkersWithMonaco(ed0, monaco0);
    setRunConsole([]);
    setStatus("");
    void executeCoreUpdate(rpcClient, frameId)
      .then((result) => {
        const ed = editorRef.current;
        const monaco = monacoRef.current;
        if (result.status === "ok") {
          setStatus("Run: ok ✓"); setStatusOk(true);
        } else {
          setStatus("Run: error ✗"); setStatusOk(false);
          const err = result.error ?? "unknown runtime error";
          setRunConsole((cur) => [...cur.slice(-9), err]);
          if (ed && monaco) setLuaRunErrorMarkers(ed, monaco, err);
        }
      })
      .catch((error: unknown) => {
        setStatus("Run: error ✗"); setStatusOk(false);
        const message = error instanceof Error ? error.message : String(error);
        setRunConsole((cur) => [...cur.slice(-9), message]);
        const ed = editorRef.current;
        const monaco = monacoRef.current;
        if (ed && monaco) setLuaRunErrorMarkers(ed, monaco, message);
      });
  };

  const handleEditorMount = (ed: MonacoEditor, monaco: MonacoNs) => {
    editorRef.current = ed;
    monacoRef.current = monaco;
    luaCompletionDisposeRef.current?.();
    if (sourcesSupported) {
      const { dispose } = registerLuaStateCompletionProvider(monaco, rpcClient, () => completionFrameRef.current);
      luaCompletionDisposeRef.current = dispose;
    }
    if (mode === "attribute" && canSave) {
      ed.addCommand(2097 /* KeyMod.CtrlCmd | KeyCode.KeyS */, handleSave);
    } else if (mode === "source" && canSaveSource) {
      ed.addCommand(2097 /* KeyMod.CtrlCmd | KeyCode.KeyS */, handleSave);
    }
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", overflow: "hidden" }}>
      {/* Toolbar */}
      <div style={toolbarStyle}>
        {mode === "attribute" ? (
          <>
            <button
              type="button"
              style={canSave ? saveBtn : disabledBtn}
              disabled={!canSave}
              title="Save attribute (Ctrl+S)"
              onClick={handleSave}
            >
              Save
            </button>
            <button
              type="button"
              style={canRun ? runBtn : disabledBtn}
              disabled={!canRun}
              title={executeSupported ? "Run frame update" : "Run unavailable in this capability set"}
              onClick={handleRun}
            >
              Run
            </button>
            {runConsole.length > 0 && (
              <button
                type="button"
                style={btnBase}
                title="Clear error messages and editor markers"
                onClick={clearRunErrors}
              >
                Clear errors
              </button>
            )}
            <span style={{ borderLeft: "1px solid #334155", height: 16, margin: "0 2px" }} />
            <button
              type="button"
              style={fileBtn}
              title="Save editor content to a .lua file"
              onClick={handleSaveToFile}
            >
              Export
            </button>
            <button
              type="button"
              style={fileBtn}
              title="Load content from a .lua file"
              onClick={handleLoadFromFile}
            >
              Import
            </button>
            <input
              ref={fileInputRef}
              type="file"
              accept=".lua"
              style={{ display: "none" }}
              onChange={handleFileInputChange}
            />
            {coreCodeContext && (
              <div
                style={{
                  display: "flex",
                  alignItems: "center",
                  gap: 6,
                  paddingLeft: 8,
                  marginLeft: 4,
                  borderLeft: "1px solid #334155",
                }}
              >
                <Badge label={coreCodeContext.state} variant="state" />
                <ComponentControls
                  frameId={coreCodeContext.frameId}
                  componentId={coreCodeContext.componentId}
                  componentState={coreCodeContext.state}
                  rpcClient={rpcClient}
                />
              </div>
            )}
            {status && (
              <span style={{ fontSize: 11, marginLeft: "auto", color: statusOk ? "#4ade80" : "#f87171" }}>
                {status}
              </span>
            )}
          </>
        ) : (
          <>
            <span style={{ fontSize: 11, color: "#6b7280" }}>
              {sourceName ?? "source"}
            </span>
            {defEditorSupported && sourcesSupported && (
              <button
                type="button"
                style={canSaveSource ? saveBtn : disabledBtn}
                disabled={!canSaveSource}
                title="Save definition (Ctrl+S)"
                onClick={handleSave}
              >
                Save
              </button>
            )}
            {isReadOnly && (
              <span style={{ fontSize: 10, color: "#f59e0b", background: "#451a03", border: "1px solid #92400e", borderRadius: 3, padding: "1px 5px", marginLeft: "auto" }}>
                read-only
              </span>
            )}
            {status && (
              <span style={{ fontSize: 11, marginLeft: "auto", color: statusOk ? "#4ade80" : "#f87171" }}>
                {status}
              </span>
            )}
          </>
        )}
      </div>

      {/* Editor */}
      {loading ? (
        <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", color: "#9ca3af", fontSize: 12 }}>
          Loading…
        </div>
      ) : (
        <Suspense fallback={<div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", color: "#9ca3af", fontSize: 12 }}>Loading editor…</div>}>
          <div style={{ flex: 1, minHeight: 0 }}>
            <Editor
              language="lua"
              theme="vs-dark"
              value={code}
              onChange={(v) => setCode(v ?? "")}
              onMount={handleEditorMount}
              options={{
                minimap: { enabled: false },
                fontSize: 12,
                readOnly: isReadOnly,
                scrollBeyondLastLine: false,
              }}
              height="100%"
            />
          </div>
        </Suspense>
      )}

      {/* Console — only in attribute mode when there are errors */}
      {mode === "attribute" && runConsole.length > 0 && (
        <div style={{ fontSize: 11, color: "#f87171", background: "#0b1020", padding: 8, maxHeight: 80, overflowY: "auto", flexShrink: 0 }}>
          {runConsole.join("\n")}
        </div>
      )}
    </div>
  );
}
