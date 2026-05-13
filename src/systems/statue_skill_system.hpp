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

    static void createSkillInstance(entt::registry& registry, entt::entity statue, const std::string& skillKey, const config::ConfigManager& configMgr) {
        if (skillKey == "CARD_ROSARY") {
            auto& cfg = configMgr.skills.rosary;
            component::RosarySkill rosary;
            rosary.damage = cfg.damage;
            rosary.knockbackForce = cfg.knockbackForce;
            rosary.rotationSpeed = cfg.rotationSpeed;
            rosary.radius = cfg.radius;
            rosary.duration = cfg.duration;
            rosary.passiveCooldown = cfg.passiveCooldown;
            rosary.owner = statue;
            registry.emplace<component::RosarySkill>(registry.create(), rosary);
        } 
        else if (skillKey == "CARD_GOD_RAY") {
            auto& gcfg = configMgr.skills.godRay;
            auto& godRay = registry.emplace<component::GodRaySkill>(registry.create());
            godRay.owner = statue;
            godRay.cooldown = gcfg.attackInterval;
            godRay.damage = gcfg.damage;
            godRay.range = gcfg.range;
            godRay.stunDuration = 0.5f; 
            godRay.duration = gcfg.duration;
            godRay.passiveCooldown = gcfg.passiveCooldown;
            godRay.remainingTime = godRay.duration;
            godRay.timer = godRay.cooldown; 
        } 
        else if (skillKey == "CARD_HOLY") {
            if (!registry.any_of<component::HolyAttackSkill>(statue)) {
                auto& hcfg = configMgr.skills.holy;
                auto& holy = registry.emplace<component::HolyAttackSkill>(statue);
                holy.cooldown = hcfg.cooldown;
                holy.damage = hcfg.damage;
                holy.radius = hcfg.radius;
                holy.timer = hcfg.cooldown;
            }
        } 
        else if (skillKey == "CARD_SPAWN_KNIGHT") {
            if (!registry.any_of<component::SpawnKnightSkill>(statue)) {
                auto& scfg = configMgr.skills.spawnKnight;
                auto& spawn = registry.emplace<component::SpawnKnightSkill>(statue);
                spawn.cooldown = scfg.cooldown;
                spawn.spawnCount = scfg.spawnCount;
                spawn.timer = scfg.cooldown;
            }
        }
    }

private:
    static void updateStatueCore(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats>();
        statueView.each([&](auto statueEntity, auto& transform, auto& stats) {
            stats.currentHealth = std::min(stats.maxHealth, stats.currentHealth + stats.hpRegen * deltaTime);

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
                auto& de = registry.get_or_emplace<component::DamagedEvent>(enemy);
                de.addDamage(skill.damage, 0.1f);
                hitAny = true;
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
