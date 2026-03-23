import { useCallback, useEffect, useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { usePatchStore, Patch } from "../../stores/patches";
import type { ConnectionDTO, FrameDTO, PowerNetworkDTO } from "../../rpc/types";
import { CollapsibleSection, NumberField, TransformEditor, inputStyle, smallBtnStyle } from "../ui";
import { FramePanel } from "../frame";
import { useWindowLayoutStore } from "../../stores/windowLayout";
import { EntitiesSection } from "./EcsEntityInspector";

type Props = {
  rpcClient: RpcClient;
};

const CONNECTION_TYPES = ["POWER", "DATA", "CONVEYOR", "POE"] as const;
const CONNECTION_MEDIA = ["WIRE", "WIRELESS", "BEAM"] as const;

// ─── Environment ─────────────────────────────────────────────────────────────

function EnvironmentSection({ rpcClient }: { rpcClient: RpcClient }) {
  const env = useGameStore((s) => s.environment);
  const setField = useGameStore((s) => s.setEnvironmentField);

  const handle = useCallback(
    (field: string) => (value: number) => setField(rpcClient, field, value),
    [rpcClient, setField],
  );

  if (!env) return <div style={{ color: "#6b7280" }}>No environment data</div>;

  return (
    <CollapsibleSection title={"\uD83C\uDF0D Environment"}>
      <NumberField label="Minutes" value={env.minutes} onChange={handle("minutes")} />
      <NumberField label="Days" value={env.days} onChange={handle("days")} />
      <NumberField label="Temperature" value={env.temperature} onChange={handle("temperature")} />
      <NumberField label="Air Flow" value={env.air_flow} onChange={handle("air_flow")} />
      <NumberField label="Sun" value={env.sun} onChange={handle("sun")} />
    </CollapsibleSection>
  );
}

// ─── Frames section ──────────────────────────────────────────────────────────

function FramesSection({ rpcClient, filter }: { rpcClient: RpcClient; filter: string }) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const createFrameAt = useGameStore((s) => s.createFrameAt);
  const openPinnedMiniInspectorForFrame = useWindowLayoutStore((s) => s.openPinnedMiniInspectorForFrame);
  const [creating, setCreating] = useState(false);
  const [newName, setNewName] = useState("New Frame");
  const [expandedFrames, setExpandedFrames] = useState<Record<number, 0 | 1 | 2>>({});
  const pendingExpandFrameId = useWindowLayoutStore((s) => s.pendingStateInspectorExpandFrameId);
  const clearPendingExpand = useWindowLayoutStore((s) => s.clearPendingStateInspectorExpand);

  useEffect(() => {
    if (pendingExpandFrameId == null) return;
    setExpandedFrames((prev) => ({ ...prev, [pendingExpandFrameId]: 1 }));
    clearPendingExpand();
  }, [pendingExpandFrameId, clearPendingExpand]);

  const filtered = useMemo(() => {
    if (!filter) return frames;
    const lower = filter.toLowerCase();
    return frames.filter(
      (f) =>
        f.name.toLowerCase().includes(lower) ||
        String(f.id).includes(lower) ||
        f.components?.some((c) => c.name.toLowerCase().includes(lower)),
    );
  }, [frames, filter]);

  return (
    <CollapsibleSection title={`\uD83D\uDCE6 Frames (${frames.length})`}>
      <div style={{ display: "grid", gap: 4 }}>
        {filtered.map((frame) => {
          const selected = frame.id === selectedFrameId;
          return (
            <div
              key={frame.id}
              style={{
                borderRadius: 6,
                border: selected ? "2px solid #3b82f6" : "2px solid transparent",
                background: selected ? "rgba(59, 130, 246, 0.07)" : undefined,
                boxSizing: "border-box",
                display: "flex",
                gap: 6,
                alignItems: "flex-start",
              }}
            >
              <div style={{ flex: 1, minWidth: 0 }}>
                <FramePanel
                  frame={frame}
                  rpcClient={rpcClient}
                  level={expandedFrames[frame.id] ?? 0}
                  onLevelChange={(lvl) => setExpandedFrames((prev) => ({ ...prev, [frame.id]: lvl }))}
                />
              </div>
              <button
                type="button"
                title="Pin mini inspector for this frame"
                onClick={(e) => {
                  e.stopPropagation();
                  openPinnedMiniInspectorForFrame(frame.id);
                }}
                style={{
                  width: 24,
                  height: 24,
                  flexShrink: 0,
                  borderRadius: 3,
                  border: "1px solid #374151",
                  background: "#020617",
                  color: "#e5e7eb",
                  fontSize: 10,
                  lineHeight: "16px",
                  padding: 0,
                  cursor: "pointer",
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  marginTop: 6,
                }}
              >
                📌
              </button>
            </div>
          );
        })}
      </div>
      {creating ? (
        <div style={{ display: "flex", gap: 4, marginTop: 4 }}>
          <input
            type="text"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
            style={{ ...inputStyle, width: 120 }}
          />
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() => {
              createFrameAt(rpcClient, newName, 0, 0);
              setCreating(false);
              setNewName("New Frame");
            }}
          >
            Create
          </button>
          <button type="button" style={smallBtnStyle} onClick={() => setCreating(false)}>
            Cancel
          </button>
        </div>
      ) : (
        <button type="button" style={{ ...smallBtnStyle, marginTop: 4 }} onClick={() => setCreating(true)}>
          + Create Frame
        </button>
      )}
    </CollapsibleSection>
  );
}

// ─── Connections section ─────────────────────────────────────────────────────

function ConnectionRow({
  rpcClient,
  conn,
  frames,
}: {
  rpcClient: RpcClient;
  conn: ConnectionDTO;
  frames: FrameDTO[];
}) {
  const removeConnection = useGameStore((s) => s.removeConnection);
  const sourceName = frames.find((f) => f.id === conn.source)?.name;
  const targetName = frames.find((f) => f.id === conn.target)?.name;
  const medium = conn.medium?.trim() ? conn.medium : "—";

  return (
    <div
      style={{
        display: "grid",
        gridTemplateColumns: "1fr auto",
        gap: "4px 8px",
        padding: "6px 0",
        borderBottom: "1px solid #1f2937",
        fontSize: 11,
        alignItems: "start",
      }}
    >
      <div style={{ minWidth: 0 }}>
        <div style={{ color: "#e5e7eb", fontWeight: 500 }}>
          #{conn.id}{" "}
          <span style={{ color: "#9ca3af", fontWeight: 400 }}>{conn.type}</span>
          {" · "}
          <span style={{ color: "#a78bfa" }}>medium {medium}</span>
        </div>
        <div style={{ color: "#e5e7eb", marginTop: 2, fontFamily: "ui-monospace, monospace" }}>
          frames {conn.source} → {conn.target}
        </div>
        {(sourceName || targetName) && (
          <div style={{ color: "#6b7280", marginTop: 2 }}>
            {sourceName ?? `frame ${conn.source}`} → {targetName ?? `frame ${conn.target}`}
          </div>
        )}
      </div>
      <button
        type="button"
        style={{ ...smallBtnStyle, color: "#ef4444", alignSelf: "start" }}
        onClick={() => removeConnection(rpcClient, conn.id)}
      >
        Remove
      </button>
    </div>
  );
}

function ConnectionsSection({ rpcClient, filter }: { rpcClient: RpcClient; filter: string }) {
  const connections = useGameStore((s) => s.connections);
  const frames = useGameStore((s) => s.frames);
  const createConnection = useGameStore((s) => s.createConnection);
  const [creating, setCreating] = useState(false);
  const [newSource, setNewSource] = useState("");
  const [newTarget, setNewTarget] = useState("");
  const [newType, setNewType] = useState<"POWER" | "DATA" | "CONVEYOR" | "POE">("POWER");
  const [newMedium, setNewMedium] = useState<(typeof CONNECTION_MEDIA)[number]>("WIRE");

  const filtered = useMemo(() => {
    if (!filter) return connections;
    const lower = filter.toLowerCase();
    return connections.filter(
      (c) =>
        String(c.id).includes(lower) ||
        String(c.source).includes(lower) ||
        String(c.target).includes(lower) ||
        c.type.toLowerCase().includes(lower) ||
        (c.medium?.toLowerCase().includes(lower) ?? false),
    );
  }, [connections, filter]);

  return (
    <CollapsibleSection title={`\uD83D\uDD17 Connections (${connections.length})`}>
      {filtered.map((conn) => (
        <ConnectionRow key={conn.id} rpcClient={rpcClient} conn={conn} frames={frames} />
      ))}
      {creating ? (
        <div style={{ display: "flex", gap: 4, marginTop: 4, flexWrap: "wrap" }}>
          <input
            type="number"
            placeholder="Source ID"
            value={newSource}
            onChange={(e) => setNewSource(e.target.value)}
            style={{ ...inputStyle, width: 60 }}
          />
          <input
            type="number"
            placeholder="Target ID"
            value={newTarget}
            onChange={(e) => setNewTarget(e.target.value)}
            style={{ ...inputStyle, width: 60 }}
          />
          <select
            value={newType}
            onChange={(e) => setNewType(e.target.value as typeof newType)}
            style={{
              fontSize: 11,
              borderRadius: 3,
              border: "1px solid #374151",
              background: "#0b1220",
              color: "#e5e7eb",
              padding: "1px 4px",
            }}
          >
            {CONNECTION_TYPES.map((t) => (
              <option key={t} value={t}>
                {t}
              </option>
            ))}
          </select>
          <select
            value={newMedium}
            onChange={(e) => setNewMedium(e.target.value as (typeof CONNECTION_MEDIA)[number])}
            style={{
              fontSize: 11,
              borderRadius: 3,
              border: "1px solid #374151",
              background: "#0b1220",
              color: "#e5e7eb",
              padding: "1px 4px",
            }}
            title="Medium"
          >
            {CONNECTION_MEDIA.map((m) => (
              <option key={m} value={m}>
                {m}
              </option>
            ))}
          </select>
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() => {
              const s = parseInt(newSource, 10);
              const t = parseInt(newTarget, 10);
              if (!Number.isNaN(s) && !Number.isNaN(t)) {
                void createConnection(rpcClient, s, t, newType, newMedium);
                setCreating(false);
                setNewSource("");
                setNewTarget("");
              }
            }}
          >
            Create
          </button>
          <button type="button" style={smallBtnStyle} onClick={() => setCreating(false)}>
            Cancel
          </button>
        </div>
      ) : (
        <button type="button" style={{ ...smallBtnStyle, marginTop: 4 }} onClick={() => setCreating(true)}>
          + Create Connection
        </button>
      )}
    </CollapsibleSection>
  );
}

// ─── Power Networks (read-only) ─────────────────────────────────────────────

function PowerNetworkRow({ net }: { net: PowerNetworkDTO }) {
  return (
    <CollapsibleSection title={net.name ?? "Network"} defaultOpen={false}>
      <div style={{ padding: "1px 0", color: "#9ca3af" }}>Production: {net.production.toFixed(1)}</div>
      <div style={{ padding: "1px 0", color: "#9ca3af" }}>Consumption: {net.consumption.toFixed(1)}</div>
      <div style={{ padding: "1px 0", color: "#9ca3af" }}>Accumulated: {net.accumulated.toFixed(1)}</div>
      {net.battery_count != null && (
        <div style={{ padding: "1px 0", color: "#9ca3af" }}>Batteries: {net.battery_count}</div>
      )}
      <div style={{ padding: "1px 0", color: "#9ca3af" }}>
        Frames: {net.frames.join(", ")}
      </div>
    </CollapsibleSection>
  );
}

function PowerNetworksSection() {
  const powerNetworks = useGameStore((s) => s.powerNetworks);

  return (
    <CollapsibleSection title={`\u26A1 Power Networks (${powerNetworks.length})`}>
      {powerNetworks.length === 0 && <div style={{ color: "#6b7280" }}>No networks</div>}
      {powerNetworks.map((net, i) => (
        <PowerNetworkRow key={`${net.name ?? "net"}-${i}`} net={net} />
      ))}
    </CollapsibleSection>
  );
}

// ─── Patches section ──────────────────────────────────────────────────────────

function PatchRow({
  rpcClient,
  patch,
}: {
  rpcClient: RpcClient;
  patch: Patch;
}) {
  const removePatch = usePatchStore((s) => s.removePatch);
  const movePatch = usePatchStore((s) => s.movePatch);
  const [expanded, setExpanded] = useState(false);

  const handleDelete = async () => {
    try {
      await rpcClient.call("patches.delete", { id: patch.id });
      removePatch(patch.id);
    } catch (e) {
      console.error("Failed to delete patch:", e);
    }
  };

  return (
    <div style={{ marginBottom: 4 }}>
      <div
        style={{ display: "flex", alignItems: "center", gap: 8, padding: "2px 0", cursor: "pointer" }}
        onClick={() => setExpanded(!expanded)}
      >
        <span style={{ color: "#6b7280", fontSize: 10 }}>{expanded ? "▼" : "▶"}</span>
        <span
          style={{
            width: 12,
            height: 12,
            borderRadius: 2,
            background: `rgba(${patch.color.r}, ${patch.color.g}, ${patch.color.b}, ${patch.color.a / 255})`,
            border: "1px solid #374151",
            flexShrink: 0,
          }}
        />
        <span style={{ color: "#e5e7eb" }}>
          #{patch.id} {patch.name}
        </span>
        <span style={{ color: "#9ca3af", fontSize: 10 }}>
          [{patch.cells.length} cells]
        </span>
        <button
          type="button"
          style={{ ...smallBtnStyle, color: "#ef4444", marginLeft: "auto" }}
          onClick={(e) => { e.stopPropagation(); handleDelete(); }}
        >
          Delete
        </button>
      </div>
      {expanded && (
        <div style={{ paddingLeft: 20, paddingTop: 4, fontSize: 11, color: "#9ca3af" }}>
          <div>Type: <span style={{ color: "#e5e7eb" }}>{patch.type}</span></div>
          <div>Item: <span style={{ color: "#e5e7eb" }}>{patch.item}</span></div>
          <div style={{ marginTop: 4 }}>
            <TransformEditor
              x={patch.bounds.x}
              y={patch.bounds.y}
              onChange={(x, y) => void movePatch(rpcClient, patch.id, x, y)}
            />
          </div>
        </div>
      )}
    </div>
  );
}

function PatchesSection({ rpcClient, filter }: { rpcClient: RpcClient; filter: string }) {
  const patches = usePatchStore((s) => s.patches);

  const filtered = useMemo(() => {
    if (!filter) return patches;
    const lower = filter.toLowerCase();
    return patches.filter(
      (p) =>
        p.name.toLowerCase().includes(lower) ||
        String(p.id).includes(lower) ||
        p.type.toLowerCase().includes(lower) ||
        p.item.toLowerCase().includes(lower),
    );
  }, [patches, filter]);

  return (
    <CollapsibleSection title={`⛏ Patches (${patches.length})`}>
      {filtered.length === 0 && <div style={{ color: "#6b7280" }}>No patches</div>}
      {filtered.map((patch) => (
        <PatchRow key={patch.id} rpcClient={rpcClient} patch={patch} />
      ))}
    </CollapsibleSection>
  );
}

// ─── Main StateInspector ─────────────────────────────────────────────────────

export function StateInspector({ rpcClient }: Props) {
  const [filter, setFilter] = useState("");

  return (
    <div style={{ padding: 8, color: "#e5e7eb", fontSize: 12, overflowY: "auto", height: "100%" }}>
      <input
        type="text"
        placeholder="Filter by name..."
        value={filter}
        onChange={(e) => setFilter(e.target.value)}
        style={{
          width: "100%",
          padding: "4px 8px",
          fontSize: 12,
          borderRadius: 4,
          border: "1px solid #374151",
          background: "#0b1220",
          color: "#e5e7eb",
          marginBottom: 8,
          boxSizing: "border-box",
        }}
      />
      <EnvironmentSection rpcClient={rpcClient} />
      <FramesSection rpcClient={rpcClient} filter={filter} />
      <ConnectionsSection rpcClient={rpcClient} filter={filter} />
      <PowerNetworksSection />
      <PatchesSection rpcClient={rpcClient} filter={filter} />
      <EntitiesSection rpcClient={rpcClient} filter={filter} />
    </div>
  );
}
