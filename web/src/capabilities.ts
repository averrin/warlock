export const capabilityMethods = {
  session: ["session.info", "session.claim", "session.release"],
  frameInspector: ["frame.list", "frame.get", "frame.move", "frame.activate"],
  powerPanel: ["power.networks"],
  codeEditor: ["code.sources", "code.update"],
  environment: ["env.status"],
  blueprintPalette: ["code.blueprints", "frame.create_from_blueprint"],
  componentPalette: ["code.sources", "component.add"],
  connectionEditor: ["connection.list", "connection.create", "connection.remove"],
} as const;

const unsupportedMethods = new Set<string>([
  "code.get_script",
  "code.execute",
]);

export function isFeatureSupported(methods: readonly string[]): boolean {
  for (const method of methods) {
    if (unsupportedMethods.has(method)) {
      return false;
    }
  }
  return true;
}
