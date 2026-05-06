#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <execution> 
#include <cmath>
#include "../components/unit_components.hpp"

class UnitMovementSystem {
public:
    // [OPTIMIZED] 더 이상 SpatialHashGrid를 사용하지 않음 (유닛 간 겹침 허용)
    static void update(entt::registry& registry, float deltaTime) noexcept {
        auto view = registry.view<component::Transform, component::Velocity>();
        
        // 병렬 처리를 위해 엔티티 수집
        static std::vector<entt::entity> entities;
        entities.clear();
        entities.reserve(view.size_hint());
        for(auto entity : view) entities.push_back(entity);

        if (entities.empty()) return;

        // 단순 위치 적분 (물리 회피 로직 제거로 성능 극대화)
        std::for_each(std::execution::par, entities.begin(), entities.end(), [&](entt::entity entity) {
            auto& transform = registry.get<component::Transform>(entity);
            auto& velocity = registry.get<component::Velocity>(entity);

            // 위치 업데이트 (P = P + V * dt)
            transform.position += velocity.value * deltaTime;
        });
    }
};
