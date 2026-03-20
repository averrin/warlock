export const FRAME_SIZES = ["XS", "S", "M", "L", "G"] as const;
export const COMPONENT_SIZES = ["S", "M", "L"] as const;
export const MATERIALS = ["ALUMINIUM", "COPPER", "STEEL", "TITANIUM", "PLASTIC", "GLASS"] as const;
export const CONNECTION_TYPES = ["POWER", "DATA", "CONVEYOR"] as const;

export const STATE_COLORS: Record<string, string> = {
  ACTIVE: "#14532d",
  ACTIVATING: "#3f6212",
  DEACTIVATING: "#78350f",
  DEACTIVATED: "#1f2937",
  COMP_ERROR: "#7f1d1d",
  DESTROYED: "#450a0a",
  BLOCKED: "#4a1d96",
  BROKEN: "#7f1d1d",
  OK: "#14532d",
  WARN: "#78350f",
  BAD: "#7f1d1d",
  ERROR: "#7f1d1d",
};

export const EFFECT_LABELS: Record<string, string> = {
  OVERHEAT: "🔥",
  FREEZE: "❄️",
};
