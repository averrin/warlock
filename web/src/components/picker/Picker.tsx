import "react-cmdk/dist/cmdk.css";
import "./picker.css";
import CommandPalette, { filterItems, getItemIndex } from "react-cmdk";
import { useState, useMemo } from "react";
import { createPortal } from "react-dom";

export type PickerItem<T = unknown> = {
  id: string;
  label: string;
  data: T;
};

export type PickerSection<T> = {
  id: string;
  heading: string;
  items: PickerItem<T>[];
};

type Props<T> = {
  isOpen: boolean;
  onClose: () => void;
  items?: PickerItem<T>[];
  sections?: PickerSection<T>[];
  isLoading?: boolean;
  onSelect: (data: T) => void;
  placeholder?: string;
  heading?: string;
};

export function Picker<T>({
  isOpen,
  onClose,
  items = [],
  sections,
  isLoading = false,
  onSelect,
  placeholder = "Search...",
  heading = "Items",
}: Props<T>) {
  const [search, setSearch] = useState("");

  const structuredItems = useMemo(() => {
    const makeListItem = (item: PickerItem<T>) => ({
      id: item.id,
      children: item.label,
      onClick: () => {
        onSelect(item.data);
        onClose();
      },
    });

    if (sections) {
      return sections.map((section) => ({
        id: section.id,
        heading: section.heading,
        items: section.items.map(makeListItem),
      }));
    }

    return [
      {
        id: "items",
        heading,
        items: items.map(makeListItem),
      },
    ];
  }, [items, sections, heading, onSelect, onClose]);

  const filteredItems = search ? filterItems(structuredItems, search) : structuredItems;

  const palette = (
    <CommandPalette
      isOpen={isOpen}
      page="root"
      onChangeOpen={(open) => {
        if (!open) onClose();
      }}
      onChangeSearch={setSearch}
      search={search}
      placeholder={placeholder}
    >
      <CommandPalette.Page id="root">
        {isLoading ? (
          <div style={{ padding: "16px", textAlign: "center", color: "#9ca3af" }}>
            Loading...
          </div>
        ) : (sections ? sections.every((s) => s.items.length === 0) : items.length === 0) ? (
          <div style={{ padding: "16px", textAlign: "center", color: "#9ca3af" }}>
            No items available
          </div>
        ) : filteredItems[0]?.items.length ? (
          filteredItems.map((list) => (
            <CommandPalette.List key={list.id} heading={list.heading}>
              {list.items.map(({ id, ...rest }) => (
                <CommandPalette.ListItem
                  key={id}
                  index={getItemIndex(filteredItems, id)}
                  {...rest}
                />
              ))}
            </CommandPalette.List>
          ))
        ) : (
          <div style={{ padding: "16px", textAlign: "center", color: "#9ca3af" }}>
            No results found
          </div>
        )}
      </CommandPalette.Page>
    </CommandPalette>
  );

  return typeof document !== "undefined" ? createPortal(palette, document.body) : palette;
}
