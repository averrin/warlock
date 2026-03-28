# Lua Scripting API Reference

Components and blueprints in Warlock are scripted in Lua 5.4. This document is a
complete reference for all globals, helpers, and types available to scripts.

---

## Table of Contents

1. [Component Spec Format](#1-component-spec-format)
2. [Calling Convention](#2-calling-convention)
3. [Global Variables](#3-global-variables)
4. [Locator — Finding Components](#4-locator--finding-components)
5. [Attribute Helpers](#5-attribute-helpers)
6. [Component API](#6-component-api)
7. [Data Links (send / receive)](#7-data-links-send--receive)
8. [Item Storage API](#8-item-storage-api)
9. [World API (frameWorld)](#9-world-api-frameworld)
10. [UI API (nexus)](#10-ui-api-nexus)
11. [Enums Reference](#11-enums-reference)
12. [Blueprint Format](#12-blueprint-format)
13. [Worked Examples](#13-worked-examples)
14. [Suggested Improvements](#14-suggested-improvements)

---

## 1. Component Spec Format

A component file must return a single table.

```lua
return {
  -- Required
  name        = "My Component",        -- unique display name; used as type key
  category    = "Power",               -- Power | Data | Production | Movement
                                       -- Structure | Network | Connectors | Other
  description = "What this does.",

  -- Optional
  icon = "icon-name.png",              -- white icon, no background (game-icons.net)

  -- Initial state / size / material
  state    = ComponentState.DEACTIVATED,
  size     = ComponentSize.S,          -- S | M | L
  material = ComponentMaterial.STEEL,  -- ALUMINIUM|COPPER|STEEL|TITANIUM|PLASTIC|GLASS

  -- Co-existence rules (component names)
  require  = { "Power Wire Connector" },  -- must share frame with these
  conflict = { "Other Component" },       -- cannot share frame with these

  -- Attribute definitions
  attributes = {
    my_attr = {
      title       = "Display Name",
      description = "Shown in tooltip.",
      type        = AttributeType.FLOAT,   -- INT | FLOAT | STRING | BOOL
      value       = 1.0,                   -- initial base value

      -- Optional easing (animates the final value)
      easing = {
        type   = AttributeEasingType.SIN,  -- NONE|JITTER|SAW|SIN|RANDOM_STEP
        range  = 0.1,                      -- amplitude
        period = 1000.0,                   -- ms per cycle
      },

      -- Optional inspector hints (for the web UI)
      inspector = {
        readonly  = true,
        widget    = "select",             -- "select" | "progress" | "link"
        options   = { "A", "B" },         -- for widget = "select"
        min       = 0,                    -- for widget = "progress"
        max_attr  = "capacity",           -- progress max from another attribute
        precision = 1,                    -- decimal places for floats
        link_scope  = "frame",            -- for widget = "link"
        link_filter = "Storage",          -- filter linked component by type
      },
    },
  },

  -- Scripted methods callable from blueprints and the web UI
  api = {
    myMethod = function(self, arg1)
      -- `self` is the Component when called from a blueprint
      -- See Section 2 for the full calling convention
      return attr(self, "my_attr")
    end,
  },
}
```

### Built-in injected attributes

Every component automatically receives these extra attributes after creation:

| Key            | Type  | Description                                      |
|----------------|-------|--------------------------------------------------|
| `temp`         | FLOAT | Current component temperature in °C (read-only) |
| `efficiency`   | FLOAT | 0–1 efficiency factor (set by systems)           |
| `counterpart`  | INT   | Connected counterpart component ID, -1 if none (read-only) |

---

## 2. Calling Convention

### From a blueprint (`code` block)

Blueprint code runs in its own Lua chunk. `frame` is a global pointing to the
owning frame. Components are retrieved via `locator`, then their API is called
with the **component** as the explicit first argument:

```lua
-- Blueprint code (inside `code = [[ ... ]]`)
local prop = locator(frame, ".Propulsion")
prop.api.move(frame, "N")     -- first arg is Frame for movement calls

local bat = locator(frame, ".Battery")
local charge = bat.api.getCharge(bat)  -- first arg is the Component
```

### From the web UI (`component.call_api` RPC)

The server always passes the **Frame** as the first argument, plus any extra
args from the JSON payload. Design API functions so that the first parameter is
either the Frame (for world-interaction calls like `move`, `scan`) or is
intentionally ignored when calling from the UI.

### Summary

| Caller          | First arg   | Subsequent args      |
|-----------------|-------------|----------------------|
| Blueprint       | varies      | whatever you pass    |
| RPC call_api    | Frame       | JSON args (max 4)    |
| Tick function   | Frame       | —                    |

---

## 3. Global Variables

### Blueprint globals

These are set in the Lua state for blueprint code execution:

| Name          | Type          | Description                              |
|---------------|---------------|------------------------------------------|
| `frame`       | Frame         | The frame that owns the executing code   |
| `oracle`      | Oracle        | Reserved; currently empty                |

### API-only variables

These are available **only** inside component API definitions (the `api = {}`
table in component spec files), **not** in blueprint code:

| Name          | Type          | Description                              |
|---------------|---------------|------------------------------------------|
| `environment` | Environment   | World environment (temperature, time…)   |
| `frameWorld`  | FrameWorld    | World interaction — see Section 9        |

### Environment fields (read-only)

```lua
environment.temperature   -- float: ambient °C
environment.minutes       -- int:   minutes elapsed today (0–1440)
environment.days          -- int:   total days elapsed
```

### UI access

The Nexus component's `api` table provides UI feedback methods (see Section 10).
There is no global `nexus` variable — access it through the Nexus component.

---

## 4. Locator — Finding Components

`locator` and `locatorAll` replace the older `getComponentByType` /
`getComponentByName` methods. They accept flexible selector strings or tables.

### `locator(frame, selector) → Component | nil`

Returns the **first** component that matches, or `nil`.

```lua
-- By type attribute (all equivalent)
locator(frame, "type:Propulsion")
locator(frame, ".Propulsion")          -- CSS dot-shorthand

-- By component name
locator(frame, "name:Motor Drive")
locator(frame, "Motor Drive")          -- bare string defaults to name search

-- By component ID
locator(frame, "#42")                  -- decimal
locator(frame, "#0x2A")               -- hexadecimal (0x prefix)

-- Table form
locator(frame, { type = "Battery" })
locator(frame, { name = "My Battery" })
```

### `locatorAll(frame, selector) → Component[]`

Returns **all** matching components as a Lua array (may be empty).

```lua
local storages = locatorAll(frame, ".Storage")
for _, s in ipairs(storages) do
  print(s.data.name)
end
```

---

## 5. Attribute Helpers

These work on any object that has a `.data.attributes` map — both `Component`
and `Frame`.

### `attr(obj, key) → value | nil`

Returns the **final** (eased/computed) value of the attribute, or `nil` if the
attribute does not exist.

```lua
local speed   = attr(comp, "speed")    -- number
local enabled = attr(comp, "enabled")  -- bool
local label   = attr(comp, "label")    -- string
```

### `setAttr(obj, key, value)`

Sets the **base** value of an attribute. The final value will drift toward the
base value according to the attribute's easing settings.

```lua
setAttr(self, "mode", "SEND")
setAttr(self, "target", battery.data.id)
setAttr(self, "load", 0.75)
```

### Direct access (advanced)

If you need the raw Attribute object (e.g. to change easing at runtime):

```lua
local a = comp.data.attributes["key"]
if a then
  a:GetFinalValue()          -- returns current value (with easing)
  a:GetBaseValue()           -- returns the set base value
  a:SetBaseValue(newValue)
  a:SetEasing(easingObject)
end
```

---

## 6. Component API

### Properties

```lua
comp.data.id          -- int: unique id
comp.data.name        -- string: component name
comp.data.description -- string
comp.data.attributes  -- map<string, Attribute>
comp.state            -- ComponentState enum value
comp.size             -- ComponentSize enum value
```

### State control (dot notation)

```lua
comp.activate()    -- DEACTIVATED/ERROR → ACTIVATING → ACTIVE
comp.deactivate()  -- ACTIVE → DEACTIVATING → DEACTIVATED
comp.repair()      -- ERROR/BROKEN → DEACTIVATED (clears error)
```

`activate()` and `deactivate()` return `true` if the transition was accepted,
`false` if the component is passive or already in a terminal state. These are
called with **dot notation** — no `self` or colon needed.

### Counterpart

Connector components store a linked partner ID in the `counterpart` attribute:

```lua
local cp_id = attr(comp, "counterpart")  -- int: counterpart id, or -1
```

---

## 7. Data Links (send / receive)

Data link methods are injected into every component's `api` table by the engine.
Use **dot notation** through `comp.api`:

### Sending

```lua
-- Raw string
comp.api.sendRaw("hello")

-- Structured packet
comp.api.send({
  destination = other_comp.data.id,  -- optional: target component id (-1 = broadcast)
  body        = "payload string",
  headers     = { topic = "sensor", priority = "high" },
})

-- Inject directly into own inbox (testing / self-loop)
comp.api.injectRaw("debug")
comp.api.injectPacket({ destination = comp.data.id, body = "test" })
```

### Receiving

```lua
-- Read one raw message (nil if inbox empty)
local msg = comp.api.readRaw()
if msg then print("got: " .. msg) end

-- Read one structured packet
local pkt = comp.api.read()
if pkt then
  print(pkt.source)       -- int: sender component id
  print(pkt.destination)  -- int: intended recipient
  print(pkt.body)         -- string
  print(pkt.headers["topic"])
end

-- Check queue depth without consuming
local depth = comp.api.queueDepthRaw()
local pkts  = comp.api.queueDepthPacket()
```

---

## 8. Item Storage API

Storage methods are injected into the `api` table of components that have an
attached ItemStorage. Use dot notation through `comp.api`:

```lua
comp.api.slotsCount()               -- int: total slot count
comp.api.slots()                    -- ItemSlot[]
comp.api.getStorage()               -- the raw ItemStorage object

-- Look up a stack by item name
local stack = comp.api.getStackByItem("Iron Ore")
if stack then
  print(stack.amount)
  print(stack.item.name)
  print(stack.item.stack)   -- max stack size
  print(stack.item.tier)
end

-- Take a stack out (by slot id)
local taken = comp.api.take(slot.id)

-- Transfer between storages
comp.api.transferTo(dst_storage, stack)
comp.api.transferFrom(src_storage, stack)
```

The raw `ItemStorage` object (from `comp.api.getStorage()` or
`frame:getStorages()`) still supports colon-call methods for direct use:

```lua
local storages = frame:getStorages()  -- ItemStorage[]
for _, st in ipairs(storages) do
  local stack = st:getStackByItem("Iron Ore")
  st:transferTo(other_storage, stack)
end
```

---

## 9. World API (frameWorld)

`frameWorld` is available **only inside component API definitions** — it is not
a global in blueprint code. Component APIs wrap it for safe, scoped access:

| Component                  | Wrapper method         | Underlying call           |
|----------------------------|------------------------|---------------------------|
| Propulsion                 | `prop.api.move(f, dir)`| `frameWorld.moveFrame`    |
| Lidar                      | `lidar.api.scan(f)`    | `frameWorld.scanAdjacent` |
| Near Field Communicator    | `nfc.api.getConnectedFrames(f)` | `frameWorld.nfcFrames` |

### Inside component API definitions

In the `api = {}` table of a component spec file, `frameWorld` is captured as
a local upvalue:

```lua
-- propulsion.lua (api section)
api = {
  move = function(frame, direction)
    local comp = locator(frame, "type:Propulsion")
    local spd = attr(comp, "speed") or 1.0
    return frameWorld.moveFrame(frameWorld, frame.data.id, direction,
                                subcellStep(), spd, 0.0)
  end,
}
```

### Step size constants

These are global and available everywhere:

```lua
subcellStep()   -- 25.0 px — smallest step; matches XS frame unit
cellStep()      -- 75.0 px — one full grid cell
```

---

## 10. UI API (nexus)

The Nexus component's `api` table provides UI feedback. Access it through the
Nexus component on the frame:

```lua
local nex = locator(frame, ".Nexus")

-- Toast notification (visible to the player)
nex.api.showToast("Overheating!", "warning")  -- type: "info" | "warning" | "error"

-- Map markers (persists until removed)
nex.api.setMapMarker(x, y, "Depot", "#ff0000")
nex.api.removeMapMarker("Depot")
nex.api.clearMapMarkers()

-- Read current markers
local markers = nex.api.getMapMarkers()

-- HUD indicator (top bar)
nex.api.setGlobalIndicator("heat", "Frame Heat", "72°C", "#ff6600")

-- Mouse position (world coordinates)
local mx = nex.api.getMouseX()
local my = nex.api.getMouseY()
```

---

## 11. Enums Reference

### AttributeType
```lua
AttributeType.INT     AttributeType.FLOAT
AttributeType.STRING  AttributeType.BOOL
```

### AttributeEasingType
```lua
AttributeEasingType.NONE         -- no animation
AttributeEasingType.JITTER       -- random noise around base
AttributeEasingType.SAW          -- sawtooth wave
AttributeEasingType.SIN          -- sine wave
AttributeEasingType.RANDOM_STEP  -- periodic random jumps
```

### ComponentState
```lua
ComponentState.DEACTIVATED    -- off, idle
ComponentState.ACTIVATING     -- transitioning on (time_switch countdown)
ComponentState.ACTIVE         -- fully operational
ComponentState.DEACTIVATING   -- transitioning off
ComponentState.ERROR          -- faulted; call repair() to reset
ComponentState.BROKEN         -- physically damaged; call repair() to reset
ComponentState.DESTROYED      -- permanently gone
ComponentState.BLOCKED        -- cannot activate (missing requirements)
```

### ComponentSize / FrameSize
```lua
ComponentSize.S   ComponentSize.M   ComponentSize.L
FrameSize.XS   FrameSize.S   FrameSize.M   FrameSize.L   FrameSize.G
```

### ComponentMaterial
```lua
ComponentMaterial.ALUMINIUM   ComponentMaterial.COPPER
ComponentMaterial.STEEL       ComponentMaterial.TITANIUM
ComponentMaterial.PLASTIC     ComponentMaterial.GLASS
```

### ConnectionType
```lua
ConnectionType.POWER    ConnectionType.DATA    ConnectionType.CONVEYOR
```

---

## 12. Blueprint Format

Blueprints configure a pre-built frame, including its components and runtime
code.

```lua
return {
  name       = "My Blueprint",
  size       = FrameSize.M,
  components = {
    "Core",
    "Battery",
    "Power Wire Connector",
  },
  code = [[
return {
  -- Called once when the frame is first activated
  start = function()
    bat = locator(frame, ".Battery")
    bat.activate()
  end,

  -- Called every game tick while ACTIVE
  update = function()
    local charge = bat.api.getCharge(bat)
    if charge < 100 then
      local nex = locator(frame, ".Nexus")
      if nex then nex.api.showToast("Low battery!", "warning") end
    end
  end,

  -- Called every tick while DEACTIVATED
  idle = function() end,

  -- Called once when the frame is deactivated
  stop = function() end,
}
  ]],
}
```

### Blueprint code globals

Inside `code`, only `frame` and `oracle` are available as globals. Use
component APIs to access world state (`frameWorld`), environment data, and
UI functions. Local variables persist between calls within the same session
(but not across saves).

---

## 13. Worked Examples

### Move a frame north when active

```lua
-- propulsion.lua (api section — frameWorld is available as upvalue)
move = function(frame, direction)
  local comp = locator(frame, "type:Propulsion")
  local spd      = attr(comp, "speed") or 1.0
  local moveCost = attr(comp, "move_consumption") or 0.0
  if spd < 0.05 then spd = 0.05 end
  return frameWorld.moveFrame(frameWorld, frame.data.id, direction,
                              subcellStep(), spd, moveCost)
end
```

### Read a temperature and trigger a warning

```lua
-- temperature_sensor.lua (api section)
getComponentTemperature = function(component)
  return attr(component, "temp")
end,

-- In a blueprint update():
local thermo = locator(frame, ".Temperature Sensor")
local t = thermo.api.getComponentTemperature(bat)
if t and t > 80 then
  local nex = locator(frame, ".Nexus")
  if nex then nex.api.showToast("Battery overheating: " .. t .. "°C", "warning") end
end
```

### Route data packets between two connectors

```lua
-- In a blueprint update():
local wire = locator(frame, ".Data Connector")
local pkt = wire.api.read()
if pkt then
  -- Forward to another component by id
  wire.api.send({
    destination = pkt.destination,
    body = pkt.body,
    headers = pkt.headers,
  })
end
```

### Find all batteries and discharge the fullest one first

```lua
local batteries = locatorAll(frame, ".Battery")
table.sort(batteries, function(a, b)
  return (attr(a, "charge") or 0) > (attr(b, "charge") or 0)
end)
if #batteries > 0 then
  batteries[1].activate()
end
```

---

## 14. Suggested Improvements

The following ideas could make the scripting system more powerful and easier
to use. None are implemented yet.

### A. Component event callbacks

Currently scripts poll for state changes every tick. A lightweight event/signal
system would allow components to subscribe to events:

```lua
-- hypothetical future API
onStateChange(comp, function(old, new)
  if new == ComponentState.ACTIVE then
    local nex = locator(frame, ".Nexus")
    if nex then nex.api.showToast("Motor started", "info") end
  end
end)
```

### B. Persistent per-component scratch storage

Local variables in `code` blocks survive ticks but are lost on save/load.
A small key-value store persisted alongside attributes would let scripts save
lightweight state without declaring a formal attribute:

```lua
-- hypothetical: survives save/load, not shown in inspector
scratch.set("last_target_id", id)
local prev = scratch.get("last_target_id")
```

### C. Compound locator queries

Selectors currently match on a single field. Supporting logical operators
would make complex lookups more readable:

```lua
locator(frame, "type:Battery AND state:ACTIVE")
locatorAll(frame, "type:Battery OR type:Capacitor")
```

### D. Typed attribute getters

`attr()` returns the raw variant value. Typed helpers would remove the need
for manual casts and make errors more obvious:

```lua
attrInt(comp, "charge")      -- asserts INT type, returns int
attrFloat(comp, "speed")     -- asserts FLOAT type, returns float
attrBool(comp, "enabled")    -- asserts BOOL type, returns bool
attrStr(comp, "mode")        -- asserts STRING type, returns string
```

### E. Blueprint-to-component message bus

Components can already exchange data packets on connected wires, but there is
no built-in way for a blueprint's `update` function to directly call an API
method on a remote frame's component. A named message bus would enable
frame-to-frame coordination without physical connections:

```lua
-- hypothetical
local nex = locator(frame, ".Nexus")
nex.api.publish("depot.request", { item = "Iron Ore", amount = 10 })
```

### F. `locator` integer overload

Allow passing a component id directly as a number for the common "stored ID"
lookup pattern (currently requires `"#" .. id` string formatting):

```lua
-- hypothetical
locator(frame, comp_id)   -- id as number, no # prefix needed
```

### G. API namespace versioning

As more helpers are added, global namespace pollution becomes a risk. Grouping
helpers under a namespace table would make scripts more self-documenting and
avoid collisions with Lua builtins or future additions:

```lua
wl.locator(frame, ".Battery")
wl.attr(comp, "charge")
wl.setAttr(comp, "target", id)
```
