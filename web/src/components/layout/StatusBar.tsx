import { useCallback, useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useConnectionStore } from "../../stores/connection";
import { useGameStore } from "../../stores/game";
import { useSceneStore } from "../../stores/scene";
import { useSaveStore } from "../../stores/saveState";
import { useSaveSlotsStore } from "../../stores/saveSlots";

type Props = {
  rpcClient: RpcClient;
};

export function StatusBar({ rpcClient }: Props) {
  const status = useConnectionStore((s) => s.status);
  const claimed = useConnectionStore((s) => s.claimed);
  const gameStarted = useGameStore((s) => s.started);
  const env = useGameStore((s) => s.environment);
  const tick = useSceneStore((s) => s.snapshot.tick);

  const saving = useSaveStore((s) => s.saving);
  const lastSavedAt = useSaveStore((s) => s.lastSavedAt);
  const autosaveEnabled = useSaveStore((s) => s.autosaveEnabled);
  const autosaveInterval = useSaveStore((s) => s.autosaveInterval);
  const initSave = useSaveStore((s) => s.init);
  const fetchStatus = useSaveStore((s) => s.fetchStatus);
  const save = useSaveStore((s) => s.save);
  const load = useSaveStore((s) => s.load);
  const configureAutosave = useSaveStore((s) => s.configureAutosave);

  const [intervalInput, setIntervalInput] = useState(String(autosaveInterval));
  const [slotSaving, setSlotSaving] = useState(false);

  const initSaveSlots = useSaveSlotsStore((s) => s.init);
  const currentSlotName = useSaveSlotsStore((s) => s.currentSlotName);
  const saveSlot = useSaveSlotsStore((s) => s.saveSlot);

  useEffect(() => {
    initSave(rpcClient);
    fetchStatus(rpcClient);
    initSaveSlots(rpcClient);
  }, [rpcClient, initSave, fetchStatus, initSaveSlots]);

  useEffect(() => {
    setIntervalInput(String(autosaveInterval));
  }, [autosaveInterval]);

  const handleSave = useCallback(() => {
    if (currentSlotName) {
      setSlotSaving(true);
      void saveSlot(rpcClient, currentSlotName).finally(() => setSlotSaving(false));
    } else {
      void save(rpcClient);
    }
  }, [rpcClient, currentSlotName, saveSlot, save]);
  const handleLoad = useCallback(() => load(rpcClient), [rpcClient, load]);

  const handleToggleAutosave = useCallback(() => {
    const next = !autosaveEnabled;
    configureAutosave(rpcClient, next, autosaveInterval);
  }, [rpcClient, autosaveEnabled, autosaveInterval, configureAutosave]);

  const handleIntervalBlur = useCallback(() => {
    const n = parseInt(intervalInput, 10);
    if (!Number.isNaN(n) && n >= 5) {
      configureAutosave(rpcClient, autosaveEnabled, n);
    } else {
      setIntervalInput(String(autosaveInterval));
    }
  }, [rpcClient, autosaveEnabled, autosaveInterval, intervalInput, configureAutosave]);

  const formatSavedAt = (iso: string | null) => {
    if (!iso) return "-";
    try {
      const d = new Date(iso);
      return d.toLocaleTimeString();
    } catch {
      return iso;
    }
  };

  const btnStyle = (disabled: boolean) => ({
    padding: "2px 8px",
    fontSize: 11,
    borderRadius: 4,
    border: "1px solid #374151",
    background: "#111827",
    color: "#e5e7eb",
    cursor: disabled ? ("default" as const) : ("pointer" as const),
    opacity: disabled ? 0.4 : 1,
  });

  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        gap: 16,
        padding: "0 12px",
        borderTop: "1px solid #1f2937",
        background: "#111827",
        fontSize: 12,
      }}
    >
      <span>conn: {status}</span>
      <span>claimed: {claimed ? "yes" : "no"}</span>
      <span>game: {gameStarted ? "running" : "stopped"}</span>
      <span>tick: {tick}</span>
      <span>
        env: day {String(env?.days ?? "-")} / minute {String(env?.minutes ?? "-")}
      </span>

      <div style={{ flex: 1 }} />

      <button
        type="button"
        onClick={handleSave}
        disabled={(currentSlotName ? slotSaving : saving) || !gameStarted}
        style={{
          ...btnStyle((currentSlotName ? slotSaving : saving) || !gameStarted),
          background: (currentSlotName ? slotSaving : saving) ? "#1e3a5f" : "#111827",
        }}
      >
        {(currentSlotName ? slotSaving : saving) ? "Saving\u2026" : currentSlotName ? `Save \u2192 ${currentSlotName}` : "Save"}
      </button>

      <button
        type="button"
        onClick={handleLoad}
        disabled={!gameStarted}
        style={btnStyle(!gameStarted)}
      >
        Load
      </button>

      <div style={{ display: "flex", alignItems: "center", gap: 4 }}>
        <label style={{ display: "flex", alignItems: "center", gap: 4, cursor: "pointer" }}>
          <input
            type="checkbox"
            checked={autosaveEnabled}
            onChange={handleToggleAutosave}
            style={{ margin: 0 }}
          />
          <span>Auto</span>
        </label>
        <input
          type="number"
          value={intervalInput}
          onChange={(e) => setIntervalInput(e.target.value)}
          onBlur={handleIntervalBlur}
          onKeyDown={(e) => {
            if (e.key === "Enter") handleIntervalBlur();
          }}
          min={5}
          max={3600}
          style={{
            width: 42,
            padding: "1px 4px",
            fontSize: 11,
            borderRadius: 3,
            border: "1px solid #374151",
            background: "#0b1220",
            color: "#e5e7eb",
            textAlign: "center",
          }}
        />
        <span style={{ color: "#9ca3af" }}>s</span>
      </div>

      <span style={{ color: "#9ca3af", minWidth: 80 }}>
        Saved: {formatSavedAt(lastSavedAt)}
      </span>
    </div>
  );
}
