#pragma once
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "config_manager.hpp"
#include "statue_skill_system.hpp"
#include "ui_types.hpp"

class StatuePassiveSystem {
public:
    struct SkillStats { float duration, cooldown; };

    static SkillStats getSkillStats(const std::string& key, int level, const config::ConfigManager& configMgr) {
        int lvIdx = std::clamp(level - 1, 0, 2);
        if (key == "CARD_ROSARY") {
            const auto& lv = configMgr.skills.rosary.levels[lvIdx];
            return { lv.duration, lv.passiveCooldown };
        } else if (key == "CARD_GOD_RAY") {
            const auto& lv = configMgr.skills.godRay.levels[lvIdx];
            return { lv.duration, lv.passiveCooldown };
        }
        return { 10.f, 5.f }; // Default fallback
    }

    static void update(entt::registry& registry, float deltaTime, const config::ConfigManager& configMgr, std::vector<ui::PassiveSlot>& slots) {
        auto statueView = registry.view<component::StatueTag>();
        if (statueView.begin() == statueView.end()) return;
        auto statue = statueView.front();

        for (int i = 0; i < (int)slots.size(); ++i) {
            auto& slot = slots[i];
            if (!slot.isOccupied) continue;

            auto stats = getSkillStats(slot.cardNameKey, slot.level, configMgr);
            slot.timer += deltaTime;
            float cycleTime = stats.duration + stats.cooldown;

            if (slot.timer >= cycleTime) {
                slot.timer = 0.f;
                StatueSkillSystem::createSkillInstance(registry, statue, slot.cardNameKey, configMgr, slot.level, i);
            }
        }
    }

    static void syncPassivesFromSlots(const std::vector<ui::PassiveSlot>& slots, const config::ConfigManager& configMgr, entt::registry& registry) {
        updateActiveSkillStats(registry, slots, configMgr);
    }

    static void updateActiveSkillStats(entt::registry& registry, const std::vector<ui::PassiveSlot>& slots, const config::ConfigManager& configMgr) {
        // 1. Rosary Sync
        auto rosaryView = registry.view<component::RosarySkill>();
        rosaryView.each([&](auto& rosary) {
            if (rosary.sourceSlotIndex >= 0 && rosary.sourceSlotIndex < (int)slots.size()) {
                const auto& slot = slots[rosary.sourceSlotIndex];
                if (slot.isOccupied && slot.cardNameKey == "CARD_ROSARY") {
                    const auto& lv = configMgr.skills.rosary.levels[std::clamp(slot.level - 1, 0, 2)];
                    rosary.damage = lv.damage;
                    rosary.knockbackForce = lv.knockbackForce;
                    rosary.rotationSpeed = lv.rotationSpeed;
                    rosary.radius = lv.radius;
                    
                    // [USER-REQUESTED] Sync remainingTime with UI timer!
                    if (slot.timer < lv.duration) {
                        rosary.remainingTime = lv.duration - slot.timer;
                    } else {
                        rosary.remainingTime = 0.f; // Force end if UI already in Cooldown
                    }
                }
            }
        });

        // 2. GodRay Sync
        auto godRayView = registry.view<component::GodRaySkill>();
        godRayView.each([&](auto& godRay) {
            if (godRay.sourceSlotIndex >= 0 && godRay.sourceSlotIndex < (int)slots.size()) {
                const auto& slot = slots[godRay.sourceSlotIndex];
                if (slot.isOccupied && slot.cardNameKey == "CARD_GOD_RAY") {
                    const auto& lv = configMgr.skills.godRay.levels[std::clamp(slot.level - 1, 0, 2)];
                    godRay.damage = lv.damage;
                    godRay.range = lv.range;
                    godRay.cooldown = lv.attackInterval;
                    
                    // [USER-REQUESTED] Sync remainingTime with UI timer!
                    if (slot.timer < lv.duration) {
                        godRay.remainingTime = lv.duration - slot.timer;
                    } else {
                        godRay.remainingTime = 0.f;
                    }
                }
            }
        });

        // 3. Statue-based Skills (Holy, Spawn)
        auto statueView = registry.view<component::StatueTag>();
        statueView.each([&](auto entity) {
            if (auto* holy = registry.try_get<component::HolyAttackSkill>(entity)) {
                if (holy->sourceSlotIndex >= 0 && holy->sourceSlotIndex < (int)slots.size()) {
                    const auto& slot = slots[holy->sourceSlotIndex];
                    if (slot.isOccupied && slot.cardNameKey == "CARD_HOLY") {
                        const auto& lv = configMgr.skills.holy.levels[std::clamp(slot.level - 1, 0, 2)];
                        holy->damage = lv.damage;
                        holy->radius = lv.radius;
                        holy->cooldown = lv.cooldown;
                        // Immediate fired skill, timer logic stays internal or we could sync
                    }
                }
            }
            if (auto* spawn = registry.try_get<component::SpawnKnightSkill>(entity)) {
                if (spawn->sourceSlotIndex >= 0 && spawn->sourceSlotIndex < (int)slots.size()) {
                    const auto& slot = slots[spawn->sourceSlotIndex];
                    if (slot.isOccupied && slot.cardNameKey == "CARD_SPAWN_KNIGHT") {
                        const auto& lv = configMgr.skills.spawnKnight.levels[std::clamp(slot.level - 1, 0, 2)];
                        spawn->spawnCount = lv.spawnCount;
                        spawn->cooldown = lv.cooldown;
                    }
                }
            }
        });
    }
};
