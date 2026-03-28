---
name: production-chain
description: Add or modify production chains, resource items, recipes, source patches, and spendable outputs. Use when asked to create new resources, processing chains, or balance existing ones.
argument-hint: "[description of the chain or resource to add]"
allowed-tools: Read, Glob, Grep, Edit, Write, Bash, Agent
---

# Production Chain & Resource Skill

You are adding or modifying production chains in the Hellfrost/Warlock game.
A production chain is: **source patch → mined item → processed items → … → spendable output**.

User request: $ARGUMENTS

## Definitions Reference

### Item (`scripts/items/<name>.lua`)
```lua
return {
  name = "Human Readable Name",   -- unique key used everywhere
  description = "Short description",
  stack = 100,                     -- max stack size (100 for raw, 500 for spendable)
  -- optional for spendable/currency items:
  spendable = true,
  tier = 1,                        -- 0 = raw, 1 = basic spendable, 2 = advanced
}
```

### Recipe (`scripts/recipes/<name>.lua`)
```lua
return {
  name = "Recipe Name",            -- unique, shown in UI recipe selector
  description = "What this does",
  inputs = {                       -- empty table {} for mining recipes
    { name = "Input Item", amount = 2 },
  },
  outputs = {
    { name = "Output Item", amount = 1 },
    { name = "Byproduct", amount = 5 },  -- optional secondary outputs
  },
  timeCost = 2.5,                  -- seconds per execution
  powerCost = 250.0,               -- watts consumed while running
  available = { "Refinery" },      -- which components can run this
}
```

**Available machines:**
| Component | Role | Size | Power draw |
|-----------|------|------|------------|
| Miner | Extracts from patches (inputs={}) | M | 750W base |
| Refinery | Multi-input/output processing | M | 500W base |
| Packer | Compacting, pressing, assembly | M | 200W base |
| Generator | Consumes fuel for power (outputs={}) | L | produces power |

### Source Patch (`scripts/patches/<name>.lua`)
```lua
return {
  z_index = 0,
  name = "Deposit Name",
  description = "Flavor text",
  item = "Mined Item Name",        -- must match an item name exactly
  color = { r = 255, g = 200, b = 50, a = 100 },  -- RGBA overlay
  generation = {
    min_width = 10, max_width = 20,
    min_height = 10, max_height = 20,
    fill_probability = 0.45,        -- 0.0–1.0, higher = denser blob
    smoothing_rounds = 3,           -- cellular automata passes
  }
}
```
Shape tips: extreme aspect ratio → rift/vein (e.g. width 30–50, height 3–5). High fill + few rounds → dense. Low fill + many rounds → sparse islands.

### Miner Production Rate (engine feature)
The Miner component has a `production_rate` attribute with SIN easing (range 0.4, period 20s).
Recipe execution time = `timeCost / production_rate`. This means mining speed oscillates between 60–100% of base rate. Factor this into throughput calculations (~80% average).

## Existing Production Chains

### Spark → Power
```
[Sparkstone Deposit] → Miner(1 ore/1s)
  → Refinery: 2 Spark Ore → 1 Spark Stone + 25 Carbon Dust (2.5s)
    → Generator: consumes 1 Spark Stone/5s for power
    → Packer: 25 Carbon Dust → 1 Compacted Carbon Dust (5s)
```
Throughput: 1 miner → 60 ore/min → 24 stones/min → feeds 2 generators (12 stones/min each).

### Iron → Frame Parts
```
[Iron Vein] → Miner(1 ore/1s)
  → Refinery: 4 Iron Ore → 1 Enriched Iron Ore (3s)        [basic]
  → Refinery: 2 Iron Ore + 1 Iron Slag → 2 Enriched Iron Ore (2s)  [catalytic, 4x efficient]
    → Refinery: 2 Enriched Iron Ore → 1 Iron Ingot + 1 Iron Slag (4s)
      → Packer: 2 Iron Ingot → 5 Frame Parts (5s)
```
Catalytic loop: slag from smelting feeds back into enrichment. Barely self-sustaining.

### Copper → Electronic Parts
```
[Copper Deposit] → Miner(1 ore/1s, sine-eased ~48 avg/min)
  → Refinery: 3 Copper Ore → 1 Copper Ingot (3s)
    → Packer: 1 Copper Ingot + 1 Spark Stone → 1 Electronic Parts (5s)
```
Bottleneck: Spark Stone split between Generator and Electronics. Sine easing causes periodic copper starvation.

### Spendable Items (currency)
| Item | Tier | Used For |
|------|------|----------|
| Frame Parts | 1 | Building S/M/L/G frames |
| Ultralight Structures | 1 | Building XS/G frames |
| Electronic Parts | 1 | Adding components to frames |
| Repair Packs | 1 | Repairs |
| Science Packs | 2 | Research |

Frame costs: XS=10 Ultralight, S=25 FP, M=50 FP, L=100 FP, G=50 FP+50 UL.
Component costs: S=10 EP, M=25 EP, L=50 EP.

## Step-by-step Process

### 1. Understand the request
- What is the end product? (new spendable, intermediate, or extending existing chain)
- What source? (new patch or existing resource)
- Any special mechanics? (byproducts, loops, easing, competing demands)

### 2. Balance first
Before writing files, calculate throughput:
- Mining: `60 / timeCost` items/min per miner (×0.8 average with sine easing)
- Processing: `60 / timeCost` runs/min per machine
- Check that inputs can be supplied and outputs consumed
- Cross-chain dependencies (e.g. Spark Stone shared between power and recipes)
- "Barely sustainable" = average throughput just barely meets demand; sine troughs cause starvation

### 3. Create items
One `.lua` per item in `scripts/items/`. Use `snake_case` filenames. Name field is the unique key used in recipes and patches.

### 4. Create patch (if new source)
One `.lua` in `scripts/patches/`. The `item` field must exactly match an item name. Choose color to be visually distinct from existing patches (sparkstone=yellow, iron=brown, copper=orange).

### 5. Create recipes
One `.lua` per recipe in `scripts/recipes/`. Mining recipes have `inputs = {}` and `available = { "Miner" }`. Processing recipes specify machine in `available`. Multiple recipes can target the same machine — player selects which to run.

### 5a. Gate recipes behind research (if appropriate)
New recipes are visible in machines from the start unless listed in a research node's `unlocks_recipes`. If the recipe requires advanced tech or is part of a mid/late-game chain, add its name to the matching research node in `scripts/research/`. Example: `Advanced Production` gates all base production recipes. Check `scripts/research/` to find the right node or create a new one with the `/research` skill.

### 6. Wire up spendable costs (if new spendable)
- C++ frame costs: `include/game/frame_costs.hpp` — `frame_cost_for_size()`
- TS frame costs: `web/src/game/economy.ts` — `FRAME_SIZE_COSTS`
- Component costs: `scripts/components/<name>.lua` — `spendable_cost` table
- Keep C++ and TS costs in sync.

### 7. Verify
- All item names referenced in recipes exist in `scripts/items/`
- Patch `item` field matches an item name
- Recipe `available` machines exist in `scripts/components/`
- No duplicate recipe names
- Throughput math adds up
