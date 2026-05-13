#pragma once
#include <SFML/System/Vector2.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace config {

struct StatueConfig {
    float maxHealth = 1000.0f;
    float scale = 0.3f;
    sf::Vector2f boxSize = { 80.0f, 40.0f };
    sf::Vector2f boxOffset = { 0.0f, 60.0f };
    float hpRegen = 1.0f;
    float armor = 5.0f;
};

struct SkillConfig {
    struct Turret {
        float fireInterval = 0.5f;
        float projectileSpeed = 500.0f;
        float projectileDamage = 25.0f;
        float attackRange = 500.0f;
    } turret;

    struct HolyAttack {
        float cooldown = 5.0f;
        float damage = 100.0f;
        float radius = 200.0f;
    } holy;

    struct SpawnKnight {
        float cooldown = 10.0f;
        int spawnCount = 3;
    } spawnKnight;

    struct Rosary {
        float damage = 10.0f;
        float knockbackForce = 150.0f;
        float rotationSpeed = 2.0f;
        float radius = 120.0f;
        float duration = 10.0f;      // Skill active time
        float passiveCooldown = 5.0f; // Recharge time in card slot
    } rosary;

    struct GodRay {
        float attackInterval = 0.5f; // Time between beams while active
        float damage = 150.0f;
        float range = 600.0f;
        float duration = 10.0f;      // How long the skill lasts
        float passiveCooldown = 5.0f; // Recharge time in card slot
        float knockbackForce = 80.0f;
        float knockbackDuration = 0.2f;
    } godRay;
};

struct EnemyConfig {
    std::string name;
    float speed = 60.0f;
    float damage = 10.0f;
    float maxHealth = 50.0f;
    float attackSpeed = 1.0f;
    float attackRange = 50.0f;
    float scale = 0.1f;
    float radius = 3.0f;
    bool isPushable = true; 
    bool isStunnable = true;
    std::string aiType = "SeekStatue"; // [NEW] Default behavior
    sf::Vector2f pivotOffset = { 0.f, 0.f }; 
    std::string texturePath;

    struct AnimationData {
        bool enabled = false;
        int frameCount = 1;
        int framesPerRow = 1; 
        float frameDuration = 0.1f;
        int frameWidth = 0;
        int frameHeight = 0;
    } animation;
};

struct WaveConfig {
    int totalEnemies = 30;
    int minPerGroup = 5;
    int maxPerGroup = 10;
    float minInterval = 3.0f;
    float maxInterval = 5.0f;
    float spawnDistance = 400.0f;
    float waveWaitTime = 3.0f; 
    float cameraZoom = 1.0f;
    std::vector<std::string> enemyTypes;
};

class ConfigManager {
public:
    StatueConfig statue;
    SkillConfig skills;
    std::map<std::string, EnemyConfig> enemies;
    std::vector<WaveConfig> waves;

    bool loadAll() {
        std::filesystem::path root = findProjectRoot();
        bool success = true;
        if (!loadStatue(root / "configs/Statue.json")) success = false;
        if (!loadSkills(root / "configs/Skills.json")) {
            std::cout << "Warning: configs/Skills.json not found, using defaults." << std::endl;
        }
        if (!loadEnemies(root / "configs/enemies")) success = false;
        if (!loadWaves(root / "configs/waves")) success = false;
        return success;
    }

private:
    std::filesystem::path findProjectRoot() {
        std::filesystem::path current = std::filesystem::current_path();
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(current / "configs")) return current;
            if (current.has_parent_path()) current = current.parent_path();
            else break;
        }
        return std::filesystem::current_path();
    }

    nlohmann::json parseWithComments(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) return nullptr;
        try {
            return nlohmann::json::parse(file, nullptr, true, true);
        } catch (...) { return nullptr; }
    }

    bool loadStatue(const std::filesystem::path& path) {
        try {
            auto j = parseWithComments(path);
            if (j.is_null()) return false;
            statue.maxHealth = j.value("maxHealth", statue.maxHealth);
            statue.scale = j.value("scale", statue.scale);
            
            if (j.contains("boxSize")) {
                statue.boxSize.x = j["boxSize"].value("x", statue.boxSize.x);
                statue.boxSize.y = j["boxSize"].value("y", statue.boxSize.y);
            }
            if (j.contains("boxOffset")) {
                statue.boxOffset.x = j["boxOffset"].value("x", statue.boxOffset.x);
                statue.boxOffset.y = j["boxOffset"].value("y", statue.boxOffset.y);
            }

            statue.hpRegen = j.value("hpRegen", statue.hpRegen);
            statue.armor = j.value("armor", statue.armor);
            return true;
        } catch (...) { return false; }
    }

    bool loadSkills(const std::filesystem::path& path) {
        try {
            auto j = parseWithComments(path);
            if (j.is_null()) return false;

            if (j.contains("turret")) {
                auto& t = j["turret"];
                skills.turret.fireInterval = t.value("fireInterval", skills.turret.fireInterval);
                skills.turret.projectileSpeed = t.value("projectileSpeed", skills.turret.projectileSpeed);
                skills.turret.projectileDamage = t.value("projectileDamage", skills.turret.projectileDamage);
                skills.turret.attackRange = t.value("attackRange", skills.turret.attackRange);
            }

            if (j.contains("holy")) {
                auto& h = j["holy"];
                skills.holy.cooldown = h.value("cooldown", skills.holy.cooldown);
                skills.holy.damage = h.value("damage", skills.holy.damage);
                skills.holy.radius = h.value("radius", skills.holy.radius);
            }

            if (j.contains("spawnKnight")) {
                auto& s = j["spawnKnight"];
                skills.spawnKnight.cooldown = s.value("cooldown", skills.spawnKnight.cooldown);
                skills.spawnKnight.spawnCount = s.value("spawnCount", skills.spawnKnight.spawnCount);
            }

            if (j.contains("rosary")) {
                auto& r = j["rosary"];
                skills.rosary.damage = r.value("damage", skills.rosary.damage);
                skills.rosary.knockbackForce = r.value("knockbackForce", skills.rosary.knockbackForce);
                skills.rosary.rotationSpeed = r.value("rotationSpeed", skills.rosary.rotationSpeed);
                skills.rosary.radius = r.value("radius", skills.rosary.radius);
                skills.rosary.duration = r.value("duration", skills.rosary.duration);
                skills.rosary.passiveCooldown = r.value("passiveCooldown", skills.rosary.passiveCooldown);
            }

            if (j.contains("godRay")) {
                auto& g = j["godRay"];
                // Map "cooldown" from JSON to attackInterval to maintain compatibility but fix logic
                skills.godRay.attackInterval = g.value("cooldown", skills.godRay.attackInterval); 
                skills.godRay.damage = g.value("damage", skills.godRay.damage);
                skills.godRay.range = g.value("range", skills.godRay.range);
                skills.godRay.duration = g.value("duration", skills.godRay.duration);
                skills.godRay.passiveCooldown = g.value("passiveCooldown", skills.godRay.passiveCooldown);
                skills.godRay.knockbackForce = g.value("knockbackForce", skills.godRay.knockbackForce);
                skills.godRay.knockbackDuration = g.value("knockbackDuration", skills.godRay.knockbackDuration);
            }
            return true;
        } catch (...) { return false; }
    }

    bool loadEnemies(const std::filesystem::path& dir) {
        try {
            if (!std::filesystem::exists(dir)) return false;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".json") {
                    auto j = parseWithComments(entry.path());
                    if (j.is_null()) continue;
                    EnemyConfig cfg;
                    cfg.name = j.value("name", entry.path().stem().string());
                    cfg.speed = j.value("speed", cfg.speed);
                    cfg.damage = j.value("damage", cfg.damage);
                    cfg.maxHealth = j.value("maxHealth", cfg.maxHealth);
                    cfg.attackSpeed = j.value("attackSpeed", cfg.attackSpeed);
                    cfg.attackRange = j.value("attackRange", cfg.attackRange);
                    cfg.scale = j.value("scale", cfg.scale);
                    cfg.radius = j.value("radius", cfg.radius);
                    cfg.isPushable = j.value("isPushable", true);
                    cfg.isStunnable = j.value("isStunnable", true);
                    cfg.aiType = j.value("aiType", "SeekStatue"); // [NEW] Parse AI type
                    cfg.texturePath = j.value("texturePath", "");

                    if (j.contains("pivotOffset")) {
                        cfg.pivotOffset.x = j["pivotOffset"].value("x", 0.f);
                        cfg.pivotOffset.y = j["pivotOffset"].value("y", 0.f);
                    }

                    if (j.contains("animation")) {
                        auto& anim = j["animation"];
                        cfg.animation.enabled = true;
                        cfg.animation.frameCount = anim.value("frameCount", 1);
                        cfg.animation.framesPerRow = anim.value("framesPerRow", 1);
                        cfg.animation.frameDuration = anim.value("frameDuration", 0.1f);
                        cfg.animation.frameWidth = anim.value("frameWidth", 0);
                        cfg.animation.frameHeight = anim.value("frameHeight", 0);
                    }

                    enemies[cfg.name] = cfg;
                }
            }
            return true;
        } catch (...) { return false; }
    }

    bool loadWaves(const std::filesystem::path& dir) {
        try {
            if (!std::filesystem::exists(dir)) return false;
            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".json") paths.push_back(entry.path());
            }
            std::sort(paths.begin(), paths.end());
            for (const auto& path : paths) {
                auto j = parseWithComments(path);
                if (j.is_null()) continue;
                WaveConfig cfg;
                cfg.totalEnemies = j.value("totalEnemies", cfg.totalEnemies);
                cfg.minPerGroup = j.value("minPerGroup", cfg.minPerGroup);
                cfg.maxPerGroup = j.value("maxPerGroup", cfg.maxPerGroup);
                cfg.minInterval = j.value("minInterval", cfg.minInterval);
                cfg.maxInterval = j.value("maxInterval", cfg.maxInterval);
                cfg.spawnDistance = j.value("spawnDistance", cfg.spawnDistance);
                cfg.waveWaitTime = j.value("waveWaitTime", cfg.waveWaitTime);
                cfg.cameraZoom = j.value("cameraZoom", cfg.cameraZoom);
                cfg.enemyTypes = j.value("enemyTypes", std::vector<std::string>{});
                waves.push_back(cfg);
            }
            return true;
        } catch (...) { return false; }
    }
};

} // namespace config
