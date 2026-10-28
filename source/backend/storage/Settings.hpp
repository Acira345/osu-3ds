#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <3ds.h>
#include "OszExtractor.hpp"
#include "Localization.hpp"

enum GameMode {
    MODE_STD   = 0,
    MODE_TAIKO = 1,
    MODE_CATCH = 2,
    MODE_MANIA = 3
};

struct GameSettings {
    // Modo de Juego seleccionado actualmente (0: Standard, 1: Taiko, 2: Catch, 3: Mania)
    int gameMode = MODE_STD;

    // Área de la "Tablet" en la pantalla inferior (320x240)
    float tabletScale   = 1.0f;
    float tabletCenterX = 160.0f;
    float tabletCenterY = 120.0f;

    // Modo de entrada y pantalla de juego:
    // playOnBottomScreen = true  -> Modo Tactil Directo (los circulos aparecen en la pantalla inferior y los tocas directamente)
    // playOnBottomScreen = false -> Modo Tableta PC (los circulos aparecen arriba y la pantalla inferior es el area de tablet)
    bool playOnBottomScreen = true;
    bool tapWithStylus      = true;

    // Mapeo de botones físicos personalizables (libctru key masks)
    // osu!standard
    uint32_t keyOsuK1       = KEY_A | KEY_X | KEY_L | KEY_DLEFT;
    uint32_t keyOsuK2       = KEY_B | KEY_Y | KEY_R | KEY_DRIGHT;
    uint32_t keyOsuSmoke    = KEY_SELECT | KEY_DUP;
    uint32_t keyQuickRetry  = KEY_SELECT;

    // osu!taiko (Don = centro rojo, Katsu = borde azul)
    uint32_t keyTaikoDonLeft    = KEY_DDOWN | KEY_DRIGHT;
    uint32_t keyTaikoDonRight   = KEY_A | KEY_B;
    uint32_t keyTaikoKatsuLeft  = KEY_DLEFT | KEY_DUP | KEY_L;
    uint32_t keyTaikoKatsuRight = KEY_X | KEY_Y | KEY_R;
    uint32_t keyTaikoDon        = KEY_DDOWN | KEY_DRIGHT | KEY_A | KEY_B;
    uint32_t keyTaikoKatsu      = KEY_DLEFT | KEY_DUP | KEY_L | KEY_X | KEY_Y | KEY_R;

    // osu!catch (Movimiento horizontal y aceleración)
    uint32_t keyCatchLeft   = KEY_DLEFT | KEY_Y;
    uint32_t keyCatchRight  = KEY_DRIGHT | KEY_A;
    uint32_t keyCatchDash   = KEY_B | KEY_L | KEY_R | KEY_ZL | KEY_ZR;

    // osu!mania (Columnas 1..4 para 4K)
    uint32_t keyManiaCol1   = KEY_DLEFT | KEY_L;
    uint32_t keyManiaCol2   = KEY_DUP | KEY_DDOWN;
    uint32_t keyManiaCol3   = KEY_X | KEY_Y;
    uint32_t keyManiaCol4   = KEY_A | KEY_B | KEY_R;

    // Audio y Calibración
    int musicVolume   = 85;  // 0 .. 100 %
    int sfxVolume     = 100; // 0 .. 100 %
    int audioOffsetMs = 0;   // -150 .. +150 ms
    bool songPreview  = true;

    // Opciones visuales, Skin, Atenuación de Fondo, Video y Mods
    float cursorScale   = 1.0f; // 0.5 .. 2.0
    int skinIndex       = 0;
    int bgDim           = 75;   // 20 .. 100 % (Atenuación del fondo de la canción)
    bool enableVideo    = true; // Video de fondo optimizado (.3dsv / .mjpg)
    bool snakingSliders = true; // Sliders que se dibujan como serpientes estilo osu! / lazer
    bool comboBursts    = true; // Aparición de mascota o personaje lateral en hitos de combo
    bool showHit300     = true; // Mostrar efecto visual / texto flotante de 300 al golpear
    bool beatmapHitsounds = true; // Hitsounds propios de la carpeta del beatmap (si existen)
    bool showPlayfieldBorder = false; // Cuadro / Marco blanco del playfield (Safe Area)
    int taikoBgMode = 0; // 0: Festivo (Arcade / Bailarines), 1: Imagen Beatmap, 2: Video Beatmap
    int multiDlOption = 1; // 0: Solo Nivel (.osu + Audio), 1: Nivel + Fondo (Recomendado)
    uint32_t activeMods = 0;
    int language = -1; // -1: autodetect from 3DS system, 0: ES, 1: EN, 2: RU, 3: PL

    const char* getLanguageName() const {
        return Localization::getLanguageName(language);
    }

    const char* getMultiDlOptionName() const {
        AppLanguage lang = Localization::getLanguage();
        if (multiDlOption == 0) {
            switch (lang) {
                case LANG_EN: return "Map Only";
                case LANG_RU: return "Только карта";
                case LANG_PL: return "Tylko mapa";
                case LANG_DE: return "Nur beatmap";
                case LANG_ES: return "Solo Nivel";
                default:      return "Map Only";
            }
        } else {
            switch (lang) {
                case LANG_EN: return "Map + Background";
                case LANG_RU: return "Карта + фон";
                case LANG_PL: return "Mapa + tło";
                case LANG_DE: return "Map + Hintergrund";
                case LANG_ES: return "Nivel + Fondo";
                default:      return "Nivel + Fondo";
            }
        }
    }

    const char* getTaikoBgModeName() const {
        AppLanguage lang = Localization::getLanguage();
        switch (taikoBgMode) {
            case 0:
                switch (lang) {
                    case LANG_EN: return "Festive (Arcade)";
                    case LANG_RU: return "Праздничный (Аркада)";
                    case LANG_PL: return "Świąteczny (Arcade)";
                    case LANG_DE: return "Festlich (Arcade)";
                    default:      return "Festivo (Arcade)";
                }
            case 1:
                switch (lang) {
                    case LANG_EN: return "Beatmap Image";
                    case LANG_RU: return "Изображение карты";
                    case LANG_PL: return "Obrazek mapy";
                    case LANG_DE: return "Beatmap Bild";
                    default:      return "Imagen Beatmap";
                }
            case 2:
                switch (lang) {
                    case LANG_EN: return "Beatmap Video";
                    case LANG_RU: return "Видео карты";
                    case LANG_PL: return "Wideo mapy";
                    case LANG_DE: return "Beatmap Video";
                    default:      return "Video Beatmap";
                }
            default:
                switch (lang) {
                    case LANG_EN: return "Festive";
                    case LANG_RU: return "Праздничный";
                    case LANG_PL: return "Świąteczny";
                    case LANG_DE: return "Festlich";
                    default:      return "Festivo";
                }
        }
    }

    static std::string formatKeyMask(uint32_t mask) {
        AppLanguage lang = Localization::getLanguage();
        if (mask == 0) {
            switch (lang) {
                case LANG_EN: return "None";
                case LANG_RU: return "Нет";
                case LANG_PL: return "Brak";
                case LANG_DE: return "Keine";
                default:      return "Ninguno";
            }
        }
        std::string res;
        auto add = [&](const char* name) {
            if (!res.empty()) res += " / ";
            res += name;
        };
        if (mask & KEY_A) add("A");
        if (mask & KEY_B) add("B");
        if (mask & KEY_X) add("X");
        if (mask & KEY_Y) add("Y");
        if (mask & KEY_L) add("L");
        if (mask & KEY_R) add("R");
        if (mask & KEY_ZL) add("ZL");
        if (mask & KEY_ZR) add("ZR");
        if (mask & KEY_DLEFT) {
            switch (lang) {
                case LANG_EN: add("D-Left"); break;
                case LANG_RU: add("D-Влево"); break;
                case LANG_PL: add("D-Lewo"); break;
                case LANG_DE: add("D-Links"); break;
                default:      add("D-Izq"); break;
            }
        }
        if (mask & KEY_DRIGHT) {
            switch (lang) {
                case LANG_EN: add("D-Right"); break;
                case LANG_RU: add("D-Вправо"); break;
                case LANG_PL: add("D-Prawo"); break;
                case LANG_DE: add("D-Rechts"); break;
                default:      add("D-Der"); break;
            }
        }
        if (mask & KEY_DUP) {
            switch (lang) {
                case LANG_EN: add("D-Up"); break;
                case LANG_RU: add("D-Вверх"); break;
                case LANG_PL: add("D-Góra"); break;
                case LANG_DE: add("D-Oben"); break;
                default:      add("D-Arr"); break;
            }
        }
        if (mask & KEY_DDOWN) {
            switch (lang) {
                case LANG_EN: add("D-Down"); break;
                case LANG_RU: add("D-Вниз"); break;
                case LANG_PL: add("D-Dół"); break;
                case LANG_DE: add("D-Unten"); break;
                default:      add("D-Aba"); break;
            }
        }
        if (mask & KEY_SELECT) add("SELECT");
        if (mask & KEY_START) add("START");
        if (res.empty()) res = "Custom";
        return res;
    }

    void resetControlsToDefault(int mode = -1) {
        if (mode == -1 || mode == MODE_STD) {
            keyOsuK1       = KEY_A | KEY_X | KEY_L | KEY_DLEFT;
            keyOsuK2       = KEY_B | KEY_Y | KEY_R | KEY_DRIGHT;
            keyOsuSmoke    = KEY_SELECT | KEY_DUP;
            keyQuickRetry  = KEY_SELECT;
        }
        if (mode == -1 || mode == MODE_TAIKO) {
            keyTaikoDonLeft    = KEY_DDOWN | KEY_DRIGHT;
            keyTaikoDonRight   = KEY_A | KEY_B;
            keyTaikoKatsuLeft  = KEY_DLEFT | KEY_DUP | KEY_L;
            keyTaikoKatsuRight = KEY_X | KEY_Y | KEY_R;
            keyTaikoDon        = keyTaikoDonLeft | keyTaikoDonRight;
            keyTaikoKatsu      = keyTaikoKatsuLeft | keyTaikoKatsuRight;
        }
        if (mode == -1 || mode == MODE_CATCH) {
            keyCatchLeft   = KEY_DLEFT | KEY_Y;
            keyCatchRight  = KEY_DRIGHT | KEY_A;
            keyCatchDash   = KEY_B | KEY_L | KEY_R | KEY_ZL | KEY_ZR;
            keyQuickRetry  = KEY_SELECT;
        }
        if (mode == -1 || mode == MODE_MANIA) {
            keyManiaCol1   = KEY_DLEFT | KEY_L;
            keyManiaCol2   = KEY_DUP | KEY_DDOWN;
            keyManiaCol3   = KEY_X | KEY_Y;
            keyManiaCol4   = KEY_A | KEY_B | KEY_R;
        }
    }

    const char* getGameModeName() const {
        switch (gameMode) {
            case MODE_TAIKO: return "osu!taiko";
            case MODE_CATCH: return "osu!catch";
            case MODE_MANIA: return "osu!mania";
            default:         return "osu! (Standard)";
        }
    }

    const char* getGameModeShortName() const {
        switch (gameMode) {
            case MODE_TAIKO: return "TAIKO";
            case MODE_CATCH: return "CATCH";
            case MODE_MANIA: return "MANIA";
            default:         return "STD";
        }
    }

    void clampValues() {
        tabletScale = std::max(0.30f, std::min(1.00f, tabletScale));

        float halfW = (266.6667f * tabletScale) * 0.5f;
        float halfH = (200.0f * tabletScale) * 0.5f;

        tabletCenterX = std::max(halfW, std::min(320.0f - halfW, tabletCenterX));
        tabletCenterY = std::max(halfH, std::min(240.0f - halfH, tabletCenterY));

        musicVolume   = std::max(0, std::min(100, musicVolume));
        sfxVolume     = std::max(0, std::min(100, sfxVolume));
        audioOffsetMs = std::max(-150, std::min(150, audioOffsetMs));
        cursorScale   = std::max(0.5f, std::min(2.0f, cursorScale));
        bgDim         = std::max(20, std::min(100, bgDim));
        taikoBgMode   = std::max(0, std::min(2, taikoBgMode));
        if (skinIndex < 0) skinIndex = 0;
        if (gameMode < MODE_STD || gameMode > MODE_MANIA) gameMode = MODE_STD;
        if (keyOsuK1 == 0) keyOsuK1 = KEY_A | KEY_X | KEY_L | KEY_DLEFT;
        if (keyOsuK2 == 0) keyOsuK2 = KEY_B | KEY_Y | KEY_R | KEY_DRIGHT;
        if (keyQuickRetry == 0) keyQuickRetry = KEY_SELECT;
        activeMods &= 0x7FFFu;
        if (language < 0 || language >= LANG_COUNT) {
            language = static_cast<int>(Localization::detectConsoleLanguage());
        }
        Localization::setLanguage(static_cast<AppLanguage>(language));
    }

    void getTabletRect(float& outX, float& outY, float& outW, float& outH) const {
        outW = 266.6667f * tabletScale;
        outH = 200.0f * tabletScale;
        outX = tabletCenterX - (outW * 0.5f);
        outY = tabletCenterY - (outH * 0.5f);
    }

    void resetTabletArea() {
        tabletScale   = 1.0f;
        tabletCenterX = 160.0f;
        tabletCenterY = 120.0f;
    }

    void mapTouchToOsu(float touchX, float touchY, float& outOsuX, float& outOsuY) const {
        if (playOnBottomScreen) {
            constexpr float ox = 26.6667f;
            constexpr float oy = 20.0f;
            constexpr float scale = 0.5208333f;
            outOsuX = (touchX - ox) / scale;
            outOsuY = (touchY - oy) / scale;
            outOsuX = std::max(0.0f, std::min(512.0f, outOsuX));
            outOsuY = std::max(0.0f, std::min(384.0f, outOsuY));
            return;
        }

        float rx, ry, rw, rh;
        getTabletRect(rx, ry, rw, rh);

        float normX = (rw > 1.0f) ? ((touchX - rx) / rw) : 0.5f;
        float normY = (rh > 1.0f) ? ((touchY - ry) / rh) : 0.5f;

        normX = std::max(0.0f, std::min(1.0f, normX));
        normY = std::max(0.0f, std::min(1.0f, normY));

        outOsuX = normX * 512.0f;
        outOsuY = normY * 384.0f;
    }

    const char* getInputModeLabel() const {
        AppLanguage lang = Localization::getLanguage();
        if (playOnBottomScreen) {
            switch (lang) {
                case LANG_EN: return "Direct Touch (Bottom Screen)";
                case LANG_RU: return "Прямой ввод (Нижний экран)";
                case LANG_PL: return "Dotyk bezpośredni (Dół)";
                case LANG_DE: return "Direkte Eingabe (Unterer Bildschirm)";
                default:      return "Tactil Directo (Pantalla Inf.)";
            }
        }
        if (tapWithStylus) {
            switch (lang) {
                case LANG_EN: return "Tablet + Tap (Top Screen)";
                case LANG_RU: return "Планшет + тап (Верхний)";
                case LANG_PL: return "Tablet + Tap (Góra)";
                case LANG_DE: return "Tablet + Tippen (Oberer Bildschirm)";
                default:      return "Tablet + Tap (Pantalla Sup.)";
            }
        } else {
            switch (lang) {
                case LANG_EN: return "PC Hybrid (Stylus aims only)";
                case LANG_RU: return "Гибрид ПК (Стилус только цель)";
                case LANG_PL: return "Hybryda PC (Stylus celuje)";
                case LANG_DE: return "PC-Hybrid (mit Stylus zielen)";
                default:      return "Hibrido PC (Stylus solo apunta)";
            }
        }
    }

    const char* getShortInputModeLabel() const {
        AppLanguage lang = Localization::getLanguage();
        if (playOnBottomScreen) {
            switch (lang) {
                case LANG_EN: return "Touch Bottom";
                case LANG_RU: return "Сенсор низ";
                case LANG_PL: return "Dotyk dół";
                case LANG_DE: return "Berührung unten";
                default:      return "Tactil Abajo";
            }
        }
        if (tapWithStylus) {
            switch (lang) {
                case LANG_EN: return "Tablet+Tap";
                case LANG_RU: return "Планшет+Тап";
                case LANG_PL: return "Tablet+Tap";
                case LANG_DE: return "Tablet+Tippen";
                default:      return "Tablet+Tap";
            }
        } else {
            switch (lang) {
                case LANG_EN: return "PC Hybrid";
                case LANG_RU: return "ПК Гибрид";
                case LANG_PL: return "Hybryda PC";
                case LANG_DE: return "PC-Hybrid";
                default:      return "Hibrido PC";
            }
        }
    }

    void cycleInputMode(int dir = 1) {
        // 0: Tactil Directo (Inferior), 1: Tablet + Tap (Superior), 2: Tablet Hibrido PC (Superior)
        int cur = playOnBottomScreen ? 0 : (tapWithStylus ? 1 : 2);
        cur = (cur + (dir >= 0 ? 1 : 2)) % 3;
        if (cur == 0) {
            playOnBottomScreen = true;
            tapWithStylus = true;
        } else if (cur == 1) {
            playOnBottomScreen = false;
            tapWithStylus = true;
        } else {
            playOnBottomScreen = false;
            tapWithStylus = false;
        }
    }

    bool load(const std::string& path = "sdmc:/3ds/osu-3ds/settings.cfg") {
        std::ifstream f(path.c_str());
        if (!f.is_open()) {
            clampValues();
            return false;
        }

        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "tabletScale")             tabletScale        = std::strtof(val.c_str(), nullptr);
            else if (key == "tabletCenterX")      tabletCenterX      = std::strtof(val.c_str(), nullptr);
            else if (key == "tabletCenterY")      tabletCenterY      = std::strtof(val.c_str(), nullptr);
            else if (key == "playOnBottomScreen") playOnBottomScreen = (std::atoi(val.c_str()) != 0);
            else if (key == "tapWithStylus")      tapWithStylus      = (std::atoi(val.c_str()) != 0);
            else if (key == "musicVolume")        musicVolume        = std::atoi(val.c_str());
            else if (key == "sfxVolume")          sfxVolume          = std::atoi(val.c_str());
            else if (key == "audioOffsetMs")      audioOffsetMs      = std::atoi(val.c_str());
            else if (key == "songPreview")        songPreview        = (std::atoi(val.c_str()) != 0);
            else if (key == "cursorScale")        cursorScale        = std::strtof(val.c_str(), nullptr);
            else if (key == "skinIndex")          skinIndex          = std::atoi(val.c_str());
            else if (key == "bgDim")              bgDim              = std::atoi(val.c_str());
            else if (key == "enableVideo")        enableVideo        = (std::atoi(val.c_str()) != 0);
            else if (key == "snakingSliders")     snakingSliders     = (std::atoi(val.c_str()) != 0);
            else if (key == "comboBursts")        comboBursts        = (std::atoi(val.c_str()) != 0);
            else if (key == "showHit300")         showHit300         = (std::atoi(val.c_str()) != 0);
            else if (key == "beatmapHitsounds")   beatmapHitsounds   = (std::atoi(val.c_str()) != 0);
            else if (key == "showPlayfieldBorder") showPlayfieldBorder = (std::atoi(val.c_str()) != 0);
            else if (key == "taikoBgMode")        taikoBgMode        = std::atoi(val.c_str());
            else if (key == "multiDlOption")      multiDlOption      = std::atoi(val.c_str());
            else if (key == "language")           language           = std::atoi(val.c_str());
            else if (key == "activeMods")         activeMods         = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "gameMode")           gameMode           = std::atoi(val.c_str());
            else if (key == "keyOsuK1")           keyOsuK1           = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyOsuK2")           keyOsuK2           = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyOsuSmoke")        keyOsuSmoke        = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyQuickRetry")      keyQuickRetry      = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoDon")        keyTaikoDon        = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoKatsu")      keyTaikoKatsu      = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoDonLeft")    keyTaikoDonLeft    = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoDonRight")   keyTaikoDonRight   = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoKatsuLeft")  keyTaikoKatsuLeft  = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyTaikoKatsuRight") keyTaikoKatsuRight = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyCatchLeft")       keyCatchLeft       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyCatchRight")      keyCatchRight      = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyCatchDash")       keyCatchDash       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyManiaCol1")       keyManiaCol1       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyManiaCol2")       keyManiaCol2       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyManiaCol3")       keyManiaCol3       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            else if (key == "keyManiaCol4")       keyManiaCol4       = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
        }
        keyTaikoDon = keyTaikoDonLeft | keyTaikoDonRight;
        keyTaikoKatsu = keyTaikoKatsuLeft | keyTaikoKatsuRight;
        clampValues();
        return true;
    }

    bool save(const std::string& path = "sdmc:/3ds/osu-3ds/settings.cfg") {
        clampValues();
        keyTaikoDon = keyTaikoDonLeft | keyTaikoDonRight;
        keyTaikoKatsu = keyTaikoKatsuLeft | keyTaikoKatsuRight;
        OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds");
        std::ofstream f(path.c_str(), std::ios::trunc);
        if (!f.is_open()) return false;

        f << "tabletScale="        << tabletScale << "\n";
        f << "tabletCenterX="      << tabletCenterX << "\n";
        f << "tabletCenterY="      << tabletCenterY << "\n";
        f << "playOnBottomScreen=" << (playOnBottomScreen ? 1 : 0) << "\n";
        f << "tapWithStylus="      << (tapWithStylus ? 1 : 0) << "\n";
        f << "musicVolume="        << musicVolume << "\n";
        f << "sfxVolume="          << sfxVolume << "\n";
        f << "audioOffsetMs="      << audioOffsetMs << "\n";
        f << "songPreview="        << (songPreview ? 1 : 0) << "\n";
        f << "cursorScale="        << cursorScale << "\n";
        f << "skinIndex="          << skinIndex << "\n";
        f << "bgDim="              << bgDim << "\n";
        f << "enableVideo="        << (enableVideo ? 1 : 0) << "\n";
        f << "snakingSliders="     << (snakingSliders ? 1 : 0) << "\n";
        f << "comboBursts="        << (comboBursts ? 1 : 0) << "\n";
        f << "showHit300="         << (showHit300 ? 1 : 0) << "\n";
        f << "beatmapHitsounds="   << (beatmapHitsounds ? 1 : 0) << "\n";
        f << "showPlayfieldBorder="<< (showPlayfieldBorder ? 1 : 0) << "\n";
        f << "taikoBgMode="        << taikoBgMode << "\n";
        f << "multiDlOption="      << multiDlOption << "\n";
        f << "language="           << language << "\n";
        f << "activeMods="         << activeMods << "\n";
        f << "gameMode="           << gameMode << "\n";
        f << "keyOsuK1="           << keyOsuK1 << "\n";
        f << "keyOsuK2="           << keyOsuK2 << "\n";
        f << "keyOsuSmoke="        << keyOsuSmoke << "\n";
        f << "keyQuickRetry="      << keyQuickRetry << "\n";
        f << "keyTaikoDon="        << keyTaikoDon << "\n";
        f << "keyTaikoKatsu="      << keyTaikoKatsu << "\n";
        f << "keyTaikoDonLeft="    << keyTaikoDonLeft << "\n";
        f << "keyTaikoDonRight="   << keyTaikoDonRight << "\n";
        f << "keyTaikoKatsuLeft="  << keyTaikoKatsuLeft << "\n";
        f << "keyTaikoKatsuRight=" << keyTaikoKatsuRight << "\n";
        f << "keyCatchLeft="       << keyCatchLeft << "\n";
        f << "keyCatchRight="      << keyCatchRight << "\n";
        f << "keyCatchDash="       << keyCatchDash << "\n";
        f << "keyManiaCol1="       << keyManiaCol1 << "\n";
        f << "keyManiaCol2="       << keyManiaCol2 << "\n";
        f << "keyManiaCol3="       << keyManiaCol3 << "\n";
        f << "keyManiaCol4="       << keyManiaCol4 << "\n";
        return true;
    }
};
