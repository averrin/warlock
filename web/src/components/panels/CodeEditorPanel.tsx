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
  const [sourceNames, setSourceNames] = useState<string[]>([]);
  const [code, setCode] = useState("-- Select source");
  const [selectedSource, setSelectedSource] = useState<string>("");
  const supported = isFeatureSupported(capabilityMethods.codeEditor);

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
    <Panel title="Lua Code Editor" unsupported={supported ? "missing code.execute" : "code editor"}>
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
          height="200px"
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
            void updateCoreCode(rpcClient, selectedFrameId, code);
          }}
        >
          Save to Selected Frame Core
        </button>
      </div>
    </Panel>
  );
}
