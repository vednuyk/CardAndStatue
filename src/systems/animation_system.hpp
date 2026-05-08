#pragma once
#include <entt/entt.hpp>
#include "../components/unit_components.hpp"

class AnimationSystem {
public:
    static void update(entt::registry& registry, float deltaTime) {
        auto view = registry.view<component::Animation, component::SpriteData>();
        
        view.each([&](component::Animation& anim, component::SpriteData& sprite) {
            if (anim.frameCount <= 1) return;

            anim.timer += deltaTime;
            if (anim.timer >= anim.frameDuration) {
                anim.timer -= anim.frameDuration;
                anim.currentFrame = (anim.currentFrame + 1) % anim.frameCount;

                // SFML 3.0: Grid-based calculation (2x2 or any framesPerRow)
                int col = anim.currentFrame % anim.framesPerRow;
                int row = anim.currentFrame / anim.framesPerRow;

                sprite.textureRect.position.x = static_cast<float>(col * anim.frameWidth);
                sprite.textureRect.position.y = static_cast<float>(row * anim.frameHeight); 
                sprite.textureRect.size.x = static_cast<float>(anim.frameWidth);
                sprite.textureRect.size.y = static_cast<float>(anim.frameHeight);
            }
        });
    }
};
