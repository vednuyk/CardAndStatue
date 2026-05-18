#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <limits>
#include <random>
#include "../components/unit_components.hpp"
#include "entity_factory.hpp"
#include "proximity_grid.hpp"
#include "skills/rosary_system.hpp"
#include "skills/god_ray_system.hpp"

class StatueSkillSystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        // 1. Core Statue Logic & Simple Skills
        updateStatueCore(registry, deltaTime, enemyGrid);

        // 2. Delegate to Specialized Systems
        RosarySystem::update(registry, deltaTime, enemyGrid);
        GodRaySystem::update(registry, deltaTime, enemyGrid);
    }

    static void createSkillInstance(entt::registry& registry, entt::entity statue, const std::string& skillKey, const config::ConfigManager& configMgr, int level = 1, int sourceSlotIndex = -1) {
        int lvIdx = std::clamp(level - 1, 0, 2);

        if (skillKey == "CARD_ROSARY") {
            const auto& lv = configMgr.skills.rosary.levels[lvIdx];
            component::RosarySkill rosary;
            rosary.damage = lv.damage;
            rosary.knockbackForce = lv.knockbackForce;
            rosary.rotationSpeed = lv.rotationSpeed;
            rosary.radius = lv.radius;
            rosary.duration = lv.duration;
            rosary.passiveCooldown = lv.passiveCooldown;
            rosary.owner = statue;
            rosary.sourceSlotIndex = sourceSlotIndex;
            registry.emplace<component::RosarySkill>(registry.create(), rosary);
        } 
        else if (skillKey == "CARD_GOD_RAY") {
            const auto& lv = configMgr.skills.godRay.levels[lvIdx];
            auto& godRay = registry.emplace<component::GodRaySkill>(registry.create());
            godRay.owner = statue;
            godRay.cooldown = lv.attackInterval;
            godRay.damage = lv.damage;
            godRay.range = lv.range;
            godRay.stunDuration = 0.5f; 
            godRay.duration = lv.duration;
            godRay.passiveCooldown = lv.passiveCooldown;
            godRay.remainingTime = godRay.duration;
            godRay.timer = godRay.cooldown; 
            godRay.sourceSlotIndex = sourceSlotIndex;
        } 
        else if (skillKey == "CARD_HOLY") {
            if (!registry.any_of<component::HolyAttackSkill>(statue)) {
                const auto& lv = configMgr.skills.holy.levels[lvIdx];
                auto& holy = registry.emplace<component::HolyAttackSkill>(statue);
                holy.cooldown = lv.cooldown;
                holy.damage = lv.damage;
                holy.radius = lv.radius;
                holy.timer = holy.cooldown;
                holy.sourceSlotIndex = sourceSlotIndex;
            }
        } 
        else if (skillKey == "CARD_SPAWN_KNIGHT") {
            if (!registry.any_of<component::SpawnKnightSkill>(statue)) {
                const auto& lv = configMgr.skills.spawnKnight.levels[lvIdx];
                auto& spawn = registry.emplace<component::SpawnKnightSkill>(statue);
                spawn.cooldown = lv.cooldown;
                spawn.spawnCount = lv.spawnCount;
                spawn.timer = spawn.cooldown;
                spawn.sourceSlotIndex = sourceSlotIndex;
            }
        }
    }

private:
    static void updateStatueCore(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats>();
        statueView.each([&](auto statueEntity, auto& transform, auto& stats) {
            if (auto* holy = registry.try_get<component::HolyAttackSkill>(statueEntity)) {
                updateHolyAttack(registry, *holy, transform, deltaTime, enemyGrid);
            }

            if (auto* spawn = registry.try_get<component::SpawnKnightSkill>(statueEntity)) {
                updateSpawnKnight(registry, *spawn, transform, deltaTime);
            }
        });
    }

    static void updateHolyAttack(entt::registry& registry, component::HolyAttackSkill& skill, const component::Transform& trans, float dt, ProximityGrid& enemyGrid) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            bool hitAny = false;
            enemyGrid.queryRange(trans.position, skill.radius, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                if (UnitCombatSystem::applyDamage(registry, enemy, skill.damage, 0.1f)) {
                    hitAny = true;
                }
            });
            if (hitAny) skill.timer = 0.f;
        }
    }

    static void updateSpawnKnight(entt::registry& registry, component::SpawnKnightSkill& skill, const component::Transform& trans, float dt) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            for (int i = 0; i < skill.spawnCount; ++i) {
                float angle = (2.f * 3.14159f / skill.spawnCount) * i;
                sf::Vector2f spawnPos = trans.position + sf::Vector2f(std::cos(angle), std::sin(angle)) * 60.f;
                EntityFactory::createPlayerUnit(registry, spawnPos, "Knight");
            }
            skill.timer = 0.f;
        }
    }
};
