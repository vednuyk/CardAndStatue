#pragma once
#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>
#include <algorithm>
#include <vector>
#include "../components/unit_components.hpp"

class UISystem {
public:
    // UI 요소들의 렌더링 우선순위나 타입을 정의할 수 있는 기반
    enum class UIType {
        WorldSpace, // 유닛 머리 위 등 월드 좌표계 UI
        ScreenSpace // 화면에 고정된 HUD 등
    };

    // [NEW] Card UI Rendering
    struct CardInfo {
        sf::String name;
        sf::Vector2f position;
        sf::Vector2f size;
        float rotation; // [NEW] Angle
        bool isDragging;
    };

    static void render(entt::registry& registry, sf::RenderTarget& target, const sf::View& worldView, const std::vector<CardInfo>& cards, const sf::Font& font) {
        // 1. 월드 공간 UI (HP Bar 등)
        renderWorldUI(registry, target);

        // 2. 스크린 공간 UI (Card 등)
        renderScreenUI(target, cards, font);
    }

private:
    static void renderScreenUI(sf::RenderTarget& target, const std::vector<CardInfo>& cards, const sf::Font& font) {
        for (const auto& card : cards) {
            sf::RectangleShape shape(card.size);
            // [NEW] Set origin to bottom center for natural fanning
            shape.setOrigin({card.size.x / 2.f, card.size.y});
            shape.setPosition(card.position + sf::Vector2f(card.size.x / 2.f, card.size.y));
            shape.setRotation(sf::degrees(card.rotation));
            
            shape.setFillColor(card.isDragging ? sf::Color(100, 100, 250, 255) : sf::Color(220, 220, 220, 255));
            shape.setOutlineThickness(2.f);
            shape.setOutlineColor(sf::Color::Black);
            target.draw(shape);

            // [FIX] Use sf::String directly for proper Korean support
            sf::Text text(font);
            text.setString(card.name);
            text.setCharacterSize(12);
            text.setFillColor(sf::Color::Black);
            
            sf::FloatRect textBounds = text.getLocalBounds();
            text.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f});
            
            // Calculate text position relative to card center top
            sf::Transform cardTransform;
            cardTransform.translate(shape.getPosition()).rotate(sf::degrees(card.rotation));
            sf::Vector2f textLocalPos = {0.f, -card.size.y + 25.f};
            text.setPosition(cardTransform.transformPoint(textLocalPos));
            text.setRotation(sf::degrees(card.rotation));
            
            target.draw(text);
        }
    }
    static void renderWorldUI(entt::registry& registry, sf::RenderTarget& target) {
        // 본진(Statue) HP Bar
        auto statueView = registry.view<component::Transform, component::StatueStats>();
        for (auto entity : statueView) {
            auto& trans = registry.get<component::Transform>(entity);
            auto& stats = registry.get<component::StatueStats>(entity);
            
            // Statue는 항상 혹은 일정 피해를 입었을 때 표시 (상단 오프셋 -70.f)
            if (stats.currentHealth < stats.maxHealth) {
                drawHealthBar(target, trans.position, stats.currentHealth, stats.maxHealth, {120.f, 8.f}, -80.f);
            }
        }

        // 유닛(Enemy, PlayerUnit) HP Bar
        auto unitView = registry.view<component::Transform, component::UnitStats>();
        for (auto entity : unitView) {
            auto& trans = registry.get<component::Transform>(entity);
            auto& stats = registry.get<component::UnitStats>(entity);
            
            // 유닛은 피해를 입었을 때만 표시 (상단 오프셋 -30.f)
            if (stats.currentHealth < stats.maxHealth) {
                drawHealthBar(target, trans.position, stats.currentHealth, stats.maxHealth, {40.f, 5.f}, -35.f);
            }
        }
    }

    static void drawHealthBar(sf::RenderTarget& target, sf::Vector2f position, float current, float max, sf::Vector2f size, float offsetY) {
        // 배경 (어두운 빨강)
        sf::RectangleShape bg(size);
        bg.setOrigin({size.x / 2.f, size.y / 2.f});
        bg.setPosition({position.x, position.y + offsetY});
        bg.setFillColor(sf::Color(60, 0, 0, 200)); // 투명도 약간 추가
        target.draw(bg);

        // 전경 (밝은 빨강/녹색 등)
        float percentage = std::max(0.f, std::min(1.f, current / max));
        sf::RectangleShape fg({size.x * percentage, size.y});
        fg.setOrigin({0.f, size.y / 2.f}); // 왼쪽 정렬을 위해 오리진 변경
        fg.setPosition({position.x - size.x / 2.f, position.y + offsetY});
        
        // 체력 상태에 따라 색상 변경 (옵션)
        if (percentage > 0.5f) fg.setFillColor(sf::Color::Green);
        else if (percentage > 0.2f) fg.setFillColor(sf::Color::Yellow);
        else fg.setFillColor(sf::Color::Red);

        target.draw(fg);
    }
};
