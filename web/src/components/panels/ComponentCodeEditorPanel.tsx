import { Suspense, lazy, useState, useEffect } from "react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";

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
  const supported = isFeatureSupported(capabilityMethods.codeEditor);

  const [code, setCode] = useState(initialCode);
  const [status, setStatus] = useState("");
  const [loading, setLoading] = useState(mode === "source");

  // Source mode: load from code.sources by name
  useEffect(() => {
    if (mode !== "source" || !sourceName) {
      setLoading(false);
      return;
    }
    setLoading(true);
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => {
        const sources = data.sources ?? {};
        // Exact match first, then case-insensitive, then partial
        const found =
          sources[sourceName] ??
          Object.entries(sources).find(([k]) => k.toLowerCase() === sourceName.toLowerCase())?.[1] ??
          Object.entries(sources).find(([k]) => k.toLowerCase().includes(sourceName.toLowerCase()))?.[1] ??
          `-- Source "${sourceName}" not found`;
        setCode(found);
      })
      .catch(() => setCode(`-- Failed to load source "${sourceName}"`))
      .finally(() => setLoading(false));
  }, [mode, sourceName, rpcClient]);

  const handleSave = () => {
    if (mode !== "attribute" || frameId == null || componentId == null) return;
    void updateComponentAttribute(rpcClient, frameId, componentId, attrKey, code)
      .then(() => setStatus("Saved ✓"))
      .catch(() => setStatus("Save failed ✗"));
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", overflow: "hidden" }}>
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
              options={{
                minimap: { enabled: false },
                fontSize: 12,
                readOnly: mode === "source" && !supported,
                scrollBeyondLastLine: false,
              }}
              height="100%"
            />
          </div>
        </Suspense>
      )}

      {mode === "attribute" && (
        <div style={{ display: "flex", gap: 8, alignItems: "center", padding: "6px 8px", borderTop: "1px solid #1e293b", flexShrink: 0 }}>
          <button
            type="button"
            onClick={handleSave}
            style={{
              fontSize: 12,
              padding: "3px 12px",
              background: "#1e3a5f",
              border: "1px solid #3b82f6",
              borderRadius: 4,
              color: "#93c5fd",
              cursor: "pointer",
            }}
          >
            Save
          </button>
          {status && (
            <span style={{ fontSize: 11, color: status.includes("✓") ? "#4ade80" : "#f87171" }}>
              {status}
            </span>
          )}
        </div>
      )}
    </div>
  );
}
