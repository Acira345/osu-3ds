#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"
#include "ScoreManager.hpp"

enum SongSortMode {
    SORT_BY_TITLE = 0,
    SORT_BY_ARTIST = 1,
    SORT_BY_STARS = 2,
    SORT_BY_RECORD = 3
};

struct BeatmapEntry {
    int mode = 0; // 0: Standard, 1: Taiko, 2: Catch, 3: Mania
    std::string osuPath;
    std::string version;
    std::string creator;
    std::string audioPath;
    std::string backgroundPath;
    std::string videoPath;
    int videoOffsetMs = 0;
    int previewTimeMs = -1;
    float cs = 4.0f;
    float ar = 5.0f;
    float od = 5.0f;
    float hp = 5.0f;
    float starRating = 1.5f;
    float beatLengthMs = 500.0f;
    float timingOffsetMs = 0.0f;
    int maxPossibleCombo = 0;
    int circleCount = 0;
    int sliderCount = 0;
    int spinnerCount = 0;
    int firstObjectTime = 0;
    int lastObjectTime = 0;
    std::vector<float> strainGraph;

    int totalObjects() const {
        return circleCount + sliderCount + spinnerCount;
    }

    float difficultyRating() const {
        return starRating;
    }

    float getBPM(float speedMult = 1.0f) const {
        return (60000.0f / std::max(60.0f, beatLengthMs)) * speedMult;
    }
};

struct BeatmapSet {
    std::string title;
    std::string artist;
    std::string folderPath;
    std::string backgroundPath;
    bool hasVideo = false;
    bool fromRomfs = false;
    std::vector<BeatmapEntry> difficulties;
};

class SongManager {
public:
    std::vector<BeatmapSet> allSets;
    std::vector<BeatmapSet> sets;
    int extractedOszCount = 0;
    int sortMode = SORT_BY_TITLE;
    std::string searchQuery = "";

    static const char* getSortModeName(int mode) {
        switch (mode % 4) {
            case SORT_BY_TITLE:  return "Titulo (A-Z)";
            case SORT_BY_ARTIST: return "Artista (A-Z)";
            case SORT_BY_STARS:  return "Estrellas (*)";
            case SORT_BY_RECORD: return "Mejor PP";
            default:             return "Titulo";
        }
    }

    void initAndScan(const ScoreManager* scoreMgr = nullptr) {
        allSets.clear();
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

        // 4. Ordenar dificultades de cada canción por Estrellas (★)
        for (auto& set : allSets) {
            std::sort(set.difficulties.begin(), set.difficulties.end(),
                [](const BeatmapEntry& a, const BeatmapEntry& b) {
                    return a.starRating < b.starRating;
                }
            );
        }

        applySortAndFilter(scoreMgr);
    }

    bool deleteSet(size_t setIndex, const ScoreManager* scoreMgr = nullptr) {
        if (setIndex >= sets.size()) return false;
        const auto& s = sets[setIndex];
        if (s.fromRomfs) return false;
        bool ok = OszExtractor::removeDirectoryRecursive(s.folderPath);
        if (ok) {
            initAndScan(scoreMgr);
        }
        return ok;
    }

    void applySortAndFilter(const ScoreManager* scoreMgr = nullptr) {
        sets.clear();
        std::string rawQ = Beatmap::toLower(Beatmap::trim(searchQuery));

        // Detectar filtros especiales por etiquetas (mode: y stars:)
        int filterMode = -1; // -1: cualquiera, 0: std, 1: taiko, 2: catch, 3: mania
        float filterMinStars = -1.0f;
        std::string textQ = "";

        if (!rawQ.empty()) {
            std::stringstream ss(rawQ);
            std::string token;
            while (ss >> token) {
                if (token == "mode:std" || token == "mode:osu" || token == "mode:0") {
                    filterMode = 0;
                } else if (token == "mode:taiko" || token == "mode:1") {
                    filterMode = 1;
                } else if (token == "mode:catch" || token == "mode:ctb" || token == "mode:2") {
                    filterMode = 2;
                } else if (token == "mode:mania" || token == "mode:3") {
                    filterMode = 3;
                } else if (token == "stars>4" || token == "★4" || token == "★4+") {
                    filterMinStars = 4.0f;
                } else if (token == "stars>5" || token == "★5" || token == "★5+") {
                    filterMinStars = 5.0f;
                } else if (token == "stars>3" || token == "★3" || token == "★3+") {
                    filterMinStars = 3.0f;
                } else {
                    if (!textQ.empty()) textQ += " ";
                    textQ += token;
                }
            }
        }

        for (const auto& s : allSets) {
            // 1. Filtrar por modo de juego si está activo
            if (filterMode != -1) {
                bool hasMode = false;
                for (const auto& d : s.difficulties) {
                    if (d.mode == filterMode) {
                        hasMode = true;
                        break;
                    }
                }
                if (!hasMode) continue;
            }

            // 2. Filtrar por estrellas mínimas si está activo
            if (filterMinStars > 0.0f) {
                bool hasStars = false;
                for (const auto& d : s.difficulties) {
                    if (d.starRating >= filterMinStars) {
                        hasStars = true;
                        break;
                    }
                }
                if (!hasStars) continue;
            }

            // 3. Filtrar por texto de búsqueda
            if (!textQ.empty()) {
                bool match = (Beatmap::toLower(s.title).find(textQ) != std::string::npos) ||
                             (Beatmap::toLower(s.artist).find(textQ) != std::string::npos);
                if (!match) {
                    for (const auto& d : s.difficulties) {
                        if (Beatmap::toLower(d.version).find(textQ) != std::string::npos ||
                            Beatmap::toLower(d.creator).find(textQ) != std::string::npos) {
                            match = true;
                            break;
                        }
                    }
                }
                if (!match) continue;
            }

            sets.push_back(s);
        }

        auto getSetMaxStars = [](const BeatmapSet& s) -> float {
            if (s.difficulties.empty()) return 0.0f;
            return s.difficulties.back().starRating;
        };

        auto getSetBestPP = [scoreMgr](const BeatmapSet& s) -> float {
            if (!scoreMgr) return 0.0f;
            float best = 0.0f;
            for (const auto& d : s.difficulties) {
                const auto* rec = scoreMgr->getRecord(d.osuPath);
                if (rec && rec->bestPP > best) best = rec->bestPP;
            }
            return best;
        };

        std::sort(sets.begin(), sets.end(),
            [&](const BeatmapSet& a, const BeatmapSet& b) {
                if (sortMode == SORT_BY_ARTIST) {
                    std::string aa = Beatmap::toLower(a.artist);
                    std::string ba = Beatmap::toLower(b.artist);
                    if (aa != ba) return aa < ba;
                } else if (sortMode == SORT_BY_STARS) {
                    float sa = getSetMaxStars(a);
                    float sb = getSetMaxStars(b);
                    if (std::fabs(sa - sb) > 0.01f) return sa < sb;
                } else if (sortMode == SORT_BY_RECORD) {
                    float pa = getSetBestPP(a);
                    float pb = getSetBestPP(b);
                    if (std::fabs(pa - pb) > 0.05f) return pa > pb;
                }
                if (a.fromRomfs != b.fromRomfs) return !a.fromRomfs;
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
        // Soportar osu!standard (0), Taiko (1), Catch (2), Mania (3)
        if (meta.mode < 0 || meta.mode > 3) return;

        BeatmapEntry entry;
        entry.mode             = meta.mode;
        entry.osuPath          = osuPath;
        entry.version          = meta.version;
        entry.creator          = meta.creator;
        entry.audioPath        = meta.fullAudioPath;
        entry.backgroundPath   = meta.fullBackgroundPath;
        entry.videoPath        = meta.fullVideoPath;
        entry.videoOffsetMs    = meta.videoOffsetMs;
        entry.previewTimeMs    = meta.previewTime;
        entry.cs               = meta.diff.cs;
        entry.ar               = meta.diff.ar;
        entry.od               = meta.diff.od;
        entry.hp               = meta.diff.hp;
        entry.starRating       = meta.starRating;
        entry.beatLengthMs     = meta.baseBeatLengthMs;
        entry.timingOffsetMs   = meta.firstTimingOffsetMs;
        entry.maxPossibleCombo = std::max(1, meta.maxPossibleCombo);
        entry.circleCount      = meta.circleCount;
        entry.sliderCount      = meta.sliderCount;
        entry.spinnerCount     = meta.spinnerCount;
        entry.firstObjectTime  = meta.firstObjectTime;
        entry.lastObjectTime   = meta.lastObjectTime;
        entry.strainGraph      = meta.strainGraph;

        // Agrupar por título y carpeta
        std::string keyTitle = Beatmap::toLower(meta.title);
        for (auto& set : allSets) {
            if (set.folderPath == meta.folderPath && Beatmap::toLower(set.title) == keyTitle) {
                if (set.backgroundPath.empty() && !entry.backgroundPath.empty()) {
                    set.backgroundPath = entry.backgroundPath;
                }
                if (!entry.videoPath.empty()) {
                    set.hasVideo = true;
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
        newSet.hasVideo       = !entry.videoPath.empty();
        newSet.fromRomfs      = isRomfs;
        newSet.difficulties.push_back(entry);
        allSets.push_back(newSet);
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
