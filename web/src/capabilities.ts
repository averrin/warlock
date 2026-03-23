export const capabilityMethods = {
  session: ["session.info", "session.claim", "session.release"],
  frameInspector: ["frame.list", "frame.get", "frame.move", "frame.activate", "frame.deactivate"],
  powerPanel: ["power.networks"],
  codeEditor: ["code.sources", "code.update", "code.get_script", "code.completion"],
  codeDefEditor: ["code.update_source"],
  codeExecute: ["code.execute"],
  environment: ["env.status"],
  speedControl: ["game.speed.set", "game.speed.get"],
  blueprintPalette: ["code.blueprints", "frame.create_from_blueprint"],
  componentPalette: ["code.sources", "component.add"],
} as const;

const unsupportedMethods = new Set<string>();

function runtimeUnsupportedMethods(): Set<string> {
  if (typeof window === "undefined") {
    return new Set<string>();
  }
  const methods = (window as { __warlockUnsupportedMethods?: string[] }).__warlockUnsupportedMethods;
  return new Set(methods ?? []);
}

export function isFeatureSupported(methods: readonly string[]): boolean {
  const runtimeUnsupported = runtimeUnsupportedMethods();
  for (const method of methods) {
    if (unsupportedMethods.has(method) || runtimeUnsupported.has(method)) {
      return false;
    }
  }
  return true;
}
