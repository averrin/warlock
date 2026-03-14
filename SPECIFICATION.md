# Technical Specification & Implementation Plan

This document outlines the detailed specification and implementation roadmap for the Automation Survival Game, bridging the gap between the currently implemented core mechanics (ECS, thermodynamics, power simulation, Lua execution) and the unimplemented features derived from the design concept.

## 1. Existing Systems Overview

The current engine utilizes `entt` for Entity Component System architecture in C++, with `sol2` for Lua scripting and `cereal` for state serialization.

*   **Game State & Entities:** `Frame` entities act as chassis, holding `Component` instances.
*   **EnvironmentSystem (`environment.cpp`):** Handles day/night cycles, ambient temperature tweening, and sun intensity affecting components like Solar Panels.
*   **PowerSystem (`power.cpp`):** Implements a robust graph-based flood-fill to identify "Nets" of connected power components. It calculates production, consumption, battery accumulation (charging/discharging), and handles brownouts/shutdowns when demand exceeds supply.
*   **CodeExecutionSystem (`code_execution.cpp`):** Injects the frame's environment context into isolated `sol::state` instances per frame. Evaluates user-defined Lua code inside the `Core` component, allowing hooks like `update()` to control adjacent components.
*   **PresentationSystem (`presentation.cpp`):** Renders Frames, connections, and hitboxes via ImGui/SFML using `wl::transform` hierarchies.

---

## 2. Unimplemented Features Specification

### 2.1. Procedural Landscape & Resource Generation

*   **Concept:** The game world needs to be dynamically generated to provide replayability and diverse challenges.
*   **Specification:**
    *   **Terrain Generation:** Implement Perlin noise or Simplex noise algorithms to generate terrain elevation, obstacles, and biomes.
    *   **Resource Distribution:** Seed mineral deposits and natural hazards using weighted Voronoi diagrams or scattered noise thresholds. Store these as spatial entities in `entt`.

### 2.2. Extensive Area of Effect (AoE) System

*   **Concept:** Spatial simulation logic for temperature, wireless data, and boosts extending outwards from Frames.
*   **Specification:**
    *   **AoE Component:** Add an `AoE` tag/component to entities defining radius, intensity, and falloff function (linear, inverse-square).
    *   **Thermal AoE:** The `ThermalSystem` must query components within radiuses to calculate convective/radiant heat transfer between adjacent active components.
    *   **Wireless AoE:** Components like Antennas form temporary or short-range data links to overlapping wireless radii.
    *   **Boost AoE:** Specific components (e.g., overclockers) apply efficiency modifiers to any frame intersecting their radius.

### 2.3. Advanced Data Links & Tethering

*   **Concept:** A robust Ethernet-like protocol for data transfer and "Tethering" across the map.
*   **Specification:**
    *   **Addressing:** Every capable component must be assigned a unique UUID upon initialization.
    *   **Tethering:** Advanced data systems allow "tethering"—sharing of Lua library functions, map markers, and sensor info across the network.
    *   **Network Topology:** Data links are 1-to-1 physical connections. Complex topologies require intermediate networking components (Hubs, Switches, Routers).
    *   **RPC Abstraction:** Advanced components expose Remote Procedure Call (RPC) interfaces accessible via the data network.
    *   **Hazards:** Environmental hazards (solar flares, radiation spikes) introduce packet loss or data corruption, challenging the player's Lua fault tolerance.

### 2.4. Meta Layer & Lua APIs

*   **Concept:** A specialized UI/Interaction layer exposed securely through Lua.
*   **Specification:**
    *   **Meta Components:** Introduce `MetaInterface` components that do not perform physical work but act as bridges between Lua code and the UI client.
    *   **Lua APIs:** Bind functions via `sol2` that allow the Lua `Core` to `addMarker(x,y)`, `sendNotification("Alert")`, or `drawIndicator(component_id, color)`.
    *   **GUI Synchronization:** Send these updates over the Web Socket RPC backend for rendering on the final Web GUI.

### 2.5. Character Survival & Life Support

*   **Concept:** The player is an active entity in the world, not an omniscient cursor.
*   **Specification:**
    *   **Player Entity:** Introduce a `Player` entity with position, health, and a finite building range (radius).
    *   **Life Support Module:** A critical starting Frame that regulates oxygen/temperature. Loss of power = Game Over.
    *   **Building Constraints:** Players can only deploy Frames/Components within their interaction radius.
    *   **Camera Network:** Players can deploy "Camera" components to extend their building and interaction range remotely.

### 2.6. Logistics & Link Constraints

*   **Concept:** Movement of items and physical wiring limitations.
*   **Specification:**
    *   **Non-Intersection:** Power and Data links cannot intersect physically with `Frame` bounding boxes (hitboxes).
    *   **Gravitational Links:** Dedicated components (e.g., "Mass Drivers") forming a network pushing items along defined paths.
    *   **Drone Delivery:** "Drone Port" components dispatch aerial drones to transport items. Low throughput, easy setup, unaffected by ground obstacles.

---

## 3. Implementation Plan

The implementation is broken down into manageable phases, prioritizing core dependencies first.

### Phase 1: Procedural Generation & AoE Foundations

1.  **Map Generator:** Implement a noise-based generation system in a new `WorldGenSystem`.
2.  **Spatial Partitioning:** Integrate a Quadtree or Grid-based spatial hash into the `State` to optimize proximity queries for AoE.
3.  **AoE System:** Create a generic `AoESystem` that updates overlapping component lists for heat, wireless, and boosts.

### Phase 2: Player Entity & Interaction Constraints

1.  **Add Player Entity:** Define `PlayerComponent` (position, health).
2.  **Building Restrictions:** Modify the input handler to reject builds outside the `Player` radius or `Camera` radiuses.
3.  **Life Support Module:** Create a `LifeSupport` component prototype and associated `SurvivalSystem`.

### Phase 3: Advanced Data Network System & Tethering

1.  **Data Link Graph:** Create a new `DataNetworkSystem` that flood-fills `ConnectionType::DATA` connections.
2.  **Addressing & RPC:** Add a `NetworkInterface` component. Expose RPC capabilities to Lua state via `sol2`.
3.  **Tethering Implementation:** Create a central Lua registry shared across the `DataNetworkSystem` graph, allowing Frames on the same network to access tethered modules and markers.
4.  **Environmental Disruption:** Update the `EnvironmentSystem` to generate radiation events that calculate packet drop rates for RPCs.

### Phase 4: Meta Layer Implementation

1.  **Meta APIs:** Expose `sol2` bindings for markers, indicators, and notifications within the `CodeExecutionSystem`.
2.  **State Synchronization:** Update the Web GUI RPC backend to transmit active notifications and markers to the client. Update the local ImGui frontend to draw these elements overlaying the game world.

### Phase 5: Link Validation & Logistics

1.  **Hitbox Validation:** Implement a 2D line-segment vs AABB intersection test in the connection creation logic.
2.  **Drone Network:** Create a `Drone` entity and `DroneSystem` to handle flight paths and item transfers.
3.  **Gravitational Links:** Implement "Pusher" and "Receiver" components with a `LogisticsSystem` handling capacity-limited transfers over time.