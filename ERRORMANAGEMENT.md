# Error Management & Code Integrity Log

## 🛡️ Core Principles for Stability
1. **Lessons Learned:** Document the root cause and solution for every major error to prevent recurrence.
2. **Code Integrity:** Strictly compare code states before and after editing to ensure existing features (movement, combat, skills) are never accidentally deleted or broken.
3. **Chain Reaction Prevention:** When editing headers (`.hpp`), prioritize structural integrity (braces, semicolons) as syntax errors there can paralyze the entire project.

---

## 📋 Incident Log (Error & Fix History)

### [2026-05-10] Massive Compilation Failure due to Header Syntax Corruption
- **Symptoms:** Hundreds of "identifier not found" errors in standard libraries (`numeric`, `valarray`) and external headers (`nlohmann-json`).
- **Root Cause Analysis:**
    - During the implementation of the "God Ray" skill, a `replace` operation duplicated namespace braces (`}`) in `unit_components.hpp`.
    - This corruption, combined with a duplicate struct declaration (`GlobalTargetLocation`) outside the namespace, caused the compiler to fail before reaching library includes in `main.cpp`.
    - Resulted in "syntax error: '}' was unexpected" bleeding into every file that included the component header.
- **Resolution:**
    - Performed a full rewrite (`write_file`) of `unit_components.hpp` to normalize the namespace structure.
    - Restored the missing `GodRay` components while ensuring all legacy components (Rosary, StatueStats, etc.) remained intact.
- **Prevention Strategy:**
    - **Context Awareness:** When using `replace`, include more surrounding context (braces/newlines) to ensure the insertion point is unambiguous.
    - **Self-Verification:** Always perform a `read_file` of the entire file after an edit to verify syntax integrity.
    - **Heuristic:** If massive errors appear in system headers, immediately investigate the most recently modified project header for syntax leaks.

### [2026-05-10] Compilation Error: Missing Member in CardSystem
- **Symptoms:** `Error C2039: 'getLastUsedCardKey': is not a member of 'ui::CardSystem'` in `main.cpp`.
- **Root Cause Analysis:**
    - When implementing the "God Ray" skill activation in `main.cpp`, I called `cardSystem.getLastUsedCardKey()` to identify which card was used.
    - However, I neglected to actually define this method and the supporting data member in `card_system.hpp`.
- **Resolution:**
    - Added `std::string m_lastUsedCardKey` private member to `CardSystem`.
    - Updated `handleEvent` to store the dragged card's `nameKey` into `m_lastUsedCardKey` when a drop on the Statue is detected.
    - Implemented the `getLastUsedCardKey()` getter method.
- **Prevention Strategy:**
    - **API Verification:** Before calling a new method on an existing class, always verify that the method exists in the header.
    - **Holistic Implementation:** When a feature requires changes across multiple files (e.g., UI and Main Loop), ensuring the interface (Header) is updated before the implementation (Source) or usage.

### [2026-05-10] Recurrence of Header Syntax Corruption (CardSystem)
- **Symptoms:** Syntax errors (`C2059`, `C2143`) at the end of `card_system.hpp` and subsequent errors in `entt.hpp`.
- **Root Cause Analysis:**
    - Multiple sequential `replace` operations on `card_system.hpp` caused the end of the file to become mangled with overlapping braces and corrupted code fragments (e.g., `> m_triggeredPassiveSkills;`).
    - This is a recurring issue with the `replace` tool's precision when handling closing braces at the end of a file.
- **Resolution:**
    - Performed a full `write_file` to completely restore and correctly implement all methods (including `getLastUsedCardKey`) in `card_system.hpp`.
- **Prevention Strategy:**
    - **Avoid Batch Replaces:** When adding multiple members/methods to a class, prefer a single `write_file` or a single large `replace` covering the entire class body rather than multiple small increments.
    - **Final Verification:** Always check for "ghost" characters or trailing corruption at the end of the file after an edit.

### [2026-05-10] Critical Logic and Syntax Corruption in CardSystem
- **Symptoms:** `C2065: 'score' undeclared`, `C2065: 'card' undeclared`, and `is not a member of ui::entt` (namespace leakage).
- **Root Cause Analysis:**
    - A botched `write_file` operation in `card_system.hpp` introduced several logic errors:
        - `distSq` calculation was circular (`diff.x * distSq`).
        - `score` variable was used without declaration.
        - Garbage code fragments were appended to the end of the file, preventing the `ui` namespace from closing properly.
    - The unclosed `ui` namespace caused all subsequent includes (like `entt.hpp`) to be interpreted as being inside `ui`, leading to project-wide failures.
- **Resolution:**
    - Rewrote `card_system.hpp` from scratch using `write_file` with verified logic for the `update` method and clean closing braces.
    - Fixed the `distSq` and `score` logic.
- **Prevention Strategy:**
    - **Logical Consistency:** Triple-check math formulas and variable declarations in generated code snippets.
    - **Namespace Hygiene:** Always ensure that every opening `{` has a corresponding closing `}` in the same logical block, especially for namespaces wrapping entire files.
    - **Zero-Tolerance for Garbage:** If a tool operation produces trailing noise at the end of a file, it must be purged immediately.

### [2026-05-10] Final Quality Polish: Math Typo and Redefinition Cleanup
- **Symptoms:** Logic bug in card hovering/selection due to incorrect distance calculation. Potential redefinition warnings in `main.cpp`.
- **Root Cause Analysis:**
    - A typo was introduced in the `distSq` formula (`diff.x * distSq`) during the previous emergency fix of `card_system.hpp`.
    - The "redefinition of component::Transform" error in `main.cpp` was confirmed to be a cascading compiler error from the unclosed `ui` namespace in the header, not an actual duplicate in the source file.
- **Resolution:**
    - Corrected the `distSq` formula to `diff.x * diff.x + diff.y * diff.y`.
    - Verified `main.cpp` structure and confirmed no duplicate definitions exist after the header fix.
- **Prevention Strategy:**
    - **Peer-Review Logic:** Always re-read math formulas after a "quick fix" to ensure fundamental laws (like the Pythagorean theorem) are correctly applied.
    - **Distinguish Cascades:** Recognize that "redefinition" errors for types defined in headers are often just signs of a broken namespace or include guard in that header.

### [2026-05-10] Logic Integration Oversight: God Ray Activation Failure
- **Symptoms:** God's Ray card is consumed, but no visual effect or damage occurs in-game.
- **Root Cause Analysis:**
    - The `updateGodRay` and `updateGodRayEffects` functions were implemented in `StatueSkillSystem.hpp`, but the calls to these functions were omitted from the main `update` loop during a manual recovery phase.
    - `ConfigManager` was missing the `GodRay` configuration fields, and `Skills.json` was not updated to provide external values.
- **Resolution:**
    - Re-inserted the `updateGodRay` call inside the `StatueSkillSystem::update` loop.
    - Added `GodRay` struct to `SkillConfig` and implemented the loading logic in `ConfigManager`.
    - Updated `main.cpp` to properly initialize the `GodRaySkill` component using config values.
- **Prevention Strategy:**
    - **Integration Checklist:** When adding a new system or feature, verify the full chain: Data (JSON) -> Load (Config) -> Apply (Main/Factory) -> Update (System Loop).
    - **Verification Loop:** After implementing a function body, immediately verify where it is being called.

### [2026-05-10] Compilation Error: Incomplete Struct Definition in ConfigManager
- **Symptoms:** `Error C2039: 'godRay': is not a member of 'config::SkillConfig'` in `config_manager.hpp` and `main.cpp`.
- **Root Cause Analysis:**
    - While the loading logic for "God Ray" was added to `ConfigManager::loadSkills`, the actual data member was missing from the `SkillConfig` struct definition.
    - This resulted in an attempt to access a non-existent member, causing cascading errors across the project.
- **Resolution:**
    - Properly added the `GodRay` nested struct and the `godRay` instance member to `SkillConfig` in `config_manager.hpp`.
- **Prevention Strategy:**
    - **Struct-First Implementation:** When adding support for new data (JSON), always update the data container (Struct/Class) before writing the loading or usage logic.
    - **Cross-Reference Check:** Ensure that every variable used in a function exists in its parent scope.

### [2026-05-10] Severe Syntax Corruption and Duplication in StatueSkillSystem
- **Symptoms:** `C3927`, `C2065`, and `C2059` errors indicating broken declarators, undeclared identifiers, and mismatched braces at the end of `StatueSkillSystem.hpp`.
- **Root Cause Analysis:**
    - A `replace` operation failed to correctly manage the insertion point, leading to duplicated code fragments (e.g., `fect ...`) being appended after the closing class brace.
    - This corruption was caused by overlapping context during a manual fix attempt, once again highlighting the risks of incremental `replace` calls at file boundaries.
- **Resolution:**
    - Performed a full `write_file` to completely reset and verify the structure of `StatueSkillSystem.hpp`.
    - Verified all related systems (`main.cpp`, `ConfigManager.hpp`) to ensure the entire skill chain is correctly invoked and consistent.
- **Prevention Strategy:**
    - **Boundary Vigilance:** When editing near the end of a file, prefer `write_file` or a `replace` that includes the final `};` or namespace closing.
    - **Total Chain Verification:** Never assume a fix is complete by looking at one file; always trace the data and logic flow across all associated scripts (JSON -> Config -> System -> Main).

### [2026-05-10] Logic Refactoring: Decoupling Knockback Multipliers
- **Symptoms:** God's Ray knockback was disproportionately strong compared to its JSON values.
- **Root Cause Analysis:**
    - A hardcoded `* 15.f` multiplier and fixed `0.2s` duration were used in `StatueSkillSystem.hpp` for God's Ray, bypassing the intended balance in `Skills.json`.
- **Resolution:**
    - Removed hardcoded multipliers from the system logic.
    - Added `knockbackDuration` parameter to the `SkillConfig` and `GodRaySkill` component.
    - Exposed `knockbackDuration` in `Skills.json`, allowing designers to tune both force and time externally.
- **Prevention Strategy:**
    - **No "Magic Numbers":** Avoid arbitrary multipliers in logic scripts. If a value needs to be "boosted," do it via the configuration file or a dedicated balancing factor exposed to the config manager.

### [2026-05-10] Critical Recovery: Missing Member and Total Consistency
- **Symptoms:** Persistent `knockbackDuration` is not a member error despite previous successful edit reports.
- **Root Cause Analysis:**
    - Incremental `replace` edits to `config_manager.hpp` failed to update the struct definition correctly, likely due to match ambiguity or tool latency.
    - Usage in `main.cpp` became misaligned with the header definition.
- **Resolution:**
    - Performed a **Total Write (`write_file`)** for both `config_manager.hpp` and `main.cpp` to force-sync the codebase.
    - Verified that `SkillConfig::GodRay` now explicitly includes `knockbackDuration`.
    - Verified that `main.cpp` correctly references the member for component initialization.
- **Prevention Strategy:**
    - **Zero-Tolerance for Incremental Failure:** If an incremental `replace` fails to resolve a member error, immediately switch to a full `write_file` for that script to purge any hidden state or corruption.
    - **Mandatory Peer Check:** Always read the file content back after a write to confirm the presence of the critical member.

### [2026-05-10] Physics Unification & Damping Implementation
- **Symptoms:** God's Ray knockback was overly powerful and felt "teleporty," while Rosary felt inconsistent because they used different movement logic.
- **Root Cause Analysis:**
    - Rosary used direct position modification (vulnerable to AI speed cancellation), while God's Ray used the additive `Knockback` component.
    - `Knockback` component lacked "Damping" (friction), causing entities to move at 100% force until the timer expired, creating an abrupt, unnatural stop.
- **Resolution:**
    - **Unified Logic:** Refactored `StatueSkillSystem.hpp` so that **both** Rosary and God's Ray exclusively use the `Knockback` component.
    - **Force Damping:** Updated `UnitMovementSystem.hpp` to apply a 15% decay (`* 0.85f`) to the knockback force each frame. This creates a "burst then slide" effect.
    - **Balanced Tuning:** Adjusted `Skills.json` to compensate for the unified physics (Rosary 400, God's Ray 1200 with 0.15s duration).
- **Prevention Strategy:**
    - **Unified Systems:** Always implement a single "Source of Truth" for physical movement to ensure all skills behave predictably according to their configuration values.

---
*(Add new entries here as errors occur)*

### [2026-05-11] Compilation Errors: SFML Type Mismatch and Syntax Duplication
- **Symptoms:** 
    - `Error C2039: 'Uint8': is not a member of 'sf'` in `main.cpp`.
    - `Error C2059: syntax error: 'if'` and `Error C2238` at the end of `statue_skill_system.hpp`.
- **Root Cause Analysis:**
    - **Type Mismatch:** Assumed `sf::Uint8` existed in SFML 3.x based on legacy patterns (SFML 2.x), but SFML 3.x uses standard C++ types or has changed namespace members. In this project's environment, `uint8_t` is the standard for color alpha casting.
    - **Syntax Duplication:** A `replace` operation on `statue_skill_system.hpp` accidentally appended duplicate cleanup code and mismatched braces at the end of the file due to an overlapping match pattern.
- **Resolution:**
    - Replaced all occurrences of `sf::Uint8` with `uint8_t` in `main.cpp`.
    - Cleaned up the trailing garbage code and restored the class structure in `statue_skill_system.hpp` using a targeted `replace`.
- **Prevention Strategy:**
    - **Verify External Types:** Always cross-reference documentation or existing code for external library types (like SFML) rather than assuming legacy naming conventions.
    - **Tool Precision:** When performing `replace` near file boundaries or closing braces, use wider context to ensure the tool doesn't misidentify the insertion point or leave trailing fragments.
    - **Post-Edit Sanity Check:** Review the end of the file after any multi-line `replace` to catch trailing syntax corruption immediately.
