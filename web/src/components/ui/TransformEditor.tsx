import { NumberField } from "./NumberField";

type Props = {
  x: number;
  y: number;
  onChange: (x: number, y: number) => void;
};

export function TransformEditor({ x, y, onChange }: Props) {
  return (
    <div style={{ display: "flex", gap: 8 }}>
      <NumberField label="X" value={x} onChange={(v) => onChange(v, y)} labelWidth={20} />
      <NumberField label="Y" value={y} onChange={(v) => onChange(x, v)} labelWidth={20} />
    </div>
  );
}
