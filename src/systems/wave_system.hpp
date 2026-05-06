#pragma once
#include <random>
#include "entity_factory.hpp"
#include "config_manager.hpp"

class WaveSystem {
public:
    enum class State {
        Spawning,
        WaitingForClear,
        WaveCooledDown
    };

    WaveSystem(const std::vector<config::WaveConfig>& waveConfigs) 
        : m_waveConfigs(waveConfigs) {
        std::random_device rd;
        gen.seed(rd());
        
        if (!m_waveConfigs.empty()) {
            m_state = State::WaveCooledDown;
            m_timer = m_waveConfigs[0].waveWaitTime;
            currentWaveIndex = 0;
        }
    }

    void update(entt::registry& registry, float deltaTime, sf::Vector2f center, 
                const config::ConfigManager& configMgr) {
        if (currentWaveIndex >= m_waveConfigs.size()) return;

        const auto& currentWave = m_waveConfigs[currentWaveIndex];

        switch (m_state) {
            case State::Spawning:
                m_timer -= deltaTime;
                if (m_timer <= 0.f) {
                    spawnGroup(registry, center, currentWave, configMgr);
                    
                    if (enemiesSpawnedThisWave >= currentWave.totalEnemies) {
                        m_state = State::WaitingForClear;
                    } else {
                        prepareNextGroup(currentWave);
                    }
                }
                break;

            case State::WaitingForClear:
                {
                    auto enemyView = registry.view<component::EnemyTag>();
                    if (enemyView.begin() == enemyView.end()) {
                        currentWaveIndex++;
                        if (currentWaveIndex < m_waveConfigs.size()) {
                            m_timer = m_waveConfigs[currentWaveIndex].waveWaitTime;
                            m_state = State::WaveCooledDown;
                        }
                    }
                }
                break;

            case State::WaveCooledDown:
                m_timer -= deltaTime;
                if (m_timer <= 0.f) {
                    startWave(currentWaveIndex);
                }
                break;
        }
    }

    int getCurrentWaveNumber() const { return currentWaveIndex + 1; }
    float getCurrentWaveZoom() const { 
        if (currentWaveIndex < m_waveConfigs.size()) return m_waveConfigs[currentWaveIndex].cameraZoom;
        return 1.0f;
    }

private:
    void startWave(int index) {
        currentWaveIndex = index;
        enemiesSpawnedThisWave = 0;
        m_state = State::Spawning;
        m_timer = 0.f;
    }

    void prepareNextGroup(const config::WaveConfig& config) {
        std::uniform_real_distribution<float> delayDist(config.minInterval, config.maxInterval);
        m_timer = delayDist(gen);
    }

    void spawnGroup(entt::registry& registry, sf::Vector2f center, const config::WaveConfig& config,
                    const config::ConfigManager& configMgr) {
        
        std::uniform_int_distribution<int> countDist(config.minPerGroup, config.maxPerGroup);
        int count = countDist(gen);
        
        int remaining = config.totalEnemies - enemiesSpawnedThisWave;
        if (count > remaining) count = remaining;

        if (count <= 0) return;

        if (config.enemyTypes.empty()) return;
        std::uniform_int_distribution<int> typeDist(0, static_cast<int>(config.enemyTypes.size()) - 1);
        std::string enemyTypeName = config.enemyTypes[typeDist(gen)];

        // 유닛 생성을 위한 각도 계산 (원형으로 분산 생성)
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * 3.14159f);
        
        for (int i = 0; i < count; ++i) {
            float angle = angleDist(gen);
            sf::Vector2f spawnPos = center + sf::Vector2f(std::cos(angle), std::sin(angle)) * config.spawnDistance;
            
            EntityFactory::createEnemy(registry, spawnPos, enemyTypeName, configMgr);
        }
        enemiesSpawnedThisWave += count;
    }

    std::vector<config::WaveConfig> m_waveConfigs;
    int currentWaveIndex = 0;
    int enemiesSpawnedThisWave = 0;
    float m_timer = 0.f;
    State m_state = State::Spawning;
    std::mt19937 gen;
};
