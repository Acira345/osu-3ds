#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Máscara de bits para los botones grabados en cada fotograma de la repetición
enum ReplayKeys : uint32_t {
    REPLAY_KEY_NONE          = 0,
    REPLAY_KEY_K1            = (1 << 0),  // Botón K1 (estándar)
    REPLAY_KEY_K2            = (1 << 1),  // Botón K2 (estándar)
    REPLAY_KEY_TOUCH         = (1 << 2),  // Pantalla táctil en contacto
    REPLAY_KEY_SMOKE         = (1 << 3),  // Humo activo
    REPLAY_KEY_TAIKO_DON_L   = (1 << 4),  // Taiko Don izquierdo
    REPLAY_KEY_TAIKO_DON_R   = (1 << 5),  // Taiko Don derecho
    REPLAY_KEY_TAIKO_KATSU_L = (1 << 6),  // Taiko Katsu izquierdo
    REPLAY_KEY_TAIKO_KATSU_R = (1 << 7),  // Taiko Katsu derecho
    REPLAY_KEY_MANIA_0       = (1 << 8),  // Mania Carril 1
    REPLAY_KEY_MANIA_1       = (1 << 9),  // Mania Carril 2
    REPLAY_KEY_MANIA_2       = (1 << 10), // Mania Carril 3
    REPLAY_KEY_MANIA_3       = (1 << 11), // Mania Carril 4
    REPLAY_KEY_CATCH_L       = (1 << 12), // Catch izquierda
    REPLAY_KEY_CATCH_R       = (1 << 13), // Catch derecha
    REPLAY_KEY_CATCH_DASH    = (1 << 14)  // Catch aceleración
};

#pragma pack(push, 1)
struct ReplayFrame {
    int32_t timeMs;      // Tiempo en ms dentro del beatmap
    float x;             // Posición X en coordenadas osu! (0..512)
    float y;             // Posición Y en coordenadas osu! (0..384)
    uint32_t keys;       // Teclas / botones activos en este instante
};
#pragma pack(pop)

struct ReplayData {
    std::string osuKey;       // Clave del beatmap (nombre de archivo normalizado)
    std::string playerName;   // Nombre del jugador
    std::string title;        // Título de la canción
    std::string artist;       // Artista
    std::string difficulty;   // Nombre de la versión / dificultad
    int gameMode = 0;         // 0: STD, 1: TAIKO, 2: CATCH, 3: MANIA
    uint32_t activeMods = 0;  // Mods activos
    int score = 0;
    int maxCombo = 0;
    float accuracy = 0.0f;
    std::string grade = "D";
    int count300 = 0;
    int count100 = 0;
    int count50 = 0;
    int countMiss = 0;
    float pp = 0.0f;
    uint64_t timestamp = 0;   // Fecha / marca temporal de la partida

    std::vector<ReplayFrame> frames;

    void clear() {
        osuKey.clear();
        playerName.clear();
        title.clear();
        artist.clear();
        difficulty.clear();
        gameMode = 0;
        activeMods = 0;
        score = 0;
        maxCombo = 0;
        accuracy = 0.0f;
        grade = "D";
        count300 = count100 = count50 = countMiss = 0;
        pp = 0.0f;
        timestamp = 0;
        frames.clear();
    }

    bool isValid() const {
        return !frames.empty();
    }

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
};

namespace ReplayManager {
    void init();
    std::string getTopReplayPath(const std::string& osuPath, int mode, int rank);
    bool hasTopReplay(const std::string& osuPath, int mode, int rank);
    bool saveTopReplay(const std::string& osuPath, int mode, int rank, const ReplayData& replay);
    std::string exportReplay(const ReplayData& replay, std::string* outError = nullptr);
    bool loadReplay(const std::string& path, ReplayData& outReplay);
}
