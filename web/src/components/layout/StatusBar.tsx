import { useConnectionStore } from "../../stores/connection";
import { useGameStore } from "../../stores/game";
import { useSceneStore } from "../../stores/scene";

export function StatusBar() {
  const status = useConnectionStore((s) => s.status);
  const claimed = useConnectionStore((s) => s.claimed);
  const gameStarted = useGameStore((s) => s.started);
  const env = useGameStore((s) => s.environment);
  const tick = useSceneStore((s) => s.snapshot.tick);

  return (
    <div
      style={{
        gridColumn: "1 / span 2",
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
    </div>
  );
}
