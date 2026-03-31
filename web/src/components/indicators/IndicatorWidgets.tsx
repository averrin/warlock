import type { CSSProperties } from "react";
import type { IndicatorData } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { GaugeIndicator } from "./GaugeWidget";
import { ChartIndicator } from "./ChartWidget";

function formatValue(n: number, precision?: number): string {
  if (precision !== undefined) return n.toFixed(precision);
  return String(n);
}

function clamp01(n: number, min: number, max: number): number {
  if (max <= min) return 0;
  return Math.max(0, Math.min(1, (n - min) / (max - min)));
}

// --- Text Indicator (default) ---

export function TextIndicator({ label, value, color }: {
  label: string; value: string; color: string;
}) {
  return (
    <div
      style={{
        display: "flex",
        justifyContent: "space-between",
        padding: "4px 8px",
        background: "#1e293b",
        borderRadius: 4,
        borderLeft: `4px solid ${color || "#3b82f6"}`,
        fontSize: 12,
      }}
    >
      <span style={{ color: "#94a3b8" }}>{label}</span>
      <span style={{ color: "#e5e7eb", fontWeight: 600 }}>{value}</span>
    </div>
  );
}

// --- Colored Text Indicator ---

export function ColoredTextIndicator({ label, value, color }: {
  label: string; value: string; color: string;
}) {
  return (
    <div
      style={{
        display: "flex",
        justifyContent: "space-between",
        padding: "4px 8px",
        background: "#1e293b",
        borderRadius: 4,
        borderLeft: `4px solid ${color || "#3b82f6"}`,
        fontSize: 12,
      }}
    >
      <span style={{ color: "#94a3b8" }}>{label}</span>
      <span style={{ color: color || "#e5e7eb", fontWeight: 600 }}>{value}</span>
    </div>
  );
}

// --- Progress Indicator ---

const progressBarOuter: CSSProperties = {
  height: 6,
  borderRadius: 3,
  background: "#1f2937",
  overflow: "hidden",
  border: "1px solid #374151",
};

export function ProgressIndicator({ value, min, max, label, color, unit, precision }: {
  value: number; min: number; max: number; label: string;
  color: string; unit?: string; precision?: number;
}) {
  const frac = clamp01(value, min, max);
  const shown = formatValue(value, precision) + (unit ? ` ${unit}` : "");
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4, padding: "4px 8px", background: "#1e293b", borderRadius: 4, borderLeft: `4px solid ${color || "#3b82f6"}` }}>
      <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12 }}>
        <span style={{ color: "#94a3b8" }}>{label}</span>
        <span style={{ color: "#e5e7eb", fontWeight: 600 }}>{shown}</span>
      </div>
      <div style={progressBarOuter}>
        <div
          style={{
            height: "100%",
            width: `${frac * 100}%`,
            background: color || "#3b82f6",
            transition: "width 0.15s ease",
          }}
        />
      </div>
    </div>
  );
}

// --- Indicator Renderer (dispatcher) ---

export function IndicatorRenderer({ indicator, historyKey }: {
  indicator: IndicatorData;
  historyKey?: string;
}) {
  const meta = indicator.widget_meta;
  const widget = meta?.widget ?? "text";

  switch (widget) {
    case "progress": {
      const num = Number(indicator.value);
      return (
        <ProgressIndicator
          value={Number.isFinite(num) ? num : 0}
          min={meta?.min ?? 0}
          max={meta?.max ?? 100}
          label={indicator.label}
          color={indicator.color}
          unit={meta?.unit}
          precision={meta?.precision}
        />
      );
    }
    case "gauge": {
      const num = Number(indicator.value);
      return (
        <GaugeIndicator
          value={Number.isFinite(num) ? num : 0}
          min={meta?.min ?? 0}
          max={meta?.max ?? 100}
          label={indicator.label}
          color={indicator.color}
          unit={meta?.unit}
          precision={meta?.precision}
        />
      );
    }
    case "chart": {
      return (
        <ChartIndicatorConnected indicator={indicator} historyKey={historyKey ?? ""} />
      );
    }
    case "colored_text":
      return (
        <ColoredTextIndicator
          label={indicator.label}
          value={indicator.value}
          color={indicator.color}
        />
      );
    default:
      return (
        <TextIndicator
          label={indicator.label}
          value={indicator.value}
          color={indicator.color}
        />
      );
  }
}

// Connected chart indicator that reads from store history
function ChartIndicatorConnected({ indicator, historyKey }: {
  indicator: IndicatorData;
  historyKey: string;
}) {
  const data = useGameStore((s) =>
    s.indicatorHistory[historyKey] ?? s.frameIndicatorHistory[historyKey] ?? [],
  );
  return (
    <ChartIndicator
      data={data}
      label={indicator.label}
      color={indicator.color}
    />
  );
}
