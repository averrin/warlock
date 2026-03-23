import { usePatchStore } from "./stores/patches";

/** Resolves UI "material" to a real patches.create type key (must match scripts/patches/*.lua stems). */
export function resolveSurfacePatchTypeKey(material: string): string | null {
  const types = usePatchStore.getState().patchTypes;
  if (types.length === 0) return null;
  if (types.some((t) => t.key === material)) return material;
  const ore = types.find((t) => t.key === "sparkstone_deposit");
  if (ore) return ore.key;
  return types[0]!.key;
}

/** Same entry as Sidebar → Surface Palette → Draw Surface. */
export function startSurfacePaintMode(detail?: {
  material: string;
  color: { r: number; g: number; b: number; a: number };
  icon?: string;
}) {
  const d = detail ?? {
    material: "sparkstone_deposit",
    color: { r: 34, g: 197, b: 94, a: 128 },
  };
  window.dispatchEvent(
    new CustomEvent("warlock:createSurface", {
      detail: { kind: "paint" as const, ...d, icon: d.icon ?? d.material },
    }),
  );
}

/** Drag a rectangle to erase paint_surface cells (concrete, road, …). */
export function startSurfaceRemoveMode() {
  window.dispatchEvent(new CustomEvent("warlock:createSurface", { detail: { kind: "remove" as const } }));
}
