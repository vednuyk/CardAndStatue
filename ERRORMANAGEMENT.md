# Error Management & Code Integrity Log

## 🛡️ Core Principles for Stability
1. **Lessons Learned:** Document the root cause and solution for every major error to prevent recurrence.
2. **Code Integrity:** Strictly compare code states before and after implementation to ensure existing functionality is preserved.
3. **Validation:** Always verify changes with small-scale tests before final delivery.
4. **Logic Separation (Physics vs Damage):** Never let "Invincibility Frames" (`hitFlashTimer`) block physical movement effects like Knockback or Sweeping. Damage and Physics must be processed independently to prevent clipping/tunnelling.
5. **JSON Data Preservation:** Use Surgical Edits for Config files. Never overwrite whole JSON files if they contain existing critical data like `animation` or `pivotOffset`.
6. **Interface Synchronization:** When changing class constructors or function signatures, immediately synchronize all call sites (especially in `main.cpp`) to prevent chain compilation errors.

## 📝 Error History & Lessons
### [2026-05-12] Card UI Feature Regression
- **Issue:** Layout improvement caused loss of glow effects, correct text alignment, and hover feedback.
- **Cause:** Overwriting a complex rendering system (`UISystem::render`) with a simplified local loop to achieve layout changes.
- **Solution:** Rolled back to the stable `UISystem` integration and applied layout changes surgically only to math constants.
- **Prevention:** **"Never replace, only adjust."** If a system has complex visual feedback, modifications must be restricted to parameter tuning, preserving the established call stack.

### [2026-05-12] Enemy Animation/Rendering Corruption
- **Issue:** Enemy sprites rendered as full sprite sheets instead of individual frames.
- **Cause:** Overwriting `GoblinJunior.json` for physics fields accidentally deleted the `animation` block.
- **Solution:** Restored via Git, implemented surgical JSON updates.
- **Prevention:** Follow "JSON Data Preservation" principle.

### [2026-05-12] Rosary Clipping/Tunnelling
- **Issue:** Fast-moving beads skipped enemies or enemies walked through them during expansion.
- **Cause:** `hitFlashTimer` was blocking all interactions, including physics push.
- **Solution:** Separated damage (conditional) from knockback (unconditional). Added velocity-matching "Sweeping" logic.
- **Prevention:** Follow "Logic Separation" principle.

### [2026-05-12] Widespread Compilation Errors in CardSystem
- **Issue:** Multiple undefined identifiers and signature mismatches.
- **Cause:** Refactoring `CardSystem` to use `configMgr` without updating all call sites or member visibility.
- **Solution:** Stored `configMgr` as a member reference and synchronized `main.cpp`.
- **Prevention:** Follow "Interface Synchronization" principle.

### [2024-05-11] Missing GodRay Components Restoration
- **Issue:** GodRay instances were not correctly initialized after a major refactor.
- **Cause:** Direct modification of the statue entity without maintaining a separate instance for skill duration.
- **Solution:** Restored the missing `GodRay` components while ensuring all legacy components (Rosary, StatueStats, etc.) remained intact.
- **Prevention:** Use `EntityFactory` as the source of truth for all entity creations.

### [2024-05-11] StatueSkillSystem Logic Error
- **Issue:** The `updateGodRay` and `updateGodRayEffects` functions were implemented in `StatueSkillSystem.hpp`, but the calls to these functions were missing in the main `update` loop.
- **Cause:** Incomplete implementation during the transition to modular skills.
- **Solution:** Added the missing function calls within the `StatueSkillSystem::update` method.
- **Prevention:** Verify that all static update methods within a system are called from the primary `update` entry point.
