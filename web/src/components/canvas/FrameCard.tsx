import { memo, useMemo } from "react";
import type { FrameDTO, ComponentDTO } from "../../rpc/types";
import { STATE_COLORS, EFFECT_LABELS } from "../ui";
import { CELL } from "./connectionGeometry";
import "./FrameCard.css";

/** Frame with computed world position */
export type PlacedFrame = FrameDTO & { _x: number; _y: number };

export interface WiringState {
  type: string;
  isSource: boolean;
  isEligible: boolean;
}

export interface FrameCardProps {
  frame: PlacedFrame;
  cellSize: number;
  isSelected: boolean;
  wiringState: WiringState | null;
  /** When control zones exist and this frame is outside all of them */
  outsideControlZone?: boolean;
  zoom: number;
  onPointerDown: (e: React.PointerEvent, frameId: number) => void;
  onContextMenu: (e: React.MouseEvent, frameId: number) => void;
}

const CONN_CSS_COLOR: Record<string, string> = {
  POWER: "#eab308",
  DATA: "#3b82f6",
  CONVEYOR: "#22c55e",
};

const HEALTH_COLOR: Record<string, string> = {
  ok: "#22c55e",
  warn: "#eab308",
  bad: "#ef4444",
};

const POWER_LABELS: Record<string, string> = {
  surplus: "\u26A1 surplus",
  balanced: "\u26A1 balanced",
  deficit: "\u26A1 deficit",
  offline: "\u26A1 offline",
};

/** Map cells per side (world / {@link CELL}) to a size tier class */
function getSizeTier(cellsAlongSide: number): string {
  if (cellsAlongSide <= 1) return "tiny";
  if (cellsAlongSide <= 3) return "small";
  if (cellsAlongSide <= 6) return "medium";
  return "large";
}

/** How many component rows to show per size tier */
function getMaxComps(sizeTier: string): number {
  switch (sizeTier) {
    case "tiny": return 0;
    case "small": return 3;
    case "medium": return 5;
    case "large": return 8;
    default: return 0;
  }
}


function getEffectPillClass(effect: string): string {
  const e = effect.toLowerCase();
  if (e === "overheat") return "frame-card__effect-pill--overheat";
  if (e === "freeze") return "frame-card__effect-pill--freeze";
  return "frame-card__effect-pill--default";
}

function tempToColor(t: number): string {
  if (t < -50) return "#60a5fa";  // extreme cold - bright blue
  if (t < 0) return "#38bdf8";    // cold blue
  if (t < 20) return "#22d3ee";   // cool cyan
  if (t < 50) return "#22c55e";   // cool green
  if (t < 80) return "#eab308";   // warm yellow
  if (t < 120) return "#f97316";  // hot orange
  return "#ef4444";               // danger red
}

/** Map temperature to a 0-100% bar fill. Range: -1000 to +500 */
function tempToPercent(t: number): number {
  // Map from [-1000, 500] to [0, 100]
  const clamped = Math.min(500, Math.max(-1000, t));
  return ((clamped + 1000) / 1500) * 100;
}

function getEfficiency(comp: ComponentDTO): string | null {
  const attrs = comp.attributes as Record<string, { final_value?: unknown }> | undefined;
  const eff = attrs?.efficiency?.final_value;
  if (typeof eff === "number") {
    const pct = Math.round(eff * 100);
    if (pct === 100) return null; // don't show 100% — it's the default
    return `${pct}%`;
  }
  return null;
}

/** Collect all storage info across components */
function aggregateStorage(components: ComponentDTO[]): {
  used: number;
  total: number;
  topItems: { name: string; amount: number }[];
} | null {
  let totalUsed = 0;
  let totalSlots = 0;
  const itemMap = new Map<string, number>();
  let hasStorage = false;

  for (const comp of components) {
    if (!comp.storage) continue;
    hasStorage = true;
    totalUsed += comp.storage.slots_used ?? comp.storage.slots.filter(s => s.stack).length;
    totalSlots += comp.storage.slots_total ?? comp.storage.slots_count ?? comp.storage.slots.length;
    for (const item of comp.storage.top_items ?? []) {
      itemMap.set(item.name, (itemMap.get(item.name) ?? 0) + item.amount);
    }
  }

  if (!hasStorage) return null;

  const topItems = [...itemMap.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, 3)
    .map(([name, amount]) => ({ name, amount }));

  return { used: totalUsed, total: totalSlots, topItems };
}

/** Collect all effects across frame components */
function collectEffects(components: ComponentDTO[]): string[] {
  const seen = new Set<string>();
  const result: string[] = [];
  for (const comp of components) {
    for (const eff of comp.effects ?? []) {
      if (!seen.has(eff)) {
        seen.add(eff);
        result.push(eff);
      }
    }
  }
  return result;
}

export const FrameCard = memo(function FrameCard({
  frame,
  cellSize,
  isSelected,
  wiringState,
  outsideControlZone = false,
  zoom,
  onPointerDown,
  onContextMenu,
}: FrameCardProps) {
  const isCompact = zoom < 0.4;
  const cells = cellSize / CELL;
  const sizeTier = getSizeTier(cells);
  const components = frame.components ?? [];
  const badges = frame.canvas_badges;
  const healthColor = HEALTH_COLOR[badges?.health ?? "ok"] ?? "#22c55e";
  // Read player-set accent color from frame attributes
  const accentAttr = frame.metadata?.attributes?.accent;
  const accent = accentAttr
    ? String(accentAttr.final_value ?? accentAttr.base_value ?? "")
    : undefined;

  // Max components to show based on size
  const maxComps = getMaxComps(sizeTier);
  const visibleComps = components.slice(0, maxComps);
  const hiddenCount = Math.max(0, components.length - maxComps);

  const storage = useMemo(() => aggregateStorage(components), [components]);
  const effects = useMemo(() => collectEffects(components), [components]);

  // Build class names
  let className = "frame-card";
  if (isCompact) className += " frame-card--compact";
  className += ` frame-card--${sizeTier}`;
  if (isSelected) className += " frame-card--selected";
  if (wiringState) {
    if (wiringState.isSource) className += " frame-card--wiring-source";
    else if (wiringState.isEligible) className += " frame-card--wiring-eligible";
    else className += " frame-card--wiring-ineligible";
  }
  if (outsideControlZone) className += " frame-card--outside-zone";

  const style: React.CSSProperties = {
    // Position/size set by FrameOverlay.syncTransform() via direct DOM writes
    "--health-color": healthColor,
    ...(wiringState ? { "--wiring-color": CONN_CSS_COLOR[wiringState.type] ?? "#3b82f6" } : {}),
    ...(accent ? { "--accent-color": accent } : {}),
  } as React.CSSProperties;

  const iconFile = (frame.metadata?.icon || "").trim();

  return (
    <div
      className={className}
      style={style}
      data-frame-id={frame.id}
      data-world-x={frame._x}
      data-world-y={frame._y}
      data-world-size={cellSize}
      data-accent={accent || undefined}
      onPointerDown={(e) => onPointerDown(e, frame.id)}
      onContextMenu={(e) => onContextMenu(e, frame.id)}
    >
      {/* HEADER */}
      <div className="frame-card__header">
        {iconFile ? (
          <img
            className="frame-card__icon"
            src={`/icons/${iconFile}`}
            alt=""
            draggable={false}
          />
        ) : (
          <div className="frame-card__icon-fallback">
            {(frame.name || "?")[0]?.toUpperCase()}
          </div>
        )}
        <span className="frame-card__name">{frame.name}</span>
        <span className="frame-card__id">#{frame.id}</span>
        {outsideControlZone && (
          <span className="frame-card__zone-badge" title="Outside control zone">
            NC
          </span>
        )}
        <span className="frame-card__size-badge">{frame.size}</span>
      </div>

      <div className="frame-card__body">
      {/* STATUS ROW */}
      {badges && (
        <div className="frame-card__status">
          {badges.temperature != null && badges.temperature > -999 && (
            <>
              <div className="frame-card__temp-bar">
                <div
                  className="frame-card__temp-fill"
                  style={{
                    width: `${tempToPercent(badges.temperature)}%`,
                    background: tempToColor(badges.temperature),
                  }}
                />
              </div>
              <span className="frame-card__temp-label">
                {Math.round(badges.temperature)}&deg;C
              </span>
            </>
          )}
          {badges.power && (
            <span className={`frame-card__power-chip frame-card__power-chip--${badges.power}`}>
              {POWER_LABELS[badges.power] ?? badges.power}
            </span>
          )}
        </div>
      )}

      {/* DIVIDER before components */}
      {visibleComps.length > 0 && <div className="frame-card__divider" />}

      {/* COMPONENT LIST — uses STATE_COLORS from shared UI (same as ComponentCard) */}
      {visibleComps.length > 0 && (
        <div className="frame-card__components">
          {visibleComps.map((comp) => {
            const eff = getEfficiency(comp);
            const stateColor = STATE_COLORS[comp.state] ?? "#1f2937";
            const compIcon = (comp.metadata?.icon || comp.icon || "").trim();
            const compEffects = (comp.effects ?? []).map((e) => EFFECT_LABELS[e] ?? e);
            return (
              <div
                key={comp.id}
                className="frame-card__comp-row"
                data-component-id={comp.id}
              >
                <span
                  className="frame-card__comp-dot"
                  style={{ background: stateColor }}
                />
                {compIcon ? (
                  <img
                    className="frame-card__comp-icon"
                    src={`/icons/${compIcon}`}
                    alt=""
                    draggable={false}
                  />
                ) : null}
                <span className="frame-card__comp-name">{comp.name}</span>
                {compEffects.length > 0 && (
                  <span className="frame-card__comp-effects">
                    {compEffects.join("")}
                  </span>
                )}
                <span className="frame-card__comp-state">
                  {comp.state === "ACTIVE" ? "" : comp.state.slice(0, 4)}
                </span>
                {eff && <span className="frame-card__comp-eff">{eff}</span>}
              </div>
            );
          })}
          {hiddenCount > 0 && (
            <div className="frame-card__comp-more">+{hiddenCount} more</div>
          )}
        </div>
      )}

      {/* STORAGE */}
      {storage && (
        <>
          <div className="frame-card__divider" />
          <div className="frame-card__storage">
            <div className="frame-card__storage-bar">
              <div
                className="frame-card__storage-fill"
                style={{ width: `${storage.total > 0 ? (storage.used / storage.total) * 100 : 0}%` }}
              />
            </div>
            <span className="frame-card__storage-label">
              {storage.used}/{storage.total}
            </span>
            {storage.topItems.length > 0 && (
              <span className="frame-card__storage-items">
                {storage.topItems.map(i => `${i.name} \u00D7${i.amount}`).join("  ")}
              </span>
            )}
          </div>
        </>
      )}

      {/* EFFECTS — uses EFFECT_LABELS from shared UI */}
      {effects.length > 0 && (
        <>
          <div className="frame-card__divider" />
          <div className="frame-card__effects">
            {effects.map((eff) => (
              <span
                key={eff}
                className={`frame-card__effect-pill ${getEffectPillClass(eff)}`}
              >
                {EFFECT_LABELS[eff] ?? eff}
              </span>
            ))}
          </div>
        </>
      )}
      </div>
    </div>
  );
});
