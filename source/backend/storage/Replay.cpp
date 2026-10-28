#include "Replay.hpp"
#include "OszExtractor.hpp"
#include "Beatmap.hpp"
#include "backend/i18n/Localization.hpp"
#include <fstream>
#include <cstdio>
#include <ctime>
#include <algorithm>

static const uint32_t REPLAY_MAGIC = 0x3352534F; // "OSR3" en little endian ('O','S','R','3')
static const uint32_t REPLAY_VERSION = 1;

static void writeString(std::ofstream& f, const std::string& str) {
    uint16_t len = static_cast<uint16_t>(std::min<size_t>(str.size(), 65535));
    f.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        f.write(str.data(), len);
    }
}

static bool readString(std::ifstream& f, std::string& str) {
    uint16_t len = 0;
    if (!f.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
    str.resize(len);
    if (len > 0) {
        if (!f.read(&str[0], len)) return false;
    }
    return true;
}

bool ReplayData::saveToFile(const std::string& path) const {
    OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds/replays");
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;

    // Encabezado
    f.write(reinterpret_cast<const char*>(&REPLAY_MAGIC), sizeof(REPLAY_MAGIC));
    f.write(reinterpret_cast<const char*>(&REPLAY_VERSION), sizeof(REPLAY_VERSION));

    // Metadatos
    writeString(f, osuKey);
    writeString(f, playerName);
    writeString(f, title);
    writeString(f, artist);
    writeString(f, difficulty);
    writeString(f, grade);

    int32_t gMode = gameMode;
    f.write(reinterpret_cast<const char*>(&gMode), sizeof(gMode));
    f.write(reinterpret_cast<const char*>(&activeMods), sizeof(activeMods));

    int32_t sc = score;
    int32_t mc = maxCombo;
    float ac = accuracy;
    f.write(reinterpret_cast<const char*>(&sc), sizeof(sc));
    f.write(reinterpret_cast<const char*>(&mc), sizeof(mc));
    f.write(reinterpret_cast<const char*>(&ac), sizeof(ac));

    int32_t c3 = count300;
    int32_t c1 = count100;
    int32_t c5 = count50;
    int32_t cm = countMiss;
    f.write(reinterpret_cast<const char*>(&c3), sizeof(c3));
    f.write(reinterpret_cast<const char*>(&c1), sizeof(c1));
    f.write(reinterpret_cast<const char*>(&c5), sizeof(c5));
    f.write(reinterpret_cast<const char*>(&cm), sizeof(cm));

    float p = pp;
    uint64_t ts = timestamp;
    f.write(reinterpret_cast<const char*>(&p), sizeof(p));
    f.write(reinterpret_cast<const char*>(&ts), sizeof(ts));

    // Fotogramas
    uint32_t frameCount = static_cast<uint32_t>(frames.size());
    f.write(reinterpret_cast<const char*>(&frameCount), sizeof(frameCount));
    if (frameCount > 0) {
        f.write(reinterpret_cast<const char*>(frames.data()), frameCount * sizeof(ReplayFrame));
    }

    return f.good();
}

bool ReplayData::loadFromFile(const std::string& path) {
    clear();
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t magic = 0;
    uint32_t ver = 0;
    if (!f.read(reinterpret_cast<char*>(&magic), sizeof(magic)) || magic != REPLAY_MAGIC) return false;
    if (!f.read(reinterpret_cast<char*>(&ver), sizeof(ver)) || ver != REPLAY_VERSION) return false;

    if (!readString(f, osuKey)) return false;
    if (!readString(f, playerName)) return false;
    if (!readString(f, title)) return false;
    if (!readString(f, artist)) return false;
    if (!readString(f, difficulty)) return false;
    if (!readString(f, grade)) return false;

    int32_t gMode = 0;
    if (!f.read(reinterpret_cast<char*>(&gMode), sizeof(gMode))) return false;
    gameMode = gMode;

    if (!f.read(reinterpret_cast<char*>(&activeMods), sizeof(activeMods))) return false;

    int32_t sc = 0, mc = 0;
    float ac = 0.0f;
    if (!f.read(reinterpret_cast<char*>(&sc), sizeof(sc))) return false;
    if (!f.read(reinterpret_cast<char*>(&mc), sizeof(mc))) return false;
    if (!f.read(reinterpret_cast<char*>(&ac), sizeof(ac))) return false;
    score = sc;
    maxCombo = mc;
    accuracy = ac;

    int32_t c3 = 0, c1 = 0, c5 = 0, cm = 0;
    if (!f.read(reinterpret_cast<char*>(&c3), sizeof(c3))) return false;
    if (!f.read(reinterpret_cast<char*>(&c1), sizeof(c1))) return false;
    if (!f.read(reinterpret_cast<char*>(&c5), sizeof(c5))) return false;
    if (!f.read(reinterpret_cast<char*>(&cm), sizeof(cm))) return false;
    count300 = c3;
    count100 = c1;
    count50 = c5;
    countMiss = cm;

    float p = 0.0f;
    uint64_t ts = 0;
    if (!f.read(reinterpret_cast<char*>(&p), sizeof(p))) return false;
    if (!f.read(reinterpret_cast<char*>(&ts), sizeof(ts))) return false;
    pp = p;
    timestamp = ts;

    uint32_t frameCount = 0;
    if (!f.read(reinterpret_cast<char*>(&frameCount), sizeof(frameCount))) return false;
    if (frameCount > 200000) return false; // Límite de seguridad contra archivos corruptos

    frames.resize(frameCount);
    if (frameCount > 0) {
        if (!f.read(reinterpret_cast<char*>(frames.data()), frameCount * sizeof(ReplayFrame))) {
            frames.clear();
            return false;
        }
    }

    return true;
}

namespace ReplayManager {

static std::string sanitizeFilename(const std::string& str) {
    std::string safe = str;
    for (char& c : safe) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return safe;
}

static std::string getSafeKey(const std::string& osuPath) {
    size_t slash = osuPath.find_last_of("/\\");
    std::string base = (slash != std::string::npos) ? osuPath.substr(slash + 1) : osuPath;
    return sanitizeFilename(Beatmap::toLower(base));
}

void init() {
    OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds/replays");
}

std::string getTopReplayPath(const std::string& osuPath, int mode, int rank) {
    std::string safeKey = getSafeKey(osuPath);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "sdmc:/3ds/osu-3ds/replays/top_%s_m%d_r%d.osr3d", safeKey.c_str(), mode, rank);
    return std::string(buf);
}

bool hasTopReplay(const std::string& osuPath, int mode, int rank) {
    std::string p = getTopReplayPath(osuPath, mode, rank);
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);
    return (sz > 32);
}

bool saveTopReplay(const std::string& osuPath, int mode, int rank, const ReplayData& replay) {
    if (!replay.isValid() || rank < 1 || rank > 3) return false;
    init();

    // Desplazar repeticiones inferiores si es necesario
    if (rank == 1) {
        std::string p2 = getTopReplayPath(osuPath, mode, 2);
        std::string p3 = getTopReplayPath(osuPath, mode, 3);
        if (hasTopReplay(osuPath, mode, 2)) {
            std::rename(p2.c_str(), p3.c_str());
        }
        std::string p1 = getTopReplayPath(osuPath, mode, 1);
        if (hasTopReplay(osuPath, mode, 1)) {
            std::rename(p1.c_str(), p2.c_str());
        }
    } else if (rank == 2) {
        std::string p2 = getTopReplayPath(osuPath, mode, 2);
        std::string p3 = getTopReplayPath(osuPath, mode, 3);
        if (hasTopReplay(osuPath, mode, 2)) {
            std::rename(p2.c_str(), p3.c_str());
        }
    }

    std::string targetPath = getTopReplayPath(osuPath, mode, rank);
    return replay.saveToFile(targetPath);
}

std::string exportReplay(const ReplayData& replay, std::string* outError) {
    if (!replay.isValid()) {
        if (outError) *outError = "La partida no contiene datos de repeticion.";
        return "";
    }
    init();

    std::string pName = replay.playerName.empty() ? "Player" : sanitizeFilename(replay.playerName);
    std::string art   = replay.artist.empty() ? "Artist" : sanitizeFilename(replay.artist);
    std::string tit   = replay.title.empty() ? "Title" : sanitizeFilename(replay.title);
    std::string diff  = replay.difficulty.empty() ? "Diff" : sanitizeFilename(replay.difficulty);

    char filename[320];
    std::snprintf(filename, sizeof(filename), "%s - %s [%s] (%d) - %s.osr3d",
                  art.c_str(), tit.c_str(), diff.c_str(), replay.score, pName.c_str());

    std::string fullPath = "sdmc:/3ds/osu-3ds/replays/" + std::string(filename);
    if (!replay.saveToFile(fullPath)) {
        if (outError) *outError = "Error al escribir en la tarjeta SD.";
        return "";
    }

    return fullPath;
}

bool loadReplay(const std::string& path, ReplayData& outReplay) {
    return outReplay.loadFromFile(path);
}

} // namespace ReplayManager
