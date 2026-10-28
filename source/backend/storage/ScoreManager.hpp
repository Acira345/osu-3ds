#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdint>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"

// Máscara de bits para Mods (definida en Beatmap.hpp como OsuMods)


inline float getModsScoreMultiplier(uint32_t mods) {
    if (mods & (MOD_AUTO | MOD_RX | MOD_AP)) return 0.0f;
    float mult = 1.0f;
    if (mods & MOD_NF) mult *= 0.50f;
    if (mods & MOD_EZ) mult *= 0.50f;
    if (mods & MOD_HT) mult *= 0.30f;
    if (mods & MOD_SO) mult *= 0.90f;
    if (mods & MOD_HR) mult *= 1.06f;
    if (mods & (MOD_DT | MOD_NC)) mult *= 1.12f;
    if (mods & MOD_HD) mult *= 1.06f;
    if (mods & MOD_FL) mult *= 1.12f;
    return mult;
}

inline float getModsSpeedMultiplier(uint32_t mods) {
    if (mods & (MOD_DT | MOD_NC)) return 1.50f;
    if (mods & MOD_HT) return 0.75f;
    return 1.0f;
}

inline float getModAdjustedStars(float baseStars, uint32_t mods) {
    float s = baseStars;
    if (mods & MOD_EZ) s *= 0.80f;
    if (mods & MOD_HR) s *= 1.10f;
    if (mods & MOD_HT) s *= 0.76f;
    if (mods & (MOD_DT | MOD_NC)) s *= 1.36f;
    if (mods & MOD_FL) s *= 1.15f;
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
    if (mods & (MOD_DT | MOD_NC)) { adjOD = std::min(11.0f, adjOD * 1.15f); adjAR = std::min(11.0f, adjAR * 1.15f); }
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
    if (mods & MOD_FL) modMult *= 1.15f;
    if (mods & MOD_NF) modMult *= std::max(0.50f, 1.0f - 0.02f * cMiss);
    if (mods & (MOD_AUTO | MOD_RX | MOD_AP)) modMult = 0.0f;

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
    if (mods & MOD_SD)   add("SD");
    if (mods & MOD_PF)   add("PF");
    if (mods & MOD_DT)   add("DT");
    if (mods & MOD_NC)   add("NC");
    if (mods & MOD_HD)   add("HD");
    if (mods & MOD_FL)   add("FL");
    if (mods & MOD_RX)   add("RX");
    if (mods & MOD_AP)   add("AP");
    if (mods & MOD_SO)   add("SO");
    if (mods & MOD_MR)   add("MR");
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
    int mode = 0; // 0: Standard, 1: Taiko, 2: Catch, 3: Mania
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
                rec.mode      = (parts.size() >= 8) ? std::atoi(parts[7].c_str()) : 0;
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
              << r.bestPP << "|"
              << r.mode << "\n";
        }
    }

    const ScoreRecord* getRecord(const std::string& osuPath, int mode = -1) const {
        std::string key = makeKey(osuPath);
        const ScoreRecord* best = nullptr;
        for (const auto& r : records) {
            if (r.osuKey == key && (mode < 0 || r.mode == mode)) {
                if (!best || r.bestScore > best->bestScore) {
                    best = &r;
                }
            }
        }
        return best;
    }

    std::vector<ScoreRecord> getTopRecords(const std::string& osuPath, int maxCount = 10, int mode = -1) const {
        std::string key = makeKey(osuPath);
        std::vector<ScoreRecord> result;
        for (const auto& r : records) {
            if (r.osuKey == key && (mode < 0 || r.mode == mode)) {
                result.push_back(r);
            }
        }
        std::sort(result.begin(), result.end(), [](const ScoreRecord& a, const ScoreRecord& b) {
            return a.bestScore > b.bestScore;
        });
        if (static_cast<int>(result.size()) > maxCount) {
            result.resize(maxCount);
        }
        return result;
    }

    // Guarda el récord si supera la puntuación o PP anterior (y no se usó Autoplay)
    bool submitScore(const std::string& osuPath, int score, int maxCombo, float acc, const std::string& grade, uint32_t mods, float pp, int mode = 0, int* outRank = nullptr) {
        if (outRank) *outRank = -1;
        if (mods & MOD_AUTO) return false;
        std::string key = makeKey(osuPath);

        ScoreRecord newRec;
        newRec.osuKey    = key;
        newRec.bestScore = score;
        newRec.bestCombo = maxCombo;
        newRec.bestAcc   = acc;
        newRec.bestGrade = grade;
        newRec.bestMods  = mods;
        newRec.bestPP    = pp;
        newRec.mode      = mode;

        const ScoreRecord* prevBest = getRecord(osuPath, mode);
        bool improved = (!prevBest || score > prevBest->bestScore);

        std::vector<ScoreRecord> mapRecords = getTopRecords(osuPath, 10, mode);
        mapRecords.push_back(newRec);
        std::sort(mapRecords.begin(), mapRecords.end(), [](const ScoreRecord& a, const ScoreRecord& b) {
            return a.bestScore > b.bestScore;
        });

        if (outRank) {
            for (size_t i = 0; i < mapRecords.size() && i < 10; ++i) {
                if (mapRecords[i].bestScore == score && mapRecords[i].bestCombo == maxCombo &&
                    mapRecords[i].bestMods == mods && std::abs(mapRecords[i].bestAcc - acc) < 0.001f) {
                    *outRank = static_cast<int>(i + 1);
                    break;
                }
            }
        }

        if (mapRecords.size() > 10) mapRecords.resize(10);

        std::vector<ScoreRecord> remaining;
        remaining.reserve(records.size() + 5);
        for (const auto& r : records) {
            if (r.osuKey != key || r.mode != mode) remaining.push_back(r);
        }
        for (const auto& r : mapRecords) {
            remaining.push_back(r);
        }
        records = remaining;
        save();
        return improved;
    }
};
