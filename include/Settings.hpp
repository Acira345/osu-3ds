#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "OszExtractor.hpp"

struct GameSettings {
    // Área de la "Tablet" en la pantalla inferior (320x240)
    // tabletScale = 1.0f -> 320x240 completo; 0.5f -> 160x120 (doble sensibilidad, menos movimiento de muñeca)
    float tabletScale   = 1.0f;
    float tabletCenterX = 160.0f;
    float tabletCenterY = 120.0f;

    // Modo de entrada:
    // false = Híbrido estilo PC (Stylus solo mueve el cursor, se golpea con botones L/R/D-Pad/ABXY)
    // true  = Tocar pantalla táctil también cuenta como golpe (Tap + Aim)
    bool tapWithStylus = true;

    // Audio y Calibración
    int musicVolume   = 85;  // 0 .. 100 %
    int sfxVolume     = 100; // 0 .. 100 %
    int audioOffsetMs = 0;   // -150 .. +150 ms

    // Opciones visuales y de juego
    float cursorScale = 1.0f; // 0.5 .. 2.0
    bool noFail       = false;

    void clampValues() {
        tabletScale = std::max(0.30f, std::min(1.00f, tabletScale));

        float halfW = (320.0f * tabletScale) * 0.5f;
        float halfH = (240.0f * tabletScale) * 0.5f;

        tabletCenterX = std::max(halfW, std::min(320.0f - halfW, tabletCenterX));
        tabletCenterY = std::max(halfH, std::min(240.0f - halfH, tabletCenterY));

        musicVolume   = std::max(0, std::min(100, musicVolume));
        sfxVolume     = std::max(0, std::min(100, sfxVolume));
        audioOffsetMs = std::max(-150, std::min(150, audioOffsetMs));
        cursorScale   = std::max(0.5f, std::min(2.0f, cursorScale));
    }

    void getTabletRect(float& outX, float& outY, float& outW, float& outH) const {
        outW = 320.0f * tabletScale;
        outH = 240.0f * tabletScale;
        outX = tabletCenterX - (outW * 0.5f);
        outY = tabletCenterY - (outH * 0.5f);
    }

    void resetTabletArea() {
        tabletScale   = 1.0f;
        tabletCenterX = 160.0f;
        tabletCenterY = 120.0f;
    }

    // Convierte un punto de la pantalla táctil (0..320, 0..240) a coordenadas osu!pixels (0..512, 0..384)
    // según el área activa configurada por el jugador.
    void mapTouchToOsu(float touchX, float touchY, float& outOsuX, float& outOsuY) const {
        float rx, ry, rw, rh;
        getTabletRect(rx, ry, rw, rh);

        float normX = (rw > 1.0f) ? ((touchX - rx) / rw) : 0.5f;
        float normY = (rh > 1.0f) ? ((touchY - ry) / rh) : 0.5f;

        normX = std::max(0.0f, std::min(1.0f, normX));
        normY = std::max(0.0f, std::min(1.0f, normY));

        outOsuX = normX * 512.0f;
        outOsuY = normY * 384.0f;
    }

    bool load(const std::string& path = "sdmc:/3ds/osu-3ds/settings.cfg") {
        std::ifstream f(path.c_str());
        if (!f.is_open()) return false;

        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "tabletScale")        tabletScale   = std::strtof(val.c_str(), nullptr);
            else if (key == "tabletCenterX") tabletCenterX = std::strtof(val.c_str(), nullptr);
            else if (key == "tabletCenterY") tabletCenterY = std::strtof(val.c_str(), nullptr);
            else if (key == "tapWithStylus") tapWithStylus = (std::atoi(val.c_str()) != 0);
            else if (key == "musicVolume")   musicVolume   = std::atoi(val.c_str());
            else if (key == "sfxVolume")     sfxVolume     = std::atoi(val.c_str());
            else if (key == "audioOffsetMs") audioOffsetMs = std::atoi(val.c_str());
            else if (key == "cursorScale")   cursorScale   = std::strtof(val.c_str(), nullptr);
            else if (key == "noFail")        noFail        = (std::atoi(val.c_str()) != 0);
        }
        clampValues();
        return true;
    }

    bool save(const std::string& path = "sdmc:/3ds/osu-3ds/settings.cfg") {
        clampValues();
        OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds");
        std::ofstream f(path.c_str(), std::ios::trunc);
        if (!f.is_open()) return false;

        f << "tabletScale="   << tabletScale << "\n";
        f << "tabletCenterX=" << tabletCenterX << "\n";
        f << "tabletCenterY=" << tabletCenterY << "\n";
        f << "tapWithStylus=" << (tapWithStylus ? 1 : 0) << "\n";
        f << "musicVolume="   << musicVolume << "\n";
        f << "sfxVolume="     << sfxVolume << "\n";
        f << "audioOffsetMs=" << audioOffsetMs << "\n";
        f << "cursorScale="   << cursorScale << "\n";
        f << "noFail="        << (noFail ? 1 : 0) << "\n";
        return true;
    }
};
