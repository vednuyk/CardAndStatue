#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <memory>
#include <string>

namespace component {

struct Transform {
    sf::Vector2f position{0.f, 0.f};
    float radius{0.f};
    float rotation{0.f}; 
};

struct Velocity {
    sf::Vector2f value{0.f, 0.f};
};

struct BoxCollider {
    sf::Vector2f size{0.f, 0.f};
    sf::Vector2f offset{0.f, 0.f}; 
};

// 유닛 기초 능력치 (공격/이동 가능한 모든 개체)
struct UnitStats {
    float maxHealth{100.f};
    float currentHealth{100.f};
    float speed{60.f};
    float damage{10.f};
    float attackSpeed{1.f}; // 초당 공격 횟수
    float attackRange{100.f};
    float hitFlashTimer{0.f};
};

// 본진(Statue) 전용 능력치
struct StatueStats {
    float maxHealth{1000.f};
    float currentHealth{1000.f};
    float hpRegen{1.0f};    // 초당 회복량
    float armor{5.0f};      // 데미지 감소
};

// 탈부착식 스킬 컴포넌트들
struct TurretSkill {
    float fireInterval{0.5f};
    float timer{0.f};
    float projectileSpeed{500.f};
    float damage{25.f};
    float range{500.f};
};

struct HolyAttackSkill {
    float cooldown{5.f};
    float timer{0.f};
    float damage{100.f};
    float radius{200.f};
};

struct SpawnKnightSkill {
    float cooldown{10.f};
    float timer{0.f};
    int spawnCount{3};
};

struct RosarySkill {
    bool initialized{false};
    bool isClosing{false}; // [NEW] Transition flag for shrinking effect
    float damage{10.f};
    float knockbackForce{150.f};
    float rotationSpeed{2.0f};
    float radius{120.f};
    float duration{10.f};
    float remainingTime{0.f};
};

struct OrbitalSphere {
    entt::entity parent{entt::null};
    float orbitAngle{0.f};         // [NEW] Shared rotation component
    float targetSpreadOffset{0.f}; // [NEW] Individual target offset (e.g., i * 2PI/16)
    float radius{120.f};
    float rotationSpeed{2.0f};
    float knockbackForce{150.f};
    float damage{10.f};
    float expansionProgress{0.f};  // [NEW] 0.0 (center) to 1.0 (full orbit)
};

enum class TextureID : uint8_t {
    Statue = 0,
    Enemy1,
    Grass,
    Flower,
    Knight,
    FallbackRedSquare,
    RosarySphere,
    WhiteFlash,
    Count
};

// 배치 렌더링을 위해 어떤 텍스처를 쓸지에 대한 식별자만 가짐
struct SpriteData {
    TextureID textureID{TextureID::Enemy1};
    std::string textureName; // 디버깅용
    sf::FloatRect textureRect; 
    sf::Vector2f scale{1.f, 1.f};
    bool flipX{false};
};

struct PlayerUnitTag {};
struct EnemyTag {};
struct StatueTag {};
struct HelperTag {};

struct Target {
    entt::entity entity{entt::null};
};

} // namespace component
