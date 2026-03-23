import { useMemo } from "react";
import { Picker, type PickerSection } from "./Picker";
import type { PatchType } from "../../stores/patches";

type Props = {
  isOpen: boolean;
  onClose: () => void;
  patchTypes: PatchType[];
  isLoading?: boolean;
  onSelect: (patchKey: string) => void;
};

export function SurfacePatchPicker({ isOpen, onClose, patchTypes, isLoading, onSelect }: Props) {
  const sections = useMemo((): PickerSection<string>[] => {
    const sorted = [...patchTypes].sort((a, b) => a.name.localeCompare(b.name));
    return [
      {
        id: "patches",
        heading: "Patch type",
        items: sorted.map((pt) => ({
          id: pt.key,
          label: pt.name,
          data: pt.key,
        })),
      },
    ];
  }, [patchTypes]);

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      sections={sections}
      onSelect={onSelect}
      isLoading={isLoading}
      placeholder="Search patch types…"
      heading="Surface paint"
    />
  );
}
