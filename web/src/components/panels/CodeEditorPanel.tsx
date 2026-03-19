import { useEffect, useRef, useState } from "react";
import Editor, { type OnMount } from "@monaco-editor/react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";
import { btnBase } from "../ui/styles";

type MonacoEditor = Parameters<OnMount>[0];

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

type Props = {
  rpcClient: RpcClient;
};

export function CodeEditorPanel({ rpcClient }: Props) {
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const updateCoreCode = useGameStore((s) => s.updateCoreCode);
  const loadCoreCode = useGameStore((s) => s.loadCoreCode);
  const executeCoreUpdate = useGameStore((s) => s.executeCoreUpdate);
  const [sourceNames, setSourceNames] = useState<string[]>([]);
  const [code, setCode] = useState("-- Select source");
  const [selectedSource, setSelectedSource] = useState<string>("");
  const [status, setStatus] = useState("");
  const [statusOk, setStatusOk] = useState(true);
  const [runConsole, setRunConsole] = useState<string[]>([]);
  const supported = isFeatureSupported(capabilityMethods.codeEditor);
  const executeSupported = isFeatureSupported(capabilityMethods.codeExecute);
  const editorRef = useRef<MonacoEditor | null>(null);

  useEffect(() => {
    if (!supported) return;
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => {
        const names = Object.keys(data.sources ?? {});
        setSourceNames(names);
        if (names.length > 0) {
          setSelectedSource(names[0]);
          setCode(data.sources[names[0]] ?? "");
        }
      })
      .catch(() => setSourceNames([]));
  }, [rpcClient, supported]);

  const canSave = selectedFrameId !== null && supported;
  const canRun = selectedFrameId !== null && supported && executeSupported;

  const handleSave = () => {
    if (selectedFrameId === null) return;
    void updateCoreCode(rpcClient, selectedFrameId, code)
      .then(() => { setStatus("Saved ✓"); setStatusOk(true); })
      .catch((error: unknown) => {
        setStatus("Save failed ✗");
        setStatusOk(false);
        const message = error instanceof Error ? error.message : String(error);
        setRunConsole((current) => [...current.slice(-9), `save: ${message}`]);
      });
  };

  const handleLoad = () => {
    if (selectedFrameId === null) return;
    void loadCoreCode(rpcClient, selectedFrameId)
      .then((script) => { setCode(script); setStatus("Loaded ✓"); setStatusOk(true); })
      .catch((error: unknown) => {
        setStatus("Load failed ✗");
        setStatusOk(false);
        const message = error instanceof Error ? error.message : String(error);
        setRunConsole((current) => [...current.slice(-9), `load: ${message}`]);
      });
  };

  const handleRun = () => {
    if (selectedFrameId === null) return;
    void executeCoreUpdate(rpcClient, selectedFrameId)
      .then((result) => {
        if (result.status === "ok") {
          setStatus("Run: ok ✓"); setStatusOk(true);
        } else {
          setStatus("Run: error ✗"); setStatusOk(false);
          setRunConsole((current) => [...current.slice(-9), result.error ?? "unknown runtime error"]);
        }
      })
      .catch((error: unknown) => {
        setStatus("Run: error ✗"); setStatusOk(false);
        const message = error instanceof Error ? error.message : String(error);
        setRunConsole((current) => [...current.slice(-9), message]);
      });
  };

  const handleEditorMount = (ed: MonacoEditor) => {
    editorRef.current = ed;
    ed.addCommand(2097 /* KeyMod.CtrlCmd | KeyCode.KeyS */, () => {
      if (canSave) handleSave();
    });
  };

  return (
    <Panel title="Lua Code Editor" unsupported={supported ? undefined : "code editor"}>
      <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
        {/* Toolbar */}
        <div style={toolbarStyle}>
          <button
            type="button"
            style={canSave ? saveBtn : disabledBtn}
            disabled={!canSave}
            title="Save to selected frame core (Ctrl+S)"
            onClick={handleSave}
          >
            Save
          </button>
          <button
            type="button"
            style={canSave ? btnBase : disabledBtn}
            disabled={!canSave}
            title="Load script from selected frame"
            onClick={handleLoad}
          >
            Load
          </button>
          <button
            type="button"
            style={canRun ? runBtn : disabledBtn}
            disabled={!canRun}
            title={executeSupported ? "Run update on selected frame" : "Run unavailable in this capability set"}
            onClick={handleRun}
          >
            Run
          </button>
          {selectedFrameId === null && (
            <span style={{ fontSize: 11, color: "#6b7280" }}>No frame selected</span>
          )}
          {status && (
            <span style={{ fontSize: 11, marginLeft: "auto", color: statusOk ? "#4ade80" : "#f87171" }}>
              {status}
            </span>
          )}
        </div>

        {/* Source selector */}
        <div style={{ padding: "5px 8px", borderBottom: "1px solid #1e293b", flexShrink: 0 }}>
          <select
            value={selectedSource}
            style={{ width: "100%", fontSize: 12, background: "#0b1220", color: "#e5e7eb", border: "1px solid #374151", borderRadius: 4, padding: "2px 4px" }}
            onChange={(e) => {
              const value = e.target.value;
              setSelectedSource(value);
              void rpcClient
                .call<{ sources: Record<string, string> }>("code.sources")
                .then((data) => setCode(data.sources[value] ?? ""));
            }}
          >
            {sourceNames.map((name) => (
              <option key={name} value={name}>{name}</option>
            ))}
          </select>
        </div>

        {/* Editor */}
        <div style={{ flex: 1, minHeight: 0 }}>
          <Editor
            language="lua"
            theme="vs-dark"
            value={code}
            onChange={(value) => setCode(value ?? "")}
            onMount={handleEditorMount}
            options={{
              minimap: { enabled: false },
              fontSize: 12,
              scrollBeyondLastLine: false,
            }}
            height="100%"
          />
        </div>

        {/* Console */}
        <div style={{ fontSize: 11, color: "#e5e7eb", background: "#0b1020", padding: 8, maxHeight: 80, overflowY: "auto", flexShrink: 0 }}>
          {runConsole.length === 0 ? "No runtime errors." : runConsole.join("\n")}
        </div>
      </div>
    </Panel>
  );
}
