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
- **Unified Combat & VFX Architecture:** 
  - **Unified Damage Pipeline:** All damage sources must call `UnitCombatSystem::applyDamage` (Static Public). This method generates a `DamagedEvent` and handles **Global I-Frames (0.01s)** to prevent overlapping damage bugs.
  - **DamageProcessingSystem:** The central authority for applying health changes and minimum damage (always >= 1) based on `DamagedEvent`.
  - **Data-Driven VFX:** Use `vfxHeight` from `UnitStats` or `StatueStats` for all world-space indicators (damage numbers, bars).
- **UI & Visual Integrity:**
  - **Preservation First:** Never replace established high-fidelity rendering systems (e.g., `UISystem`) with raw implementations during refactoring.
  - **Conditional World UI:** Statue HP Bar is shown only when damaged or recently hit (`hitFlashTimer > 0`). Unit HP Bars are hidden by default to reduce clutter.
- **Skill Progression & Sync:**
  - **Unified Timer Authority:** `CardSystem` (UI) owns the physical slots and timers, but `StatuePassiveSystem` (Logic) is the sole authority for incrementing them.
  - **Proportional Phase-Rescaling:** When a card levels up (merges), its timer MUST be rescaled proportionally based on the current phase (Active or Cooldown) to ensure an intuitive and snappy transition.
  - **Live Synchronization:** Active skill entities (Rosary, God Ray) must track their `sourceSlotIndex` to receive instant stat updates (damage, radius, etc.) when the originating card is upgraded.
- **Robust Initialization:** 
  - **Designated Initializers:** Always use `{ .member = value }` for component initialization (especially `StatueStats`) to prevent aggregate initialization errors when struct orders change.

---

## 🤖 Agent Operating Logic (Base Knowledge)
All agents must operate based on the following context hierarchy:
1. **Context 0 (Fundamental):** `GEMINI.md` - Core tech stack, SOLID principles, and optimization rules.
2. **Context 1 (Safety):** `ERRORMANAGEMENT.md` - Historical errors and prevention checklists.
3. **Context 2 (Specialized):** Agent-specific `.md` - Specialized instructions for the task at hand.

---

## ⚡ Optimization Guidelines
- **Spatial Partitioning:** Always use `ProximityGrid` for nearby entity queries (AI, Combat, Skills). Never use O(N²) nested loops for distance checks.
- **Cache Friendliness:** Utilize EnTT `view` and `group` for efficient iteration. Avoid `registry.get` inside tight loops if `registry.view<...>().each(...)` can be used.
- **Math Optimization:** 
  - Use squared distance (`distSq`) instead of `sqrt` for range checks.
  - **Interaction Buffer:** Add a small buffer (e.g., 5px) to attack ranges in `UnitCombatSystem` to account for AI stopping distances and prevent "near-miss" logical errors.
- **Collision Standards:** Never use point-based collision for units. All interactions must use actual mathematical Collider calculations (Circle-to-Circle, Box-to-Box) based on `.json` radius/size.
- **DeltaTime:** Always use `deltaTime` for frame-independent movement and timers. Note: `deltaTime` is capped at 0.05s in `main.cpp` to prevent "spiral of death".

---

## 📜 Development Rules
1. **System Integrity:** Never modify existing systems in a way that disrupts core loops (Movement, Combat, Waves).
2. **Independent Skills:** Statue skills must be implemented as modular units. Every combat skill should consider/implement:
   - **Cooldown:** Managed via a timer in the component.
   - **Damage:** Applied via `UnitCombatSystem::applyDamage`.
   - **Feedback:** Use `DamagedEvent` for numbers and hit flashes.
   - **Knockback:** Requested via `PhysicsRequest`.
3. **Localization:** All UI strings must be retrieved via `LocalizationManager::getInstance().get("KEY")`.
4. **Safety:** Rigorously check `registry.valid(entity)` before access.
5. **Clean Code:** Adhere to existing naming conventions (PascalCase for classes, camelCase for variables/methods, `component::` namespace for components).
6. **Error Management:** Follow the guidelines in [ERRORMANAGEMENT.md](./ERRORMANAGEMENT.md). **ABSOLUTE RULE: Never silently delete or regress existing features during refactoring.**

---

## 📁 Key Components & Systems
- `ProximityGrid`: 64x64 fixed grid for O(1) cell access.
- `StatueSkillSystem`: Manages Statue-specific active and passive skills.
- `UnitCombatSystem`: The entry point for all combat interactions and I-Frame management.
- `DamageProcessingSystem`: The final stage of the combat pipeline that applies stats.
- `VFXSystem`: Data-driven visual effects (Floating numbers, etc.).
- `EntityFactory`: The source of truth for creating players, enemies, and special entities.
- `ConfigManager`: Loads JSON configs from `configs/`.
