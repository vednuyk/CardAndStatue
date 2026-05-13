#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "proximity_grid.hpp"

class UnitCombatSystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid, ProximityGrid& playerGrid) {
        destroyDeadEntities(registry);

        auto unitView = registry.view<component::UnitStats>();
        unitView.each([&](auto entity, auto& stats) {
            if (stats.hitFlashTimer > 0.f) stats.hitFlashTimer -= deltaTime;
            if (stats.attackTimer > 0.f) stats.attackTimer -= deltaTime;
            if (stats.invincibilityTimer > 0.f) stats.invincibilityTimer -= deltaTime;
        });

        auto statueView = registry.view<component::StatueStats>();
        statueView.each([&](auto entity, auto& stats) {
            stats.currentHealth = std::min(stats.maxHealth, stats.currentHealth + stats.hpRegen * deltaTime);
            if (stats.hitFlashTimer > 0.f) stats.hitFlashTimer -= deltaTime;
            if (stats.invincibilityTimer > 0.f) stats.invincibilityTimer -= deltaTime;
        });

        auto combatView = registry.view<component::Transform, component::UnitStats, component::AIBehavior>();
        combatView.each([&](auto entity, auto& trans, auto& stats, auto& ai) {
            if (stats.attackTimer <= 0.f) {
                if (executeAttack(entity, trans, stats, ai, registry, enemyGrid, playerGrid)) {
                    stats.attackTimer = 1.0f / std::max(0.1f, stats.attackSpeed);
                }
            }
        });
    }

    static bool applyDamage(entt::registry& registry, entt::entity target, float damage, float flash) {
        if (!registry.valid(target)) return false;

        // [I-FRAME CHECK] Prevent overlapping damage within 0.01s
        if (auto* stats = registry.try_get<component::UnitStats>(target)) {
            if (stats->invincibilityTimer > 0.f) return false;
            stats->invincibilityTimer = 0.01f;
        } else if (auto* statueStats = registry.try_get<component::StatueStats>(target)) {
            if (statueStats->invincibilityTimer > 0.f) return false;
            statueStats->invincibilityTimer = 0.01f;
        }

        auto& de = registry.get_or_emplace<component::DamagedEvent>(target);
        de.addDamage(damage, flash);
        return true;
    }

private:
    static bool executeAttack(entt::entity attacker, const component::Transform& trans, const component::UnitStats& stats, const component::AIBehavior& ai, entt::registry& registry, ProximityGrid& enemyGrid, ProximityGrid& playerGrid) {
        bool isEnemy = registry.any_of<component::EnemyTag>(attacker);
        entt::entity target = ai.targetEntity;
        
        if (isEnemy && !registry.valid(target) && ai.targeting == component::AIBehavior::Targeting::ToStatue) {
            auto statueView = registry.view<component::StatueTag, component::Transform, component::BoxCollider>();
            if (statueView.begin() != statueView.end()) {
                target = statueView.front();
            }
        }

        if (!registry.valid(target)) return false;

        if (registry.any_of<component::StatueTag>(target)) {
            return attackStatue(attacker, trans, stats, target, registry);
        } else {
            return attackUnit(attacker, trans, stats, target, registry, enemyGrid, playerGrid, ai);
        }
    }

    static bool attackUnit(entt::entity attacker, const component::Transform& trans, const component::UnitStats& stats, entt::entity target, entt::registry& registry, ProximityGrid& enemyGrid, ProximityGrid& playerGrid, const component::AIBehavior& ai) {
        auto& tt = registry.get<component::Transform>(target);
        float dSq = distSq(trans.position, tt.position);
        float combinedReach = stats.attackRange + trans.radius + tt.radius;

        if (dSq <= combinedReach * combinedReach) {
            if (ai.attack == component::AIBehavior::Attack::AoE) {
                bool isEnemy = registry.any_of<component::EnemyTag>(attacker);
                ProximityGrid& targetGrid = isEnemy ? playerGrid : enemyGrid;
                targetGrid.queryNearby(trans.position, [&](entt::entity t) {
                    if (!registry.valid(t)) return;
                    auto& otherT = registry.get<component::Transform>(t);
                    if (distSq(trans.position, otherT.position) <= combinedReach * combinedReach) {
                        applyDamage(registry, t, stats.damage, 0.1f);
                    }
                });
                return true;
            } else {
                applyDamage(registry, target, stats.damage, 0.1f);
                return true;
            }
        }
        return false;
    }

    static bool attackStatue(entt::entity attacker, const component::Transform& trans, const component::UnitStats& stats, entt::entity statue, entt::registry& registry) {
        const auto& statueBox = registry.get<component::BoxCollider>(statue);
        const auto& statuePos = registry.get<component::Transform>(statue).position;

        float boxMinX = statuePos.x + statueBox.offset.x - statueBox.size.x / 2.f;
        float boxMaxX = statuePos.x + statueBox.offset.x + statueBox.size.x / 2.f;
        float boxMinY = statuePos.y + statueBox.offset.y - statueBox.size.y / 2.f;
        float boxMaxY = statuePos.y + statueBox.offset.y + statueBox.size.y / 2.f;

        float closestX = std::max(boxMinX, std::min(trans.position.x, boxMaxX));
        float closestY = std::max(boxMinY, std::min(trans.position.y, boxMaxY));
        sf::Vector2f diff = sf::Vector2f(closestX, closestY) - trans.position;
        
        // [RELAXED-CHECK] Added +5.f buffer to ensure attacks land when AI stops
        float combinedReach = stats.attackRange + trans.radius + 5.f; 
        if (diff.x * diff.x + diff.y * diff.y <= combinedReach * combinedReach) {
            applyDamage(registry, statue, stats.damage, 0.1f);
            return true; // Attack was physically possible, reset timer
        }
        return false;
    }

    static void destroyDeadEntities(entt::registry& registry) {
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        auto unitView = registry.view<component::UnitStats>();
        unitView.each([&](auto entity, auto& stats) {
            if (stats.currentHealth <= 0.f) toDestroy.push_back(entity);
        });

        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static float distSq(sf::Vector2f a, sf::Vector2f b) {
        sf::Vector2f d = a - b;
        return d.x * d.x + d.y * d.y;
    }
};
