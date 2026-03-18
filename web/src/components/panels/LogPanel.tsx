import { useEffect, useRef, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

type LogEntry = {
  ts: string;
  source: string;
  status?: string;
  message?: string;
  error?: string;
  args?: Record<string, unknown>;
};

const STATE_NAMES: Record<number, string> = {
  0: "DEACTIVATED",
  1: "ACTIVATING",
  2: "ACTIVE",
  3: "DEACTIVATING",
  4: "COMP_ERROR",
  5: "DESTROYED",
  6: "BLOCKED",
  7: "BROKEN",
};

function stateName(v: unknown): string {
  if (typeof v === "number" && STATE_NAMES[v]) return STATE_NAMES[v];
  return String(v);
}

/** Pick the right color for a log entry */
function entryColor(e: LogEntry): string {
  if (e.error || e.status === "error") return "#fca5a5"; // red
  if (e.source === "lua.print") return "#a5f3fc"; // cyan — user print output
  if (e.source === "lua.compile" && e.status === "ok") return "#86efac"; // green
  if (e.source === "component.state") return "#fde68a"; // amber
  if (e.status === "queued" || e.status === "starting") return "#bfdbfe"; // blue
  return "#e5e7eb"; // gray
}

/** Format args into a human-readable string based on source type */
function formatArgs(e: LogEntry): string | null {
  const a = e.args;
  if (!a) return null;

  if (e.source === "lua.print") {
    const fid = a.frame_id;
    const msg = a.message;
    return `[frame:${fid}] ${msg}`;
  }

  if (e.source === "lua.compile" || e.source === "lua.runtime") {
    const parts: string[] = [];
    if (a.frame_id !== undefined) parts.push(`frame:${a.frame_id}`);
    if (a.function) parts.push(`fn:${String(a.function)}`);
    if (a.error) parts.push(String(a.error));
    if (a.prev_state !== undefined && a.new_state !== undefined) {
      parts.push(`${stateName(a.prev_state)} → ${stateName(a.new_state)}`);
    }
    return parts.length > 0 ? parts.join(" | ") : null;
  }

  if (e.source === "component.state") {
    const name = a.component_name ?? `comp:${a.component_id}`;
    const transition = `${stateName(a.prev_state)} → ${stateName(a.new_state)}`;
    return `${name} ${transition} (frame:${a.frame_id})`;
  }

  try {
    return JSON.stringify(a);
  } catch {
    return "[unserializable]";
  }
}

export function LogPanel({ rpcClient }: Props) {
  const [entries, setEntries] = useState<LogEntry[]>([]);
  const [filter, setFilter] = useState<string>("all");
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const off = rpcClient.on("web.log", (payload) => {
      if (!payload || typeof payload !== "object") {
        return;
      }
      const params = payload as Record<string, unknown>;
      const now = new Date();
      const entry: LogEntry = {
        ts: now.toISOString().slice(11, 23), // HH:MM:SS.mmm
        source: typeof params.source === "string" ? params.source : "rpc",
        status: typeof params.status === "string" ? params.status : undefined,
        message: typeof params.message === "string" ? params.message : undefined,
        error: typeof params.error === "string" ? params.error : undefined,
        args: params.args as Record<string, unknown> | undefined,
      };
      setEntries((current) => [...current.slice(-499), entry]);
    });
    return () => {
      off();
    };
  }, [rpcClient]);

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    el.scrollTop = el.scrollHeight;
  }, [entries]);

  const filtered =
    filter === "all"
      ? entries
      : filter === "errors"
        ? entries.filter((e) => e.status === "error" || !!e.error)
        : filter === "lua"
          ? entries.filter((e) => e.source.startsWith("lua."))
          : filter === "state"
            ? entries.filter((e) => e.source === "component.state")
            : entries;

  return (
    <Panel title="Log">
      <div style={{ display: "flex", gap: 4, marginBottom: 4, flexWrap: "wrap" }}>
        {(["all", "errors", "lua", "state"] as const).map((f) => (
          <button
            key={f}
            onClick={() => setFilter(f)}
            style={{
              fontSize: 10,
              padding: "2px 8px",
              background: filter === f ? "#1e3a5f" : "#111827",
              color: filter === f ? "#93c5fd" : "#6b7280",
              border: "1px solid",
              borderColor: filter === f ? "#2563eb" : "#1f2937",
              borderRadius: 4,
              cursor: "pointer",
            }}
          >
            {f === "all" ? "All" : f === "errors" ? "Errors" : f === "lua" ? "Lua" : "State"}
          </button>
        ))}
        <button
          onClick={() => setEntries([])}
          style={{
            fontSize: 10,
            padding: "2px 8px",
            marginLeft: "auto",
            background: "#111827",
            color: "#6b7280",
            border: "1px solid #1f2937",
            borderRadius: 4,
            cursor: "pointer",
          }}
        >
          Clear
        </button>
      </div>
      <div
        ref={containerRef}
        style={{
          fontFamily:
            "ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, Liberation Mono, Courier New, monospace",
          fontSize: 11,
          background: "#020617",
          borderRadius: 4,
          border: "1px solid #111827",
          padding: 6,
          maxHeight: 260,
          overflow: "auto",
        }}
      >
        {filtered.length === 0 ? (
          <div style={{ color: "#6b7280" }}>No log messages yet.</div>
        ) : (
          filtered.map((e, idx) => {
            const color = entryColor(e);
            const detail = formatArgs(e);
            return (
              <div key={idx} style={{ color }}>
                <span style={{ color: "#4b5563" }}>[{e.ts}]</span>{" "}
                <span style={{ color: "#60a5fa" }}>{e.source}</span>
                {e.status ? <span> ({e.status})</span> : null}
                {e.message ? <span>: {e.message}</span> : null}
                {e.error ? <span>: {e.error}</span> : null}
                {detail ? <span> {detail}</span> : null}
              </div>
            );
          })
        )}
      </div>
    </Panel>
  );
}
