#pragma once
#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include "../components/unit_components.hpp"

#include "tilemap_system.hpp"

class RenderSystem {
public:
    static void render(entt::registry& registry, sf::RenderTarget& target, const std::map<component::TextureID, const sf::Texture*>& textureMap, const TileMap& tileMap) {
        target.draw(tileMap); // 1. 지형 렌더링
        
        renderEntities(registry, target, textureMap); // 2. 모든 엔티티 통합 배치 렌더링
    }

private:
    static void renderEntities(entt::registry& registry, sf::RenderTarget& target, const std::map<component::TextureID, const sf::Texture*>& textureMap) {
        // [SHADERS] 피격 효과를 위한 간단한 셰이더 정의
        static bool shaderLoaded = false;
        static sf::Shader hitShader;
        if (!shaderLoaded) {
            // SFML 3.0 GLSL 120 (기본 정점 색상과 텍스처를 곱하되, 알파가 254/255 미만이면 흰색으로 출력)
            const std::string fragmentShader = 
                "uniform sampler2D texture;"
                "void main() {"
                "    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);"
                "    if (gl_Color.a > 0.995 && gl_Color.a < 0.997) {"
                "        vec3 flashColor = mix(pixel.rgb, vec3(1.0), 0.5);"
                "        gl_FragColor = vec4(flashColor, pixel.a);"
                "    } else {"
                "        gl_FragColor = pixel * gl_Color;"
                "    }"
                "}";
            if (hitShader.loadFromMemory(fragmentShader, sf::Shader::Type::Fragment)) {
                shaderLoaded = true;
            }
        }

        struct RenderNode {
            entt::entity entity;
            float depth;
            const sf::Texture* texture;
            sf::FloatRect texRect;
            sf::Vector2f position;
            float rotation;
            sf::Vector2f halfSize;
            sf::Color color;
            bool flipX;
        };
        static std::vector<RenderNode> renderNodes;
        renderNodes.clear();

        auto view = registry.view<component::Transform, component::SpriteData>();
        renderNodes.reserve(view.size_hint());

        view.each([&](auto entity, auto& transform, auto& spriteData) {
            if (textureMap.count(spriteData.textureID) == 0) return;
            const sf::Texture* tex = textureMap.at(spriteData.textureID);

            sf::Color vertColor = sf::Color::White;
            if (auto* stats = registry.try_get<component::UnitStats>(entity)) {
                if (stats->hitFlashTimer > 0.f) {
                    vertColor = sf::Color(255, 255, 255, 254); 
                }
            }

            sf::FloatRect texRect = spriteData.textureRect;
            if (texRect.size.x == 0) {
                if (auto* anim = registry.try_get<component::Animation>(entity)) {
                    texRect = sf::FloatRect({0.f, 0.f}, {static_cast<float>(anim->frameWidth), static_cast<float>(anim->frameHeight)});
                } else {
                    texRect = sf::FloatRect({0.f, 0.f}, sf::Vector2f(tex->getSize()));
                }
            }

            float hw = (texRect.size.x * spriteData.scale.x) / 2.f;
            float hh = (texRect.size.y * spriteData.scale.y) / 2.f;

            float depth = transform.position.y + hh + (static_cast<float>(spriteData.renderLayer) * 10000.f);

            renderNodes.push_back({entity, depth, tex, texRect, transform.position, transform.rotation, {hw, hh}, vertColor, spriteData.flipX});
        });

        // 1. 전역 정렬 (Depth)
        std::sort(renderNodes.begin(), renderNodes.end(), [](const auto& a, const auto& b) {
            return a.depth < b.depth;
        });

        const sf::Texture* currentTexture = nullptr;
        static std::vector<sf::Vertex> vertexBuffer;
        vertexBuffer.clear();
        vertexBuffer.reserve(renderNodes.size() * 6);

        auto flush = [&]() {
            if (currentTexture != nullptr && !vertexBuffer.empty()) {
                sf::RenderStates states;
                states.texture = currentTexture;
                if (shaderLoaded) states.shader = &hitShader; 

                target.draw(vertexBuffer.data(), vertexBuffer.size(), sf::PrimitiveType::Triangles, states);
                vertexBuffer.clear();
            }
        };

        for (const auto& node : renderNodes) {
            if (node.texture != currentTexture) {
                flush();
                currentTexture = node.texture;
            }

            float hw = node.halfSize.x;
            float hh = node.halfSize.y;
            if (node.flipX) hw = -hw; 

            sf::Transform combinedTransform;
            combinedTransform.translate(node.position).rotate(sf::degrees(node.rotation));

            sf::Vector2f p1 = combinedTransform.transformPoint({-hw, -hh});
            sf::Vector2f p2 = combinedTransform.transformPoint({hw, -hh});
            sf::Vector2f p3 = combinedTransform.transformPoint({hw, hh});
            sf::Vector2f p4 = combinedTransform.transformPoint({-hw, hh});

            const auto& tr = node.texRect;
            sf::Color c = node.color;
            vertexBuffer.push_back(sf::Vertex(p1, c, {tr.position.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p2, c, {tr.position.x + tr.size.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p3, c, {tr.position.x + tr.size.x, tr.position.y + tr.size.y}));
            
            vertexBuffer.push_back(sf::Vertex(p1, c, {tr.position.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p3, c, {tr.position.x + tr.size.x, tr.position.y + tr.size.y}));
            vertexBuffer.push_back(sf::Vertex(p4, c, {tr.position.x, tr.position.y + tr.size.y}));
        }

        flush();
    }
};
