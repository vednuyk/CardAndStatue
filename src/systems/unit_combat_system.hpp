#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "proximity_grid.hpp"

class UnitCombatSystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        destroyDeadEntities(registry);

        auto unitView = registry.view<component::UnitStats>();
        unitView.each([&](auto entity, auto& stats) {
            if (stats.hitFlashTimer > 0.f) {
                stats.hitFlashTimer -= deltaTime;
            }
        });

        updateEnemyCombat(registry, deltaTime);
        updatePlayerUnitCombat(registry, deltaTime, enemyGrid);
    }

private:
    static void destroyDeadEntities(entt::registry& registry) {
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        auto unitView = registry.view<component::UnitStats>();
        unitView.each([&](auto entity, auto& stats) {
            if (stats.currentHealth <= 0.f) {
                toDestroy.push_back(entity);
            }
        });

        if (!toDestroy.empty()) {
            registry.destroy(toDestroy.begin(), toDestroy.end());
        }
    }

    static void updateEnemyCombat(entt::registry& registry, float deltaTime) {
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats, component::BoxCollider>();
        if (statueView.begin() == statueView.end()) return;

        auto statueEntity = statueView.front();
        auto& statueStats = registry.get<component::StatueStats>(statueEntity);
        auto& statueBox = registry.get<component::BoxCollider>(statueEntity);
        auto& statuePos = registry.get<component::Transform>(statueEntity).position;

        float boxMinX = statuePos.x + statueBox.offset.x - statueBox.size.x / 2.f;
        float boxMaxX = statuePos.x + statueBox.offset.x + statueBox.size.x / 2.f;
        float boxMinY = statuePos.y + statueBox.offset.y - statueBox.size.y / 2.f;
        float boxMaxY = statuePos.y + statueBox.offset.y + statueBox.size.y / 2.f;

        auto enemyView = registry.view<component::EnemyTag, component::Transform, component::UnitStats>();
        enemyView.each([&](auto entity, auto& trans, auto& stats) {
            float closestX = std::max(boxMinX, std::min(trans.position.x, boxMaxX));
            float closestY = std::max(boxMinY, std::min(trans.position.y, boxMaxY));
            sf::Vector2f diffToBox = sf::Vector2f(closestX, closestY) - trans.position;
            float distSq = diffToBox.x * diffToBox.x + diffToBox.y * diffToBox.y;

            if (distSq <= 100.f) { // Within 10px of box
                float damage = std::max(1.f, stats.damage - statueStats.armor);
                statueStats.currentHealth -= damage * deltaTime;
            }
        });
    }

    static void updatePlayerUnitCombat(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto playerUnitView = registry.view<component::PlayerUnitTag, component::Transform, component::UnitStats>();
        playerUnitView.each([&](auto entity, auto& trans, auto& stats) {
            enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                auto& et = registry.get<component::Transform>(enemy);
                sf::Vector2f diff = et.position - trans.position;
                float dSq = diff.x * diff.x + diff.y * diff.y;

                if (dSq <= stats.attackRange * stats.attackRange) {
                    auto& es = registry.get<component::UnitStats>(enemy);
                    es.currentHealth -= stats.damage * deltaTime * stats.attackSpeed;
                    es.hitFlashTimer = 0.1f;
                }
            });
        });
    }
};
