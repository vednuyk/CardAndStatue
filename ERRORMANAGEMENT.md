# Error Management & Code Integrity Log

## 🛡️ Core Principles for Stability
1. **Lessons Learned:** Document the root cause and solution for every major error to prevent recurrence.
2. **Code Integrity (ABSOLUTE RULE):** Never silently delete or regress existing features during refactoring. If a system is being updated, you MUST read the original code entirely to ensure subtle logic (buffers, UI layers, timers) is preserved or correctly migrated.
3. **Validation:** Always verify changes with small-scale tests before final delivery.
4. **Logic Separation (Physics vs Damage):** Never let "Invincibility Frames" (`hitFlashTimer`) block physical movement effects like Knockback or Sweeping. Damage and Physics must be processed independently.
5. **JSON Data Preservation:** Use Surgical Edits for Config files. Never overwrite whole JSON files if they contain existing critical data like `animation` or `pivotOffset`.
6. **Robust Initialization:** Always use **Designated Initializers** (`{ .member = value }`) for EnTT component emplace calls to prevent "Aggregate Initialization Errors" when struct members are added or reordered.

## 📝 Error History & Lessons
### [2026-05-14] Statue Invincibility Bug (120s)
- **Issue:** Statue took no damage for 120 seconds after the game started.
- **Cause:** Aggregate initialization error. A new member (`invincibilityTimer`) was added to `StatueStats`, but `EntityFactory` was still using positional initialization. The `vfxHeight` value (120) was accidentally assigned to the invincibility timer.
- **Solution:** Switched to Designated Initializers in `EntityFactory`.
- **Prevention:** Follow "Robust Initialization" principle.

### [2026-05-14] Statue Damage Feedback Missing
- **Issue:** Enemies attacked but no damage numbers appeared and HP Bar remained hidden.
- **Cause:** (1) 120s invincibility bug prevented damage. (2) `VFXSystem` only checked `UnitStats`, missing `StatueStats`. (3) `UISystem` only showed HP bar if HP < MaxHP, but HP regenerated instantly before rendering.
- **Solution:** Added `StatueStats` support to `VFXSystem`. Added `hitFlashTimer` check to `UISystem` to keep HP Bar visible during combat.
- **Prevention:** Use "Data-Driven VFX" and "State-based UI" principles from `GEMINI.md`.

### [2026-05-12] Card UI Feature Regression
- **Issue:** Layout improvement caused loss of glow effects and correct text alignment.
- **Cause:** Overwriting `UISystem::render` with a simplified version.
- **Solution:** Rolled back and applied surgical parameter tuning.
- **Prevention:** **"Never replace, only adjust."**

### [2026-05-12] Enemy Animation Corruption
- **Issue:** Enemy sprites rendered as full sprite sheets.
- **Cause:** Accidental deletion of `animation` block in JSON config.
- **Solution:** Restored via Git, followed "JSON Data Preservation".

### [2026-05-12] Rosary Clipping
- **Issue:** Fast-moving beads skipped enemies.
- **Cause:** `hitFlashTimer` was blocking all interactions.
- **Solution:** Separated damage from knockback logic.
