import "react-cmdk/dist/cmdk.css";
import "./picker.css";
import CommandPalette, { filterItems, getItemIndex } from "react-cmdk";
import { useState, useMemo } from "react";

export type PickerItem<T = unknown> = {
  id: string;
  label: string;
  data: T;
};

type Props<T> = {
  isOpen: boolean;
  onClose: () => void;
  items: PickerItem<T>[];
  isLoading?: boolean;
  onSelect: (data: T) => void;
  placeholder?: string;
  heading?: string;
};

export function Picker<T>({
  isOpen,
  onClose,
  items,
  isLoading = false,
  onSelect,
  placeholder = "Search...",
  heading = "Items",
}: Props<T>) {
  const [search, setSearch] = useState("");

  const structuredItems = useMemo(
    () => [
      {
        id: "items",
        heading,
        items: items.map((item) => ({
          id: item.id,
          children: item.label,
          onClick: () => {
            onSelect(item.data);
            onClose();
          },
        })),
      },
    ],
    [items, heading, onSelect, onClose]
  );

  const filteredItems = search ? filterItems(structuredItems, search) : structuredItems;

  return (
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
        ) : items.length === 0 ? (
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
}
