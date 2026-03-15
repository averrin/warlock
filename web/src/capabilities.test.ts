import { describe, expect, it } from "vitest";
import { capabilityMethods, isFeatureSupported } from "./capabilities";

describe("capabilities", () => {
  it("marks currently unsupported code execute flow", () => {
    expect(isFeatureSupported(["code.get_script", "code.execute"])).toBe(false);
  });

  it("marks frame inspector methods as supported", () => {
    expect(isFeatureSupported(capabilityMethods.frameInspector)).toBe(true);
  });
});
