type Props = {
  label: string;
  value: string | undefined;
  options: readonly string[];
  onChange: (v: string) => void;
  labelWidth?: number;
  disabled?: boolean;
};

export function SelectField({ label, value, options, onChange, labelWidth = 90, disabled }: Props) {
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
      <span style={{ minWidth: labelWidth, color: "#9ca3af" }}>{label}:</span>
      <select
        value={value ?? ""}
        onChange={(e) => onChange(e.target.value)}
        disabled={disabled}
        style={{
          fontSize: 11,
          borderRadius: 3,
          border: "1px solid #374151",
          background: "#0b1220",
          color: "#e5e7eb",
          padding: "1px 4px",
        }}
      >
        {options.map((opt) => (
          <option key={opt} value={opt}>{opt}</option>
        ))}
      </select>
    </div>
  );
}
