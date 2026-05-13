#include <algorithm>
#include <optional>
#include "systems/fps_display.hpp"
#include "systems/wave_system.hpp"
#include "systems/unit_movement_system.hpp"
#include "systems/render_system.hpp"
#include "systems/config_manager.hpp"
#include "components/unit_components.hpp"
#include "systems/tilemap_system.hpp"
#include "systems/entity_factory.hpp"
#include "systems/statue_skill_system.hpp"
#include "systems/unit_combat_system.hpp"
#include "systems/proximity_grid.hpp"
#include "systems/ui_system.hpp"
#include "systems/card_system.hpp"
#include "systems/ai_system.hpp"
#include "systems/localization_manager.hpp"
#include "systems/animation_system.hpp"
#include "systems/entity_physics_system.hpp"
#include "systems/damage_processing_system.hpp"
#include "systems/statue_passive_system.hpp"
#include "systems/input_manager.hpp"
#include "systems/ui_manager.hpp"
#include "systems/vfx_system.hpp"

static void updateView(sf::RenderWindow& window, sf::View& view) {
    sf::Vector2u size = window.getSize();
    float windowRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
    float viewRatio = 1280.f / 720.f; 
    float sizeX = 1.f, sizeY = 1.f, posX = 0.f, posY = 0.f;

    if (windowRatio > viewRatio) {
        sizeX = viewRatio / windowRatio;
        posX = (1.f - sizeX) / 2.f;
    } else {
        sizeY = windowRatio / viewRatio;
        posY = (1.f - sizeY) / 2.f;
    }
    view.setViewport(sf::FloatRect({posX, posY}, {sizeX, sizeY}));
}

int main() {
    config::ConfigManager configMgr;
    if (!configMgr.loadAll()) {
        std::cerr << "Critical Error: Failed to load configurations!" << std::endl;
    }

    const sf::Vector2f logicalRes(1280.f, 720.f);
    sf::View gameView(sf::FloatRect({0.f, 0.f}, logicalRes));
    sf::View uiView(sf::FloatRect({0.f, 0.f}, logicalRes));

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8; 

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Earth Defense - Mega Optimized", sf::Style::Default, sf::State::Fullscreen, settings);
    window.setVerticalSyncEnabled(true);
    
    auto syncViews = [&](sf::RenderWindow& win) {
        updateView(win, gameView);
        updateView(win, uiView);
    };
    syncViews(window);

    std::map<component::TextureID, const sf::Texture*> textureMap;
    std::vector<std::unique_ptr<sf::Texture>> textureStorage;

    auto loadTex = [&](component::TextureID id, const std::string& path) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(path)) {
            textureMap[id] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    };

    loadTex(component::TextureID::Statue, "assets/IMG/Player/Statue.png");
    loadTex(component::TextureID::GoblinJunior, "assets/IMG/Enemy/GoblinJunior/GoblinJuniorSpriteSheet.png");
    loadTex(component::TextureID::Grass, "assets/IMG/BG/Grass.png");
    loadTex(component::TextureID::Flower, "assets/IMG/BG/Flower.png");

    auto createColoredTex = [&](component::TextureID id, sf::Vector2u size, sf::Color color, bool isCircle = false) {
        sf::Image img(size, color);
        if (isCircle) {
            sf::Vector2f center(static_cast<float>(size.x) / 2.f, static_cast<float>(size.y) / 2.f);
            float radius = std::min(static_cast<float>(size.x), static_cast<float>(size.y)) / 2.f;
            for (unsigned int y = 0; y < size.y; ++y) {
                for (unsigned int x = 0; x < size.x; ++x) {
                    sf::Vector2f pos(static_cast<float>(x), static_cast<float>(y));
                    if ((pos.x - center.x) * (pos.x - center.x) + (pos.y - center.y) * (pos.y - center.y) > radius * radius) {
                        img.setPixel({x, y}, sf::Color::Transparent);
                    }
                }
            }
        }
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            textureMap[id] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    };

    createColoredTex(component::TextureID::FallbackRedSquare, {32, 32}, sf::Color::Red);
    createColoredTex(component::TextureID::WhiteFlash, {32, 32}, sf::Color::White);

    // [NUN'S ROSARY] Layer 1: Solid Black Core with Gold Rim (Enlarged)
    {
        sf::Vector2u size(32, 32); // Slightly larger canvas
        sf::Image img(size, sf::Color::Transparent);
        sf::Vector2f center(16.f, 16.f);
        float radius = 14.f; // Larger radius (10.0 -> 14.0)
        for (unsigned int y = 0; y < size.y; ++y) {
            for (unsigned int x = 0; x < size.x; ++x) {
                float dx = static_cast<float>(x) - center.x;
                float dy = static_cast<float>(y) - center.y;
                float distSq = dx*dx + dy*dy;
                if (distSq <= radius * radius) {
                    float dist = std::sqrt(distSq);
                    if (dist > 12.5f) {
                        img.setPixel({x, y}, sf::Color(255, 235, 120)); // Stronger Gold Rim
                    } else if (dx < -3.f && dy < -3.f) {
                        img.setPixel({x, y}, sf::Color(70, 70, 70)); // Brighter highlight
                    } else {
                        img.setPixel({x, y}, sf::Color(5, 5, 5));
                    }
                }
            }
        }
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            tex->setSmooth(false); 
            textureMap[component::TextureID::RosarySphere] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    }

    // [NUN'S ROSARY] Layer 2: Donut-style Holy Glow (Center is dark)
    {
        sf::Vector2u size(64, 64);
        sf::Image img(size, sf::Color::Transparent);
        sf::Vector2f center(32.f, 32.f);
        float maxRadius = 30.f;
        float innerVoidRadius = 8.f; // Darken center so the bead stands out
        
        for (unsigned int y = 0; y < size.y; ++y) {
            for (unsigned int x = 0; x < size.x; ++x) {
                float dx = static_cast<float>(x) - center.x;
                float dy = static_cast<float>(y) - center.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                if (dist < maxRadius) {
                    // Alpha falloff starting from the rim of the bead
                    float alphaFactor = 0.f;
                    if (dist > innerVoidRadius) {
                        alphaFactor = std::pow(1.f - ((dist - innerVoidRadius) / (maxRadius - innerVoidRadius)), 2.0f);
                    } else {
                        alphaFactor = 0.1f * (dist / innerVoidRadius); // Very dim inside
                    }
                    
                    img.setPixel({x, y}, sf::Color(255, 200, 50, static_cast<uint8_t>(alphaFactor * 160)));
                }
            }
        }
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            tex->setSmooth(true);
            textureMap[component::TextureID::RosaryGlow] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    }

    // [ULTIMATE HYBRID] GodRay: Smooth Core with Micro Pixel Edges
    {
        sf::Vector2u size(128, 512); 
        sf::Image img(size, sf::Color::Transparent);
        
        for (unsigned int y = 0; y < size.y; ++y) {
            for (unsigned int x = 0; x < size.x; ++x) {
                float dx = (static_cast<float>(x) - size.x / 2.f) / (size.x / 2.f);
                float absDx = std::abs(dx);
                
                // Base smooth shape
                float horizontalAlpha = std::pow(std::max(0.f, 1.f - absDx), 3.0f);
                float dy = static_cast<float>(y) / size.y;
                float verticalAlpha = (dy > 0.8f) ? 1.0f : std::pow(dy / 0.8f, 2.0f);
                float rawAlpha = horizontalAlpha * verticalAlpha;

                // [KEY CHANGE] Add micro-noise only near the edges (where rawAlpha is low)
                // This creates a "sparkling pixel powder" effect on the periphery
                float edgeFactor = std::clamp((1.0f - rawAlpha) * 2.0f, 0.f, 1.f);
                float microNoise = ((x * 17 + y * 31) % 7 == 0) ? 0.04f * edgeFactor : 0.f;
                
                float finalAlpha = std::clamp(rawAlpha + microNoise, 0.f, 1.f);
                
                // [NEW] 64-step quantization for extreme smoothness with a "digital" hint
                finalAlpha = std::floor(finalAlpha * 64.f) / 64.f;

                sf::Color beamColor;
                if (absDx < 0.2f) {
                    beamColor = sf::Color(255, 255, 230, static_cast<uint8_t>(finalAlpha * 210));
                } else {
                    beamColor = sf::Color(255, 220, 50, static_cast<uint8_t>(finalAlpha * 230));
                }
                img.setPixel({x, y}, beamColor);
            }
        }
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            tex->setSmooth(false); // Sharp edges for micro-noise pixels
            textureMap[component::TextureID::GodRay] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    }

    // [FINAL POLISH] GodRayImpact: Preserve the perfect smooth glow
    {
        sf::Vector2u size(256, 256);
        sf::Image img(size, sf::Color::Transparent);
        sf::Vector2f center(128.f, 128.f);
        float maxRadius = 120.f;
        
        for (unsigned int y = 0; y < size.y; ++y) {
            for (unsigned int x = 0; x < size.x; ++x) {
                float dx = static_cast<float>(x) - center.x;
                float dy = static_cast<float>(y) - center.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                
                if (dist < maxRadius) {
                    float alpha = std::pow(1.f - (dist / maxRadius), 3.0f);
                    sf::Color color = sf::Color(255, 210, 50, static_cast<uint8_t>(alpha * 110)); 
                    img.setPixel({x, y}, color);
                }
            }
        }

        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            textureMap[component::TextureID::GodRayImpact] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    }

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/malgun.ttf")) {
        std::cerr << "Warning: Could not load malgun.ttf" << std::endl;
    }

    entt::registry registry;
    ui::CardSystem cardSystem(logicalRes, configMgr);
    FPSDisplay fpsDisplay;
    WaveSystem waveSystem(configMgr.waves);
    auto enemyGrid = std::make_unique<ProximityGrid>();
    auto playerGrid = std::make_unique<ProximityGrid>();
    sf::Clock deltaClock;
    sf::Vector2f center = logicalRes / 2.f; 
    
    TileMap tileMap;
    if (textureMap.count(component::TextureID::Grass) && textureMap.count(component::TextureID::Flower)) {
        tileMap.load(*textureMap.at(component::TextureID::Grass), *textureMap.at(component::TextureID::Flower), {32, 32}, 128, 128);
        tileMap.setPosition({-2048.f + center.x, -2048.f + center.y}); 
    }

    auto statue = EntityFactory::createStatue(registry, center, configMgr.statue);

    float currentZoom = 1.0f;
    bool isFullscreen = true; 
    float totalTime = 0.f;

    // [L10N] Setup Toggle Button
    sf::Vector2f btnSize(150.f, 40.f);
    sf::RectangleShape langBtn(btnSize);
    langBtn.setFillColor(sf::Color(50, 50, 50, 200));
    langBtn.setOutlineColor(sf::Color::White);
    langBtn.setOutlineThickness(2.f);
    langBtn.setPosition({logicalRes.x - btnSize.x - 20.f, logicalRes.y - btnSize.y - 20.f});

    input::InputManager inputManager;
    ui::UIManager uiManager;
    sf::Vector2f dropZoneCenter = logicalRes / 2.f;

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        totalTime += deltaTime;

        // 1. Unified Input Handling
        inputManager.update(window, uiView, gameView);
        const auto& input = inputManager.getState();

        // 2. UI Context & Global Actions
        if (input.windowResized) syncViews(window);
        if (input.fullscreenToggled) {
            isFullscreen = !isFullscreen;
            window.create(isFullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode({1280, 720}), 
                         "Earth Defense", sf::Style::Default, 
                         isFullscreen ? sf::State::Fullscreen : sf::State::Windowed, settings);
            window.setVerticalSyncEnabled(true);
            syncViews(window);
        }

        if (input.pauseToggled) {
            if (uiManager.getCurrentState() == ui::UIState::PauseMenu) uiManager.popState();
            else uiManager.pushState(ui::UIState::PauseMenu);
        }

        auto currentUIState = uiManager.getCurrentState();

        if (input.isLeftPressed && currentUIState == ui::UIState::HUD) {
            if (sf::FloatRect(langBtn.getPosition(), langBtn.getSize()).contains(input.mouseUIPos)) {
                LocalizationManager::getInstance().toggleLanguage();
            }
        }

        // 3. Logic Updates (Only if not paused)
        if (currentUIState != ui::UIState::PauseMenu) {
            enemyGrid->clear();
            auto enemyView = registry.view<component::EnemyTag, component::Transform>();
            enemyView.each([&](auto entity, auto& trans) {
                enemyGrid->insert(entity, trans.position);
            });

            playerGrid->clear();
            auto playerUnitView = registry.view<component::PlayerUnitTag, component::Transform>();
            playerUnitView.each([&](auto entity, auto& trans) {
                playerGrid->insert(entity, trans.position);
            });

            waveSystem.update(registry, deltaTime, center, configMgr);
            AnimationSystem::update(registry, deltaTime);
            float targetZoom = waveSystem.getCurrentWaveZoom();
            currentZoom += (targetZoom - currentZoom) * deltaTime * 2.0f;
            gameView.setSize(logicalRes * currentZoom);

            StatueSkillSystem::update(registry, deltaTime, *enemyGrid);
            StatuePassiveSystem::update(registry, deltaTime, configMgr);
            AISystem::update(registry, deltaTime, *enemyGrid, *playerGrid); 
            UnitCombatSystem::update(registry, deltaTime, *enemyGrid, *playerGrid);
            VFXSystem::update(registry, deltaTime); // [NEW] Catch DamagedEvents before clearing
            DamageProcessingSystem::update(registry);
            EntityPhysicsSystem::update(registry);
            UnitMovementSystem::update(registry, deltaTime);
        }

        fpsDisplay.update(deltaTime);
        cardSystem.updateInput(currentUIState, input, window, gameView, uiView, registry, dropZoneCenter);
        cardSystem.update(deltaTime, input);

        if (cardSystem.consumePendingUse()) {
            std::string key = cardSystem.getLastUsedCardKey();
            StatueSkillSystem::createSkillInstance(registry, statue, key, configMgr);
            cardSystem.removeCardUnderMouse();
        }

        if (cardSystem.consumePendingPassiveDrop()) {
            cardSystem.removeCardUnderMouse();
        }

        // 4. Rendering
        window.clear(sf::Color::Black);
        window.setView(gameView);
        RenderSystem::render(registry, window, textureMap, tileMap);
        
        // [FIXED] Draw World UI (HP Bars) first, then GodRay/Skills, then VFX (Damage Numbers) on top of EVERYTHING
        cardSystem.render(registry, window, gameView, uiView, font, totalTime);
        
        // Final World-Space Pass: Damage Numbers
        window.setView(gameView); 
        VFXSystem::render(registry, window, font); 
        
        // 5. Screen-Space UI (Final Layer)
        window.setView(uiView);
        window.draw(langBtn);
        sf::Text btnText(font);
        btnText.setString(LocalizationManager::getInstance().get("LANG_TOGGLE_BTN"));
        btnText.setCharacterSize(16);
        btnText.setFillColor(sf::Color::White);
        sf::FloatRect textBounds = btnText.getLocalBounds();
        btnText.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f});
        btnText.setPosition(langBtn.getPosition() + btnSize / 2.f);
        window.draw(btnText);

        if (currentUIState == ui::UIState::PauseMenu) {
            // Very simple pause overlay
            sf::RectangleShape overlay(logicalRes);
            overlay.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(overlay);
            
            sf::Text pauseText(font);
            pauseText.setString("PAUSED");
            pauseText.setCharacterSize(64);
            pauseText.setFillColor(sf::Color::White);
            sf::FloatRect pBounds = pauseText.getLocalBounds();
            pauseText.setOrigin({pBounds.position.x + pBounds.size.x / 2.f, pBounds.position.y + pBounds.size.y / 2.f});
            pauseText.setPosition(logicalRes / 2.f);
            window.draw(pauseText);
        }

        window.setView(window.getDefaultView());
        fpsDisplay.draw(window);
        window.display();
    }

    return 0;
}
