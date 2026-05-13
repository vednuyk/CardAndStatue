#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <vector>
#include <random>
#include "../../components/unit_components.hpp"
#include "../proximity_grid.hpp"

class GodRaySystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        // 1. Manage God Ray Skill Instances
        auto godRayView = registry.view<component::GodRaySkill>();
        static std::vector<entt::entity> expiredGodRays;
        expiredGodRays.clear();

        godRayView.each([&](auto entity, auto& skill) {
            if (registry.valid(skill.owner)) {
                skill.remainingTime -= deltaTime;
                if (skill.remainingTime <= 0.f) {
                    expiredGodRays.push_back(entity);
                } else {
                    auto& statueTrans = registry.get<component::Transform>(skill.owner);
                    updateGodRayExecution(registry, entity, skill, statueTrans, deltaTime, enemyGrid);
                }
            } else {
                expiredGodRays.push_back(entity);
            }
        });

        if (!expiredGodRays.empty()) registry.destroy(expiredGodRays.begin(), expiredGodRays.end());

        // 2. Update Visual Effects
        updateGodRayEffects(registry, deltaTime);
    }

private:
    static void updateGodRayExecution(entt::registry& registry, entt::entity instance, component::GodRaySkill& skill, const component::Transform& statueTrans, float dt, ProximityGrid& enemyGrid) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            std::vector<entt::entity> potentialTargets;
            enemyGrid.queryRange(statueTrans.position, skill.range, [&](entt::entity enemy) {
                if (registry.valid(enemy)) potentialTargets.push_back(enemy);
            });

            if (!potentialTargets.empty()) {
                static std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, potentialTargets.size() - 1);
                entt::entity target = potentialTargets[dist(rng)];

                spawnEffect(registry, target, skill.damage, skill.stunDuration);
                skill.timer = 0.f;
            }
        }
    }

    static void spawnEffect(entt::registry& registry, entt::entity target, float damage, float stunDuration) {
        auto& et = registry.get<component::Transform>(target);
        sf::Vector2f pivotPos = et.position;
        if (auto* pivot = registry.try_get<component::Pivot>(target)) pivotPos += pivot->offset;

        auto& de = registry.get_or_emplace<component::DamagedEvent>(target);
        de.addDamage(damage, 0.1f);
        de.addStun(stunDuration);

        // Beam Effect
        auto effect = registry.create();
        registry.emplace<component::GodRayEffect>(effect, 0.5f, 0.f, target, pivotPos);
        registry.emplace<component::Transform>(effect, pivotPos - sf::Vector2f(0.f, 128.f)); 
        auto& sd = registry.emplace<component::SpriteData>(effect);
        sd.textureID = component::TextureID::GodRay;
        sd.scale = {0.2f, 0.5f}; 
        sd.renderLayer = 2;

        // Impact Aura
        auto aura = registry.create();
        registry.emplace<component::GodRayEffect>(aura, 0.5f, 0.f, target, pivotPos);
        registry.emplace<component::Transform>(aura, pivotPos);
        auto& asd = registry.emplace<component::SpriteData>(aura);
        asd.textureID = component::TextureID::GodRayImpact;
        asd.scale = {0.1f, 0.05f};
        asd.renderLayer = 1;
    }

    static void updateGodRayEffects(entt::registry& registry, float dt) {
        auto view = registry.view<component::GodRayEffect, component::Transform, component::SpriteData>();
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        view.each([&](auto entity, auto& effect, auto& trans, auto& sprite) {
            effect.timer += dt;
            if (effect.timer >= effect.duration) {
                toDestroy.push_back(entity);
            } else {
                sf::Vector2f refPos = effect.lastTargetPos;
                if (registry.valid(effect.target)) {
                    auto& et = registry.get<component::Transform>(effect.target);
                    refPos = et.position;
                    if (auto* pivot = registry.try_get<component::Pivot>(effect.target)) refPos += pivot->offset;
                    effect.lastTargetPos = refPos;
                }
                
                float progress = effect.timer / effect.duration;
                if (sprite.textureID == component::TextureID::GodRay) {
                    updateBeamSprite(sprite, trans, refPos, progress);
                } else if (sprite.textureID == component::TextureID::GodRayImpact) {
                    updateImpactSprite(sprite, trans, refPos, progress);
                }
            }
        });
        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static void updateBeamSprite(component::SpriteData& sprite, component::Transform& trans, sf::Vector2f refPos, float progress) {
        if (progress < 0.2f) sprite.scale.x = 0.2f + 1.0f * (progress / 0.2f);
        else if (progress < 0.7f) sprite.scale.x = 1.2f;
        else sprite.scale.x = 1.2f * (1.0f - (progress - 0.7f) / 0.3f);
        
        sprite.scale.y = 0.5f * (progress < 0.7f ? 1.0f : (1.0f - (progress - 0.7f) * 1.66f));
        trans.position = refPos - sf::Vector2f(0.f, (512.f * sprite.scale.y) / 2.f);
    }

    static void updateImpactSprite(component::SpriteData& sprite, component::Transform& trans, sf::Vector2f refPos, float progress) {
        if (progress < 0.25f) { 
            sprite.scale.x = 0.4f + 1.0f * (progress / 0.25f); 
            sprite.scale.y = 0.2f + 0.5f * (progress / 0.25f); 
        } else { 
            sprite.scale.x = 1.4f * (1.0f + (progress - 0.25f) * 0.26f); 
            sprite.scale.y = 0.7f * (1.0f - (progress - 0.25f) * 1.33f); 
        }
        trans.position = refPos; 
    }
};
