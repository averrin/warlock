import { useCallback, useEffect, useMemo, type CSSProperties } from "react";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import type { RpcClient } from "../../rpc/client";
import { Panel } from "./Panel";
import { TimeSeriesChart } from "../charts/TimeSeriesChart";

type Props = {
  rpcClient?: RpcClient;
};

function formatTime(minutes: number): string {
  const h = Math.floor(minutes / 60);
  const m = minutes % 60;
  return `${String(h).padStart(2, "0")}:${String(m).padStart(2, "0")}`;
}

const SPEED_OPTIONS = [
  { label: "Pause", value: 0 },
  { label: "x1", value: 1 },
  { label: "x5", value: 5 },
  { label: "x10", value: 10 },
] as const;

const btnBase: CSSProperties = {
  padding: "2px 8px",
  fontSize: 11,
  border: "1px solid #374151",
  borderRadius: 4,
  cursor: "pointer",
  background: "#1f2937",
  color: "#e5e7eb",
};

const btnActive: CSSProperties = {
  ...btnBase,
  background: "#3b82f6",
  borderColor: "#3b82f6",
  color: "#fff",
};

export function EnvironmentPanel({ rpcClient }: Props) {
  const env = useGameStore((s) => s.environment);
  const history = useGameStore((s) => s.environmentHistory);
  const timeControl = useGameStore((s) => s.timeControl);
  const pauseGame = useGameStore((s) => s.pauseGame);
  const resumeGame = useGameStore((s) => s.resumeGame);
  const setSpeed = useGameStore((s) => s.setSpeed);
  const fetchSpeedState = useGameStore((s) => s.fetchSpeedState);
  const supported = isFeatureSupported(capabilityMethods.environment);
  const speedSupported = isFeatureSupported(capabilityMethods.speedControl);

  useEffect(() => {
    if (!rpcClient) return;
    fetchSpeedState(rpcClient);
  }, [rpcClient, fetchSpeedState]);

  const handleSpeed = useCallback(
    (value: number) => {
      if (!rpcClient) return;
      if (value === 0) {
        pauseGame(rpcClient);
      } else {
        if (timeControl.paused) {
          resumeGame(rpcClient);
        }
        setSpeed(rpcClient, value);
      }
    },
    [rpcClient, timeControl.paused, pauseGame, resumeGame, setSpeed],
  );

  const currentSpeed = timeControl.paused ? 0 : timeControl.multiplier;

  const chartData = useMemo(() => {
    const t = history?.temperature ?? [];
    const a = history?.air_flow ?? [];
    const s = history?.sun ?? [];
    const n = Math.max(t.length, a.length, s.length);
    if (n === 0) return [];
    const start = Math.max(0, n - 600);
    const out: Array<{ i: number; temperature: number; airFlow: number; sun: number }> = [];
    for (let idx = start; idx < n; idx += 1) {
      out.push({
        i: idx,
        temperature: t[idx] ?? 0,
        airFlow: a[idx] ?? 0,
        sun: s[idx] ?? 0,
      });
    }
    return out;
  }, [history]);

  return (
    <Panel title="Environment" unsupported={supported ? undefined : "environment panel"}>
      <div style={{ fontSize: 12, display: "flex", flexDirection: "column", gap: 8 }}>
        {/* Time display */}
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <span data-testid="env-time" style={{ fontSize: 18, fontWeight: 700, fontVariantNumeric: "tabular-nums" }}>
            {env?.minutes != null ? formatTime(env.minutes) : "--:--"}
          </span>
          <span style={{ color: "#9ca3af" }}>
            Day {String(env?.days ?? "-")} · {env?.is_day ? "☀ Day" : "☾ Night"}
          </span>
        </div>

        {/* Speed controls */}
        <div style={{ display: "flex", gap: 4 }}>
          {SPEED_OPTIONS.map((opt) => (
            <button
              key={opt.value}
              aria-label={`speed-${opt.label.toLowerCase()}`}
              style={currentSpeed === opt.value ? btnActive : btnBase}
              onClick={() => handleSpeed(opt.value)}
              disabled={!speedSupported}
              title={!speedSupported ? "Speed control not available" : undefined}
            >
              {opt.label}
            </button>
          ))}
        </div>

        {/* Current metrics (always visible, even when charts are present) */}
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 4, fontSize: 11, color: "#9ca3af" }}>
          <div>
            <div style={{ color: "#e5e7eb", fontWeight: 600 }}>
              {env?.temperature != null ? `${env.temperature.toFixed(1)}°` : "-"}
            </div>
            Temp
          </div>
          <div>
            <div style={{ color: "#e5e7eb", fontWeight: 600 }}>
              {env?.air_flow != null ? env.air_flow.toFixed(1) : "-"}
            </div>
            Airflow
          </div>
          <div>
            <div style={{ color: "#e5e7eb", fontWeight: 600 }}>
              {env?.sun != null ? `${env.sun.toFixed(0)}%` : "-"}
            </div>
            Sun
          </div>
        </div>

        {/* Charts */}
        {chartData.length > 0 && (
          <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
            <div>
              <div style={{ fontSize: 11, color: "#9ca3af", marginBottom: 4 }}>Temperature history</div>
              <TimeSeriesChart
                data={chartData}
                xKey="i"
                series={[{ key: "temperature", label: "Temperature", color: "#f97316" }]}
                height={120}
              />
            </div>

            <div>
              <div style={{ fontSize: 11, color: "#9ca3af", marginBottom: 4 }}>Airflow & Sun history</div>
              <TimeSeriesChart
                data={chartData}
                xKey="i"
                series={[
                  { key: "airFlow", label: "Airflow", color: "#22d3ee" },
                  { key: "sun", label: "Sun", color: "#facc15" },
                ]}
                height={120}
              />
            </div>
          </div>
        )}
      </div>
    </Panel>
  );
}
