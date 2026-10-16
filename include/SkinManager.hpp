#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include "Beatmap.hpp"
#include "OszExtractor.hpp"

struct SkinProfile {
    std::string name = "osu! Classic";
    std::string author = "osu! 3DS";
    std::string folderPath = "";
    bool isCustomFromSD = false;

    std::vector<RGBColor> comboColors;
    RGBColor sliderBorderColor = RGBColor(255, 255, 255);
    bool hasSliderTrackOverride = false;
    RGBColor sliderTrackColor = RGBColor(30, 28, 45);

    RGBColor cursorColor = RGBColor(255, 220, 80);
    RGBColor cursorTrailColor = RGBColor(255, 190, 120);

    bool cursorExpand = true;
    bool cursorTrail = true;
    bool darkCircleCenter = false;
    float ringThickness = 2.2f;

    std::string hitNormalWav  = "romfs:/drum-hitnormal2.wav";
    std::string hitWhistleWav = "romfs:/drum-hitwhistle2.wav";
    std::string hitClapWav    = "romfs:/drum-hitclap2.wav";

    // Sprites PNG opcionales de skins personalizados (.osk)
    std::string cursorPng           = "";
    std::string hitCirclePng        = "";
    std::string hitCircleOverlayPng = "";
    std::string approachCirclePng   = "";
};

class SkinManager {
public:
    std::vector<SkinProfile> skins;
    int extractedOskCount = 0;

    void initAndScan() {
        skins.clear();
        extractedOskCount = 0;

        OszExtractor::ensureDirectoryRecursive("sdmc:/3ds/osu-3ds/skins");

        // Extraer cualquier archivo .osk en las carpetas de la SD
        extractedOskCount += OszExtractor::extractAllPendingOsk(
            "sdmc:/3ds/osu-3ds/skins", "sdmc:/3ds/osu-3ds/skins"
        );
        extractedOskCount += OszExtractor::extractAllPendingOsk(
            "sdmc:/3ds/osu-3ds", "sdmc:/3ds/osu-3ds/skins"
        );

        // 1. Añadir los 4 Skins Vectoriales de 60 FPS integrados
        addBuiltInPresets();

        // 2. Escanear carpetas de Skins personalizados en sdmc:/3ds/osu-3ds/skins/
        scanSkinsDirectory("sdmc:/3ds/osu-3ds/skins/");
    }

private:
    void addBuiltInPresets() {
        // Preset 1: osu! Classic
        {
            SkinProfile s;
            s.name = "osu! Classic";
            s.author = "Built-in";
            s.comboColors = {
                RGBColor(255, 115, 180),
                RGBColor(90, 235, 140),
                RGBColor(70, 180, 255),
                RGBColor(255, 180, 70)
            };
            s.sliderBorderColor = RGBColor(255, 255, 255);
            s.hasSliderTrackOverride = false;
            s.cursorColor = RGBColor(255, 220, 80);
            s.cursorTrailColor = RGBColor(255, 180, 90);
            s.darkCircleCenter = false;
            s.ringThickness = 2.0f;
            skins.push_back(s);
        }

        // Preset 2: WhiteCat Minimal (Centro oscuro competitivo, muy legible en streams)
        {
            SkinProfile s;
            s.name = "WhiteCat Minimal";
            s.author = "Built-in";
            s.comboColors = {
                RGBColor(235, 195, 165),
                RGBColor(165, 205, 235),
                RGBColor(220, 175, 220),
                RGBColor(175, 225, 195)
            };
            s.sliderBorderColor = RGBColor(235, 235, 235);
            s.hasSliderTrackOverride = true;
            s.sliderTrackColor = RGBColor(24, 22, 32);
            s.cursorColor = RGBColor(255, 235, 150);
            s.cursorTrailColor = RGBColor(240, 220, 140);
            s.darkCircleCenter = true;
            s.ringThickness = 2.8f;
            skins.push_back(s);
        }

        // Preset 3: Rafis Cyan (Alto contraste cian/blanco)
        {
            SkinProfile s;
            s.name = "Rafis Cyan";
            s.author = "Built-in";
            s.comboColors = {
                RGBColor(0, 200, 255),
                RGBColor(190, 235, 255),
                RGBColor(80, 160, 255)
            };
            s.sliderBorderColor = RGBColor(210, 240, 255);
            s.hasSliderTrackOverride = true;
            s.sliderTrackColor = RGBColor(16, 26, 42);
            s.cursorColor = RGBColor(255, 240, 60);
            s.cursorTrailColor = RGBColor(100, 220, 255);
            s.darkCircleCenter = true;
            s.ringThickness = 2.6f;
            skins.push_back(s);
        }

        // Preset 4: Neon Synth
        {
            SkinProfile s;
            s.name = "Neon Synth";
            s.author = "Built-in";
            s.comboColors = {
                RGBColor(255, 50, 160),
                RGBColor(0, 245, 212),
                RGBColor(180, 90, 255),
                RGBColor(255, 210, 40)
            };
            s.sliderBorderColor = RGBColor(0, 245, 212);
            s.hasSliderTrackOverride = true;
            s.sliderTrackColor = RGBColor(25, 14, 38);
            s.cursorColor = RGBColor(0, 255, 220);
            s.cursorTrailColor = RGBColor(255, 50, 160);
            s.darkCircleCenter = true;
            s.ringThickness = 2.5f;
            skins.push_back(s);
        }
    }

    static bool parseRGB(const std::string& val, RGBColor& out) {
        std::vector<std::string> parts = Beatmap::split(val, ',');
        if (parts.size() < 3) return false;
        out.r = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(parts[0].c_str()))));
        out.g = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(parts[1].c_str()))));
        out.b = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(parts[2].c_str()))));
        return true;
    }

    static std::string findWavInSkinFolder(const std::string& folder, const std::vector<std::string>& candidates, const std::string& fallback) {
        std::string openPath = folder;
        if (openPath.size() > 1 && openPath.back() == '/' && openPath.find(":/") != openPath.size() - 2) {
            openPath.pop_back();
        }
        DIR* dir = opendir(openPath.c_str());
        if (!dir) return fallback;

        std::vector<std::string> files;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            files.push_back(entry->d_name);
        }
        closedir(dir);

        for (const auto& cand : candidates) {
            std::string candLow = Beatmap::toLower(cand);
            for (const auto& f : files) {
                if (Beatmap::toLower(f) == candLow) {
                    return folder + f;
                }
            }
        }
        return fallback;
    }

    void loadCustomSkinFolder(const std::string& folderPath, const std::string& folderName) {
        SkinProfile s;
        s.name = "[SD] " + folderName;
        s.author = "Custom Skin";
        s.folderPath = folderPath;
        s.isCustomFromSD = true;
        s.darkCircleCenter = true;
        s.ringThickness = 2.6f;
        s.hasSliderTrackOverride = true;
        s.sliderTrackColor = RGBColor(22, 22, 32);

        // Buscar skin.ini (insensible a mayúsculas)
        std::string iniPath = findWavInSkinFolder(folderPath, {"skin.ini"}, "");
        if (!iniPath.empty()) {
            std::ifstream file(iniPath.c_str());
            if (file.is_open()) {
                std::string line;
                std::string section = "";
                while (std::getline(file, line)) {
                    if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
                        (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                        line = line.substr(3);
                    }
                    line = Beatmap::trim(line);
                    if (line.empty() || line.rfind("//", 0) == 0) continue;

                    if (line.front() == '[' && line.back() == ']') {
                        section = Beatmap::toLower(Beatmap::trim(line.substr(1, line.size() - 2)));
                        continue;
                    }

                    size_t sep = line.find(':');
                    if (sep == std::string::npos) continue;
                    std::string key = Beatmap::trim(line.substr(0, sep));
                    std::string val = Beatmap::trim(line.substr(sep + 1));
                    // Quitar comentarios inline si existen
                    size_t commentPos = val.find("//");
                    if (commentPos != std::string::npos) {
                        val = Beatmap::trim(val.substr(0, commentPos));
                    }

                    if (section == "general") {
                        if (key == "Name" && !val.empty()) s.name = "[SD] " + val;
                        else if (key == "Author" && !val.empty()) s.author = val;
                        else if (key == "CursorExpand") s.cursorExpand = (std::atoi(val.c_str()) != 0);
                        else if (key == "CursorTrail")  s.cursorTrail  = (std::atoi(val.c_str()) != 0);
                        else if (key == "DarkCircleCenter") s.darkCircleCenter = (std::atoi(val.c_str()) != 0);
                    } else if (section == "colours" || section == "colors") {
                        RGBColor rgb;
                        if (key.rfind("Combo", 0) == 0 && parseRGB(val, rgb)) {
                            s.comboColors.push_back(rgb);
                        } else if (key == "SliderBorder" && parseRGB(val, rgb)) {
                            s.sliderBorderColor = rgb;
                        } else if (key == "SliderTrackOverride" && parseRGB(val, rgb)) {
                            s.hasSliderTrackOverride = true;
                            s.sliderTrackColor = rgb;
                        } else if (key == "Cursor" && parseRGB(val, rgb)) {
                            s.cursorColor = rgb;
                        } else if (key == "CursorTrail" && parseRGB(val, rgb)) {
                            s.cursorTrailColor = rgb;
                        }
                    }
                }
            }
        }

        if (s.comboColors.empty()) {
            s.comboColors = {
                RGBColor(240, 200, 170),
                RGBColor(170, 215, 245),
                RGBColor(215, 180, 240),
                RGBColor(175, 235, 195)
            };
        }

        // Resolver hitsounds reales (.wav o .ogg) del skin en la SD
        s.hitNormalWav = findWavInSkinFolder(folderPath, {
            "soft-hitnormal.wav", "normal-hitnormal.wav", "drum-hitnormal.wav",
            "soft-hitnormal.ogg", "normal-hitnormal.ogg", "drum-hitnormal.ogg",
            "soft-hitnormal2.wav", "normal-hitnormal2.wav", "drum-hitnormal2.wav"
        }, "romfs:/drum-hitnormal2.wav");

        s.hitWhistleWav = findWavInSkinFolder(folderPath, {
            "soft-hitwhistle.wav", "normal-hitwhistle.wav", "drum-hitwhistle.wav",
            "soft-hitwhistle.ogg", "normal-hitwhistle.ogg", "drum-hitwhistle.ogg",
            "soft-hitwhistle2.wav", "normal-hitwhistle2.wav", "drum-hitwhistle2.wav"
        }, "romfs:/drum-hitwhistle2.wav");

        s.hitClapWav = findWavInSkinFolder(folderPath, {
            "soft-hitclap.wav", "normal-hitclap.wav", "drum-hitclap.wav",
            "soft-hitclap.ogg", "normal-hitclap.ogg", "drum-hitclap.ogg",
            "soft-hitfinish.wav", "normal-hitfinish.wav", "drum-hitfinish.wav",
            "soft-hitfinish.ogg", "normal-hitfinish.ogg", "drum-hitfinish.ogg",
            "soft-hitclap2.wav", "normal-hitclap2.wav", "drum-hitclap2.wav"
        }, "romfs:/drum-hitclap2.wav");

        // Resolver sprites PNG opcionales del skin
        s.cursorPng           = findWavInSkinFolder(folderPath, {"cursor.png"}, "");
        s.hitCirclePng        = findWavInSkinFolder(folderPath, {"hitcircle.png"}, "");
        s.hitCircleOverlayPng = findWavInSkinFolder(folderPath, {"hitcircleoverlay.png"}, "");
        s.approachCirclePng   = findWavInSkinFolder(folderPath, {"approachcircle.png"}, "");

        skins.push_back(s);
    }

    void scanSkinsDirectory(const std::string& rootFolder) {
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
            DIR* sub = opendir(fullPath.c_str());
            if (sub) {
                closedir(sub);
                loadCustomSkinFolder(fullPath + "/", name);
            }
        }
        closedir(dir);
    }
};
