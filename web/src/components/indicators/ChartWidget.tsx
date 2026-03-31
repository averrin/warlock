import { ResponsiveContainer, LineChart, Line, YAxis } from "recharts";

export function ChartIndicator({ data, label, color }: {
  data: number[];
  label: string;
  color: string;
}) {
  if (data.length === 0) {
    return (
      <div style={{
        display: "flex",
        justifyContent: "space-between",
        padding: "4px 8px",
        background: "#1e293b",
        borderRadius: 4,
        borderLeft: `4px solid ${color || "#3b82f6"}`,
        fontSize: 12,
      }}>
        <span style={{ color: "#94a3b8" }}>{label}</span>
        <span style={{ color: "#6b7280" }}>Waiting for data...</span>
      </div>
    );
  }

  const chartData = data.map((v, i) => ({ i, v }));
  const latest = data[data.length - 1]!;
  const latestStr = Number.isInteger(latest) ? String(latest) : latest.toFixed(1);

  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      gap: 2,
      padding: "4px 8px",
      background: "#1e293b",
      borderRadius: 4,
      borderLeft: `4px solid ${color || "#3b82f6"}`,
    }}>
      <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12 }}>
        <span style={{ color: "#94a3b8" }}>{label}</span>
        <span style={{ color: "#e5e7eb", fontWeight: 600 }}>{latestStr}</span>
      </div>
      <ResponsiveContainer width="100%" height={40}>
        <LineChart data={chartData}>
          <YAxis domain={["auto", "auto"]} hide />
          <Line
            type="monotone"
            dataKey="v"
            stroke={color || "#3b82f6"}
            dot={false}
            strokeWidth={1.5}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
