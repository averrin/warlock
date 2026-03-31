export function GaugeIndicator({ value, min, max, label, color, unit, precision }: {
  value: number; min: number; max: number; label: string;
  color: string; unit?: string; precision?: number;
}) {
  const frac = max > min ? Math.max(0, Math.min(1, (value - min) / (max - min))) : 0;
  const shown = (precision !== undefined ? value.toFixed(precision) : String(value))
    + (unit ? ` ${unit}` : "");

  // SVG semicircle arc parameters
  const cx = 50;
  const cy = 46;
  const r = 34;
  const strokeWidth = 8;

  // Arc from 180° (left) to 0° (right) — semicircle
  const startAngle = Math.PI;
  const endAngle = 0;
  const totalAngle = startAngle - endAngle; // PI radians

  // Background arc (full semicircle)
  const bgX1 = cx + r * Math.cos(startAngle);
  const bgY1 = cy - r * Math.sin(startAngle);
  const bgX2 = cx + r * Math.cos(endAngle);
  const bgY2 = cy - r * Math.sin(endAngle);
  const bgPath = `M ${bgX1} ${bgY1} A ${r} ${r} 0 0 1 ${bgX2} ${bgY2}`;

  // Value arc
  const valueAngle = startAngle - frac * totalAngle;
  const vX = cx + r * Math.cos(valueAngle);
  const vY = cy - r * Math.sin(valueAngle);
  const largeArc = frac > 0.5 ? 1 : 0;
  const valuePath = frac > 0
    ? `M ${bgX1} ${bgY1} A ${r} ${r} 0 ${largeArc} 1 ${vX} ${vY}`
    : "";

  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      padding: "4px 8px",
      background: "#1e293b",
      borderRadius: 4,
      borderLeft: `4px solid ${color || "#3b82f6"}`,
    }}>
      <svg width="100" height="56" viewBox="0 0 100 56">
        {/* Background arc */}
        <path
          d={bgPath}
          fill="none"
          stroke="#374151"
          strokeWidth={strokeWidth}
          strokeLinecap="round"
        />
        {/* Value arc */}
        {valuePath && (
          <path
            d={valuePath}
            fill="none"
            stroke={color || "#3b82f6"}
            strokeWidth={strokeWidth}
            strokeLinecap="round"
            style={{ transition: "d 0.15s ease" }}
          />
        )}
        {/* Center value text */}
        <text
          x={cx}
          y={cy - 4}
          textAnchor="middle"
          dominantBaseline="middle"
          fill="#e5e7eb"
          fontSize="12"
          fontWeight="600"
          fontFamily="ui-monospace, monospace"
        >
          {shown}
        </text>
      </svg>
      <span style={{ fontSize: 11, color: "#94a3b8", marginTop: -2 }}>{label}</span>
    </div>
  );
}
