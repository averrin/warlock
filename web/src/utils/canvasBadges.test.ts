import { describe, expect, it } from "vitest";
import {
  getHealthBadge,
  getPowerBadge,
  getTemperatureBadge,
  getErrorBadge,
} from "./canvasBadges";

describe("getHealthBadge", () => {
  it("returns green for ok", () => {
    const b = getHealthBadge("ok");
    expect(b).not.toBeNull();
    expect(b!.text).toBe("OK");
    expect(b!.color).toBe(0x22c55e);
  });

  it("returns yellow for warn", () => {
    const b = getHealthBadge("warn");
    expect(b!.text).toBe("WARN");
    expect(b!.color).toBe(0xeab308);
  });

  it("returns red for bad", () => {
    const b = getHealthBadge("bad");
    expect(b!.text).toBe("BAD");
    expect(b!.color).toBe(0xef4444);
  });

  it("returns null for undefined", () => {
    expect(getHealthBadge(undefined)).toBeNull();
  });

  it("returns neutral for unknown value", () => {
    const b = getHealthBadge("unknown" as any);
    expect(b).not.toBeNull();
    expect(b!.color).toBe(0x6b7280);
  });
});

describe("getPowerBadge", () => {
  it("returns gray for offline", () => {
    const b = getPowerBadge("offline");
    expect(b!.text).toBe("OFF");
    expect(b!.color).toBe(0x6b7280);
  });

  it("returns red for deficit", () => {
    const b = getPowerBadge("deficit");
    expect(b!.text).toContain("DEF");
    expect(b!.color).toBe(0xef4444);
  });

  it("returns green for balanced", () => {
    const b = getPowerBadge("balanced");
    expect(b!.color).toBe(0x22c55e);
  });

  it("returns blue for surplus", () => {
    const b = getPowerBadge("surplus");
    expect(b!.color).toBe(0x3b82f6);
  });

  it("returns null for undefined", () => {
    expect(getPowerBadge(undefined)).toBeNull();
  });
});

describe("getTemperatureBadge", () => {
  it("formats temperature with degree sign", () => {
    const b = getTemperatureBadge(78.5);
    expect(b).not.toBeNull();
    expect(b!.text).toBe("79°");
  });

  it("returns null for undefined", () => {
    expect(getTemperatureBadge(undefined)).toBeNull();
  });

  it("uses warm color for high temp", () => {
    const b = getTemperatureBadge(100);
    expect(b!.color).toBe(0xef4444);
  });

  it("uses cool color for low temp", () => {
    const b = getTemperatureBadge(-10);
    expect(b!.color).toBe(0x3b82f6);
  });

  it("uses neutral color for moderate temp", () => {
    const b = getTemperatureBadge(25);
    expect(b!.color).toBe(0x6b7280);
  });
});

describe("getErrorBadge", () => {
  it("returns error badge when has_error is true", () => {
    const b = getErrorBadge(true);
    expect(b).not.toBeNull();
    expect(b!.text).toBe("ERR");
    expect(b!.color).toBe(0xef4444);
  });

  it("returns null when has_error is false", () => {
    expect(getErrorBadge(false)).toBeNull();
  });

  it("returns null for undefined", () => {
    expect(getErrorBadge(undefined)).toBeNull();
  });
});
