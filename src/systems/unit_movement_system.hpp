#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <execution> 
#include <cmath>
#include "../components/unit_components.hpp"

class UnitMovementSystem {
public:
    static void update(entt::registry& registry, float deltaTime) noexcept {
        auto view = registry.view<component::Transform, component::Velocity>();
        
        static std::vector<entt::entity> entities;
        entities.clear();
        entities.reserve(view.size_hint());
        for(auto entity : view) entities.push_back(entity);

        if (entities.empty()) return;

        std::for_each(std::execution::par, entities.begin(), entities.end(), [&](entt::entity entity) {
            auto& transform = registry.get<component::Transform>(entity);
            auto& velocity = registry.get<component::Velocity>(entity);

            // 1. 기본 이동 (AI Velocity)
            transform.position += velocity.value * deltaTime;

            // 2. 넉백 처리 (External Force)
            if (auto* kb = registry.try_get<component::Knockback>(entity)) {
                // [NEW] Damping: 지속 시간에 따라 힘이 점점 줄어듦 (선형 감쇠)
                // 처음에 100%였다가 시간이 다 되면 0%에 가깝게
                // 지속 시간이 0.1초인 경우, 매 프레임 힘이 자연스럽게 빠짐
                transform.position += kb->force * deltaTime;
                
                // 마찰력/감쇠 효과: 매 프레임 힘의 세기를 15%씩 감소 (자연스러운 정지 연출)
                kb->force *= 0.85f; 
                
                kb->duration -= deltaTime;
            }
        });

        auto kbView = registry.view<component::Knockback>();
        static std::vector<entt::entity> toRemove;
        toRemove.clear();
        for (auto entity : kbView) {
            if (kbView.get<component::Knockback>(entity).duration <= 0.f) {
                toRemove.push_back(entity);
            }
        }
        if (!toRemove.empty()) {
            registry.remove<component::Knockback>(toRemove.begin(), toRemove.end());
        }
    }
};
