# Lua Inspector Metadata — Design Spec

**Date:** 2026-03-25
**Status:** Approved

## Problem

Inspector UI for attributes is controlled by hardcoded checks in the frontend against component names, attribute keys, and types. Adding a new control type (combobox, color picker, progress bar) requires changes in TypeScript. Lua component authors have no way to declare how an attribute should be displayed.

## Goal

Allow each attribute in a Lua component spec to optionally declare an `inspector` sub-table that controls how it is rendered in the frontend. C++ is a transparent forwarder — it stores and serializes the blob without parsing it. New widget types require only Lua and TypeScript changes.

## Architecture

```
Lua spec (inspector = { ... })
  → C++: sol::table → nlohmann::json blob, stored on Attribute
  → DTO: forwarded as-is under "inspector" key
  → TypeScript AttributeDTO.inspector?: InspectorMeta
  → ComponentAttributes.tsx: switches on inspector.widget
```

The contract is exclusively between Lua specs and the TypeScript renderer. C++ has no knowledge of widget types or inspector fields.

## Lua Attribute Schema Extension

The `inspector` key is added as an optional sub-table alongside existing attribute fields:

```lua
my_attr = {
  title = "My Attribute",
  type = AttributeType.FLOAT,
  value = 0.0,
  inspector = {
    widget = "progress",
    min = 0.0,
    max = 100.0,
    unit = "°C",
  },
}
```

Attributes without an `inspector` table render with default behavior (unchanged from today).

## Widget Catalog

### `select`
Combobox with a static options list.
```lua
inspector = { widget = "select", options = {"SEND", "RECEIVE"} }
```
- `options: string[]` — required

### `link`
Picker for a component on the same frame or any world frame.
```lua
-- same-frame component (replaces target_filter)
inspector = { widget = "link", link_scope = "frame", link_filter = "Battery" }
-- world frame (new)
inspector = { widget = "link", link_scope = "world", link_filter = "Miner" }
```
- `link_scope: "frame" | "world"` — defaults to `"frame"`
- `link_filter: string` — optional
  - frame scope: filters by `component.type` among components on the current frame
  - world scope: filters by `frame.name` among all frames in `useGameStore(s => s.frames)`; omit to show all frames
- Stored value for both scopes is an `int` entity ID; -1 means "none"
- World scope is new functionality — no existing component uses it yet

### `progress`
Numeric input with a visual progress bar. Editable by default; `readonly = true` for display-only.
```lua
inspector = { widget = "progress", min = 0.0, max = 100.0, unit = "%" }
inspector = { widget = "progress", min = 0.0, max = 1.0, readonly = true }
```
- `min`, `max` — required for bar rendering
- `unit` — optional suffix

### `color`
Color picker. Attribute must be `AttributeType.STRING`; value stored as hex string.
```lua
inspector = { widget = "color" }
```

### `code`
Code editor. Attribute is excluded from the standard attributes list and rendered as a code editor button in ComponentCard instead.
```lua
inspector = { widget = "code" }
```

### `list`
Display-only list of homogeneous int/float/string values. The attribute must be `AttributeType.STRING`; the value is a comma-separated string (e.g. `"1,2,3"` or `"alpha,beta"`). The renderer splits on `","` and displays each element. Empty string renders as an empty list.
```lua
inspector = { widget = "list" }
```

## Common Inspector Fields

These apply to any widget (or to attributes with no widget, as standalone modifiers). `readonly` in this table is not specific to `progress` — it suppresses editing on any attribute regardless of widget type.

| Field | Type | Description |
|---|---|---|
| `readonly` | bool | No editing — display only |
| `hidden` | bool | Omit from inspector entirely |
| `precision` | int | Decimal places for float display/input |
| `unit` | string | Suffix shown after the value (e.g. `"W"`, `"°C"`) |
| `min` | number | Lower bound (validation + progress bar) |
| `max` | number | Upper bound |
| `label` | string | Overrides `title` for display in the inspector |
| `color` | string | CSS color applied to the attribute row/label |
| `icon` | string | Icon name (game-icons.net) shown next to the label |

## C++ Changes

### `include/game/attributes.hpp`
- Add `nlohmann::json inspector_meta` field to `Attribute` (defaults to null/empty)
- Update constructor signature to accept it
- **Do not persist `inspector_meta` in `save`/`load`** — it is always reconstructed from the Lua spec at startup. Because inspector metadata is a pure UI concern derived from the Lua script, persisting it is unnecessary.
- **Keep `target_filter` in `save`/`load` as a legacy tombstone.** `Attribute` uses positional cereal (not FieldArchive) — the field is the 8th positional argument. Removing it from the `ar()` call would corrupt loading of all existing save files and proto files. After migration it holds no functional value but must remain in the signature to preserve binary compatibility.
- Stop reading `target_filter` from Lua in `component_bindings.cpp` (the `inspector` table replaces it)
- Stop emitting `target_filter` in `serializeAttribute()` (the frontend no longer reads it)

### `src/game/component_bindings.cpp`
- After parsing `easing`, check if `attr_data["inspector"]` is a valid table
- If present: recursively convert `sol::table` → `nlohmann::json` (generic conversion handles strings, numbers, bools, arrays, nested tables)
- Remove existing `target_filter` parsing

### `src/rpc/dto.cpp`
- In `serializeAttribute()`: append `j["inspector"] = attr.inspector_meta` when non-null
- Remove existing `target_filter` serialization

### `include/rpc/dto.hpp` / `src/rpc/dto.cpp`
- Remove `target_filter` from `AttributeDTO` struct and serialization

## TypeScript Changes

### `web/src/rpc/types.ts`
- Add `inspector?: InspectorMeta` to `AttributeDTO` (note: `target_filter` does not exist in this file — it only exists in the local `AttributeDTO` interface inside `ComponentAttributes.tsx`)
- Define and **export** `InspectorMeta` so it can be imported by `ComponentAttributes.tsx`:
```typescript
interface InspectorMeta {
  widget?: "select" | "link" | "progress" | "color" | "code" | "list";
  options?: string[];
  link_scope?: "frame" | "world";
  link_filter?: string;
  min?: number;
  max?: number;
  unit?: string;
  readonly?: boolean;
  hidden?: boolean;
  precision?: number;
  label?: string;
  color?: string;
  icon?: string;
}
```

### `web/src/components/frame/ComponentAttributes.tsx`
- This file defines a **local** `AttributeDTO` interface (not imported from `types.ts`) — update it to remove `target_filter`, add `inspector?: InspectorMeta`, and import `InspectorMeta` from `types.ts`
- Replace `key !== "code"` filter with `inspector?.widget !== "code"` (checking the attribute's inspector metadata, not the key name)
- Replace `target_filter` component-picker logic with `inspector.widget === "link"` dispatch
- Replace conveyor mode hardcoded check with `inspector.widget === "select"`
- Implement renderers for each widget type
- Apply common fields (`readonly`, `hidden`, `precision`, `unit`, `label`, `color`, `icon`) across all renderers
- Unknown or absent `widget` values fall back to the default renderer for the attribute's type (the same behavior as today when no `inspector` table is present)

### `web/src/components/frame/ComponentCard.tsx`
- Detection of the code editor attribute currently uses `component.attributes?.["code"] !== undefined` (a direct key lookup), with the discovered raw value passed to `extractCodeString` and the string `"code"` hardcoded as `attrKey`
- Replace with: iterate `Object.entries(component.attributes ?? {})` and find the first entry `[key, attr]` where `attr.inspector?.widget === "code"`
- Use the discovered `key` for both `attrKey` and as the argument to `extractCodeString` — do not hardcode `"code"`
- If no attribute has `widget = "code"`, `hasCodeAttr` is false

## Migration Plan

### Lua files
All files that currently use `target_filter` or are subject to frontend hacks:

**`scripts/components/conveyor_connector.lua`**
- `mode` attr: add `inspector = { widget = "select", options = {"SEND", "RECEIVE"} }`
- `target` attr: replace `target_filter = "Storage"` with `inspector = { widget = "link", link_scope = "frame", link_filter = "Storage" }`

**`scripts/components/charger.lua`**
- `target` attr: replace `target_filter = "Battery"` with `inspector = { widget = "link", link_scope = "frame", link_filter = "Battery" }`

**`scripts/components/nexus.lua`**
- `target` attr: replace `target_filter = "Storage"` with `inspector = { widget = "link", link_scope = "frame", link_filter = "Storage" }`
- `target_data` attr: replace `target_filter = "Data Connector"` with `inspector = { widget = "link", link_scope = "frame", link_filter = "Data Connector" }`

**`scripts/components/control_relay.lua`**
- `target` attr: replace `target_filter = "Data Connector"` with `inspector = { widget = "link", link_scope = "frame", link_filter = "Data Connector" }`

**`scripts/components/core.lua`** and **`scripts/components/main_core.lua`**
- `code` attr: add `inspector = { widget = "code" }`

### C++ cleanup
- Remove `target_filter` from the `Attribute` constructor and stop reading it from Lua after all Lua files are migrated
- Keep `target_filter` as a field in the class and in the positional cereal `save`/`load` — removing it would break existing saves
- Remove `target_filter` from `serializeAttribute()` DTO output

### Audit before migrating
Before beginning migration:
1. Grep `scripts/components/` for `target_filter` — confirm all instances are listed above
2. Grep `web/src/` for hardcoded component name strings and attribute key strings (e.g. `CONVEYOR_CONNECTOR_NAME`, `attrKey === "mode"`, `attributes\["code"\]`) — confirm no additional frontend hacks exist that also need migration

## Non-Goals

- C++ bindings (callbacks on attribute change) — deferred
- Editable `list` widget — deferred (display-only for now)
- Options sourced from the engine at runtime — deferred
- `link_scope = "world"` with server-side filtering — world frame list is read from the client store; no new RPC needed
