import { create } from "zustand";
import type { RpcClient } from "../rpc/client";

export interface PatchCell {
  x: number;
  y: number;
}

export interface Patch {
  id: number;
  name: string;
  type: string;
  item: string;
  /** Terrain obstacle: blocks frame placement and wire/beam paths. */
  obstacle?: boolean;
  cells: [number, number][];
  bounds: { x: number; y: number; w: number; h: number };
  color: { r: number; g: number; b: number; a: number };
}

export interface PatchType {
  key: string;
  name: string;
  item: string;
  obstacle?: boolean;
  color: { r: number; g: number; b: number; a: number };
}

interface PatchStore {
  patches: Patch[];
  patchTypes: PatchType[];
  selectedPatchId: number | null;

  setPatches: (patches: Patch[]) => void;
  setPatchTypes: (types: PatchType[]) => void;
  addPatch: (patch: Patch) => void;
  removePatch: (id: number) => void;
  selectPatch: (id: number | null) => void;
  movePatch: (client: RpcClient, id: number, x: number, y: number) => Promise<void>;
}

export const usePatchStore = create<PatchStore>((set) => ({
  patches: [],
  patchTypes: [],
  selectedPatchId: null,
  
  setPatches: (patches) => set({ patches }),
  setPatchTypes: (types) => set({ patchTypes: types }),
  addPatch: (patch) => set((state) => ({ patches: [...state.patches, patch] })),
  removePatch: (id) => set((state) => ({ 
    patches: state.patches.filter((p) => p.id !== id),
    selectedPatchId: state.selectedPatchId === id ? null : state.selectedPatchId
  })),
  selectPatch: (id) => set({ selectedPatchId: id }),
  movePatch: async (client, id, x, y) => {
    await client.call("patches.move", { id, x, y });
    set((state) => ({
      patches: state.patches.map((p) =>
        p.id === id
          ? { ...p, bounds: { ...p.bounds, x, y }, cells: p.cells.map(([cx, cy]) => [cx + x - p.bounds.x, cy + y - p.bounds.y] as [number, number]) }
          : p,
      ),
    }));
  },
}));
