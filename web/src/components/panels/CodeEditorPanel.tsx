import { useEffect, useState } from "react";
import Editor from "@monaco-editor/react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

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
  const [runConsole, setRunConsole] = useState<string[]>([]);
  const supported = isFeatureSupported(capabilityMethods.codeEditor);
  const executeSupported = isFeatureSupported(capabilityMethods.codeExecute);

  useEffect(() => {
    if (!supported) {
      return;
    }
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
      .catch(() => {
        setSourceNames([]);
      });
  }, [rpcClient, supported]);

  return (
    <Panel title="Lua Code Editor" unsupported={supported ? undefined : "code editor"}>
      <div style={{ display: "grid", gap: 8 }}>
        <select
          value={selectedSource}
          onChange={(e) => {
            const value = e.target.value;
            setSelectedSource(value);
            void rpcClient
              .call<{ sources: Record<string, string> }>("code.sources")
              .then((data) => setCode(data.sources[value] ?? ""));
          }}
        >
          {sourceNames.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>
        <Editor
          language="lua"
          height="800px"
          theme="vs-dark"
          value={code}
          onChange={(value) => setCode(value ?? "")}
          options={{
            minimap: { enabled: false },
            fontSize: 12,
          }}
        />
        <button
          disabled={selectedFrameId === null || !supported}
          onClick={() => {
            if (selectedFrameId === null) {
              return;
            }
            void updateCoreCode(rpcClient, selectedFrameId, code)
              .then(() => setStatus("Saved core script"))
              .catch((error: unknown) => {
                setStatus("Save failed");
                const message = error instanceof Error ? error.message : String(error);
                setRunConsole((current) => [...current.slice(-9), `save: ${message}`]);
              });
          }}
        >
          Save to Selected Frame Core
        </button>
        <button
          disabled={selectedFrameId === null || !supported}
          onClick={() => {
            if (selectedFrameId === null) {
              return;
            }
            void loadCoreCode(rpcClient, selectedFrameId)
              .then((script) => {
                setCode(script);
                setStatus("Loaded core script");
              })
              .catch((error: unknown) => {
                setStatus("Load failed");
                const message = error instanceof Error ? error.message : String(error);
                setRunConsole((current) => [...current.slice(-9), `load: ${message}`]);
              });
          }}
        >
          Load Selected Frame Script
        </button>
        <button
          disabled={selectedFrameId === null || !supported || !executeSupported}
          onClick={() => {
            if (selectedFrameId === null) {
              return;
            }
            void executeCoreUpdate(rpcClient, selectedFrameId)
              .then((result) => {
                if (result.status === "ok") {
                  setStatus("Run: ok");
                  return;
                }
                setStatus("Run: error");
                setRunConsole((current) => [...current.slice(-9), result.error ?? "unknown runtime error"]);
              })
              .catch((error: unknown) => {
                setStatus("Run: error");
                const message = error instanceof Error ? error.message : String(error);
                setRunConsole((current) => [...current.slice(-9), message]);
              });
          }}
        >
          Run update
        </button>
        {!executeSupported ? (
          <div style={{ fontSize: 11, color: "#9ca3af" }}>Run action unavailable in this capability set.</div>
        ) : null}
        {status ? <div style={{ fontSize: 12 }}>{status}</div> : null}
        <div style={{ fontSize: 11, color: "#e5e7eb", background: "#0b1020", borderRadius: 6, padding: 8 }}>
          {runConsole.length === 0 ? "No runtime errors." : runConsole.join("\n")}
        </div>
      </div>
    </Panel>
  );
}
