export type SpendableCost = Record<string, number>;

export const FRAME_SIZE_COSTS: Record<string, SpendableCost> = {
  XS: { "Ultralight Structures": 10 },
  S: { "Frame Parts": 25 },
  M: { "Frame Parts": 50 },
  L: { "Frame Parts": 100 },
  G: { "Frame Parts": 50, "Ultralight Structures": 50 },
};

export function mergeCosts(a: SpendableCost, b: SpendableCost): SpendableCost {
  const out: SpendableCost = { ...a };
  for (const [k, v] of Object.entries(b)) {
    out[k] = (out[k] ?? 0) + v;
  }
  return out;
}

export function formatCostLine(cost: SpendableCost): string {
  const parts = Object.entries(cost)
    .filter(([, v]) => v > 0)
    .map(([k, v]) => `${v} ${k}`);
  return parts.length ? parts.join(" + ") : "—";
}

export function parseLuaSpendableCost(source: string): SpendableCost {
  const out: SpendableCost = {};
  const m = source.match(/spendable_cost\s*=\s*\{([^}]*)\}/);
  if (!m) return out;
  const inner = m[1] ?? "";
  const re = /\[\s*"([^"]+)"\s*\]\s*=\s*(\d+)/g;
  let x: RegExpExecArray | null;
  while ((x = re.exec(inner)) !== null) {
    const k = x[1]!;
    const v = Number(x[2]);
    out[k] = (out[k] ?? 0) + v;
  }
  return out;
}

export function parseBlueprintFrameSizeKey(source: string): string {
  const m = source.match(/FrameSize\.(\w+)/);
  return m?.[1] ?? "S";
}

export function parseBlueprintComponentNames(source: string): string[] {
  const start = source.indexOf("components");
  if (start < 0) return [];
  const sub = source.slice(start);
  const open = sub.indexOf("{");
  if (open < 0) return [];
  let depth = 0;
  for (let i = open; i < sub.length; i++) {
    const c = sub[i]!;
    if (c === "{") depth++;
    else if (c === "}") {
      depth--;
      if (depth === 0) {
        const block = sub.slice(open + 1, i);
        const names: string[] = [];
        const re = /"([^"]+)"/g;
        let m: RegExpExecArray | null;
        while ((m = re.exec(block)) !== null) names.push(m[1]!);
        return names;
      }
    }
  }
  return [];
}

export function blueprintTotalCost(blueprintSource: string, componentSources: Record<string, string>): SpendableCost {
  const sz = parseBlueprintFrameSizeKey(blueprintSource);
  let total: SpendableCost = { ...(FRAME_SIZE_COSTS[sz] ?? FRAME_SIZE_COSTS.S ?? {}) };
  for (const name of parseBlueprintComponentNames(blueprintSource)) {
    const src = componentSources[name];
    if (src) total = mergeCosts(total, parseLuaSpendableCost(src));
  }
  return total;
}

export function canAfford(pool: Record<string, number>, cost: SpendableCost): boolean {
  for (const [k, v] of Object.entries(cost)) {
    if (v <= 0) continue;
    if ((pool[k] ?? 0) < v) return false;
  }
  return true;
}
