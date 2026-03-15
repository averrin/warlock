import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

export function PowerPanel() {
  const networks = useGameStore((s) => s.powerNetworks);
  const supported = isFeatureSupported(capabilityMethods.powerPanel);

  return (
    <Panel title="Power Networks" unsupported={supported ? undefined : "power panel"}>
      <div style={{ display: "flex", flexDirection: "column", gap: 8, fontSize: 12 }}>
        {networks.length === 0 ? (
          <div style={{ color: "#9ca3af" }}>No power network data</div>
        ) : (
          networks.map((network, index) => (
            <div
              key={index}
              style={{
                border: "1px solid #1f2937",
                borderRadius: 6,
                padding: 6,
                background: "#0b1220",
              }}
            >
              <div>frames: {network.frames.length}</div>
              <div>production: {network.production.toFixed(2)}</div>
              <div>consumption: {network.consumption.toFixed(2)}</div>
              <div>accumulated: {network.accumulated.toFixed(2)}</div>
            </div>
          ))
        )}
      </div>
    </Panel>
  );
}
