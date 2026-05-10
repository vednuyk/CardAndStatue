#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "proximity_grid.hpp"

class AISystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        updateEnemyAI(registry);
        updatePlayerUnitAI(registry, enemyGrid);
    }

private:
    static void updateEnemyAI(entt::registry& registry) {
        // [OPTIMIZED] 매 프레임 View로 Statue를 찾는 대신, Context에 캐싱된 전역 위치를 사용
        auto* globalTarget = registry.ctx().find<component::GlobalTargetLocation>();
        if (!globalTarget) return;

        const sf::Vector2f boxCenter = globalTarget->position;
        const sf::FloatRect& bounds = globalTarget->bounds;

        float boxMinX = bounds.position.x;
        float boxMaxX = bounds.position.x + bounds.size.x;
        float boxMinY = bounds.position.y;
        float boxMaxY = bounds.position.y + bounds.size.y;

        auto enemyView = registry.view<component::EnemyTag, component::Transform, component::UnitStats, component::Velocity, component::SpriteData>();
        enemyView.each([&](auto entity, auto& trans, auto& stats, auto& vel, auto& sprite) {
            sf::Vector2f moveDiff = boxCenter - trans.position;
            float moveDist = std::sqrt(moveDiff.x * moveDiff.x + moveDiff.y * moveDiff.y);

            // 클램핑 로직 (충돌 박스 경계선 체크)
            float closestX = std::max(boxMinX, std::min(trans.position.x, boxMaxX));
            float closestY = std::max(boxMinY, std::min(trans.position.y, boxMaxY));
            sf::Vector2f diffToBox = sf::Vector2f(closestX, closestY) - trans.position;
            float distSq = diffToBox.x * diffToBox.x + diffToBox.y * diffToBox.y;

            if (distSq <= 100.f) { 
                vel.value = {0.f, 0.f};
            } else {
                if (moveDist > 0.001f) {
                    vel.value = (moveDiff / moveDist) * stats.speed;
                }
            }
            sprite.flipX = (moveDiff.x < 0);
        });
    }

    static void updatePlayerUnitAI(entt::registry& registry, ProximityGrid& enemyGrid) {
        auto playerUnitView = registry.view<component::PlayerUnitTag, component::Transform, component::UnitStats, component::Velocity>();
        playerUnitView.each([&](auto entity, auto& trans, auto& stats, auto& vel) {
            entt::entity nearestEnemy = entt::null;
            float minDistSq = stats.attackRange * stats.attackRange * 4.f;

            enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                auto& et = registry.get<component::Transform>(enemy);
                sf::Vector2f diff = et.position - trans.position;
                float dSq = diff.x * diff.x + diff.y * diff.y;
                if (dSq < minDistSq) {
                    minDistSq = dSq;
                    nearestEnemy = enemy;
                }
            });

            if (registry.valid(nearestEnemy)) {
                if (minDistSq <= stats.attackRange * stats.attackRange) {
                    vel.value = {0.f, 0.f};
                } else {
                    float dist = std::sqrt(minDistSq);
                    auto& et = registry.get<component::Transform>(nearestEnemy);
                    vel.value = ((et.position - trans.position) / dist) * stats.speed;
                }
            } else {
                vel.value = {0.f, 0.f}; 
            }
        });
    }
};
