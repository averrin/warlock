import { useCallback, useMemo, useState, type CSSProperties } from "react";
import toast from "react-hot-toast";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { useRecipeStore } from "../../stores/recipes";
import { NumberField, SelectField, TextField } from "../ui";

const CONVEYOR_CONNECTOR_NAME = "Conveyor Connector";
const CONVEYOR_MODE_OPTIONS = ["SEND", "RECEIVE"] as const;

const targetSelectStyle: CSSProperties = {
  fontSize: 11,
  borderRadius: 3,
  border: "1px solid #374151",
  background: "#0b1220",
  color: "#e5e7eb",
  padding: "1px 4px",
};

const BUILTIN_MODIFIERS = ["Overheat", "Freeze", "Jitter"] as const;

const chipStyle: CSSProperties = {
  fontSize: 10,
  color: "#fbbf24",
  background: "#1c1500",
  border: "1px solid #3b2a00",
  borderRadius: 3,
  padding: "0 4px",
  display: "inline-flex",
  alignItems: "center",
  gap: 4,
};

function ModifiersIcon() {
  return (
    <svg
      width="14"
      height="14"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden
    >
      <line x1="4" y1="21" x2="4" y2="14" />
      <line x1="4" y1="10" x2="4" y2="3" />
      <line x1="12" y1="21" x2="12" y2="12" />
      <line x1="12" y1="8" x2="12" y2="3" />
      <line x1="20" y1="21" x2="20" y2="16" />
      <line x1="20" y1="12" x2="20" y2="3" />
      <line x1="1" y1="14" x2="7" y2="14" />
      <line x1="9" y1="8" x2="15" y2="8" />
      <line x1="17" y1="16" x2="23" y2="16" />
    </svg>
  );
}

type AttributeValue = string | number | boolean | unknown;

// Full AttributeDTO shape from the backend
interface AttributeDTO {
  title?: string;
  type?: string;
  base_value: string | number | boolean;
  final_value?: string | number | boolean;
  modifiers?: string[];
  target_filter?: string;
}

function isAttributeDTO(v: unknown): v is AttributeDTO {
  return (
    typeof v === "object" &&
    v !== null &&
    "base_value" in v
  );
}

// Resolve the effective type string: from DTO type field or inferred from value
function resolveType(val: unknown): "float" | "int" | "bool" | "string" {
  if (isAttributeDTO(val)) {
    const t = (val.type ?? "").toUpperCase();
    if (t === "FLOAT") return "float";
    if (t === "INT") return "int";
    if (t === "BOOL") return "bool";
    if (t === "STRING") return "string";
    // Fallback to inferred type from base_value
    return resolveType(val.base_value);
  }
  if (typeof val === "number") return Number.isInteger(val) ? "int" : "float";
  if (typeof val === "boolean") return "bool";
  return "string";
}

type SingleAttributeProps = {
  attrKey: string;
  val: AttributeValue;
  frameId: number;
  componentId: number;
  componentName: string;
  frameComponents: ComponentDTO[];
  rpcClient: RpcClient;
  updateAttribute: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    key: string,
    value: string | number | boolean,
  ) => Promise<void>;
  setAttributeModifiers: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    key: string,
    modifiers: string[],
  ) => Promise<void>;
};

function AttributeModifiersInspector({
  attrKey,
  frameId,
  componentId,
  modifiers,
  canEdit,
  rpcClient,
  setAttributeModifiers,
}: {
  attrKey: string;
  frameId: number;
  componentId: number;
  modifiers: string[];
  canEdit: boolean;
  rpcClient: RpcClient;
  setAttributeModifiers: SingleAttributeProps["setAttributeModifiers"];
}) {
  const recipes = useRecipeStore((s) => s.recipes);
  const [busy, setBusy] = useState(false);

  const recipeNames = useMemo(() => {
    const out: string[] = [];
    for (const r of recipes) {
      const pc = r.power_cost;
      if (pc === undefined) {
        out.push(r.name);
      } else if (pc > 0) {
        out.push(r.name);
      }
    }
    return [...new Set(out)].sort((a, b) => a.localeCompare(b));
  }, [recipes]);

  const addOptions = useMemo(() => {
    const have = new Set(modifiers);
    const opts: string[] = [];
    for (const b of BUILTIN_MODIFIERS) {
      if (!have.has(b)) opts.push(b);
    }
    for (const n of recipeNames) {
      if (!have.has(n)) opts.push(n);
    }
    return opts;
  }, [modifiers, recipeNames]);

  const apply = useCallback(
    async (next: string[]) => {
      setBusy(true);
      try {
        await setAttributeModifiers(rpcClient, frameId, componentId, attrKey, next);
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        toast.error(msg || "Failed to update modifiers");
      } finally {
        setBusy(false);
      }
    },
    [attrKey, componentId, frameId, rpcClient, setAttributeModifiers],
  );

  const move = (from: number, to: number) => {
    if (to < 0 || to >= modifiers.length) return;
    const next = [...modifiers];
    const [x] = next.splice(from, 1);
    next.splice(to, 0, x);
    void apply(next);
  };

  if (!canEdit) {
    if (modifiers.length === 0) return null;
    return (
      <div style={{ display: "flex", flexDirection: "column", gap: 4, paddingLeft: 94 }}>
        <span style={{ fontSize: 10, color: "#6b7280" }}>Modifiers (view only)</span>
        <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
          {modifiers.map((mod, i) => (
            <span key={`${mod}-${i}`} style={chipStyle}>
              {mod}
            </span>
          ))}
        </div>
      </div>
    );
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 6, paddingLeft: 94 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8, flexWrap: "wrap" }}>
        <span style={{ fontSize: 10, color: "#9ca3af" }}>Modifiers</span>
        <select
          key={modifiers.join("|")}
          defaultValue=""
          disabled={busy || addOptions.length === 0}
          onChange={(e) => {
            const v = e.target.value;
            if (!v) return;
            void apply([...modifiers, v]);
          }}
          style={{ ...targetSelectStyle, minWidth: 140 }}
        >
          <option value="">Add modifier…</option>
          {addOptions.map((n) => (
            <option key={n} value={n}>
              {n}
            </option>
          ))}
        </select>
        {modifiers.length > 0 && (
          <button
            type="button"
            disabled={busy}
            onClick={() => void apply([])}
            style={{
              fontSize: 10,
              padding: "1px 6px",
              borderRadius: 3,
              border: "1px solid #4b5563",
              background: "#111827",
              color: "#d1d5db",
              cursor: busy ? "not-allowed" : "pointer",
            }}
          >
            Clear all
          </button>
        )}
      </div>
      {modifiers.length > 0 && (
        <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
          {modifiers.map((mod, i) => (
            <div key={`${mod}-${i}`} style={{ display: "flex", alignItems: "center", gap: 4 }}>
              <button
                type="button"
                disabled={busy || i === 0}
                title="Move up"
                onClick={() => move(i, i - 1)}
                style={{
                  fontSize: 10,
                  padding: "0 4px",
                  borderRadius: 3,
                  border: "1px solid #374151",
                  background: "#0b1220",
                  color: "#9ca3af",
                  cursor: busy || i === 0 ? "not-allowed" : "pointer",
                }}
              >
                ↑
              </button>
              <button
                type="button"
                disabled={busy || i === modifiers.length - 1}
                title="Move down"
                onClick={() => move(i, i + 1)}
                style={{
                  fontSize: 10,
                  padding: "0 4px",
                  borderRadius: 3,
                  border: "1px solid #374151",
                  background: "#0b1220",
                  color: "#9ca3af",
                  cursor: busy || i === modifiers.length - 1 ? "not-allowed" : "pointer",
                }}
              >
                ↓
              </button>
              <span style={chipStyle}>
                {mod}
                <button
                  type="button"
                  disabled={busy}
                  title="Remove"
                  onClick={() => void apply(modifiers.filter((_, j) => j !== i))}
                  style={{
                    border: "none",
                    background: "transparent",
                    color: "#fbbf24",
                    cursor: busy ? "not-allowed" : "pointer",
                    padding: 0,
                    lineHeight: 1,
                    fontSize: 11,
                  }}
                >
                  ×
                </button>
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

function SingleAttribute({
  attrKey,
  val,
  frameId,
  componentId,
  componentName,
  frameComponents,
  rpcClient,
  updateAttribute,
  setAttributeModifiers,
}: SingleAttributeProps) {
  const [modifiersOpen, setModifiersOpen] = useState(false);
  const isDTO = isAttributeDTO(val);
  const rawValue = isDTO ? val.base_value : (val as string | number | boolean);
  const finalValue = isDTO ? val.final_value : undefined;
  const modifiers = isDTO ? (val.modifiers ?? []) : [];
  const label = isDTO && val.title ? val.title : attrKey;
  const type = resolveType(val);

  const hasModifierEffect =
    finalValue !== undefined && finalValue !== rawValue;

  const onChange = (newVal: string | number | boolean) =>
    void updateAttribute(rpcClient, frameId, componentId, attrKey, newVal);

  const isConveyorModeSelect =
    componentName === CONVEYOR_CONNECTOR_NAME && attrKey === "mode" && type === "string";
  const modeStr =
    typeof rawValue === "string" && rawValue.length > 0 ? rawValue : "SEND";
  const modeOptions = !(CONVEYOR_MODE_OPTIONS as readonly string[]).includes(modeStr)
    ? [modeStr, ...CONVEYOR_MODE_OPTIONS]
    : [...CONVEYOR_MODE_OPTIONS];

  const canEditModifiers = isDTO && type === "float";
  const showModifiersTrigger = canEditModifiers || modifiers.length > 0;

  const targetFilter = isDTO ? val.target_filter : undefined;
  const isTargetComponentSelect = !!targetFilter && type === "int";
  const targetIdRaw = typeof rawValue === "number" ? rawValue : Number(rawValue);
  const targetId = Number.isFinite(targetIdRaw) ? Math.trunc(targetIdRaw) : -1;
  const targetCandidates = frameComponents.filter((c) => {
    if (c.id === componentId) return false;
    if (!targetFilter) return true;
    return c.type === targetFilter;
  });
  const targetIds = new Set(targetCandidates.map((c) => c.id));
  const orphanTarget = targetId >= 0 && !targetIds.has(targetId);

  return (
    <div style={{ display: "grid", gap: 2 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
        {isConveyorModeSelect ? (
          <SelectField
            label={label}
            value={modeStr}
            options={modeOptions}
            onChange={(v) => onChange(v)}
          />
        ) : isTargetComponentSelect ? (
          <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
            <span style={{ minWidth: 90, fontSize: 11, color: "#9ca3af" }}>{label}:</span>
            <select
              value={targetId < 0 ? "" : String(targetId)}
              onChange={(e) => {
                const v = e.target.value;
                onChange(v === "" ? -1 : Number(v));
              }}
              style={targetSelectStyle}
            >
              <option value="">None</option>
              {orphanTarget && (
                <option value={String(targetId)}>#{targetId} (unavailable)</option>
              )}
              {targetCandidates.map((c) => (
                <option key={c.id} value={String(c.id)}>
                  {c.name} (#{c.id})
                </option>
              ))}
            </select>
          </div>
        ) : type === "bool" ? (
          <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
            <span style={{ minWidth: 90, fontSize: 11, color: "#9ca3af" }}>{label}:</span>
            <input
              type="checkbox"
              checked={rawValue as boolean}
              onChange={(e) => onChange(e.target.checked)}
            />
          </div>
        ) : type === "float" || type === "int" ? (
          <NumberField
            label={label}
            value={rawValue as number}
            onChange={onChange}
          />
        ) : (
          <TextField
            label={label}
            value={rawValue as string}
            onChange={onChange}
          />
        )}

        {/* Final value indicator when modifiers alter the base */}
        {hasModifierEffect && (
          <span
            title="Final value after modifiers"
            style={{ fontSize: 10, color: "#60a5fa", whiteSpace: "nowrap" }}
          >
            → {String(finalValue)}
          </span>
        )}
        {showModifiersTrigger && (
          <button
            type="button"
            title={modifiersOpen ? "Hide modifiers" : "Modifiers"}
            aria-label="Modifiers"
            aria-expanded={modifiersOpen}
            onClick={() => setModifiersOpen((o) => !o)}
            style={{
              position: "relative",
              display: "inline-flex",
              alignItems: "center",
              justifyContent: "center",
              width: 22,
              height: 22,
              padding: 0,
              flexShrink: 0,
              borderRadius: 4,
              border: modifiersOpen ? "1px solid #4b5563" : "1px solid transparent",
              background: modifiersOpen ? "#1f2937" : "transparent",
              color: modifiersOpen ? "#e5e7eb" : "#9ca3af",
              cursor: "pointer",
            }}
          >
            <ModifiersIcon />
            {modifiers.length > 0 && !modifiersOpen && (
              <span
                style={{
                  position: "absolute",
                  top: 2,
                  right: 2,
                  width: 6,
                  height: 6,
                  borderRadius: "50%",
                  background: "#fbbf24",
                  border: "1px solid #0b1220",
                }}
              />
            )}
          </button>
        )}
      </div>

      {modifiersOpen && (
        <AttributeModifiersInspector
          attrKey={attrKey}
          frameId={frameId}
          componentId={componentId}
          modifiers={modifiers}
          canEdit={canEditModifiers}
          rpcClient={rpcClient}
          setAttributeModifiers={setAttributeModifiers}
        />
      )}
    </div>
  );
}

type Props = {
  frameId: number;
  componentId: number;
  componentName: string;
  attributes: Record<string, AttributeValue> | undefined;
  rpcClient: RpcClient;
};

export function ComponentAttributes({
  frameId,
  componentId,
  componentName,
  attributes,
  rpcClient,
}: Props) {
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);
  const setComponentAttributeModifiers = useGameStore((s) => s.setComponentAttributeModifiers);
  const controllable = useGameStore((s) => s.isFrameControllable)(frameId);
  const frameComponents = useGameStore(
    (s) => s.frames.find((f) => f.id === frameId)?.components ?? [],
  );

  if (!attributes) return null;

  const entries = Object.entries(attributes).filter(([key]) => key !== "code");

  if (entries.length === 0) return null;

  return (
    <div style={{ display: "grid", gap: 4 }}>
      <div style={{ fontSize: 11, fontWeight: 600, color: "#9ca3af" }}>Attributes</div>
      {!controllable && (
        <div style={{ fontSize: 10, color: "#f59e0b", marginBottom: 2 }}>Outside control zone — read only</div>
      )}
      {entries.map(([key, val]) => (
        <SingleAttribute
          key={key}
          attrKey={key}
          val={val}
          frameId={frameId}
          componentId={componentId}
          componentName={componentName}
          frameComponents={frameComponents}
          rpcClient={rpcClient}
          updateAttribute={controllable ? updateComponentAttribute : (async () => {}) as typeof updateComponentAttribute}
          setAttributeModifiers={controllable ? setComponentAttributeModifiers : (async () => {}) as typeof setComponentAttributeModifiers}
        />
      ))}
    </div>
  );
}
