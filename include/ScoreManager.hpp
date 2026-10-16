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

inline float getModAdjustedStars(float baseStars, uint32_t mods) {
    float s = baseStars;
    if (mods & MOD_EZ) s *= 0.80f;
    if (mods & MOD_HR) s *= 1.10f;
    if (mods & MOD_HT) s *= 0.76f;
    if (mods & MOD_DT) s *= 1.36f;
    return std::max(0.50f, std::min(15.0f, s));
}

// Calcula los Performance Points (PP) estilo osu!standard según estrellas, precisión, combo, fallos y mods
inline float calculatePP(float baseStars, float ar, float od, int achievedCombo, int mapMaxCombo,
                         int c300, int c100, int c50, int cMiss, uint32_t mods) {
    int totalHits = c300 + c100 + c50 + cMiss;
    if (totalHits <= 0) return 0.0f;

    float adjStars = getModAdjustedStars(baseStars, mods);
    float adjOD = od;
    float adjAR = ar;
    if (mods & MOD_EZ) { adjOD *= 0.5f; adjAR *= 0.5f; }
    if (mods & MOD_HR) { adjOD = std::min(10.0f, adjOD * 1.4f); adjAR = std::min(10.0f, adjAR * 1.4f); }
    if (mods & MOD_DT) { adjOD = std::min(11.0f, adjOD * 1.15f); adjAR = std::min(11.0f, adjAR * 1.15f); }
    if (mods & MOD_HT) { adjOD *= 0.85f; adjAR *= 0.85f; }

    float accRatio = (c300 * 300.0f + c100 * 100.0f + c50 * 50.0f) / (static_cast<float>(totalHits) * 300.0f);
    accRatio = std::max(0.0f, std::min(1.0f, accRatio));

    // Valor base exponencial por estrellas (curva inspirada en osu! ppv2)
    float baseVal = std::pow(5.0f * std::max(1.0f, adjStars / 0.072f) - 4.0f, 2.85f) / 100000.0f;

    // Bonus por longitud del mapa (número de objetos)
    float lenBonus = 0.92f + 0.35f * std::min(1.0f, static_cast<float>(totalHits) / 750.0f);
    baseVal *= lenBonus;

    // Escalado por combo y penalización por misses
    int refCombo = std::max(totalHits, mapMaxCombo);
    float comboRatio = std::min(1.0f, static_cast<float>(std::max(1, achievedCombo)) / static_cast<float>(std::max(1, refCombo)));
    float comboMult = std::pow(comboRatio, 0.80f);
    float missPenalty = (cMiss > 0) ? std::pow(0.97f, static_cast<float>(cMiss)) : 1.0f;

    // Escalado por precisión y OD
    float accMult = std::pow(accRatio, 5.2f);
    float accBonusPP = std::pow(1.48f, adjOD) * std::pow(accRatio, 18.0f) * 2.2f * std::min(1.15f, std::pow(static_cast<float>(totalHits) / 400.0f, 0.3f));

    // Bonus por AR alto o Hidden (HD)
    float arFactor = 1.0f;
    if (adjAR > 10.33f) arFactor += 0.25f * (adjAR - 10.33f);
    else if (adjAR < 8.0f) arFactor += 0.02f * (8.0f - adjAR);

    float modMult = 1.0f;
    if (mods & MOD_HD) modMult *= (1.05f + 0.01f * (10.0f - std::min(10.0f, adjAR)));
    if (mods & MOD_NF) modMult *= std::max(0.50f, 1.0f - 0.02f * cMiss);

    float totalPP = (baseVal * comboMult * missPenalty * (0.35f + 0.65f * accMult) * arFactor + accBonusPP) * modMult;
    return std::max(0.0f, totalPP);
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
    float bestPP = 0.0f;
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
                rec.bestPP    = (parts.size() >= 7) ? std::strtof(parts[6].c_str(), nullptr) : 0.0f;
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
              << r.bestMods << "|"
              << r.bestPP << "\n";
        }
    }

    const ScoreRecord* getRecord(const std::string& osuPath) const {
        std::string key = makeKey(osuPath);
        for (const auto& r : records) {
            if (r.osuKey == key) return &r;
        }
        return nullptr;
    }

    // Guarda el récord si supera la puntuación o PP anterior (y no se usó Autoplay)
    bool submitScore(const std::string& osuPath, int score, int maxCombo, float acc, const std::string& grade, uint32_t mods, float pp) {
        if (mods & MOD_AUTO) return false;
        std::string key = makeKey(osuPath);
        for (auto& r : records) {
            if (r.osuKey == key) {
                bool improved = false;
                if (pp > r.bestPP) {
                    r.bestPP = pp;
                    improved = true;
                }
                if (score > r.bestScore) {
                    r.bestScore = score;
                    r.bestCombo = maxCombo;
                    r.bestAcc   = acc;
                    r.bestGrade = grade;
                    r.bestMods  = mods;
                    improved = true;
                }
                if (improved) save();
                return improved;
            }
        }

        ScoreRecord rec;
        rec.osuKey    = key;
        rec.bestScore = score;
        rec.bestCombo = maxCombo;
        rec.bestAcc   = acc;
        rec.bestGrade = grade;
        rec.bestMods  = mods;
        rec.bestPP    = pp;
        records.push_back(rec);
        save();
        return true;
    }
};
