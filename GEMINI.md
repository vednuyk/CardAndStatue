# Project: CardAndStatue (Massive ECS Game)

## 🛠 Tech Stack
- **Language:** C++20 (MSVC /utf-8)
- **Graphics/Audio:** SFML 3.x
- **ECS Framework:** [EnTT](https://github.com/skypjack/entt)
- **JSON Library:** nlohmann-json
- **Concurrency:** Taskflow
- **Build System:** CMake

---

## 🏗 Architecture & Design Principles (SOLID)
- **Entity Component System (ECS):** Data (Components) and Logic (Systems) must remain separate. Avoid putting logic inside components.
- **Single Responsibility:** Each system (e.g., `AISystem`, `RenderSystem`) should handle exactly one domain.
- **Open/Closed Principle:** New skills and entity types should be added via new components and factory methods without modifying the core game loop.
- **Modular Statue Skills:** 
  - Skills must be independent and **pluggable**.
  - **Activation Mechanism:** Skills are typically granted via the `CardSystem`. When a card is used/dropped on the Statue, the corresponding skill component (e.g., `GodRaySkill`) is attached to the Statue entity.
  - **Autonomous Execution:** Once a component is attached, the `StatueSkillSystem` must automatically detect and process its logic without additional triggers.
  - Apply skills by attaching components to the `Statue` entity or creating instance entities (like `RosarySkill`).
  - Use `EntityFactory` for centralized creation.

---

## ⚡ Optimization Guidelines
- **Spatial Partitioning:** Always use `ProximityGrid` for nearby entity queries (AI, Combat, Skills). Never use O(N²) nested loops for distance checks.
- **Cache Friendliness:** Utilize EnTT `view` and `group` for efficient iteration. Avoid `registry.get` inside tight loops if `registry.view<...>().each(...)` can be used.
- **Math Optimization:** Use squared distance (`distSq`) instead of `sqrt` for range checks unless the actual distance value is required.
- **DeltaTime:** Always use `deltaTime` for frame-independent movement and timers. Note: `deltaTime` is capped at 0.05s in `main.cpp` to prevent "spiral of death".
- **Asset Management:** Centralized via `ConfigManager` and texture maps in `main.cpp`. Avoid redundant disk I/O.

---

## 📜 Development Rules
1. **System Integrity:** Never modify existing systems in a way that disrupts core loops (Movement, Combat, Waves).
2. **Independent Skills:** Statue skills must be implemented as modular units. Every combat skill should consider/implement:
   - **Cooldown:** Managed via a timer in the component.
   - **Damage:** Applied to the target's `UnitStats`.
   - **HitEffect:** Always set `hitFlashTimer = 0.1f` upon impact.
   - **Knockback:** Apply displacement logic to the target's `Transform`.
   - Implementation steps:
     - Define a component in `unit_components.hpp`.
     - Add update logic in `StatueSkillSystem.hpp`.
     - Add a factory/apply method.
3. **Localization:** All UI strings must be retrieved via `LocalizationManager::getInstance().get("KEY")`.
4. **Safety:** Rigorously check `registry.valid(entity)` before access, especially for parent/child relationships.
5. **Clean Code:** Adhere to existing naming conventions (PascalCase for classes, camelCase for variables/methods, `component::` namespace for components).
6. **Error Management:** Follow the guidelines in [ERRORMANAGEMENT.md](./ERRORMANAGEMENT.md) to maintain code integrity and prevent regression/deletion of existing features.

---

## 📁 Key Components & Systems
- `ProximityGrid`: 64x64 fixed grid for O(1) cell access.
- `StatueSkillSystem`: Manages Statue-specific active and passive skills.
- `EntityFactory`: The source of truth for creating players, enemies, and special entities.
- `ConfigManager`: Loads JSON configs from `configs/`.
