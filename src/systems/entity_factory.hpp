#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <map>
#include "../components/unit_components.hpp"
#include "config_manager.hpp"

class EntityFactory {
public:
    static entt::entity createStatue(entt::registry& registry, sf::Vector2f position, const config::StatueConfig& cfg) {
        auto entity = registry.create();
        // [LEGACY] Radius is no longer used for Statue collision, using BoxCollider instead
        registry.emplace<component::Transform>(entity, position, 0.f, 0.f);
        registry.emplace<component::StatueStats>(entity, cfg.maxHealth, cfg.maxHealth, cfg.hpRegen, cfg.armor);
        registry.emplace<component::StatueTag>(entity);
        registry.emplace<component::BoxCollider>(entity, cfg.boxSize, cfg.boxOffset);
        
        auto& spriteData = registry.emplace<component::SpriteData>(entity);
        spriteData.textureID = component::TextureID::Statue;
        spriteData.textureName = "Statue";
        spriteData.scale = {cfg.scale, cfg.scale};

        // [OPTIMIZED] 전역 타겟 위치 캐싱
        component::GlobalTargetLocation target;
        target.position = position + cfg.boxOffset;
        target.bounds = sf::FloatRect(
            {position.x + cfg.boxOffset.x - cfg.boxSize.x / 2.f, position.y + cfg.boxOffset.y - cfg.boxSize.y / 2.f},
            cfg.boxSize
        );
        registry.ctx().emplace<component::GlobalTargetLocation>(target);
        
        return entity;
    }

    static entt::entity createEnemy(entt::registry& registry, sf::Vector2f position, const std::string& typeName, const config::ConfigManager& configMgr) {
        auto it = configMgr.enemies.find(typeName);
        if (it == configMgr.enemies.end()) return entt::null;

        const auto& cfg = it->second;
        auto entity = registry.create();
        
        registry.emplace<component::Transform>(entity, position, cfg.radius);
        registry.emplace<component::UnitStats>(entity, cfg.maxHealth, cfg.maxHealth, cfg.speed, cfg.damage, cfg.attackSpeed, cfg.attackRange);
        registry.emplace<component::EnemyTag>(entity);
        registry.emplace<component::Velocity>(entity, sf::Vector2f(0.f, 0.f));
        
        auto& sprite = registry.emplace<component::SpriteData>(entity);
        
        // Map texture ID based on name or use fallback
        if (typeName == "GoblinJunior") {
            sprite.textureID = component::TextureID::GoblinJunior;
        } else {
            sprite.textureID = component::TextureID::FallbackRedSquare;
        }

        sprite.textureName = cfg.name;
        sprite.scale = {cfg.scale, cfg.scale};

        // Initialize Animation if enabled in config
        if (cfg.animation.enabled) {
            auto& anim = registry.emplace<component::Animation>(entity);
            anim.frameCount = cfg.animation.frameCount;
            anim.framesPerRow = cfg.animation.framesPerRow; // [NEW]
            anim.frameDuration = cfg.animation.frameDuration;
            anim.frameWidth = cfg.animation.frameWidth;
            anim.frameHeight = cfg.animation.frameHeight;
            anim.currentFrame = 0;
            anim.timer = 0.f;

            // Initial texture rect
            sprite.textureRect = sf::FloatRect({0.f, 0.f}, {static_cast<float>(anim.frameWidth), static_cast<float>(anim.frameHeight)});
        }

        return entity;
    }

    static entt::entity createPlayerUnit(entt::registry& registry, sf::Vector2f position, const std::string& typeName) {
        auto entity = registry.create();
        registry.emplace<component::Transform>(entity, position, 10.f);
        registry.emplace<component::UnitStats>(entity, 100.f, 100.f, 80.f, 15.f, 1.2f, 30.f);
        registry.emplace<component::PlayerUnitTag>(entity);
        registry.emplace<component::Velocity>(entity, sf::Vector2f(0.f, 0.f));
        
        auto& sprite = registry.emplace<component::SpriteData>(entity);
        sprite.textureID = component::TextureID::Knight;
        sprite.textureName = "Knight"; 
        sprite.scale = {0.15f, 0.15f};

        return entity;
    }
};
