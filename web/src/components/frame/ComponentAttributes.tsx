import { useCallback, useEffect, useMemo, useState, type CSSProperties, type ReactNode } from "react";
import toast from "react-hot-toast";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO, FrameDTO, InspectorMeta } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { useRecipeStore } from "../../stores/recipes";
import { NumberField, TextField } from "../ui";

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
  inspector?: InspectorMeta;
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

function attrNumericFinalOrBase(entry: unknown): number | undefined {
  if (!isAttributeDTO(entry)) return undefined;
  const fv = entry.final_value;
  if (typeof fv === "number" && Number.isFinite(fv)) return fv;
  const bv = entry.base_value;
  if (typeof bv === "number" && Number.isFinite(bv)) return bv;
  if (typeof fv === "string" || typeof bv === "string") {
    const n = Number(typeof fv === "string" ? fv : bv);
    if (Number.isFinite(n)) return n;
  }
  return undefined;
}

type SingleAttributeProps = {
  attrKey: string;
  val: AttributeValue;
  siblingAttributes: Record<string, AttributeValue>;
  frameId: number;
  componentId: number;
  frameComponents: ComponentDTO[];
  allFrames: FrameDTO[];
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

function clampNum(n: number, lo?: number, hi?: number): number {
  let x = n;
  if (lo !== undefined) x = Math.max(lo, x);
  if (hi !== undefined) x = Math.min(hi, x);
  return x;
}

function formatWithPrecision(n: number, precision?: number): string {
  if (precision === undefined) return String(n);
  return n.toFixed(precision);
}

function normalizeColorHex(s: string): string {
  const t = s.trim();
  if (t.startsWith("#") && t.length >= 4) return t.slice(0, 7);
  if (/^[0-9a-fA-F]{6}$/.test(t)) return `#${t}`;
  return "#000000";
}

function SingleAttribute({
  attrKey,
  val,
  siblingAttributes,
  frameId,
  componentId,
  frameComponents,
  allFrames,
  rpcClient,
  updateAttribute,
  setAttributeModifiers,
}: SingleAttributeProps) {
  const [modifiersOpen, setModifiersOpen] = useState(false);
  const isDTO = isAttributeDTO(val);
  const rawValue = isDTO ? val.base_value : (val as string | number | boolean);
  const finalValue = isDTO ? val.final_value : undefined;
  const modifiers = isDTO ? (val.modifiers ?? []) : [];
  const insp = isDTO ? val.inspector : undefined;
  const label = isDTO && val.title ? val.title : attrKey;
  const displayLabel = insp?.label ?? label;
  const type = resolveType(val);
  const rowLabelColor = insp?.color;
  const labelEl = (
    <span
      style={{
        minWidth: 90,
        fontSize: 11,
        color: rowLabelColor ?? "#9ca3af",
        display: "inline-flex",
        alignItems: "center",
        gap: 6,
      }}
    >
      {insp?.icon ? (
        <img src={`/icons/${insp.icon}`} alt="" style={{ width: 14, height: 14, objectFit: "contain" }} />
      ) : null}
      {displayLabel}:
    </span>
  );

  if (insp?.hidden) return null;

  const hasModifierEffect =
    finalValue !== undefined && finalValue !== rawValue;

  const ro = !!insp?.readonly;
  const loBound = insp?.min;
  const hiBound = insp?.max;
  const unit = insp?.unit ?? "";

  const onChange = (newVal: string | number | boolean) => {
    if (ro) return;
    if (typeof newVal === "number") {
      void updateAttribute(rpcClient, frameId, componentId, attrKey, clampNum(newVal, loBound, hiBound));
      return;
    }
    void updateAttribute(rpcClient, frameId, componentId, attrKey, newVal);
  };

  const canEditModifiers = isDTO && type === "float" && !ro;
  const showModifiersTrigger = canEditModifiers || modifiers.length > 0;

  const w = insp?.widget;
  const targetIdRaw = typeof rawValue === "number" ? rawValue : Number(rawValue);
  const targetId = Number.isFinite(targetIdRaw) ? Math.trunc(targetIdRaw) : -1;

  const isLinkFrame =
    w === "link" && type === "int" && (insp?.link_scope ?? "frame") === "frame";
  const isLinkWorld = w === "link" && type === "int" && insp?.link_scope === "world";
  const linkFilter = insp?.link_filter;

  const frameLinkCandidates = frameComponents.filter((c) => {
    if (c.id === componentId) return false;
    if (!linkFilter) return true;
    return c.type === linkFilter;
  });
  const frameLinkIds = new Set(frameLinkCandidates.map((c) => c.id));
  const orphanFrameLink = isLinkFrame && targetId >= 0 && !frameLinkIds.has(targetId);

  const worldLinkCandidates = allFrames.filter((f) => !linkFilter || f.name === linkFilter);
  const worldLinkIds = new Set(worldLinkCandidates.map((f) => f.entity_id));
  const orphanWorldLink = isLinkWorld && targetId >= 0 && !worldLinkIds.has(targetId);

  const soleFrameLinkId =
    !ro && isLinkFrame && frameLinkCandidates.length === 1 ? frameLinkCandidates[0]!.id : null;
  const soleWorldLinkEntityId =
    !ro && isLinkWorld && worldLinkCandidates.length === 1 ? worldLinkCandidates[0]!.entity_id : null;

  useEffect(() => {
    if (soleFrameLinkId !== null && targetId !== soleFrameLinkId) {
      void updateAttribute(rpcClient, frameId, componentId, attrKey, soleFrameLinkId);
      return;
    }
    if (soleWorldLinkEntityId !== null && targetId !== soleWorldLinkEntityId) {
      void updateAttribute(rpcClient, frameId, componentId, attrKey, soleWorldLinkEntityId);
    }
  }, [
    soleFrameLinkId,
    soleWorldLinkEntityId,
    targetId,
    rpcClient,
    frameId,
    componentId,
    attrKey,
    updateAttribute,
  ]);

  const isSelectWidget = w === "select" && type === "string";
  const selectOptsRaw = insp?.options ?? [];
  const selectOpts =
    typeof rawValue === "string" && rawValue.length > 0 && !selectOptsRaw.includes(rawValue)
      ? [rawValue, ...selectOptsRaw]
      : selectOptsRaw.length > 0
        ? [...selectOptsRaw]
        : typeof rawValue === "string"
          ? [rawValue]
          : [];

  const isProgressWidget = w === "progress" && (type === "float" || type === "int");
  const refMinKey = insp?.min_attr;
  const refMaxKey = insp?.max_attr;
  const refMin =
    refMinKey && siblingAttributes[refMinKey] !== undefined
      ? attrNumericFinalOrBase(siblingAttributes[refMinKey])
      : undefined;
  const refMax =
    refMaxKey && siblingAttributes[refMaxKey] !== undefined
      ? attrNumericFinalOrBase(siblingAttributes[refMaxKey])
      : undefined;
  const pMin = refMin ?? insp?.min ?? 0;
  const pMax = refMax ?? insp?.max ?? 1;
  const progressValueRaw = typeof rawValue === "number" ? rawValue : Number(rawValue);
  const progressFinal =
    finalValue !== undefined
      ? typeof finalValue === "number"
        ? finalValue
        : Number(finalValue)
      : NaN;
  const numForProgress = Number.isFinite(progressFinal) ? progressFinal : progressValueRaw;
  const progressFrac =
    pMax > pMin && Number.isFinite(numForProgress)
      ? clampNum((numForProgress - pMin) / (pMax - pMin), 0, 1)
      : 0;

  const isColorWidget = w === "color" && type === "string";
  const isListWidget = w === "list" && type === "string";

  let mainControl: ReactNode = null;

  if (isSelectWidget && selectOpts.length > 0) {
    const modeStr = typeof rawValue === "string" && rawValue.length > 0 ? rawValue : selectOpts[0]!;
    mainControl = (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <select
          value={modeStr}
          onChange={(e) => onChange(e.target.value)}
          style={targetSelectStyle}
          disabled={ro}
        >
          {selectOpts.map((opt) => (
            <option key={opt} value={opt}>
              {opt}
            </option>
          ))}
        </select>
      </div>
    );
  } else if (isLinkFrame) {
    mainControl = (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <select
          value={targetId < 0 ? "" : String(targetId)}
          onChange={(e) => {
            const v = e.target.value;
            onChange(v === "" ? -1 : Number(v));
          }}
          style={targetSelectStyle}
          disabled={ro}
        >
          <option value="">None</option>
          {orphanFrameLink && (
            <option value={String(targetId)}>#{targetId} (unavailable)</option>
          )}
          {frameLinkCandidates.map((c) => (
            <option key={c.id} value={String(c.id)}>
              {c.name} (#{c.id})
            </option>
          ))}
        </select>
      </div>
    );
  } else if (isLinkWorld) {
    mainControl = (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <select
          value={targetId < 0 ? "" : String(targetId)}
          onChange={(e) => {
            const v = e.target.value;
            onChange(v === "" ? -1 : Number(v));
          }}
          style={targetSelectStyle}
          disabled={ro}
        >
          <option value="">None</option>
          {orphanWorldLink && (
            <option value={String(targetId)}>entity #{targetId} (unavailable)</option>
          )}
          {worldLinkCandidates.map((f) => (
            <option key={f.entity_id} value={String(f.entity_id)}>
              {f.name} (entity #{f.entity_id})
            </option>
          ))}
        </select>
      </div>
    );
  } else if (isProgressWidget) {
    const prec = insp?.precision;
    const shown = Number.isFinite(numForProgress)
      ? formatWithPrecision(numForProgress, prec) + (unit ? ` ${unit}` : "")
      : "-";
    mainControl = (
      <div style={{ display: "flex", flexDirection: "column", gap: 4, padding: "1px 0", minWidth: 160 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
          {labelEl}
          {ro ? (
            <span style={{ color: "#e5e7eb", fontSize: 11 }}>{shown}</span>
          ) : (
            <input
              type="number"
              value={Number.isFinite(numForProgress) ? numForProgress : 0}
              onChange={(e) => {
                const n = parseFloat(e.target.value);
                if (!Number.isNaN(n)) onChange(clampNum(n, pMin, pMax));
              }}
              style={{ ...targetSelectStyle, width: 88 }}
            />
          )}
        </div>
        <div
          style={{
            height: 6,
            borderRadius: 3,
            background: "#1f2937",
            overflow: "hidden",
            border: "1px solid #374151",
          }}
        >
          <div
            style={{
              height: "100%",
              width: `${progressFrac * 100}%`,
              background: rowLabelColor ?? "#3b82f6",
              transition: "width 0.15s ease",
            }}
          />
        </div>
      </div>
    );
  } else if (isColorWidget) {
    const hex = normalizeColorHex(typeof rawValue === "string" ? rawValue : "");
    mainControl = (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <input
          type="color"
          value={hex}
          onChange={(e) => onChange(e.target.value)}
          disabled={ro}
          style={{ width: 36, height: 22, padding: 0, border: "1px solid #374151", borderRadius: 3, background: "#0b1220" }}
        />
        <span style={{ fontSize: 10, color: "#6b7280" }}>{hex}</span>
      </div>
    );
  } else if (isListWidget) {
    const parts = (typeof rawValue === "string" ? rawValue : "")
      .split(",")
      .map((s) => s.trim())
      .filter(Boolean);
    mainControl = (
      <div style={{ display: "flex", flexDirection: "column", gap: 4, padding: "1px 0" }}>
        <div style={{ display: "flex", alignItems: "flex-start", gap: 8 }}>
          {labelEl}
          <ul style={{ margin: 0, paddingLeft: 16, fontSize: 11, color: "#e5e7eb" }}>
            {parts.length === 0 ? <li style={{ color: "#6b7280" }}>(empty)</li> : parts.map((p, i) => <li key={i}>{p}</li>)}
          </ul>
        </div>
      </div>
    );
  } else if (type === "bool") {
    mainControl = (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <input
          type="checkbox"
          checked={rawValue as boolean}
          onChange={(e) => onChange(e.target.checked)}
          disabled={ro}
        />
      </div>
    );
  } else if (type === "float" || type === "int") {
    const n = typeof rawValue === "number" ? rawValue : Number(rawValue);
    const prec = insp?.precision;
    mainControl = ro ? (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <span style={{ color: "#e5e7eb", fontSize: 11 }}>
          {Number.isFinite(n) ? formatWithPrecision(n, prec) : "-"}
          {unit ? ` ${unit}` : ""}
        </span>
      </div>
    ) : (
      <NumberField
        label={displayLabel}
        value={Number.isFinite(n) ? n : 0}
        precision={type === "float" ? prec : undefined}
        onChange={(v) => onChange(clampNum(v, loBound, hiBound))}
      />
    );
  } else {
    mainControl = ro ? (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        {labelEl}
        <span style={{ color: "#e5e7eb", fontSize: 11 }}>{String(rawValue ?? "")}{unit ? ` ${unit}` : ""}</span>
      </div>
    ) : (
      <TextField
        label={displayLabel}
        value={rawValue as string}
        onChange={(v) => onChange(v)}
      />
    );
  }

  return (
    <div style={{ display: "grid", gap: 2 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
        {mainControl}

        {/* Final value indicator when modifiers alter the base */}
        {hasModifierEffect && (
          <span
            title="Final value after modifiers"
            style={{ fontSize: 10, color: "#60a5fa", whiteSpace: "nowrap" }}
          >
            →{" "}
            {type === "float" && finalValue !== undefined && Number.isFinite(Number(finalValue))
              ? formatWithPrecision(Number(finalValue), insp?.precision)
              : String(finalValue)}
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
  attributes: Record<string, AttributeValue> | undefined;
  rpcClient: RpcClient;
};

export function ComponentAttributes({
  frameId,
  componentId,
  attributes,
  rpcClient,
}: Props) {
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);
  const setComponentAttributeModifiers = useGameStore((s) => s.setComponentAttributeModifiers);
  const controllable = useGameStore((s) => s.isFrameControllable)(frameId);
  const allFrames = useGameStore((s) => s.frames);
  const frameComponents = useGameStore(
    (s) => s.frames.find((f) => f.id === frameId)?.components ?? [],
  );

  if (!attributes) return null;

  const entries = Object.entries(attributes).filter(
    ([, val]) => !(isAttributeDTO(val) && val.inspector?.widget === "code"),
  );

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
          siblingAttributes={attributes}
          frameId={frameId}
          componentId={componentId}
          frameComponents={frameComponents}
          allFrames={allFrames}
          rpcClient={rpcClient}
          updateAttribute={controllable ? updateComponentAttribute : (async () => {}) as typeof updateComponentAttribute}
          setAttributeModifiers={controllable ? setComponentAttributeModifiers : (async () => {}) as typeof setComponentAttributeModifiers}
        />
      ))}
    </div>
  );
}
