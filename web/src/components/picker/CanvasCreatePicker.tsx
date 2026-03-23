import { useEffect, useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { Picker, type PickerItem, type PickerSection } from "./Picker";
import type { PatchType } from "../../stores/patches";

export type CanvasCreatePick =
  | { kind: "frame"; blueprint: string; frameSizeKey: string }
  | { kind: "patch"; patchType: string }
  | { kind: "marker" };

type BlueprintRow = { name: string; size: string };

type Props = {
  isOpen: boolean;
  onClose: () => void;
  rpcClient: RpcClient;
  patchTypes: PatchType[];
  framesEnabled: boolean;
  onSelect: (pick: CanvasCreatePick) => void;
};

export function CanvasCreatePicker({
  isOpen,
  onClose,
  rpcClient,
  patchTypes,
  framesEnabled,
  onSelect,
}: Props) {
  const [frameRows, setFrameRows] = useState<BlueprintRow[]>([]);

  useEffect(() => {
    if (!isOpen || !framesEnabled) {
      setFrameRows([]);
      return;
    }
    void rpcClient
      .call<{ blueprints: Record<string, string> }>("code.blueprints")
      .then((data) => {
        const entries = Object.entries(data.blueprints ?? {});
        setFrameRows(
          entries
            .map(([name, src]) => ({
              name,
              size: src.match(/FrameSize\.(\w+)/)?.[1] ?? "M",
            }))
            .sort((a, b) => a.name.localeCompare(b.name)),
        );
      })
      .catch(() => setFrameRows([]));
  }, [isOpen, framesEnabled, rpcClient]);

  const sections = useMemo((): PickerSection<CanvasCreatePick>[] => {
    const out: PickerSection<CanvasCreatePick>[] = [];
    if (framesEnabled && frameRows.length > 0) {
      const frameItems: PickerItem<CanvasCreatePick>[] = frameRows.map((row) => ({
        id: `frame-${row.name}`,
        label: `${row.name} (${row.size})`,
        data: { kind: "frame" as const, blueprint: row.name, frameSizeKey: row.size },
      }));
      out.push({ id: "frames", heading: "Frames", items: frameItems });
    }
    out.push({
      id: "markers",
      heading: "Markers",
      items: [{ id: "marker", label: "Marker…", data: { kind: "marker" as const } }],
    });
    if (patchTypes.length > 0) {
      const sorted = [...patchTypes].sort((a, b) => a.name.localeCompare(b.name));
      out.push({
        id: "patches",
        heading: "Resource patches",
        items: sorted.map((pt) => ({
          id: `patch-${pt.key}`,
          label: pt.name,
          data: { kind: "patch" as const, patchType: pt.key },
        })),
      });
    }
    return out;
  }, [frameRows, framesEnabled, patchTypes]);

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      sections={sections}
      onSelect={onSelect}
      placeholder="Search create options…"
      heading="Create on canvas"
    />
  );
}
