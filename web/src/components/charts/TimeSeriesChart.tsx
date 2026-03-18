import { ResponsiveContainer, LineChart, Line, XAxis, YAxis, Tooltip, Legend } from "recharts";

type SeriesConfig = {
  key: string;
  label: string;
  color: string;
};

type Props = {
  data: Array<Record<string, number>>;
  xKey: string;
  series: SeriesConfig[];
  height?: number;
};

export function TimeSeriesChart({ data, xKey, series, height = 80 }: Props) {
  if (!data.length || !series.length) return null;

  return (
    <ResponsiveContainer width="100%" height={height}>
      <LineChart data={data}>
        <XAxis
          dataKey={xKey}
          tick={{ fontSize: 10, fill: "#9ca3af" }}
          tickLine={{ stroke: "#4b5563" }}
          axisLine={{ stroke: "#4b5563" }}
        />
        <YAxis
          tick={{ fontSize: 10, fill: "#9ca3af" }}
          tickLine={{ stroke: "#4b5563" }}
          axisLine={{ stroke: "#4b5563" }}
          domain={["auto", "auto"]}
        />
        <Tooltip
          contentStyle={{ background: "#111827", border: "1px solid #374151", fontSize: 11 }}
          labelStyle={{ color: "#9ca3af" }}
        />
        <Legend
          wrapperStyle={{ fontSize: 10, paddingTop: 4 }}
          formatter={(value) => <span style={{ color: "#e5e7eb" }}>{value}</span>}
        />
        {series.map((s) => (
          <Line
            key={s.key}
            type="monotone"
            dataKey={s.key}
            name={s.label}
            stroke={s.color}
            dot={false}
            strokeWidth={1.5}
            isAnimationActive={false}
          />
        ))}
      </LineChart>
    </ResponsiveContainer>
  );
}

