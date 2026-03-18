import { useEffect, useMemo } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import type { PowerNetworkDTO } from "../../rpc/types";
import { classifyStatus, classifyTrend, netAvailable, netProduction } from "../../utils/power";
import { Panel } from "./Panel";
import { TimeSeriesChart } from "../charts/TimeSeriesChart";

type StatusPillProps = {
  net: PowerNetworkDTO;
};

function StatusPill({ net }: StatusPillProps) {
  const status = classifyStatus(net);
  const trend = classifyTrend(net.history.total ?? net.history.production ?? []);
  const trendSymbol = trend === "up" ? "↑" : trend === "down" ? "↓" : "→";

  let background = "#14532d";
  if (status === "draining") background = "#92400e";
  if (status === "brownout") background = "#7f1d1d";

  return (
    <span
      style={{
        fontSize: 11,
        borderRadius: 999,
        padding: "2px 8px",
        background,
        display: "inline-flex",
        alignItems: "center",
        gap: 4,
      }}
    >
      <span>{status}</span>
      <span style={{ opacity: 0.8 }}>{trendSymbol}</span>
    </span>
  );
}

type PowerHistoryChartProps = {
  net: PowerNetworkDTO;
};

function PowerHistoryChart({ net }: PowerHistoryChartProps) {
  const data = useMemo(() => {
    const p = net.history.production ?? [];
    const t = net.history.total ?? [];
    const c = net.history.consumption ?? [];
    const n = Math.max(p.length, t.length, c.length);
    if (n === 0) return [];
    const start = Math.max(0, n - 200);
    const out: Array<{ i: number; production: number; total: number; consumption: number }> = [];
    for (let idx = start; idx < n; idx += 1) {
      out.push({
        i: idx,
        production: p[idx] ?? 0,
        total: t[idx] ?? 0,
        consumption: c[idx] ?? 0,
      });
    }
    return out;
  }, [net.history]);

  if (data.length === 0) return null;

  return (
    <div style={{ marginTop: 6 }}>
      <div style={{ fontSize: 11, color: "#9ca3af", marginBottom: 4 }}>Production / Total / Consumption</div>
      <TimeSeriesChart
        data={data}
        xKey="i"
        series={[
          { key: "production", label: "Production", color: "#22c55e" },
          { key: "total", label: "Total", color: "#60a5fa" },
          { key: "consumption", label: "Consumption", color: "#ef4444" },
        ]}
        height={150}
      />
    </div>
  );
}

type Props = {
  rpcClient: RpcClient;
};

export function PowerPanel({ rpcClient }: Props) {
  const networks = useGameStore((s) => s.powerNetworks);
  const refreshPowerNetworks = useGameStore((s) => s.refreshPowerNetworks);
  const supported = isFeatureSupported(capabilityMethods.powerPanel);

  useEffect(() => {
    if (!supported) return;
    void refreshPowerNetworks(rpcClient);
    const id = setInterval(() => {
      void refreshPowerNetworks(rpcClient);
    }, 500);
    return () => clearInterval(id);
  }, [refreshPowerNetworks, rpcClient, supported]);

  return (
    <Panel title="Power Networks" unsupported={supported ? undefined : "power panel"}>
      <div style={{ display: "flex", flexDirection: "column", gap: 8, fontSize: 12 }}>
        {networks.length === 0 ? (
          <div style={{ color: "#9ca3af" }}>No power info available</div>
        ) : (
          networks.map((network, index) => {
            const netProd = netProduction(network);
            const netAvail = netAvailable(network);
            const name = network.name ?? `Network ${index + 1}`;
            return (
              <div
                key={index}
                style={{
                  border: "1px solid #1f2937",
                  borderRadius: 6,
                  padding: 8,
                  background: "#020617",
                  display: "grid",
                  gap: 6,
                }}
              >
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", gap: 8 }}>
                  <div style={{ fontSize: 12, fontWeight: 600 }}>{name}</div>
                  <StatusPill net={network} />
                </div>
                <div
                  style={{
                    display: "grid",
                    gridTemplateColumns: "1fr 1fr",
                    columnGap: 12,
                    rowGap: 2,
                  }}
                >
                  <div>frames: {network.frames.length}</div>
                  <div>production: {network.production.toFixed(2)}</div>
                  <div>consumption: {network.consumption.toFixed(2)}</div>
                  <div>accumulated: {network.accumulated.toFixed(2)}</div>
                  {typeof network.accumulated_available === "number" && (
                    <div>accumulated available: {network.accumulated_available.toFixed(2)}</div>
                  )}
                  {typeof network.battery_count === "number" && <div>battery count: {network.battery_count}</div>}
                  <div>net production: {netProd.toFixed(2)}</div>
                  <div>net available: {netAvail.toFixed(2)}</div>
                </div>
                <PowerHistoryChart net={network} />
              </div>
            );
          })
        )}
      </div>
    </Panel>
  );
}
