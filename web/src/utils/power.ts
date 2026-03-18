import type { PowerNetworkDTO } from "../rpc/types";

export function netProduction(net: PowerNetworkDTO): number {
  return net.production - net.consumption;
}

export function netAvailable(net: PowerNetworkDTO): number {
  const accumulatedAvailable = net.accumulated_available ?? 0;
  return netProduction(net) + accumulatedAvailable;
}

export function toDischarge(net: PowerNetworkDTO): number {
  const deficit = net.consumption - net.production;
  if (deficit <= 0) return 0;
  const accumulatedAvailable = net.accumulated_available ?? 0;
  return Math.min(deficit, accumulatedAvailable);
}

export type PowerStatus = "balanced" | "draining" | "brownout";

export function classifyStatus(net: PowerNetworkDTO): PowerStatus {
  const netAvail = netAvailable(net);
  const discharge = toDischarge(net);
  const batteryCount = net.battery_count ?? 0;

  if (netAvail < 0 || (discharge > 0 && batteryCount > 0)) {
    return "brownout";
  }

  if (netProduction(net) < 0 && netAvail >= 0) {
    return "draining";
  }

  return "balanced";
}

export type PowerTrend = "up" | "down" | "flat";

export function classifyTrend(series: number[], windowSize = 5, epsilon = 1e-3): PowerTrend {
  if (series.length === 0) return "flat";
  const n = series.length;
  const k = Math.min(windowSize, n);
  if (k <= 1) return "flat";

  const last = series[n - 1];
  let sum = 0;
  for (let i = n - k; i < n - 1; i++) {
    sum += series[i];
  }
  const mean = sum / (k - 1);
  const diff = last - mean;

  if (diff > epsilon) return "up";
  if (diff < -epsilon) return "down";
  return "flat";
}

