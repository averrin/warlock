import { useCallback, useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { usePatchStore, Patch } from "../../stores/patches";
import type { ConnectionDTO, FrameDTO, PowerNetworkDTO } from "../../rpc/types";
import { CollapsibleSection, NumberField, TransformEditor, inputStyle, smallBtnStyle } from "../ui";
import { FramePanel } from "../frame";

type Props = {
  rpcClient: RpcClient;
};

const CONNECTION_TYPES = ["POWER", "DATA", "CONVEYOR"] as const;

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
  const createFrameAt = useGameStore((s) => s.createFrameAt);
  const [creating, setCreating] = useState(false);
  const [newName, setNewName] = useState("New Frame");
  const [expandedFrames, setExpandedFrames] = useState<Record<number, 0 | 1 | 2>>({});

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
        {filtered.map((frame) => (
          <FramePanel
            key={frame.id}
            frame={frame}
            rpcClient={rpcClient}
            level={expandedFrames[frame.id] ?? 0}
            onLevelChange={(lvl) => setExpandedFrames((prev) => ({ ...prev, [frame.id]: lvl }))}
          />
        ))}
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
  const sourceName = frames.find((f) => f.id === conn.source)?.name ?? String(conn.source);
  const targetName = frames.find((f) => f.id === conn.target)?.name ?? String(conn.target);

  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "2px 0" }}>
      <span style={{ color: "#e5e7eb" }}>
        #{conn.id} {sourceName} → {targetName}
      </span>
      <span style={{ color: "#9ca3af", fontSize: 10 }}>[{conn.type}]</span>
      <button
        type="button"
        style={{ ...smallBtnStyle, color: "#ef4444" }}
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
  const [newType, setNewType] = useState<"POWER" | "DATA" | "CONVEYOR">("POWER");

  const filtered = useMemo(() => {
    if (!filter) return connections;
    const lower = filter.toLowerCase();
    return connections.filter(
      (c) =>
        String(c.id).includes(lower) ||
        String(c.source).includes(lower) ||
        String(c.target).includes(lower) ||
        c.type.toLowerCase().includes(lower),
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
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() => {
              const s = parseInt(newSource, 10);
              const t = parseInt(newTarget, 10);
              if (!Number.isNaN(s) && !Number.isNaN(t)) {
                createConnection(rpcClient, s, t, newType);
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
        <PowerNetworkRow key={net.name ?? i} net={net} />
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
    </div>
  );
}
