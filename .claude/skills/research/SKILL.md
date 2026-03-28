---
name: research
description: Add or modify research tree nodes. Use when asked to add new research, gate components or recipes behind tech requirements, restructure the research tree, or rebalance research costs.
argument-hint: "[description of the research node(s) to add or change]"
allowed-tools: Read, Glob, Grep, Edit, Write, Bash, Agent
---

# Research Tree Skill

You are adding or modifying research nodes in the Hellfrost/Warlock game.

User request: $ARGUMENTS

## How the Research System Works

- Research nodes live in `scripts/research/<snake_case>.lua`
- `ResearchManager::load()` auto-discovers all `.lua` files there — no C++ registration needed
- Nodes with `cost = {}` **and** `requires = {}` are **auto-unlocked on game load**
- Components listed in any node's `unlocks` are **hidden from the component picker** until that node is unlocked
- Recipes listed in any node's `unlocks_recipes` are **hidden from machine selectors** until unlocked
- Components absent from all `unlocks` lists are always available
- `requires` prerequisites are enforced at unlock time; all must be unlocked first
- Costs are consumed from the spendable pool (same as frame/component build costs)

## Research Node Format

```lua
return {
  name = "Node Name",              -- unique identifier used everywhere
  description = "What it enables",
  icon = "icon-name.png",          -- from https://game-icons.net/, white, no bg
  cost = {                         -- spendable resources consumed on unlock; {} = free
    ["Electronic Parts"] = 40,
    ["Advanced Chips"] = 10,
  },
  requires = { "Prerequisite Node Name" },  -- must all be unlocked before this can unlock
  unlocks = { "Component Type Name" },      -- component names as used in scripts/components/
  unlocks_recipes = { "Recipe Name" },      -- recipe names as used in scripts/recipes/
}
```

## Current Research Tree

```
Nexus Core (free, auto-unlocked)
├── Basic Power          [20 EP]   → Battery, Charger, Consumer, Capacitor, Power Meter, Load Limiter
│   ├── Power Generation [50 EP, 10 AC] → Generator, Solar Panel; recipes: Spark Stone, Consume Spark Ore/Stone
│   │   ├── Advanced Computing [30 AC, 40 EP] → Advanced Core
│   │   │   └── Mobility         [25 AC, 20 FP] → Lidar, Propulsion
│   │   └── Wireless Systems   [60 EP, 15 AC] (also needs Data Networking)
│   ├── Thermal Management [30 EP] → Cooler, Heater, Heat Sink, Temp Sensor, Life Support, AO Cooler/Heater
│   └── Advanced Production [50 EP, 20 AC] (also needs Data Networking)
│       → Miner, Refinery, Big Storage, Packer; recipes: all base mining/smelting/crafting
└── Data Networking      [40 EP]   → Data Relay, Core, Main Core, Control Relay, Clock
    ├── Logistics         [35 EP, 10 FP] → Conveyor Connector, Conveyor Relay
    ├── Wireless Systems   [60 EP, 15 AC] (also needs Power Generation)
    └── Advanced Production [50 EP, 20 AC] (also needs Basic Power)
```
Key: EP = Electronic Parts, AC = Advanced Chips, FP = Frame Parts

## Cost Guidelines

| Tier | Example | Cost range |
|------|---------|-----------|
| Starter (auto-unlocked) | Nexus Core | `{}` |
| Tier 1 (early game) | Basic Power, Data Networking | 20–40 EP |
| Tier 2 (mid game) | Power Generation, Thermal | 30–60 EP, 0–15 AC |
| Tier 3 (late game) | Advanced Computing, Wireless | 40–60 EP, 15–30 AC |
| Tier 4 (end game) | Mobility | 20–30 AC, 10–25 FP |

Science Packs are defined as a tier-2 spendable item for future use — not yet wired into research costs.

## Step-by-Step Process

### 1. Understand the request
- What components or recipes does it unlock?
- Where does it fit in the tree? (what are its prerequisites, what depends on it?)
- Is it gating something that currently has no gate?

### 2. Find affected components/recipes
```bash
ls scripts/components/   # component names (use the `name` field from the lua file)
ls scripts/recipes/      # recipe names (use the `name` field from the lua file)
grep -r "unlocks" scripts/research/   # see what's already gated
```
Check what the user-facing name is — `unlocks` uses the `name` field from `scripts/components/*.lua`, not the filename.

### 3. Plan tree changes
- If moving a component from one node to a new node, remove it from the old node
- If adding a new component that should be gated, add it to the new node's `unlocks`
- Verify prerequisites exist (the `requires` names must match other nodes' `name` fields exactly)

### 4. Create or update research files
- New node: create `scripts/research/<snake_case>.lua`
- Existing node: edit it directly
- If splitting a node: update the old node's `unlocks`/`unlocks_recipes` and create the new node

### 5. Verify
- All names in `requires` match existing node `name` fields exactly
- All names in `unlocks` match component `name` fields in `scripts/components/`
- All names in `unlocks_recipes` match recipe `name` fields in `scripts/recipes/`
- No circular prerequisites
- Auto-unlock nodes (free + no requires) are intentional
- Run `just test-e2e` to confirm the engine loads without errors
