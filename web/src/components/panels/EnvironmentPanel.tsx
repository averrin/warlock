import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

export function EnvironmentPanel() {
  const env = useGameStore((s) => s.environment);
  const supported = isFeatureSupported(capabilityMethods.environment);

  return (
    <Panel title="Environment" unsupported={supported ? undefined : "environment panel"}>
      <div style={{ fontSize: 12, display: "grid", gap: 4 }}>
        <div>day: {String(env?.days ?? "-")}</div>
        <div>minute: {String(env?.minutes ?? "-")}</div>
        <div>is day: {String(env?.is_day ?? "-")}</div>
      </div>
    </Panel>
  );
}
