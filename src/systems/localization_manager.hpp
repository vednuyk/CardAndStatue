#pragma once
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <SFML/System/String.hpp>

class LocalizationManager {
public:
    enum class Language {
        English,
        Korean
    };

    static LocalizationManager& getInstance() {
        static LocalizationManager instance;
        return instance;
    }

    bool loadLanguage(Language lang) {
        m_currentLang = lang;
        std::string filename = (lang == Language::English) ? "lang_en.json" : "lang_ko.json";
        
        std::vector<std::string> paths = {
            "configs/" + filename,
            "../configs/" + filename,
            filename
        };

        std::ifstream file;
        for (const auto& path : paths) {
            file.open(path);
            if (file.is_open()) break;
        }

        if (!file.is_open()) {
            std::cerr << "Localization Error: Could not find " << filename << " in any search path." << std::endl;
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;
            
            // [OPTIMIZED] Pre-convert and cache as sf::String to avoid per-frame conversion
            m_data.clear();
            auto rawData = j.get<std::map<std::string, std::string>>();
            for (const auto& [key, val] : rawData) {
                m_data[key] = sf::String::fromUtf8(val.begin(), val.end());
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing lang file: " << e.what() << std::endl;
            return false;
        }
    }

    void toggleLanguage() {
        if (m_currentLang == Language::English) {
            loadLanguage(Language::Korean);
        } else {
            loadLanguage(Language::English);
        }
    }

    const sf::String& get(const std::string& key) const {
        if (m_data.find(key) != m_data.end()) {
            return m_data.at(key);
        }
        static const sf::String fallback("???");
        return fallback; 
    }

    Language getCurrentLanguage() const { return m_currentLang; }

private:
    LocalizationManager() {
        loadLanguage(Language::English); // Default
    }

    Language m_currentLang = Language::English;
    std::map<std::string, sf::String> m_data; // [OPTIMIZED] Store sf::String directly
};
