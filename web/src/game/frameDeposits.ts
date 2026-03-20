import { CELL, FRAME_CELL_SIZES } from "../components/canvas/connectionGeometry";
import type { FrameDTO } from "../rpc/types";
import type { Patch } from "../stores/patches";
import type { RecipeDTO } from "../stores/recipes";

/** Grid cell item names from resource patches overlapping the frame footprint (25px grid). */
export function depositItemsUnderFrame(frame: FrameDTO, patches: readonly Patch[]): string[] {
  const side = FRAME_CELL_SIZES[frame.size] ?? 1;
  const pos = frame.position ?? { x: 0, y: 0 };
  const gx = Math.floor(pos.x / CELL);
  const gy = Math.floor(pos.y / CELL);
  const cellKeys = new Set<string>();
  for (let dy = 0; dy < side; dy++) {
    for (let dx = 0; dx < side; dx++) {
      cellKeys.add(`${gx + dx},${gy + dy}`);
    }
  }
  const items = new Set<string>();
  for (const p of patches) {
    if (p.obstacle || !p.item.trim()) continue;
    if (p.cells.some(([cx, cy]) => cellKeys.has(`${cx},${cy}`))) {
      items.add(p.item);
    }
  }
  return [...items];
}

/** Recipe names allowed in UI for this component (miner: must output a deposit item under the frame). */
export function recipeSelectOptions(
  componentName: string,
  recipes: readonly RecipeDTO[],
  frame: FrameDTO | undefined,
  patches: readonly Patch[],
  currentValue: string,
): string[] {
  const onComponent = recipes.filter((r) => r.available.includes(componentName));
  let filtered = onComponent;
  if (componentName === "Miner") {
    if (!frame) {
      filtered = [];
    } else {
      const depSet = new Set(depositItemsUnderFrame(frame, patches));
      if (depSet.size === 0) {
        filtered = [];
      } else {
        filtered = onComponent.filter((r) => r.outputs.some((o) => depSet.has(o.name)));
      }
    }
  }
  const names = filtered.map((r) => r.name).sort((a, b) => a.localeCompare(b));
  const cur = currentValue.trim();
  if (cur && !names.includes(cur)) {
    return [cur, ...names];
  }
  if (names.length === 0 && cur) {
    return [cur];
  }
  return names;
}
