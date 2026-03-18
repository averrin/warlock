import { useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

export function FrameInspector({ rpcClient }: Props) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectFrame = useGameStore((s) => s.selectFrame);
  const activateFrame = useGameStore((s) => s.activateFrame);
  const fetchInitialState = useGameStore((s) => s.fetchInitialState);
  const setComponentState = useGameStore((s) => s.setComponentState);
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);
  const supported = isFeatureSupported(capabilityMethods.frameInspector);
  const [attributeDraftByKey, setAttributeDraftByKey] = useState<Record<string, string>>({});

  const selected = frames.find((f) => f.id === selectedFrameId) ?? null;
  const components = useMemo(() => selected?.components ?? [], [selected]);

  return (
    <Panel title="Frame Inspector" unsupported={supported ? undefined : "frame inspector"}>
      <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
        <select
          disabled={!supported}
          value={selectedFrameId ?? ""}
          onChange={(e) => selectFrame(Number(e.target.value) || null)}
        >
          <option value="">Select frame</option>
          {frames.map((frame) => (
            <option key={frame.id} value={frame.id}>
              {frame.name} ({frame.id})
            </option>
          ))}
        </select>
        {selected ? (
          <>
            <div style={{ fontSize: 12 }}>
              <div>name: {selected.name}</div>
              <div>size: {selected.size}</div>
              <div>components: {selected.component_count}</div>
            </div>
            <button
              disabled={!supported}
              onClick={() => {
                void activateFrame(rpcClient, selected.id);
              }}
            >
              Activate Frame
            </button>
            <button
              disabled={!supported}
              onClick={() => {
                void fetchInitialState(rpcClient);
              }}
            >
              Refresh
            </button>
            <div style={{ display: "grid", gap: 6, marginTop: 8 }}>
              <div style={{ fontSize: 12, fontWeight: 600 }}>Components</div>
              {components.length === 0 ? (
                <div style={{ fontSize: 12, color: "#9ca3af" }}>No components</div>
              ) : (
                components.map((component) => {
                  const scalarEntries = Object.entries(component.attributes ?? {}).filter((entry) => {
                    const value = entry[1];
                    return (
                      typeof value === "string" ||
                      typeof value === "number" ||
                      typeof value === "boolean"
                    );
                  });
                  return (
                    <div
                      key={component.id}
                      style={{
                        border: "1px solid #334155",
                        borderRadius: 6,
                        padding: 8,
                        display: "grid",
                        gap: 6,
                      }}
                    >
                      <div style={{ display: "flex", justifyContent: "space-between", gap: 8 }}>
                        <span style={{ fontSize: 12 }}>{component.name}</span>
                        <span
                          style={{
                            fontSize: 11,
                            borderRadius: 999,
                            padding: "2px 6px",
                            background: component.state === "active" ? "#14532d" : "#1f2937",
                          }}
                        >
                          {component.state}
                        </span>
                      </div>
                      <div style={{ display: "flex", gap: 6 }}>
                        <button
                          disabled={!supported}
                          onClick={() => {
                            void setComponentState(rpcClient, selected.id, component.id, "active");
                          }}
                        >
                          Activate
                        </button>
                        <button
                          disabled={!supported}
                          onClick={() => {
                            void setComponentState(rpcClient, selected.id, component.id, "inactive");
                          }}
                        >
                          Deactivate
                        </button>
                      </div>
                      {scalarEntries.map(([key, value]) => {
                        const draftKey = `${component.id}:${key}`;
                        const draftValue = attributeDraftByKey[draftKey] ?? String(value);
                        return (
                          <div key={draftKey} style={{ display: "flex", gap: 6 }}>
                            <label style={{ fontSize: 11, minWidth: 90 }}>{key}</label>
                            <input
                              value={draftValue}
                              onChange={(e) => {
                                const next = e.target.value;
                                setAttributeDraftByKey((current) => ({
                                  ...current,
                                  [draftKey]: next,
                                }));
                              }}
                            />
                            <button
                              disabled={!supported}
                              onClick={() => {
                                const next =
                                  typeof value === "number"
                                    ? Number(draftValue)
                                    : typeof value === "boolean"
                                      ? draftValue === "true"
                                      : draftValue;
                                void updateComponentAttribute(
                                  rpcClient,
                                  selected.id,
                                  component.id,
                                  key,
                                  next,
                                );
                              }}
                            >
                              Save
                            </button>
                          </div>
                        );
                      })}
                    </div>
                  );
                })
              )}
            </div>
          </>
        ) : (
          <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
        )}
      </div>
    </Panel>
  );
}
