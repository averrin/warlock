import { describe, expect, it } from "vitest";
import { capabilityMethods, isFeatureSupported } from "./capabilities";

describe("capabilities", () => {
  it("marks code execute flow as supported by default", () => {
    expect(isFeatureSupported(["code.get_script", "code.execute"])).toBe(true);
  });

  it("marks frame inspector methods as supported", () => {
    expect(isFeatureSupported(capabilityMethods.frameInspector)).toBe(true);
  });

  it("marks speed control methods as supported by default", () => {
    expect(isFeatureSupported(capabilityMethods.speedControl)).toBe(true);
  });

  it("marks environment methods as supported by default", () => {
    expect(isFeatureSupported(capabilityMethods.environment)).toBe(true);
  });

  it("respects runtime unsupported methods override", () => {
    const prevWindow = (globalThis as { window?: unknown }).window;
    (globalThis as { window?: { __warlockUnsupportedMethods?: string[] } }).window = {
      __warlockUnsupportedMethods: ["code.execute"],
    };
    expect(isFeatureSupported(capabilityMethods.codeExecute)).toBe(false);
    (globalThis as { window?: unknown }).window = prevWindow;
  });
});
