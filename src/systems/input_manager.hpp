#pragma once
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>

namespace input {

struct InputState {
    sf::Vector2f mouseUIPos;
    sf::Vector2f mouseWorldPos;
    bool isLeftPressed{false};
    bool isLeftReleased{false};
    bool isLeftDown{false};
    bool pauseToggled{false};
    bool langToggled{false};
    bool fullscreenToggled{false};
    bool windowResized{false};
};

class InputManager {
public:
    void update(sf::RenderWindow& window, const sf::View& uiView, const sf::View& gameView) {
        // Reset transient states
        m_state.isLeftPressed = false;
        m_state.isLeftReleased = false;
        m_state.pauseToggled = false;
        m_state.langToggled = false;
        m_state.fullscreenToggled = false;
        m_state.windowResized = false;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (event->getIf<sf::Event::Resized>()) {
                m_state.windowResized = true;
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    m_state.pauseToggled = true;
                }
                if (keyPressed->code == sf::Keyboard::Key::F11) {
                    m_state.fullscreenToggled = true;
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    m_state.isLeftPressed = true;
                    m_state.isLeftDown = true;
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Left) {
                    m_state.isLeftReleased = true;
                    m_state.isLeftDown = false;
                }
            }
        }

        // Update continuous positions
        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
        m_state.mouseUIPos = window.mapPixelToCoords(pixelPos, uiView);
        m_state.mouseWorldPos = window.mapPixelToCoords(pixelPos, gameView);
    }

    const InputState& getState() const { return m_state; }

private:
    InputState m_state;
};

} // namespace input
