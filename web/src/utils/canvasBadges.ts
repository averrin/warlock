import type { CanvasBadgeHealth, CanvasBadgePower } from "../rpc/types";

export interface BadgeToken {
  text: string;
  shortText: string;
  color: number;
}

const GREEN = 0x22c55e;
const YELLOW = 0xeab308;
const RED = 0xef4444;
const BLUE = 0x3b82f6;
const GRAY = 0x6b7280;

const HEALTH_MAP: Record<string, BadgeToken> = {
  ok: { text: "OK", shortText: "O", color: GREEN },
  warn: { text: "WARN", shortText: "W", color: YELLOW },
  bad: { text: "BAD", shortText: "!", color: RED },
};

const POWER_MAP: Record<string, BadgeToken> = {
  offline: { text: "OFF", shortText: "-", color: GRAY },
  deficit: { text: "DEF", shortText: "D", color: RED },
  balanced: { text: "BAL", shortText: "=", color: GREEN },
  surplus: { text: "SUR", shortText: "+", color: BLUE },
};

export function getHealthBadge(
  health: CanvasBadgeHealth | undefined,
): BadgeToken | null {
  if (health === undefined) return null;
  return HEALTH_MAP[health] ?? { text: health.toUpperCase(), shortText: "?", color: GRAY };
}

export function getPowerBadge(
  power: CanvasBadgePower | undefined,
): BadgeToken | null {
  if (power === undefined) return null;
  return POWER_MAP[power] ?? { text: power.toUpperCase(), shortText: "?", color: GRAY };
}

export function getTemperatureBadge(
  temperature: number | undefined,
): BadgeToken | null {
  if (temperature === undefined) return null;
  const rounded = Math.round(temperature);
  const label = `${rounded}°`;
  let color = GRAY;
  if (temperature >= 80) color = RED;
  else if (temperature <= 0) color = BLUE;
  return { text: label, shortText: label, color };
}

export function getErrorBadge(
  hasError: boolean | undefined,
): BadgeToken | null {
  if (!hasError) return null;
  return { text: "ERR", shortText: "!", color: RED };
}
