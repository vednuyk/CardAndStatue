#pragma once
#include <entt/entt.hpp>
#include <algorithm>
#include "../components/unit_components.hpp"

class DamageProcessingSystem {
public:
    static void update(entt::registry& registry) {
        // [EVENT PROCESSING] Iterate through all DamagedEvents
        auto view = registry.view<component::DamagedEvent>();
        
        view.each([&](auto entity, auto& event) {
            // 1. Apply to Regular Units (Enemies/Allies)
            if (auto* stats = registry.try_get<component::UnitStats>(entity)) {
                stats->currentHealth -= event.damage;
                stats->hitFlashTimer = std::max(stats->hitFlashTimer, event.hitFlashTimer);

                // Check for Physics Requests (Knockback/Stun)
                bool hasKnockback = (event.knockbackVelocity.x != 0.f || event.knockbackVelocity.y != 0.f);
                bool hasStun = (event.stunDuration > 0.f);

                if (hasKnockback && hasStun) {
                    registry.emplace_or_replace<component::PhysicsRequest>(entity, component::PhysicsRequest::Type::BothEffects, event.knockbackVelocity, event.stunDuration);
                } else if (hasKnockback) {
                    registry.emplace_or_replace<component::PhysicsRequest>(entity, component::PhysicsRequest::Type::KnockbackEffect, event.knockbackVelocity, 0.05f);
                } else if (hasStun) {
                    registry.emplace_or_replace<component::PhysicsRequest>(entity, component::PhysicsRequest::Type::StunEffect, sf::Vector2f(0.f, 0.f), event.stunDuration);
                }
            }
            // 2. Apply to the Statue (Base)
            else if (auto* statueStats = registry.try_get<component::StatueStats>(entity)) {
                // Statue receives damage and hit flash
                if (event.damage > 0.f) {
                    // [MINIMAL-DAMAGE] Ensure at least 1 damage is dealt even if armor is high
                    float finalDamage = std::max(1.f, event.damage - statueStats->armor);
                    statueStats->currentHealth -= finalDamage;
                    statueStats->hitFlashTimer = 0.1f; // Force flash
                }
            }
        });

        // 3. Clear events for the next frame
        registry.clear<component::DamagedEvent>();
    }
};
