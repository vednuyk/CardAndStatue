# Project: MyCrazyPerformanceGame (Earth Defense)

## Description
High-performance 2D RTS/Defense game. 
C++, SFML 3.0, EnTT (ECS), CMake, vcpkg.

## Tech Stack & Version Notes
- **SFML 3.0**: 
    - Batch rendering with `sf::VertexArray` or `std::vector<sf::Vertex>` + `sf::PrimitiveType::Triangles`.
    - `sf::VideoMode({w, h})` syntax.
    - `sf::Text` requires font reference.
- **EnTT (3.x)**:
    - Use `for (auto entity : view)` + `registry.try_get<T>(entity)`.
    - `view.size_hint()` for optimization.
- **nlohmann/json**: Used for configuration (supports comments).

## Architecture & Systems
- **Header-only Systems**: Systems in `src/systems/` are mostly static utility classes (e.g., `RenderSystem::render`, `UnitCombatSystem::update`).
- **Data-Oriented Components**: Pure structs in `src/components/unit_components.hpp`.
- **ProximityGrid**: Spatial partitioning for efficient neighbor searches (Enemies).
- **Batch Rendering**: `RenderSystem` sorts by depth (Y-axis) and batches by `TextureID`. Flush happens when texture changes.
- **Config Management**: `ConfigManager` loads from `configs/` directory.

## High-Performance Guidelines
1. **Build Configuration**: Always use **x64-Release**.
2. **Batch Rendering**: Avoid individual `window.draw(sprite)` calls; use `RenderSystem`.
3. **Spatial Query**: Use `ProximityGrid::queryNearby` instead of iterating over all entities.
4. **Multithreading**: Use `std::execution::par` for heavy CPU tasks if needed.

## Project Structure
- `src/components/`: Pure data structs (e.g., `Transform`, `UnitStats`).
- `src/systems/`: Logic implementation (e.g., `UnitCombatSystem`, `RenderSystem`).
- `src/main.cpp`: Orchestration, event handling, and asset loading.
- `configs/`: JSON files for game balance.
- `assets/`: Textures and fonts.

## Coding Conventions
1. **Component Naming**: Use `component::` namespace.
2. **System Naming**: Use CamelCase for system classes.
3. **Texture Mapping**: Map textures via `component::TextureID` enum in `main.cpp`.
4. **Entity Creation**: Always use `EntityFactory` for consistent entity setup.

## Common Tasks
- **Adding a New Enemy**:
    1. Add a new `.json` in `configs/enemies/`.
    2. Add entry to `component::TextureID` if it needs a new texture.
    3. Load texture in `main.cpp`.
- **Adding a New Skill**:
    1. Define skill component in `unit_components.hpp`.
    2. Add logic in `StatueSkillSystem.hpp`.
    3. Update `ConfigManager` if it needs external config.
    4. Handle skill activation (e.g., card drop) in `main.cpp`.
