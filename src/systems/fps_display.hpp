#pragma once
#include <SFML/Graphics.hpp>
#include <iomanip>
#include <sstream>

class FPSDisplay {
public:
    FPSDisplay() {
        if (!m_font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
            // Log error or handle missing font
        }

        m_fpsText.setFont(m_font);
        m_fpsText.setCharacterSize(20);
        m_fpsText.setFillColor(sf::Color::Yellow);
    }

    // Modern C++ guideline: Explicitly mark logic that shouldn't throw
    void update(float deltaTime) noexcept {
        m_frameCount++;
        m_timeElapsed += deltaTime;

        if (m_timeElapsed >= 0.5f) {
            float fps = static_cast<float>(m_frameCount) / m_timeElapsed;
            
            std::stringstream ss;
            ss << "FPS: " << std::fixed << std::setprecision(1) << fps;
            m_fpsText.setString(ss.str());

            m_frameCount = 0;
            m_timeElapsed = 0.f;
        }
    }

    void draw(sf::RenderWindow& window) {
        sf::FloatRect bounds = m_fpsText.getLocalBounds();
        float x = static_cast<float>(window.getSize().x) - bounds.size.x - 20.f;
        float y = 10.f;
        
        m_fpsText.setPosition({x, y});
        window.draw(m_fpsText);
    }

private:
    sf::Font m_font;
    sf::Text m_fpsText{m_font};
    int m_frameCount{0};
    float m_timeElapsed{0.f};
};
