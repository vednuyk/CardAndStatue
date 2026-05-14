#pragma once
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

namespace ui {

struct LayoutInfo {
    sf::Vector2f position;
    float rotation;
};

class CardUILayoutSystem {
public:
    static std::vector<LayoutInfo> calculateHandLayout(int cardCount, sf::Vector2f logicalRes, sf::Vector2f cardSize) {
        std::vector<LayoutInfo> layout;
        if (cardCount <= 0) return layout;

        float t = std::clamp((static_cast<float>(cardCount) - 2.f) / 8.f, 0.f, 1.f);
        float arcRadius = 1200.f; 
        float angleStep = 4.0f - t * 0.5f; 
        sf::Vector2f handAnchor(logicalRes.x / 2.f, logicalRes.y + 5.f);
        sf::Vector2f pivot(handAnchor.x, handAnchor.y + arcRadius);
        
        float totalAngleRange = (cardCount > 1 ? static_cast<float>(cardCount - 1) * angleStep : 0.f);
        float startAngle = -totalAngleRange / 2.f;

        for (int i = 0; i < cardCount; ++i) {
            float angleDeg = startAngle + i * angleStep;
            float angleRad = angleDeg * (3.14159f / 180.0f);
            
            float x = pivot.x + arcRadius * std::sin(angleRad);
            float y = pivot.y - arcRadius * std::cos(angleRad);
            
            layout.push_back({
                {x - cardSize.x / 2.f, y - cardSize.y},
                angleDeg
            });
        }
        return layout;
    }

    static LayoutInfo calculateHoverEffect(const LayoutInfo& baseLayout, sf::Vector2f cardSize, float hoverProgress) {
        // [BOUNCE] Cubic spring-like easing for a "pop and bounce" feel
        // f(t) = -2t^3 + 3t^2 (Standard smoothstep, but let's make it more bouncy)
        // We'll use a slightly customized overshoot easing: 1 + 2.70158 * (t-1)^3 + 1.70158 * (t-1)^2
        float t = hoverProgress;
        float bouncyT = 0.f;
        if (t > 0) {
            // Overshoot effect: goes slightly above 1.0 and settles
            float c1 = 1.70158f;
            float c3 = c1 + 1.f;
            bouncyT = 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
        }

        float scale = 1.0f + (bouncyT * 0.4f);
        float lift = bouncyT * 140.f; // Increased lift for more impact
        
        sf::Vector2f animatedSize = cardSize * scale;
        sf::Vector2f animatedPos = {
            baseLayout.position.x + cardSize.x / 2.f - animatedSize.x / 2.f,
            baseLayout.position.y - lift + cardSize.y - animatedSize.y
        };

        return { animatedPos, baseLayout.rotation * (1.0f - bouncyT) }; // Flatten rotation on hover
    }
};

} // namespace ui
