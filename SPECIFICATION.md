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

### 2.1. Advanced Data Links (Ethernet-based)

*   **Concept:** A robust data network simulation moving beyond simple boolean signals to an Ethernet-like protocol.
*   **Specification:**
    *   **Addressing:** Every capable component must be assigned a unique UUID upon initialization.
    *   **Network Topology:** Data links are 1-to-1 physical connections. Complex topologies require intermediate networking components (Hubs, Switches, Routers).
    *   **Protocols & Services:** Simulated DHCP for dynamic addressing, DNS for component name resolution within Lua scripts.
    *   **RPC Abstraction:** Advanced components expose Remote Procedure Call (RPC) interfaces accessible via the data network, allowing a Core in Frame A to execute functions on a Component in Frame B.
    *   **Hazards:** Environmental hazards (solar flares, radiation spikes) introduce packet loss, latency, or data corruption (e.g., bit flips).

### 2.2. Character Survival & Life Support

*   **Concept:** The player is an active entity in the world, not an omniscient cursor.
*   **Specification:**
    *   **Player Entity:** Introduce a `Player` entity with position, health, and a finite building range (radius).
    *   **Life Support Module:** A critical starting Frame/Component that provides oxygen and thermal regulation for the character. If it loses power or breaks, the player dies (Game Over).
    *   **Building Constraints:** Players can only deploy Frames/Components within their interaction radius.
    *   **Camera Network:** Players can deploy "Camera" components to extend their building and interaction range remotely.

### 2.3. Meta Layer

*   **Concept:** A specialized UI layer overlaying the physical world simulation.
*   **Specification:**
    *   **Notifications:** A system to queue and display alerts (e.g., "Frame Overheating", "Power Grid Overload", "Drone Arrived").
    *   **Indicators:** Visual overlays on Frames showing active state, error states, temperature gradients, or data flow.
    *   **Markers:** Players can place custom waypoints/markers on the map with custom labels and colors for organization.

### 2.4. Link Constraints

*   **Concept:** Physical limitations on how networks are wired.
*   **Specification:**
    *   **Non-Intersection:** Power and Data links cannot intersect physically with `Frame` bounding boxes (hitboxes).
    *   **Routing:** The building system must validate connections. If a straight line intersects a Frame, the connection is invalid. (Future consideration: A* pathfinding for auto-routing cables around frames).

### 2.5. Logistics & Delivery (Gravitational Links & Drones)

*   **Concept:** Moving items/resources between frames without traditional conveyors.
*   **Specification:**
    *   **Gravitational Links:** Dedicated components (e.g., "Mass Drivers" or "Grav-Chutes") that form a network pushing items along defined paths. High throughput, requires complex setup.
    *   **Moving Components:** Components that physically detach and move between frames (e.g., automated carts on rails).
    *   **Drone Delivery:** "Drone Port" components dispatch aerial drones to transport items to target destinations. Low throughput, easy setup, unaffected by ground obstacles (can fly over frames).

---

## 3. Implementation Plan

The implementation is broken down into manageable phases, prioritizing core dependencies first.

### Phase 1: Player Entity & Interaction Constraints

1.  **Add Player Entity:** Define `PlayerComponent` (position, health). Integrate into `State`.
2.  **Building Restrictions:** Modify the `PresentationSystem` or input handler to calculate the distance between the intended build position and the `Player` position (or active `Camera` positions). Reject builds outside this radius.
3.  **Life Support Module:** Create a `LifeSupport` component prototype. Add a `SurvivalSystem` to drain player health if they are outside the Life Support's radius or if the module loses power.

### Phase 2: Link Validation & Constraints

1.  **Hitbox Validation:** In the connection creation logic (likely within the input handling or `GameManager`), implement a 2D line-segment vs AABB (Axis-Aligned Bounding Box) intersection test.
2.  **Enforce Non-Intersection:** Prevent the creation of `Connection` entities if the line between `source_frame` and `target_frame` intersects any other Frame's `wl::rect`.

### Phase 3: Advanced Data Network System

1.  **Data Link Graph:** Create a new `DataNetworkSystem` (similar to `PowerSystem`) that flood-fills `ConnectionType::DATA` connections.
2.  **Addressing:** Add a `NetworkInterface` component containing a UUID and an IP-like address.
3.  **Networking Components:** Implement Router and Switch component logic to bridge different Data Nets.
4.  **RPC & Lua Integration:** Expose the network interface to the Lua state. Allow Lua scripts to send JSON-like payloads to UUIDs.
5.  **Environmental Disruption:** Update the `EnvironmentSystem` to generate radiation events. Update the `DataNetworkSystem` to calculate packet drop rates based on radiation levels and apply them to in-flight RPCs.

### Phase 4: Meta Layer Implementation

1.  **Meta Components:** Introduce `MarkerComponent` and `NotificationComponent` tags.
2.  **UI Integration:** Update the Web GUI RPC backend to transmit active notifications and markers to the client. Update the local ImGui frontend to draw these elements overlaying the game world.

### Phase 5: Logistics & Drones

1.  **Item Storage Refactor:** Ensure `ItemStorage` handles asynchronous insertions/removals robustly.
2.  **Drone Network:**
    *   Create a `Drone` entity with position, target, and payload.
    *   Create a `DroneSystem` to handle drone flight paths (simple point-to-point tweening) and transfer logic upon arrival at a Drone Port.
3.  **Gravitational Links:**
    *   Implement "Pusher" and "Receiver" components.
    *   Create a `LogisticsSystem` that transfers `ItemStack` objects instantly or along a predefined path between valid linked nodes, limited by a throughput capacity attribute.