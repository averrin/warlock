# State Control & Save System

**Date:** 2026-03-27
**Status:** Approved

## Overview

Two related features:

1. **Init State Management** — select, create, and delete named init state templates. The selected template is used as the starting point when initializing or resetting the game.
2. **Named Save System** — save, load, and delete named snapshots of the running game. Automatic backups are created before destructive operations (load/restore) for emergency recovery.

Both features surface in the **top section of StateInspector**, with init state entity editing available as a dedicated Dockview panel.

---

## File Layout on Disk

```
save/
  current.state              ← unchanged: active working save (binary Cereal)
  slots/
    <name>.state             ← binary save slot
    <name>.meta.json         ← { "name": string, "created_at": ISO8601 }
  backup/
    backup_<timestamp>.state ← auto-backup before each load/restore (no meta; name encodes time)

data/
  init/
    <name>.state             ← init template (binary Cereal, same format as current.state)
    <name>.meta.json         ← { "name": string, "description": string }
    selected.json            ← { "selected": "<name>" } — persisted active selection
```

Slot and init state names are filesystem-safe strings (alphanumeric, spaces, dashes, underscores). Names are stored in meta.json rather than derived from filename to avoid encoding issues.

---

## Backend

### New RPC Endpoints

All new endpoints are added to `src/rpc/handlers/state_handler.cpp`.

#### Save Slots

| Endpoint | Params | Returns | Description |
|---|---|---|---|
| `state.slots.list` | — | `SlotInfo[]` | List all slots in `save/slots/` |
| `state.slots.save` | `{ name: string }` | `SlotInfo` | Copy current state → `save/slots/<name>` + meta |
| `state.slots.load` | `{ name: string }` | `{ backup_name: string }` | Auto-backup current, load slot into current, reload registry |
| `state.slots.delete` | `{ name: string }` | — | Delete `.state` + `.meta.json` |

`SlotInfo`: `{ name: string, created_at: string, path: string }`

#### Init States

| Endpoint | Params | Returns | Description |
|---|---|---|---|
| `state.init.list` | — | `InitInfo[]` | List all templates in `data/init/` |
| `state.init.select` | `{ name: string }` | — | Write selection to `data/init/selected.json` |
| `state.init.save_current` | `{ name: string, description?: string }` | `InitInfo` | Copy current state → `data/init/<name>` + meta |
| `state.init.delete` | `{ name: string }` | — | Delete template `.state` + `.meta.json` |
| `state.init.open` | `{ name: string }` | — | Load template into `init_registry_` for editing |
| `state.init.save_edits` | — | — | Flush `init_registry_` back to `data/init/<name>.state` |
| `state.init.close` | — | — | Unload `init_registry_` (discards unsaved changes) |

`InitInfo`: `{ name: string, description: string, selected: boolean }`

#### Backups

| Endpoint | Params | Returns | Description |
|---|---|---|---|
| `state.backups.list` | — | `BackupInfo[]` | List auto-backups in `save/backup/` |
| `state.backups.restore` | `{ name: string }` | — | Auto-backup current, then restore named backup into current |

`BackupInfo`: `{ name: string, created_at: string }`

#### Init Entity Editing — `init.entities.*`

Mirrors the full `entities.*` RPC namespace but operates on `init_registry_` instead of the live registry:

- `init.entities.list`
- `init.entities.set_field`
- `init.entities.create`
- `init.entities.delete`
- (any other `entities.*` endpoints added in future)

These endpoints return errors if no init state is currently open (i.e., `init_registry_` is null).

### GameManager Changes

`GameManager` gains:
- `init_registry_: std::optional<RegistryStore>` — loaded on demand by `state.init.open`, cleared by `state.init.close`
- `open_init_name_: std::string` — name of the currently open init state (for save_edits)
- `autoBackup()` private helper — copies `current.state` → `save/backup/backup_<timestamp>.state`. Called before every load/restore operation.
- Directory creation on first use for `save/slots/` and `save/backup/`

Auto-backup also triggers on the existing `state.load` endpoint (the original full-state reload).

### Broadcast Events

| Event | Payload | When |
|---|---|---|
| `notify.slots.changed` | — | After slots.save, slots.load, slots.delete |
| `notify.init.changed` | — | After init.select, init.save_current, init.delete, init.save_edits |
| `notify.backups.changed` | — | After any auto-backup |

---

## Frontend

### New Zustand Store: `saveSlots.ts`

Separate from existing `saveState.ts`. Manages all slot/init/backup state.

```ts
interface SaveSlotsStore {
  slots: SlotInfo[]
  initStates: InitInfo[]
  selectedInit: string
  backups: BackupInfo[]

  fetchSlots(rpc: RpcClient): Promise<void>
  saveSlot(rpc: RpcClient, name: string): Promise<void>
  loadSlot(rpc: RpcClient, name: string): Promise<void>
  deleteSlot(rpc: RpcClient, name: string): Promise<void>

  fetchInits(rpc: RpcClient): Promise<void>
  selectInit(rpc: RpcClient, name: string): Promise<void>
  saveCurrentAsInit(rpc: RpcClient, name: string, description?: string): Promise<void>
  deleteInit(rpc: RpcClient, name: string): Promise<void>

  fetchBackups(rpc: RpcClient): Promise<void>
  restoreBackup(rpc: RpcClient, name: string): Promise<void>
}
```

Subscribes to `notify.slots.changed`, `notify.init.changed`, `notify.backups.changed` broadcasts to re-fetch lists automatically.

### StateInspector Top Section

Two `CollapsibleSection` blocks added at the top of `StateInspector.tsx`, above the existing Environment section.

**Init States section:**
- List of init template names; selected one highlighted
- Per-row actions: "Select" (sets active), "Edit entities" (opens `InitEntityEditorPanel`), "Delete"
- "Save current as init..." — inline name + description input + confirm button
- Currently selected template shown prominently (takes effect on next load/reset — `GameManager::loadData()` reads `selected.json` at startup)

**Save Slots section:**
- Name input + "Save" button to create a new named slot
- List of slots: name, creation date, "Load" button, "Delete" button
- Loading a slot shows a brief "Backed up as backup_<timestamp>" confirmation
- Collapsible "Backups" sub-list — timestamped entries with "Restore" button each

### Init Entity Editor Panel

Registered in `WindowWorkspace.tsx` as `initEntityEditor` in `panelComponents`, following the `ComponentCodeEditorPanel` pattern.

**Panel params:** `{ initName: string }`
**Panel id:** `init-entity-editor-<name>` (one panel per init state; opening same init refocuses existing panel)
**Title:** `Init Entities: <name>`

Opening the panel calls `state.init.open { name }`.
The panel renders `EcsEntityInspector` with a `rpcPrefix="init.entities"` prop.
Panel header includes "Save" and "Discard" buttons:
- Save → `state.init.save_edits` then closes panel
- Discard → `state.init.close` then closes panel

Closing the panel via the Dockview × button calls `state.init.close` (treat as discard).

`EcsEntityInspector` receives a `rpcPrefix` prop (default: `"entities"`) and routes all RPC calls through it. No logic is duplicated.

### `WindowWorkspace.tsx` Changes

- Register `initEntityEditor` component in `panelComponents`
- Expose a `openInitEntityEditor(initName: string)` helper that:
  1. Checks if `init-entity-editor-<initName>` already exists and focuses it if so
  2. Otherwise calls `api.addPanel({ id, component: "initEntityEditor", params: { initName } })`
- Expose this helper via `windowLayout` store or pass down through props to `StateInspector`

---

## Error Handling

- `state.slots.load` / `state.backups.restore`: if auto-backup fails, abort the load and return error (never load without a backup succeeding first)
- `init.entities.*`: return `{ error: "no_init_open" }` if `init_registry_` is null
- Slot/init name validation: reject empty names, names with path separators (`/`, `\`, `..`)
- File not found on load/delete: return structured error, frontend shows inline message

---

## Testing

- E2E: save slot → verify file exists + meta.json → load slot → verify auto-backup created → delete slot → verify files gone
- E2E: save current as init → select it → reload game → verify state matches init
- E2E: open init for editing → set_field → save_edits → reload game with that init → verify field persisted
- Unit: name validation rejects path traversal characters
- Unit: autoBackup() creates correctly named file in `save/backup/`
