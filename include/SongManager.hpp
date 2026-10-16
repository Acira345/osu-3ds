#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"

struct BeatmapEntry {
    std::string osuPath;
    std::string version;
    std::string creator;
    std::string audioPath;
    std::string backgroundPath;
    float cs = 4.0f;
    float ar = 5.0f;
    float od = 5.0f;
    float hp = 5.0f;
    float starRating = 1.5f;
    int maxPossibleCombo = 0;
    int circleCount = 0;
    int sliderCount = 0;
    int spinnerCount = 0;

    int totalObjects() const {
        return circleCount + sliderCount + spinnerCount;
    }

    float difficultyRating() const {
        return starRating;
    }
};

struct BeatmapSet {
    std::string title;
    std::string artist;
    std::string folderPath;
    std::string backgroundPath;
    bool fromRomfs = false;
    std::vector<BeatmapEntry> difficulties;
};

class SongManager {
public:
    std::vector<BeatmapSet> sets;
    int extractedOszCount = 0;

    void initAndScan() {
        sets.clear();
        extractedOszCount = 0;

        // 1. Crear carpetas estándar en la tarjeta SD
        OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds/songs");

        // 2. Descomprimir automáticamente cualquier .osz colocado en las carpetas de la SD
        extractedOszCount += OszExtractor::extractAllPendingOsz(
            "sdmc:/3ds/osu-3ds/songs", "sdmc:/3ds/osu-3ds/songs"
        );
        extractedOszCount += OszExtractor::extractAllPendingOsz(
            "sdmc:/3ds/osu-3ds", "sdmc:/3ds/osu-3ds/songs"
        );
        extractedOszCount += OszExtractor::extractAllPendingOsz(
            "sdmc:/osu-3ds/songs", "sdmc:/3ds/osu-3ds/songs"
        );

        // 3. Escanear carpetas en la SD y en RomFS
        scanRootFolder("sdmc:/3ds/osu-3ds/songs/", false);
        scanRootFolder("sdmc:/osu-3ds/songs/", false);
        scanRootFolder("romfs:/", true);

        // 4. Ordenar dificultades por Estrellas (★) y canciones alfabéticamente
        for (auto& set : sets) {
            std::sort(set.difficulties.begin(), set.difficulties.end(),
                [](const BeatmapEntry& a, const BeatmapEntry& b) {
                    return a.starRating < b.starRating;
                }
            );
        }

        std::sort(sets.begin(), sets.end(),
            [](const BeatmapSet& a, const BeatmapSet& b) {
                if (a.fromRomfs != b.fromRomfs) return !a.fromRomfs; // Priorizar canciones de la SD
                return Beatmap::toLower(a.title) < Beatmap::toLower(b.title);
            }
        );
    }

private:
    static bool hasOsuExtension(const std::string& name) {
        if (name.size() < 5) return false;
        std::string ext = Beatmap::toLower(name.substr(name.size() - 4));
        return ext == ".osu";
    }

    void addOsuFileToSets(const std::string& osuPath, bool isRomfs) {
        Beatmap meta;
        if (!meta.loadFromFile(osuPath, true)) return;
        // Solo osu!standard (Mode 0)
        if (meta.mode != 0) return;

        BeatmapEntry entry;
        entry.osuPath          = osuPath;
        entry.version          = meta.version;
        entry.creator          = meta.creator;
        entry.audioPath        = meta.fullAudioPath;
        entry.backgroundPath   = meta.fullBackgroundPath;
        entry.cs               = meta.diff.cs;
        entry.ar               = meta.diff.ar;
        entry.od               = meta.diff.od;
        entry.hp               = meta.diff.hp;
        entry.starRating       = meta.starRating;
        entry.maxPossibleCombo = std::max(1, meta.maxPossibleCombo);
        entry.circleCount      = meta.circleCount;
        entry.sliderCount      = meta.sliderCount;
        entry.spinnerCount     = meta.spinnerCount;

        // Agrupar por título y carpeta
        std::string keyTitle = Beatmap::toLower(meta.title);
        for (auto& set : sets) {
            if (set.folderPath == meta.folderPath && Beatmap::toLower(set.title) == keyTitle) {
                if (set.backgroundPath.empty() && !entry.backgroundPath.empty()) {
                    set.backgroundPath = entry.backgroundPath;
                }
                set.difficulties.push_back(entry);
                return;
            }
        }

        BeatmapSet newSet;
        newSet.title          = meta.title;
        newSet.artist         = meta.artist;
        newSet.folderPath     = meta.folderPath;
        newSet.backgroundPath = entry.backgroundPath;
        newSet.fromRomfs      = isRomfs;
        newSet.difficulties.push_back(entry);
        sets.push_back(newSet);
    }

    void scanSongSubfolder(const std::string& folderPath, bool isRomfs) {
        std::string openPath = folderPath;
        if (openPath.size() > 1 && openPath.back() == '/' && openPath.find(":/") != openPath.size() - 2) {
            openPath.pop_back();
        }
        DIR* dir = opendir(openPath.c_str());
        if (!dir) return;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            if (hasOsuExtension(name)) {
                addOsuFileToSets(folderPath + name, isRomfs);
            }
        }
        closedir(dir);
    }

    void scanRootFolder(const std::string& rootFolder, bool isRomfs) {
        std::string openPath = rootFolder;
        if (openPath.size() > 1 && openPath.back() == '/' && openPath.find(":/") != openPath.size() - 2) {
            openPath.pop_back();
        }
        DIR* dir = opendir(openPath.c_str());
        if (!dir) return;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            std::string fullPath = rootFolder + name;
            if (hasOsuExtension(name)) {
                addOsuFileToSets(fullPath, isRomfs);
            } else {
                // Comprobar si es una subcarpeta de canción
                DIR* sub = opendir(fullPath.c_str());
                if (sub) {
                    closedir(sub);
                    scanSongSubfolder(fullPath + "/", isRomfs);
                }
            }
        }
        closedir(dir);
    }
};
