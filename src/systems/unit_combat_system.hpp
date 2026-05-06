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
        // 1. 죽은 유닛 제거
        destroyDeadEntities(registry);

        // [NEW] 피격 효과 타이머 업데이트
        auto unitView = registry.view<component::UnitStats>();
        for (auto entity : unitView) {
            auto& stats = unitView.get<component::UnitStats>(entity);
            if (stats.hitFlashTimer > 0.f) {
                stats.hitFlashTimer -= deltaTime;
            }
        }

        // 2. 적군 로직 (Statue 공격)
        updateEnemyCombat(registry, deltaTime);

        // 3. 아군 유닛 로직 (주변 적군 검색 최적화)
        updatePlayerUnitCombat(registry, deltaTime, enemyGrid);
    }

private:
    static void destroyDeadEntities(entt::registry& registry) {
        std::vector<entt::entity> toDestroy;
        auto unitView = registry.view<component::UnitStats>();
        for (auto entity : unitView) {
            if (registry.get<component::UnitStats>(entity).currentHealth <= 0.f) {
                toDestroy.push_back(entity);
            }
        }
        registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static void updateEnemyCombat(entt::registry& registry, float deltaTime) {
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats, component::BoxCollider>();
        if (statueView.begin() == statueView.end()) return;

        auto statueEntity = statueView.front();
        auto& statuePos = registry.get<component::Transform>(statueEntity).position;
        auto& statueStats = registry.get<component::StatueStats>(statueEntity);
        auto& statueBox = registry.get<component::BoxCollider>(statueEntity);

        auto enemyView = registry.view<component::EnemyTag, component::Transform, component::UnitStats, component::Velocity, component::SpriteData>();

        // [OPTIMIZED] Box Collider의 경계 및 중앙점 계산
        float boxMinX = statuePos.x + statueBox.offset.x - statueBox.size.x / 2.f;
        float boxMaxX = statuePos.x + statueBox.offset.x + statueBox.size.x / 2.f;
        float boxMinY = statuePos.y + statueBox.offset.y - statueBox.size.y / 2.f;
        float boxMaxY = statuePos.y + statueBox.offset.y + statueBox.size.y / 2.f;
        
        sf::Vector2f boxCenter = statuePos + statueBox.offset;

        for (auto entity : enemyView) {
            auto& trans = registry.get<component::Transform>(entity);
            auto& stats = registry.get<component::UnitStats>(entity);
            auto& vel = registry.get<component::Velocity>(entity);
            auto& sprite = registry.get<component::SpriteData>(entity);

            // [NEW] Box와의 최단 거리 계산
            float closestX = std::max(boxMinX, std::min(trans.position.x, boxMaxX));
            float closestY = std::max(boxMinY, std::min(trans.position.y, boxMaxY));
            
            sf::Vector2f diffToBox = sf::Vector2f(closestX, closestY) - trans.position;
            float distSq = diffToBox.x * diffToBox.x + diffToBox.y * diffToBox.y;
            
            // [MODIFIED] Statue의 BoxCollider 중앙점을 향해 이동
            sf::Vector2f moveDiff = boxCenter - trans.position;
            float moveDist = std::sqrt(moveDiff.x * moveDiff.x + moveDiff.y * moveDiff.y);

            float stopDist = 10.f; // Box 경계로부터의 여유 거리

            if (distSq <= stopDist * stopDist) {
                vel.value = {0.f, 0.f};
                float damage = std::max(1.f, stats.damage - statueStats.armor);
                statueStats.currentHealth -= damage * deltaTime;
            } else {
                if (moveDist > 0.001f) {
                    vel.value = (moveDiff / moveDist) * stats.speed;
                }
            }
            sprite.flipX = (moveDiff.x < 0);
        }
    }

    static void updatePlayerUnitCombat(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto playerUnitView = registry.view<component::PlayerUnitTag, component::Transform, component::UnitStats, component::Velocity>();

        for (auto entity : playerUnitView) {
            auto& trans = registry.get<component::Transform>(entity);
            auto& stats = registry.get<component::UnitStats>(entity);
            auto& vel = registry.get<component::Velocity>(entity);

            entt::entity nearestEnemy = entt::null;
            float minDistSq = stats.attackRange * stats.attackRange * 4.f;

            // [OPTIMIZED] 전수 조사가 아닌 그리드 기반 주변 검색
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
                    auto& es = registry.get<component::UnitStats>(nearestEnemy);
                    es.currentHealth -= stats.damage * deltaTime * stats.attackSpeed;
                } else {
                    float dist = std::sqrt(minDistSq);
                    auto& et = registry.get<component::Transform>(nearestEnemy);
                    vel.value = ((et.position - trans.position) / dist) * stats.speed;
                }
            }
        }
    }
};
