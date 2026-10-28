#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"
#include "ScoreManager.hpp"
#include "Localization.hpp"

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

struct CachedBeatmap {
    int64_t fileSize = 0;
    int64_t mtime = 0;
    std::string title;
    std::string artist;
    std::string folderPath;
    std::string setBgPath;
    bool hasVideo = false;
    bool fromRomfs = false;
    BeatmapEntry entry;
};

typedef void (*ScanProgressFn)(void* userdata, const char* statusMsg, int songsFound, int scannedFiles);

class SongManager {
public:
    std::vector<BeatmapSet> allSets;
    std::vector<BeatmapSet> sets;
    int extractedOszCount = 0;
    int sortMode = SORT_BY_TITLE;
    std::string searchQuery = "";

    static const char* getSortModeName(int mode) {
        AppLanguage lang = Localization::getLanguage();
        switch (mode % 4) {
            case SORT_BY_TITLE:
                switch (lang) {
                    case LANG_EN: return "Title (A-Z)";
                    case LANG_RU: return "Название (А-Я)";
                    case LANG_PL: return "Tytuł (A-Z)";
                    case LANG_DE: return "Titel (A-Z)";
                    default:      return "Titulo (A-Z)";
                }
            case SORT_BY_ARTIST:
                switch (lang) {
                    case LANG_EN: return "Artist (A-Z)";
                    case LANG_RU: return "Артист (А-Я)";
                    case LANG_PL: return "Artysta (A-Z)";
                    case LANG_DE: return "Künstler (A-Z)";
                    default:      return "Artista (A-Z)";
                }
            case SORT_BY_STARS:
                switch (lang) {
                    case LANG_EN: return "Stars (*)";
                    case LANG_RU: return "Сложность (*)";
                    case LANG_PL: return "Trudność (*)";
                    case LANG_DE: return "Sterne (*)";
                    default:      return "Estrellas (*)";
                }
            case SORT_BY_RECORD:
                switch (lang) {
                    case LANG_EN: return "Best PP";
                    case LANG_RU: return "Лучший PP";
                    case LANG_PL: return "Najlepsze PP";
                    case LANG_DE: return "Beste PP";
                    default:      return "Mejor PP";
                }
            default:
                switch (lang) {
                    case LANG_EN: return "Title";
                    case LANG_RU: return "Название";
                    case LANG_PL: return "Tytuł";
                    case LANG_DE: return "Titel";
                    default:      return "Titulo";
                }
        }
    }

    void initAndScan(const ScoreManager* scoreMgr = nullptr, ScanProgressFn progressFn = nullptr, void* progressUserData = nullptr) {
        allSets.clear();
        sets.clear();
        extractedOszCount = 0;

        if (progressFn) progressFn(progressUserData, "Verificando paquetes...", 0, 0);

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

        if (progressFn) progressFn(progressUserData, "Cargando cache de canciones...", 0, 0);

        // 3. Cargar caché binaria para acelerar el inicio
        std::unordered_map<std::string, CachedBeatmap> cacheMap;
        const std::string cachePath = "sdmc:/3ds/osu-3ds/songs.cache";
        loadCache(cachePath, cacheMap);

        bool cacheDirty = false;
        std::unordered_map<std::string, bool> activeFiles;
        int scannedCount = 0;

        auto fileScanner = [&](const std::string& fullPath, bool isRomfs) {
            activeFiles[fullPath] = true;
            scannedCount++;

            struct stat st;
            bool statOk = (stat(fullPath.c_str(), &st) == 0);
            int64_t fSize = statOk ? static_cast<int64_t>(st.st_size) : 0;
            int64_t mTime = statOk ? static_cast<int64_t>(st.st_mtime) : 0;

            auto it = cacheMap.find(fullPath);
            if (it != cacheMap.end() && it->second.fileSize == fSize && it->second.mtime == mTime) {
                addCachedBeatmapToSets(it->second);
            } else {
                CachedBeatmap newItem;
                newItem.fileSize = fSize;
                newItem.mtime = mTime;
                if (addOsuFileToSets(fullPath, isRomfs, &newItem)) {
                    cacheMap[fullPath] = newItem;
                    cacheDirty = true;
                }
            }

            if (progressFn && (scannedCount % 3 == 0 || allSets.size() > 0)) {
                progressFn(progressUserData, "Indexando canciones...", static_cast<int>(allSets.size()), scannedCount);
            }
        };

        if (progressFn) progressFn(progressUserData, "Escaneando biblioteca...", 0, 0);

        // 4. Escanear carpetas en la SD y en RomFS
        scanDirectoryTree("sdmc:/3ds/osu-3ds/songs/", false, fileScanner);
        scanDirectoryTree("sdmc:/osu-3ds/songs/", false, fileScanner);
        scanDirectoryTree("romfs:/", true, fileScanner);

        // 5. Si se eliminaron canciones físicas que estaban en caché, podar la caché
        if (cacheMap.size() != activeFiles.size()) {
            std::unordered_map<std::string, CachedBeatmap> pruned;
            for (const auto& pair : cacheMap) {
                if (activeFiles.find(pair.first) != activeFiles.end()) {
                    pruned[pair.first] = pair.second;
                }
            }
            cacheMap = std::move(pruned);
            cacheDirty = true;
        }

        if (cacheDirty) {
            if (progressFn) progressFn(progressUserData, "Guardando cache...", static_cast<int>(allSets.size()), scannedCount);
            saveCache(cachePath, cacheMap);
        }

        if (progressFn) progressFn(progressUserData, "Organizando biblioteca...", static_cast<int>(allSets.size()), scannedCount);

        // 6. Ordenar dificultades de cada canción por Estrellas (★)
        for (auto& set : allSets) {
            std::sort(set.difficulties.begin(), set.difficulties.end(),
                [](const BeatmapEntry& a, const BeatmapEntry& b) {
                    return a.starRating < b.starRating;
                }
            );
        }

        applySortAndFilter(scoreMgr);

        if (progressFn) progressFn(progressUserData, "Listo!", static_cast<int>(allSets.size()), scannedCount);
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

    bool addSingleBeatmap(const std::string& osuPath, bool isRomfs = false, const ScoreManager* scoreMgr = nullptr) {
        addOsuFileToSets(osuPath, isRomfs);
        for (auto& set : allSets) {
            std::sort(set.difficulties.begin(), set.difficulties.end(),
                [](const BeatmapEntry& a, const BeatmapEntry& b) {
                    return a.starRating < b.starRating;
                }
            );
        }
        applySortAndFilter(scoreMgr);
        return true;
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
                    if (sa != sb) return sa < sb;
                } else if (sortMode == SORT_BY_RECORD) {
                    float pa = getSetBestPP(a);
                    float pb = getSetBestPP(b);
                    if (pa != pb) return pa > pb;
                }
                if (a.fromRomfs != b.fromRomfs) return !a.fromRomfs;
                std::string ta = Beatmap::toLower(a.title);
                std::string tb = Beatmap::toLower(b.title);
                if (ta != tb) return ta < tb;
                return a.folderPath < b.folderPath;
            }
        );
    }

private:
    static bool hasOsuExtension(const std::string& name) {
        if (name.size() < 5) return false;
        std::string ext = Beatmap::toLower(name.substr(name.size() - 4));
        return ext == ".osu";
    }

    static void writeStr(FILE* f, const std::string& s) {
        uint16_t len = static_cast<uint16_t>(std::min<size_t>(s.size(), 65535));
        fwrite(&len, sizeof(uint16_t), 1, f);
        if (len > 0) fwrite(s.data(), 1, len, f);
    }

    static bool readStr(FILE* f, std::string& s) {
        uint16_t len = 0;
        if (fread(&len, sizeof(uint16_t), 1, f) != 1) return false;
        s.resize(len);
        if (len > 0) {
            if (fread(&s[0], 1, len, f) != len) return false;
        }
        return true;
    }

    static void saveCache(const std::string& cachePath, const std::unordered_map<std::string, CachedBeatmap>& cacheMap) {
        FILE* f = fopen(cachePath.c_str(), "wb");
        if (!f) return;
        char iobuf[32768];
        setvbuf(f, iobuf, _IOFBF, sizeof(iobuf));

        uint32_t magic = 0x43534F21; // "!OSC"
        uint32_t ver = 2;
        uint32_t count = static_cast<uint32_t>(cacheMap.size());
        fwrite(&magic, sizeof(uint32_t), 1, f);
        fwrite(&ver, sizeof(uint32_t), 1, f);
        fwrite(&count, sizeof(uint32_t), 1, f);

        for (const auto& pair : cacheMap) {
            const std::string& osuPath = pair.first;
            const CachedBeatmap& c = pair.second;
            writeStr(f, osuPath);
            fwrite(&c.fileSize, sizeof(int64_t), 1, f);
            fwrite(&c.mtime, sizeof(int64_t), 1, f);
            writeStr(f, c.title);
            writeStr(f, c.artist);
            writeStr(f, c.folderPath);
            writeStr(f, c.setBgPath);
            uint8_t hasVid = c.hasVideo ? 1 : 0;
            fwrite(&hasVid, 1, 1, f);
            uint8_t romfs = c.fromRomfs ? 1 : 0;
            fwrite(&romfs, 1, 1, f);

            fwrite(&c.entry.mode, sizeof(int), 1, f);
            writeStr(f, c.entry.version);
            writeStr(f, c.entry.creator);
            writeStr(f, c.entry.audioPath);
            writeStr(f, c.entry.backgroundPath);
            writeStr(f, c.entry.videoPath);
            fwrite(&c.entry.videoOffsetMs, sizeof(int), 1, f);
            fwrite(&c.entry.previewTimeMs, sizeof(int), 1, f);
            fwrite(&c.entry.cs, sizeof(float), 1, f);
            fwrite(&c.entry.ar, sizeof(float), 1, f);
            fwrite(&c.entry.od, sizeof(float), 1, f);
            fwrite(&c.entry.hp, sizeof(float), 1, f);
            fwrite(&c.entry.starRating, sizeof(float), 1, f);
            fwrite(&c.entry.beatLengthMs, sizeof(float), 1, f);
            fwrite(&c.entry.timingOffsetMs, sizeof(float), 1, f);
            fwrite(&c.entry.maxPossibleCombo, sizeof(int), 1, f);
            fwrite(&c.entry.circleCount, sizeof(int), 1, f);
            fwrite(&c.entry.sliderCount, sizeof(int), 1, f);
            fwrite(&c.entry.spinnerCount, sizeof(int), 1, f);
            fwrite(&c.entry.firstObjectTime, sizeof(int), 1, f);
            fwrite(&c.entry.lastObjectTime, sizeof(int), 1, f);

            uint16_t strainCount = static_cast<uint16_t>(c.entry.strainGraph.size());
            fwrite(&strainCount, sizeof(uint16_t), 1, f);
            if (strainCount > 0) {
                fwrite(c.entry.strainGraph.data(), sizeof(float), strainCount, f);
            }
        }
        fclose(f);
    }

    static void loadCache(const std::string& cachePath, std::unordered_map<std::string, CachedBeatmap>& cacheMap) {
        cacheMap.clear();
        FILE* f = fopen(cachePath.c_str(), "rb");
        if (!f) return;
        char iobuf[32768];
        setvbuf(f, iobuf, _IOFBF, sizeof(iobuf));

        uint32_t magic = 0, ver = 0, count = 0;
        if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != 0x43534F21) {
            fclose(f);
            return;
        }
        if (fread(&ver, sizeof(uint32_t), 1, f) != 1 || ver != 2) {
            fclose(f);
            return;
        }
        if (fread(&count, sizeof(uint32_t), 1, f) != 1) {
            fclose(f);
            return;
        }

        for (uint32_t i = 0; i < count; ++i) {
            std::string osuPath;
            if (!readStr(f, osuPath)) break;
            CachedBeatmap c;
            c.entry.osuPath = osuPath;
            if (fread(&c.fileSize, sizeof(int64_t), 1, f) != 1) break;
            if (fread(&c.mtime, sizeof(int64_t), 1, f) != 1) break;
            if (!readStr(f, c.title)) break;
            if (!readStr(f, c.artist)) break;
            if (!readStr(f, c.folderPath)) break;
            if (!readStr(f, c.setBgPath)) break;
            uint8_t hasVid = 0, romfs = 0;
            if (fread(&hasVid, 1, 1, f) != 1) break;
            c.hasVideo = (hasVid != 0);
            if (fread(&romfs, 1, 1, f) != 1) break;
            c.fromRomfs = (romfs != 0);

            if (fread(&c.entry.mode, sizeof(int), 1, f) != 1) break;
            if (!readStr(f, c.entry.version)) break;
            if (!readStr(f, c.entry.creator)) break;
            if (!readStr(f, c.entry.audioPath)) break;
            if (!readStr(f, c.entry.backgroundPath)) break;
            if (!readStr(f, c.entry.videoPath)) break;
            if (fread(&c.entry.videoOffsetMs, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.previewTimeMs, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.cs, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.ar, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.od, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.hp, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.starRating, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.beatLengthMs, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.timingOffsetMs, sizeof(float), 1, f) != 1) break;
            if (fread(&c.entry.maxPossibleCombo, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.circleCount, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.sliderCount, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.spinnerCount, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.firstObjectTime, sizeof(int), 1, f) != 1) break;
            if (fread(&c.entry.lastObjectTime, sizeof(int), 1, f) != 1) break;

            uint16_t strainCount = 0;
            if (fread(&strainCount, sizeof(uint16_t), 1, f) != 1) break;
            c.entry.strainGraph.resize(strainCount);
            if (strainCount > 0) {
                if (fread(c.entry.strainGraph.data(), sizeof(float), strainCount, f) != strainCount) break;
            }

            cacheMap[osuPath] = c;
        }
        fclose(f);
    }

    void addCachedBeatmapToSets(const CachedBeatmap& c) {
        std::string keyTitle = Beatmap::toLower(c.title);
        for (auto& set : allSets) {
            if (set.folderPath == c.folderPath && Beatmap::toLower(set.title) == keyTitle) {
                for (const auto& d : set.difficulties) {
                    if (d.osuPath == c.entry.osuPath) return;
                }
                if (set.backgroundPath.empty() && !c.entry.backgroundPath.empty()) {
                    set.backgroundPath = c.entry.backgroundPath;
                }
                if (!c.entry.videoPath.empty()) {
                    set.hasVideo = true;
                }
                set.difficulties.push_back(c.entry);
                return;
            }
        }

        BeatmapSet newSet;
        newSet.title          = c.title;
        newSet.artist         = c.artist;
        newSet.folderPath     = c.folderPath;
        newSet.backgroundPath = c.setBgPath;
        newSet.hasVideo       = c.hasVideo;
        newSet.fromRomfs      = c.fromRomfs;
        newSet.difficulties.push_back(c.entry);
        allSets.push_back(newSet);
    }

    bool addOsuFileToSets(const std::string& osuPath, bool isRomfs, CachedBeatmap* outCached = nullptr) {
        Beatmap meta;
        if (!meta.loadFromFile(osuPath, true)) return false;
        // Soportar osu!standard (0), Taiko (1), Catch (2), Mania (3)
        if (meta.mode < 0 || meta.mode > 3) return false;

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

        std::string setBg = entry.backgroundPath;
        bool setHasVid = !entry.videoPath.empty();

        // Agrupar por título y carpeta
        std::string keyTitle = Beatmap::toLower(meta.title);
        bool foundSet = false;
        for (auto& set : allSets) {
            if (set.folderPath == meta.folderPath && Beatmap::toLower(set.title) == keyTitle) {
                foundSet = true;
                bool diffFound = false;
                for (const auto& d : set.difficulties) {
                    if (d.osuPath == osuPath) { diffFound = true; break; }
                }
                if (!diffFound) {
                    if (set.backgroundPath.empty() && !entry.backgroundPath.empty()) {
                        set.backgroundPath = entry.backgroundPath;
                    }
                    if (!entry.videoPath.empty()) {
                        set.hasVideo = true;
                    }
                    set.difficulties.push_back(entry);
                }
                setBg = set.backgroundPath;
                setHasVid = set.hasVideo;
                break;
            }
        }

        if (!foundSet) {
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

        if (outCached) {
            outCached->title = meta.title;
            outCached->artist = meta.artist;
            outCached->folderPath = meta.folderPath;
            outCached->setBgPath = setBg;
            outCached->hasVideo = setHasVid;
            outCached->fromRomfs = isRomfs;
            outCached->entry = entry;
        }
        return true;
    }

    template <typename Callback>
    void scanDirectoryTree(const std::string& rootFolder, bool isRomfs, Callback&& cb) {
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
                cb(fullPath, isRomfs);
            } else {
                DIR* sub = opendir(fullPath.c_str());
                if (sub) {
                    struct dirent* subEntry;
                    std::string subPrefix = fullPath + "/";
                    while ((subEntry = readdir(sub)) != nullptr) {
                        std::string subName = subEntry->d_name;
                        if (subName == "." || subName == "..") continue;
                        if (hasOsuExtension(subName)) {
                            cb(subPrefix + subName, isRomfs);
                        }
                    }
                    closedir(sub);
                }
            }
        }
        closedir(dir);
    }
};
