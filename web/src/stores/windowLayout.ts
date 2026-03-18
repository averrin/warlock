import { create } from "zustand";
import type { DockviewApi, SerializedDockview } from "dockview-core";

const STORAGE_KEY = "warlock.web.window-layout";

interface WindowLayoutStore {
  save: (layout: SerializedDockview) => void;
  load: () => SerializedDockview | null;
  clear: () => void;
  collapsedGroups: Record<string, boolean>;
  toggleGroupCollapsed: (groupId: string) => void;
  // Legacy single pinned inspector (deprecated, kept for compatibility)
  miniInspectorPinned: boolean;
  setMiniInspectorPinned: (pinned: boolean) => void;
  miniInspectorFrameId: number | null;
  setMiniInspectorFrame: (frameId: number | null) => void;
  // New: multiple pinned inspectors (panelId -> frameId)
  pinnedMiniInspectors: Record<string, number>;
  pinMiniInspector: (panelId: string, frameId: number) => void;
  unpinMiniInspector: (panelId: string) => void;
  isPanelPinned: (panelId: string) => boolean;
  getPinnedFrameId: (panelId: string) => number | null;
  dockviewApi: DockviewApi | null;
  setDockviewApi: (api: DockviewApi) => void;
  focusPanel: (panelId: string) => void;
  openComponentCodeEditor: (
    params: Record<string, unknown>,
    title: string,
    position?: { x: number; y: number; width: number; height: number },
  ) => void;
}

export const useWindowLayoutStore = create<WindowLayoutStore>((set, get) => ({
  collapsedGroups: {},
  toggleGroupCollapsed: (groupId) =>
    set((s) => ({
      collapsedGroups: { ...s.collapsedGroups, [groupId]: !s.collapsedGroups[groupId] },
    })),
  miniInspectorPinned: false,
  setMiniInspectorPinned: (pinned) => set(() => ({ miniInspectorPinned: pinned })),
  miniInspectorFrameId: null,
  setMiniInspectorFrame: (frameId) => set(() => ({ miniInspectorFrameId: frameId })),
  pinnedMiniInspectors: {},
  pinMiniInspector: (panelId, frameId) =>
    set((s) => ({
      pinnedMiniInspectors: { ...s.pinnedMiniInspectors, [panelId]: frameId },
    })),
  unpinMiniInspector: (panelId) =>
    set((s) => {
      const { [panelId]: _, ...rest } = s.pinnedMiniInspectors;
      return { pinnedMiniInspectors: rest };
    }),
  isPanelPinned: (panelId) => panelId in get().pinnedMiniInspectors,
  getPinnedFrameId: (panelId) => get().pinnedMiniInspectors[panelId] ?? null,
  save: (layout) => {
    if (typeof window === "undefined") {
      return;
    }
    try {
      window.localStorage.setItem(STORAGE_KEY, JSON.stringify(layout));
    } catch {
      // Ignore storage errors to avoid breaking runtime.
    }
  },
  load: () => {
    if (typeof window === "undefined") {
      return null;
    }
    try {
      const raw = window.localStorage.getItem(STORAGE_KEY);
      if (!raw) {
        return null;
      }
      return JSON.parse(raw) as SerializedDockview;
    } catch {
      return null;
    }
  },
  clear: () => {
    if (typeof window === "undefined") {
      return;
    }
    window.localStorage.removeItem(STORAGE_KEY);
  },
  dockviewApi: null,
  setDockviewApi: (api) => set({ dockviewApi: api }),
  focusPanel: (panelId) => {
    const api = get().dockviewApi;
    if (!api) return;
    const panel = api.getPanel(panelId);
    if (panel) {
      panel.api.setActive();
    }
  },
  openComponentCodeEditor: (params, title, position) => {
    const api = get().dockviewApi;
    if (!api) return;
    const id = `component-code-editor-${Date.now()}`;
    const panel = api.addPanel({ id, component: "componentCodeEditor", title, params });
    api.addFloatingGroup(panel, position ?? { x: 220, y: 140, width: 640, height: 500 });
  },
}));
