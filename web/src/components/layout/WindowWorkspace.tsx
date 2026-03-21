import { Suspense, useCallback, useEffect, useMemo, useRef, useState, type FunctionComponent, type MouseEvent } from "react";
import {
  DockviewReact,
  type DockviewApi,
  type DockviewReadyEvent,
  type IDockviewPanelProps,
  type IDockviewHeaderActionsProps,
} from "dockview";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { useWindowLayoutStore } from "../../stores/windowLayout";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { GameCanvas } from "../canvas/GameCanvas";
import { BlueprintPalette } from "../panels/BlueprintPalette";
import { CodeEditorPanel } from "../panels/CodeEditorPanel";
import { ComponentPalette } from "../panels/ComponentPalette";
import { ConnectionEditor } from "../panels/ConnectionEditor";
import { EnvironmentPanel } from "../panels/EnvironmentPanel";
import { LogPanel } from "../panels/LogPanel";
import { FrameInspector } from "../panels/FrameInspector";
import { FrameMiniInspector } from "../panels/FrameMiniInspector";
import { PowerPanel } from "../panels/PowerPanel";
import { StateInspector } from "../panels/StateInspector";
import { GlobalIndicatorsPanel } from "../panels/GlobalIndicatorsPanel";
import { ComponentCodeEditorPanel, type ComponentCodeEditorParams } from "../panels/ComponentCodeEditorPanel";

type Props = {
  rpcClient: RpcClient;
};

const ROLLUP_GROUP_HEIGHT = 32;

function formatTime(minutes: number): string {
  const h = Math.floor(minutes / 60);
  const m = minutes % 60;
  return `${String(h).padStart(2, "0")}:${String(m).padStart(2, "0")}`;
}

const SPEED_OPTIONS = [
  { label: "Pause", value: 0 },
  { label: "x1", value: 1 },
  { label: "x5", value: 5 },
  { label: "x10", value: 10 },
] as const;

function GroupHeaderActions({ api, group }: IDockviewHeaderActionsProps) {
  const groupId = group.id;
  const collapsed = useWindowLayoutStore((s) => s.collapsedGroups[groupId] ?? false);
  const toggleGroupCollapsed = useWindowLayoutStore((s) => s.toggleGroupCollapsed);
  const pinnedMiniInspectors = useWindowLayoutStore((s) => s.pinnedMiniInspectors);
  const pinMiniInspector = useWindowLayoutStore((s) => s.pinMiniInspector);
  const unpinMiniInspector = useWindowLayoutStore((s) => s.unpinMiniInspector);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);

  // Find any mini-inspector panel in this group
  const miniInspectorPanel = group.panels?.find((panel) => 
    panel.id === "frame-mini-inspector" || panel.id.startsWith("frame-mini-inspector-")
  ) ?? null;
  const containsMiniInspector = miniInspectorPanel != null;
  const panelId = miniInspectorPanel?.id ?? null;
  const isPinned = panelId ? panelId in pinnedMiniInspectors : false;

  const handleClick = () => {
    const next = !collapsed;
    toggleGroupCollapsed(groupId);
    api.setSize({ height: next ? ROLLUP_GROUP_HEIGHT : 260 });
  };

  const rootProps =
    containsMiniInspector
      ? { "data-mini-inspector-root": "true" }
      : {};

  return (
    <div
      style={{ display: "flex", alignItems: "center", paddingRight: 4, gap: 4 }}
      {...rootProps}
    >
      <button
        type="button"
        onClick={(e) => {
          e.stopPropagation();
          handleClick();
        }}
        title={collapsed ? "Expand window" : "Roll up window"}
        style={{
          width: 18,
          height: 18,
          borderRadius: 3,
          border: "1px solid #374151",
          background: collapsed ? "#111827" : "#020617",
          color: "#e5e7eb",
          fontSize: 10,
          lineHeight: "16px",
          padding: 0,
          cursor: "pointer",
        }}
      >
        {collapsed ? "▢" : "▁"}
      </button>
      {containsMiniInspector && panelId ? (
        <button
          type="button"
          onClick={(e) => {
            e.stopPropagation();
            if (isPinned) {
              unpinMiniInspector(panelId);
            } else if (selectedFrameId != null) {
              pinMiniInspector(panelId, selectedFrameId);
            }
          }}
          title={isPinned ? "Unpin mini inspector" : "Pin mini inspector to current frame"}
          style={{
            width: 18,
            height: 18,
            borderRadius: 3,
            border: "1px solid #374151",
            background: isPinned ? "#1d283a" : "#020617",
            color: "#e5e7eb",
            fontSize: 10,
            lineHeight: "16px",
            padding: 0,
            cursor: "pointer",
          }}
        >
          📌
        </button>
      ) : null}
    </div>
  );
}

function seedDefaultPanels(api: DockviewApi) {
  const world = api.addPanel({
    id: "world",
    component: "world",
    title: "World",
  });

  const frameInspector = api.addPanel({
    id: "frame-inspector",
    component: "frameInspector",
    title: "Frame Inspector",
    position: { referencePanel: world, direction: "right" },
  });

  const registerTab = (id: string, component: string, title: string) => {
    api.addPanel({
      id,
      component,
      title,
      position: { referencePanel: frameInspector, direction: "within" },
    });
  };

  registerTab("connection-editor", "connectionEditor", "Connection Editor");
  registerTab("power-panel", "powerPanel", "Power Networks");
  const environment = api.addPanel({
    id: "environment",
    component: "environmentPanel",
    title: "Environment",
    position: { referencePanel: frameInspector, direction: "within" },
  });
  registerTab("blueprint-palette", "blueprintPalette", "Blueprints");
  registerTab("component-palette", "componentPalette", "Components");
  registerTab("code-editor", "codeEditor", "Code Editor");
  registerTab("global-indicators", "globalIndicatorsPanel", "Global Indicators");

  api.addFloatingGroup(environment, {
    x: 140,
    y: 90,
    width: 460,
    height: 360,
  });
}

const TOP_BAR_BUTTONS: { id: string; component: string; title: string; label: string }[] = [
  { id: "frame-inspector", component: "frameInspector", title: "Frame Inspector", label: "Frame" },
  { id: "connection-editor", component: "connectionEditor", title: "Connection Editor", label: "Connections" },
  { id: "power-panel", component: "powerPanel", title: "Power Networks", label: "Power" },
  { id: "environment", component: "environmentPanel", title: "Environment", label: "Environment" },
  { id: "log", component: "logPanel", title: "Log", label: "Log" },
  { id: "blueprint-palette", component: "blueprintPalette", title: "Blueprints", label: "Blueprints" },
  { id: "component-palette", component: "componentPalette", title: "Components", label: "Components" },
  { id: "code-editor", component: "codeEditor", title: "Code Editor", label: "Code" },
  { id: "state-inspector", component: "stateInspector", title: "State Inspector", label: "State" },
  { id: "global-indicators", component: "globalIndicatorsPanel", title: "Global Indicators", label: "Indicators" },
];

export function WindowWorkspace({ rpcClient }: Props) {
  const saveLayout = useWindowLayoutStore((s) => s.save);
  const loadLayout = useWindowLayoutStore((s) => s.load);
  const collapsedGroups = useWindowLayoutStore((s) => s.collapsedGroups);
  const pinnedMiniInspectors = useWindowLayoutStore((s) => s.pinnedMiniInspectors);
  const frames = useGameStore((s) => s.frames);
  const env = useGameStore((s) => s.environment);
  const showThermalField = useGameStore((s) => s.showThermalField);
  const setShowThermalField = useGameStore((s) => s.setShowThermalField);
  const timeControl = useGameStore((s) => s.timeControl);
  const pauseGame = useGameStore((s) => s.pauseGame);
  const resumeGame = useGameStore((s) => s.resumeGame);
  const setSpeed = useGameStore((s) => s.setSpeed);
  const fetchSpeedState = useGameStore((s) => s.fetchSpeedState);
  const apiRef = useRef<DockviewApi | null>(null);
  const lastMiniInspectorOpenAtRef = useRef<number>(0);
  const [activePanelId, setActivePanelId] = useState<string | null>(null);
  const envSupported = isFeatureSupported(capabilityMethods.environment);
  const speedSupported = isFeatureSupported(capabilityMethods.speedControl);

  useEffect(() => {
    if (!rpcClient) return;
    fetchSpeedState(rpcClient);
  }, [rpcClient, fetchSpeedState]);

  const handleSpeed = useCallback(
    (value: number) => {
      if (!rpcClient) return;
      if (value === 0) {
        pauseGame(rpcClient);
      } else {
        if (timeControl.paused) {
          resumeGame(rpcClient);
        }
        setSpeed(rpcClient, value);
      }
    },
    [rpcClient, timeControl.paused, pauseGame, resumeGame, setSpeed],
  );

  const currentSpeed = timeControl.paused ? 0 : timeControl.multiplier;

  const openFrameMiniInspectorAt = useCallback(
    (frameId: number, clientX: number, clientY: number, opts?: { keepInPlace?: boolean }) => {
      if (useGameStore.getState().selectedFrameIds.length > 1) return;
      const api = apiRef.current;
      if (!api) return;

      const pinned = useWindowLayoutStore.getState().pinnedMiniInspectors;

      if (opts?.keepInPlace) {
        for (const panel of api.panels) {
          const isMiniInspector = panel.id === "frame-mini-inspector" || panel.id.startsWith("frame-mini-inspector-");
          if (!isMiniInspector || panel.id in pinned) continue;
          lastMiniInspectorOpenAtRef.current = performance.now();
          panel.api.setActive();
          return;
        }
      }

      lastMiniInspectorOpenAtRef.current = performance.now();

      // Close any existing unpinned mini-inspectors (switching to a different frame)
      for (const panel of api.panels) {
        const isMiniInspector = panel.id === "frame-mini-inspector" || panel.id.startsWith("frame-mini-inspector-");
        if (isMiniInspector && !(panel.id in pinned)) {
          api.removePanel(panel);
        }
      }

      // Create a new ephemeral mini-inspector with a unique ID
      const newId = `frame-mini-inspector-${Date.now()}`;
      const panel = api.addPanel({
        id: newId,
        component: "frameMiniInspector",
        title: "Frame Mini Inspector",
      });

      api.addFloatingGroup(panel, {
        x: clientX + 12,
        y: clientY + 12,
        width: 380,
        height: 260,
      });

      panel.api.setActive();

      // Frame id is intentionally unused for now; GameCanvas will drive selection.
      void frameId;
    },
    [],
  );

  // Do NOT pass rpcClient through Dockview params: layouts are serialized to JSON,
  // which turns class instances into `{}` on restore.
  const panelComponents = useMemo(() => {
    const codeEditorNode = (
      <Suspense fallback={<div style={{ padding: 10, color: "#9ca3af", fontSize: 12 }}>Loading editor…</div>}>
        <CodeEditorPanel rpcClient={rpcClient} />
      </Suspense>
    );

    return {
      world: (() => <GameCanvas rpcClient={rpcClient} onFrameMiniInspect={openFrameMiniInspectorAt} />) as FunctionComponent<IDockviewPanelProps>,
      frameInspector: (() => <FrameInspector rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      frameMiniInspector: ((props: IDockviewPanelProps) => <FrameMiniInspector rpcClient={rpcClient} panelApi={props.api} />) as FunctionComponent<IDockviewPanelProps>,
      connectionEditor: (() => <ConnectionEditor rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      powerPanel: (() => <PowerPanel rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      environmentPanel: (() => <EnvironmentPanel rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      logPanel: (() => <LogPanel rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      blueprintPalette: (() => <BlueprintPalette rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      componentPalette: (() => <ComponentPalette rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      codeEditor: (() => codeEditorNode) as FunctionComponent<IDockviewPanelProps>,
      stateInspector: (() => <StateInspector rpcClient={rpcClient} />) as FunctionComponent<IDockviewPanelProps>,
      globalIndicatorsPanel: (() => <GlobalIndicatorsPanel />) as FunctionComponent<IDockviewPanelProps>,
      componentCodeEditor: ((props: IDockviewPanelProps) => (
        <ComponentCodeEditorPanel rpcClient={rpcClient} {...(props.params as ComponentCodeEditorParams)} />
      )) as FunctionComponent<IDockviewPanelProps>,
    } satisfies Record<string, FunctionComponent<IDockviewPanelProps>>;
  }, [openFrameMiniInspectorAt, rpcClient]);

  const ensurePanel = useCallback(
    (id: string, component: string, title: string) => {
      const api = apiRef.current;
      if (!api) return;
      const existing = api.getPanel(id);
      if (existing) {
        existing.api.setActive();
        return;
      }
      api.addPanel({ id, component, title });
    },
    [],
  );

  const onReady = useCallback(
    (event: DockviewReadyEvent) => {
      const api = event.api;
      apiRef.current = api;
      useWindowLayoutStore.getState().setDockviewApi(api);
      const saved = loadLayout();
      if (saved) {
        try {
          api.fromJSON(saved);
        } catch {
          api.clear();
        }
      }
      if (api.totalPanels === 0) {
        seedDefaultPanels(api);
      }
      setActivePanelId(api.activePanel?.id ?? null);
      api.onDidActivePanelChange((panel) => setActivePanelId(panel?.id ?? null));
      api.onDidLayoutChange(() => {
        saveLayout(api.toJSON());
      });
    },
    [loadLayout, saveLayout],
  );

  return (
    <div className="dockview-theme-dark" style={{ width: "100%", height: "100%", display: "grid", gridTemplateRows: "32px 1fr" }}>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          padding: "0 10px",
          borderBottom: "1px solid #1f2937",
          background: "#0b1220",
          fontSize: 12,
          color: "#e5e7eb",
          userSelect: "none",
        }}
      >
        <span style={{ fontWeight: 700 }}>Windows</span>
        {TOP_BAR_BUTTONS.map(({ id, component, title, label }) => {
          const isActive = activePanelId === id;
          const api = apiRef.current;
          const groupId = api?.getPanel(id)?.group.id;
          const isRolledUp = groupId ? collapsedGroups[groupId] : false;
          return (
            <button
              key={id}
              type="button"
              onMouseDown={(e: MouseEvent<HTMLButtonElement>) => {
                const apiInstance = apiRef.current;
                if (!apiInstance) return;
                if (e.button === 1) {
                  e.preventDefault();
                  let panel = apiInstance.getPanel(id);
                  if (!panel) {
                    panel = apiInstance.addPanel({ id, component, title });
                  }
                  apiInstance.addFloatingGroup(panel);
                  panel.api.setActive();
                  return;
                }
              }}
              onClick={() => ensurePanel(id, component, title)}
              style={{
                padding: "4px 8px",
                border: "1px solid transparent",
                borderRadius: 4,
                background: isActive ? "#1e3a5f" : "transparent",
                color: isActive ? "#93c5fd" : "#e5e7eb",
                cursor: "pointer",
                fontWeight: isActive ? 600 : 400,
              }}
            >
              {label}
              {isRolledUp && <span style={{ marginLeft: 4, opacity: 0.7 }} title="Rolled up">−</span>}
            </button>
          );
        })}
        {Object.entries(pinnedMiniInspectors).map(([panelId, frameId]) => {
          const frame = frames.find((f) => f.id === frameId);
          const label = frame ? `${frame.name} #${frame.id}` : `#${frameId}`;
          const isActive = activePanelId === panelId;
          return (
            <button
              key={`${panelId}-taskbar`}
              type="button"
              onClick={() => {
                const api = apiRef.current;
                if (!api) return;
                const panel = api.getPanel(panelId);
                if (panel) {
                  panel.api.setActive();
                }
              }}
              style={{
                padding: "4px 8px",
                border: "1px solid transparent",
                borderRadius: 4,
                background: isActive ? "#1e3a5f" : "transparent",
                color: isActive ? "#93c5fd" : "#e5e7eb",
                cursor: "pointer",
                fontWeight: isActive ? 600 : 400,
                display: "flex",
                alignItems: "center",
                gap: 4,
              }}
            >
              <span aria-hidden="true">📌</span>
              {label}
            </button>
          );
        })}
        <div style={{ flex: 1 }} />
        {envSupported && (
          <div style={{ display: "flex", alignItems: "center", gap: 16 }}>
            <div
              style={{
                display: "flex",
                alignItems: "center",
                gap: 8,
                minWidth: 120,
              }}
            >
              <span
                style={{
                  fontSize: 14,
                  fontWeight: 600,
                  fontVariantNumeric: "tabular-nums",
                }}
              >
                {env?.minutes != null ? formatTime(env.minutes) : "--:--"}
              </span>
              <span style={{ color: "#9ca3af" }}>
                Day {String(env?.days ?? "-")} · {env?.is_day ? "☀ Day" : "☾ Night"}
              </span>
            </div>
            <div
              style={{
                display: "flex",
                alignItems: "center",
                gap: 10,
                fontSize: 11,
                color: "#9ca3af",
              }}
            >
              <span
                style={{
                  display: "inline-flex",
                  alignItems: "baseline",
                  minWidth: 50,
                  justifyContent: "flex-end",
                  fontVariantNumeric: "tabular-nums",
                }}
              >
                <span style={{ color: "#e5e7eb", fontWeight: 600, marginRight: 4 }}>
                  {env?.temperature != null ? `${env.temperature.toFixed(1)}°` : "-"}
                </span>
                <span>Temp</span>
              </span>
              <span
                style={{
                  display: "inline-flex",
                  alignItems: "baseline",
                  minWidth: 50,
                  justifyContent: "flex-end",
                  fontVariantNumeric: "tabular-nums",
                }}
              >
                <span style={{ color: "#e5e7eb", fontWeight: 600, marginRight: 4 }}>
                  {env?.air_flow != null ? env.air_flow.toFixed(1) : "-"}
                </span>
                <span>Airflow</span>
              </span>
              <span
                style={{
                  display: "inline-flex",
                  alignItems: "baseline",
                  minWidth: 50,
                  justifyContent: "flex-end",
                  fontVariantNumeric: "tabular-nums",
                }}
              >
                <span style={{ color: "#e5e7eb", fontWeight: 600, marginRight: 4 }}>
                  {env?.sun != null ? `${env.sun.toFixed(0)}%` : "-"}
                </span>
                <span>Sun</span>
              </span>
              <label
                style={{
                  display: "inline-flex",
                  alignItems: "center",
                  gap: 6,
                  cursor: "pointer",
                  color: "#e5e7eb",
                  marginLeft: 4,
                }}
              >
                <input
                  type="checkbox"
                  checked={showThermalField}
                  onChange={(e) => setShowThermalField(e.target.checked)}
                  aria-pressed={showThermalField}
                />
                <span>Temperature field</span>
              </label>
            </div>
            <div style={{ display: "flex", gap: 4 }}>
              {SPEED_OPTIONS.map((opt) => (
                <button
                  key={opt.value}
                  type="button"
                  aria-label={`speed-${opt.label.toLowerCase()}`}
                  style={{
                    padding: "2px 6px",
                    fontSize: 10,
                    borderRadius: 4,
                    border: "1px solid #374151",
                    cursor: speedSupported ? "pointer" : "default",
                    background: currentSpeed === opt.value ? "#3b82f6" : "#111827",
                    color: currentSpeed === opt.value ? "#fff" : "#e5e7eb",
                    opacity: speedSupported ? 1 : 0.4,
                  }}
                  onClick={() => handleSpeed(opt.value)}
                  disabled={!speedSupported}
                  title={!speedSupported ? "Speed control not available" : undefined}
                >
                  {opt.label}
                </button>
              ))}
            </div>
          </div>
        )}
      </div>
      <div
        style={{ width: "100%", height: "100%", minHeight: 0, minWidth: 0 }}
        onMouseDownCapture={(e) => {
          // Guard: avoid immediately closing from the same pointer sequence that opened it.
          if (performance.now() - lastMiniInspectorOpenAtRef.current < 200) {
            return;
          }

          const target = e.target;
          if (target instanceof Element) {
            if (target.closest("[data-mini-inspector-root]")) return;
            if (target.closest("[data-mini-inspector-panel]")) return;
            if (target.closest("[data-context-menu]")) return;
          }

          const api = apiRef.current;
          if (!api) return;

          // Close any unpinned mini-inspector panels
          const pinned = useWindowLayoutStore.getState().pinnedMiniInspectors;
          const allPanels = api.panels;
          for (const panel of allPanels) {
            const isMiniInspector = panel.id === "frame-mini-inspector" || panel.id.startsWith("frame-mini-inspector-");
            if (isMiniInspector && !(panel.id in pinned)) {
              api.removePanel(panel);
            }
          }
        }}
      >
        <DockviewReact components={panelComponents} rightHeaderActionsComponent={GroupHeaderActions} onReady={onReady} />
      </div>
    </div>
  );
}
