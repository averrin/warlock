import { create } from "zustand";

export interface PatchCell {
  x: number;
  y: number;
}

export interface Patch {
  id: number;
  name: string;
  type: string;
  item: string;
  cells: [number, number][];
  bounds: { x: number; y: number; w: number; h: number };
  color: { r: number; g: number; b: number; a: number };
}

export interface PatchType {
  key: string;
  name: string;
  item: string;
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
}));
