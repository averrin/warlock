import { Suspense, lazy, useState, useEffect, useRef } from "react";
import type { OnMount } from "@monaco-editor/react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { btnBase } from "../ui/styles";

type MonacoEditor = Parameters<OnMount>[0];

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

  const [code, setCode] = useState(initialCode);
  const [status, setStatus] = useState("");
  const [statusOk, setStatusOk] = useState(true);
  const [runConsole, setRunConsole] = useState<string[]>([]);
  const [loading, setLoading] = useState(mode === "source");
  const editorRef = useRef<MonacoEditor | null>(null);

  const isReadOnly = mode === "source" && !(defEditorSupported && sourcesSupported);
  const canSave = mode === "attribute" && frameId != null && componentId != null;
  const canRun = mode === "attribute" && frameId != null && executeSupported;
  const canSaveSource = mode === "source" && defEditorSupported && sourcesSupported && !!sourceName;

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

  const handleRun = () => {
    if (frameId == null) return;
    void executeCoreUpdate(rpcClient, frameId)
      .then((result) => {
        if (result.status === "ok") {
          setStatus("Run: ok ✓"); setStatusOk(true);
        } else {
          setStatus("Run: error ✗"); setStatusOk(false);
          setRunConsole((cur) => [...cur.slice(-9), result.error ?? "unknown runtime error"]);
        }
      })
      .catch((error: unknown) => {
        setStatus("Run: error ✗"); setStatusOk(false);
        const message = error instanceof Error ? error.message : String(error);
        setRunConsole((cur) => [...cur.slice(-9), message]);
      });
  };

  const handleEditorMount = (ed: MonacoEditor) => {
    editorRef.current = ed;
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
