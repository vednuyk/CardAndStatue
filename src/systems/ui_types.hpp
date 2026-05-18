#pragma once
#include <SFML/System/Vector2.hpp>
#include <string>

namespace ui {

struct Card {
    std::string nameKey;
    sf::Vector2f position;
    sf::Vector2f size{80.f, 112.f};
    float rotation = 0.f;
    bool isDragging = false;
    float hoverProgress = 0.f;
    bool isHovered = false;
    float dimProgress = 0.f; 
};

struct PassiveSlot {
    std::string cardNameKey;
    sf::Vector2f position;
    sf::Vector2f size{100.f, 140.f};
    bool isOccupied = false;
    float timer = 0.0f;
    int level = 1;
    int experience = 0;
};

} // namespace ui
