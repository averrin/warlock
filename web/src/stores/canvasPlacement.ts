import { create } from "zustand";

export type CanvasPlacementMode =
  /** `frameSizeKey` from parsed blueprint source (XS|S|M|L|G); avoids ghost size when blueprint map ref is empty. */
  | { kind: "frame"; blueprint: string; frameSizeKey?: string }
  | { kind: "patch"; patchType: string }
  | { kind: "marker"; label: string; color: string };

type CanvasPlacementState = {
  mode: CanvasPlacementMode | null;
  setMode: (mode: CanvasPlacementMode | null) => void;
};

/** Synced in setMode for Pixi listeners that close over stale state. */
export const placementModeRef: { current: CanvasPlacementMode | null } = { current: null };

export const useCanvasPlacementStore = create<CanvasPlacementState>((set) => ({
  mode: null,
  setMode: (mode) => {
    placementModeRef.current = mode;
    set({ mode });
  },
}));
