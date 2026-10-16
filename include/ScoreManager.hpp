#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdint>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"

// Máscara de bits para Mods estilo osu!
enum ModFlags : uint32_t {
    MOD_NONE = 0,
    MOD_NF   = (1u << 0), // No Fail     (0.50x)
    MOD_EZ   = (1u << 1), // Easy        (0.50x)
    MOD_HT   = (1u << 2), // Half Time   (0.30x, 0.75x velocidad)
    MOD_HR   = (1u << 3), // Hard Rock   (1.06x, invierte Y y sube CS/AR/OD/HP)
    MOD_DT   = (1u << 4), // Double Time (1.12x, 1.50x velocidad)
    MOD_HD   = (1u << 5), // Hidden      (1.06x, desaparece el círculo y sin anillo de aproximación)
    MOD_AUTO = (1u << 6)  // Autoplay    (Bot perfecto de demostración)
};

inline float getModsScoreMultiplier(uint32_t mods) {
    float mult = 1.0f;
    if (mods & MOD_NF) mult *= 0.50f;
    if (mods & MOD_EZ) mult *= 0.50f;
    if (mods & MOD_HT) mult *= 0.30f;
    if (mods & MOD_HR) mult *= 1.06f;
    if (mods & MOD_DT) mult *= 1.12f;
    if (mods & MOD_HD) mult *= 1.06f;
    return mult;
}

inline float getModsSpeedMultiplier(uint32_t mods) {
    if (mods & MOD_DT) return 1.50f;
    if (mods & MOD_HT) return 0.75f;
    return 1.0f;
}

inline std::string formatModsString(uint32_t mods) {
    if (mods == MOD_NONE) return "Ninguno";
    std::string s = "";
    auto add = [&](const char* tag) {
        if (!s.empty()) s += "+";
        s += tag;
    };
    if (mods & MOD_EZ)   add("EZ");
    if (mods & MOD_NF)   add("NF");
    if (mods & MOD_HT)   add("HT");
    if (mods & MOD_HR)   add("HR");
    if (mods & MOD_DT)   add("DT");
    if (mods & MOD_HD)   add("HD");
    if (mods & MOD_AUTO) add("AUTO");
    return s;
}

struct ScoreRecord {
    std::string osuKey;
    int bestScore = 0;
    int bestCombo = 0;
    float bestAcc = 0.0f;
    std::string bestGrade = "D";
    uint32_t bestMods = 0;
};

class ScoreManager {
public:
    std::vector<ScoreRecord> records;

    static std::string makeKey(const std::string& osuPath) {
        size_t slash = osuPath.find_last_of("/\\");
        std::string base = (slash != std::string::npos) ? osuPath.substr(slash + 1) : osuPath;
        return Beatmap::toLower(base);
    }

    void load(const std::string& path = "sdmc:/3ds/osu-3ds/scores.cfg") {
        records.clear();
        std::ifstream f(path.c_str());
        if (!f.is_open()) return;

        std::string line;
        while (std::getline(f, line)) {
            line = Beatmap::trim(line);
            if (line.empty()) continue;
            std::vector<std::string> parts = Beatmap::split(line, '|');
            if (parts.size() >= 6) {
                ScoreRecord rec;
                rec.osuKey    = parts[0];
                rec.bestScore = std::atoi(parts[1].c_str());
                rec.bestCombo = std::atoi(parts[2].c_str());
                rec.bestAcc   = std::strtof(parts[3].c_str(), nullptr);
                rec.bestGrade = parts[4];
                rec.bestMods  = static_cast<uint32_t>(std::atoi(parts[5].c_str()));
                records.push_back(rec);
            }
        }
    }

    void save(const std::string& path = "sdmc:/3ds/osu-3ds/scores.cfg") const {
        OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds");
        std::ofstream f(path.c_str(), std::ios::trunc);
        if (!f.is_open()) return;

        for (const auto& r : records) {
            f << r.osuKey << "|"
              << r.bestScore << "|"
              << r.bestCombo << "|"
              << r.bestAcc << "|"
              << r.bestGrade << "|"
              << r.bestMods << "\n";
        }
    }

    const ScoreRecord* getRecord(const std::string& osuPath) const {
        std::string key = makeKey(osuPath);
        for (const auto& r : records) {
            if (r.osuKey == key) return &r;
        }
        return nullptr;
    }

    // Guarda el récord si supera la puntuación anterior (y no se usó Autoplay)
    bool submitScore(const std::string& osuPath, int score, int maxCombo, float acc, const std::string& grade, uint32_t mods) {
        if (mods & MOD_AUTO) return false;
        std::string key = makeKey(osuPath);
        for (auto& r : records) {
            if (r.osuKey == key) {
                if (score > r.bestScore) {
                    r.bestScore = score;
                    r.bestCombo = maxCombo;
                    r.bestAcc   = acc;
                    r.bestGrade = grade;
                    r.bestMods  = mods;
                    save();
                    return true;
                }
                return false;
            }
        }

        ScoreRecord rec;
        rec.osuKey    = key;
        rec.bestScore = score;
        rec.bestCombo = maxCombo;
        rec.bestAcc   = acc;
        rec.bestGrade = grade;
        rec.bestMods  = mods;
        records.push_back(rec);
        save();
        return true;
    }
};
