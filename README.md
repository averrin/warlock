# Automation Survival Game Concept

## Overview

This is an automation survival game with a focus on hardcore, nearly scientific simulation. The game takes a top-down 2D, grid-based approach to building and management, challenging players to survive in a harsh environment by optimizing their setups through coding and careful resource management.

Instead of constructing traditional buildings, players deploy "Frames" of varying sizes. These frames act as chassis that can be populated with different "Components." Components do not operate autonomously; they must be orchestrated by a "Core" component, which executes user-written Lua code to manage the frame's operations.

Mistakes are costly. Inefficient coding, overheating, lack of resources, or improper power management will lead to long pauses, breakage, and potentially catastrophic failures.

## Core Mechanics

### Frames & Components

*   **Frames:** The physical housing for your machinery. They come in four sizes: Small (S), Medium (M), Large (L), and Giant (G). A frame has a strict limit on the number and size of components it can hold. For example, a Small frame might hold 8 Small components or 1 Medium component.
*   **Components:** The functional units of the game. They range from simple passive items to active machines requiring power and data connections.
    *   **Component Sizes:** Small (S), Medium (M), Large (L).
    *   **Component Types:** Batteries, Chargers, Consumers, Generators, Solar Panels, Miners, Refineries, Storage, Sensors (Temperature, etc.), and more.
    *   **Core Component:** A special component that allows the execution of Lua scripts. This is the brain of the frame, required to automate and control other components within the same frame.

### Lua Programming

Automation is not achieved through simple drag-and-drop logic gates but through writing actual Lua code.
*   The `Core` component executes user-provided Lua scripts.
*   Scripts define lifecycle hooks (e.g., `start`, `update`, `idle`, `stop`).
*   Scripts can read sensor data, monitor power levels, check storage, and activate/deactivate other components within the frame.
*   *Optimal working requires coding.* Players must write efficient scripts to balance load, manage heat, and ensure continuous operation.

### Hardcore Simulation

The environment is unforgiving and fully simulated:
*   **Thermal Simulation:** Temperature is a critical factor. Components generate heat (e.g., batteries discharging, generators running). If heat is not dissipated using coolers or heat sinks, components can overheat (or freeze in extreme cold), leading to the `ERROR` or `BROKEN` states.
*   **Day-Night Cycle:** The environment features a realistic day-night cycle, directly affecting solar power generation and ambient temperature.
*   **Weather/Environment:** Factors like sun intensity, airflow, and ambient temperature fluctuate dynamically.
*   **Power & Data Networks:** Frames must be connected via Power and Data networks. Players must balance power production (generators, solar) with consumption and storage (batteries). The power system models production, consumption, load, and battery charging/discharging rates.

## Advanced Systems

*   **Data Links:** Built on an Ethernet-like protocol. 1-to-1 links are straightforward, but complex topologies require Routers, DHCP/DNS servers, etc. UUIDs are used for addressing, and advanced components can expose an RPC abstraction. Environmental factors matter: solar radiation or other hazards can cause data loss or corruption, forcing players to write fault-tolerant code.
*   **Link Constraints:** Connections between frames must be routed carefully, as links are not allowed to intersect with frames.
*   **Character Survival:** The player starts with a Life Support Module that must be protected. Initially, the player character can move around, but their building ability is limited to a set range around them. Later in the game, this range can be extended by deploying camera networks.
*   **Meta Layer:** A dedicated interface layer exists to provide players with notifications, indicators, and the ability to set custom map markers.
*   **Logistics & Delivery:** Traditional conveyors are eschewed in favor of "gravitational links" or more advanced systems. There will be specific components dedicated to movement, allowing players to build complex delivery networks. Alternatively, drone delivery offers an easier setup but with a severely limited throughput.

## Technical Architecture

The game is built using modern C++ with an Entity Component System (ECS) architecture.

*   **Backend / Core Engine:** C++ utilizing the `entt` library for the ECS. This allows for highly performant and modular simulation of thousands of components and intricate systems.
*   **Systems:** Dedicated systems handle specific simulation aspects, such as `PowerSystem` (network calculation and distribution), `EnvironmentSystem` (weather and day/night cycle), `ThermalSystem` (heat transfer and status effects), and `CodeExecutionSystem` (Lua scripting via `sol2`).
*   **Serialization:** State is serialized using the `cereal` library for saving and loading.
*   **GUI / Frontend:** The current debug GUI uses ImGui, but the intended architecture includes a Web Socket RPC backend to interface with a Web GUI for the final presentation layer.
*   **Scripting:** Lua (`sol2`) is used heavily, not just for player automation, but also for defining prototypes, blueprints, and data definitions within the engine.

## The Challenge

To survive and thrive, a player must:
1.  Design optimal physical layouts within the constraints of Frames.
2.  Wire robust power and data networks, ensuring data integrity.
3.  Write resilient Lua code to handle normal operations, edge cases, data corruption, and emergency shutdowns.
4.  Manage thermodynamics to prevent catastrophic meltdowns or freezing.
5.  Adapt to a dynamic, ever-changing environment while expanding logistics networks to support survival.
