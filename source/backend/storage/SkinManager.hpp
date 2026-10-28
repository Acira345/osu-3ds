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
    bool hitCircleOverlayAboveNumber = false;
    float ringThickness = 2.2f;

    std::string hitNormalWav  = "romfs:/drum-hitnormal2.wav";
    std::string hitWhistleWav = "romfs:/drum-hitwhistle2.wav";
    std::string hitFinishWav  = "romfs:/drum-hitclap2.wav";
    std::string hitClapWav    = "romfs:/drum-hitclap2.wav";
    std::string comboBreakWav = "";
    std::string nightcoreKickWav = "";
    std::string nightcoreClapWav = "";
    std::string nightcoreHatWav  = "";
    int synthPreset = 0; // 0 = RomFS Classic, 1 = Soft/Minimal Click, 2 = Rafis Ping, 3 = Neon Synth

    // Sprites opcionales de skins personalizados (.osk)
    // osu!standard
    std::string cursorPng           = "";
    std::string cursorTrailPng      = "";
    std::string hitCirclePng        = "";
    std::string hitCircleOverlayPng = "";
    std::string approachCirclePng   = "";
    std::string comboBurstPng       = "";
    std::string sliderBallPng       = "";
    std::string sliderFollowPng     = "";
    std::string reverseArrowPng     = "";
    std::string hit0Png             = "";
    std::string hit50Png            = "";
    std::string hit100Png           = "";
    std::string hit300Png           = "";

    // osu!taiko
    std::string taikoDonPng         = "";
    std::string taikoKatPng         = "";
    std::string taikoDonBigPng      = "";
    std::string taikoKatBigPng      = "";
    std::string taikoHitCirclePng   = "";
    std::string taikoHitCircleOverlayPng = "";
    std::string taikoDrumPng        = "";
    std::string taikoDrumDonPng     = "";
    std::string taikoDrumKatPng     = "";

    // osu!mania
    std::string maniaNote1Png       = "";
    std::string maniaNote2Png       = "";
    std::string maniaNoteSPng       = "";
    std::string maniaStageLightPng  = "";
    std::string maniaStageBottomPng = "";

    // osu!catch
    std::string catchFruitApplePng  = "";
    std::string catchFruitOrangePng = "";
    std::string catchFruitGrapePng  = "";
    std::string catchFruitStrawberryPng = "";
    std::string catchFruitBananaPng = "";
    std::string catchFruitDropPng   = "";
    std::string catchPlatePng       = "";
    std::string catchCatcherPng     = "";
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
            s.synthPreset = 0;
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
            s.synthPreset = 1;
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
            s.synthPreset = 2;
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
            s.synthPreset = 3;
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
        out.r = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(Beatmap::trim(parts[0]).c_str()))));
        out.g = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(Beatmap::trim(parts[1]).c_str()))));
        out.b = static_cast<uint8_t>(std::max(0, std::min(255, std::atoi(Beatmap::trim(parts[2]).c_str()))));
        return true;
    }

    static std::vector<std::string> getFilesInFolder(const std::string& folder) {
        std::vector<std::string> files;
        std::string openPath = folder;
        if (openPath.size() > 1 && openPath.back() == '/' && openPath.find(":/") != openPath.size() - 2) {
            openPath.pop_back();
        }
        DIR* dir = opendir(openPath.c_str());
        if (!dir) return files;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            files.push_back(entry->d_name);
        }
        closedir(dir);
        return files;
    }

    static std::string findFileInList(const std::string& folder, const std::vector<std::string>& files, const std::vector<std::string>& candidates) {
        for (const auto& cand : candidates) {
            std::string candLow = Beatmap::toLower(cand);
            for (const auto& f : files) {
                if (Beatmap::toLower(f) == candLow) {
                    return folder + f;
                }
            }
        }
        return "";
    }

    static std::string findImageInFolder(const std::string& folder, const std::vector<std::string>& files, const std::vector<std::string>& baseNames) {
        for (const auto& base : baseNames) {
            std::vector<std::string> cands = {
                base + ".png",
                base + "@2x.png",
                base + ".jpg",
                base + "@2x.jpg",
                base + ".jpeg"
            };
            std::string res = findFileInList(folder, files, cands);
            if (!res.empty()) return res;
        }
        return "";
    }

    static std::string findAudioInFolder(const std::string& folder, const std::vector<std::string>& files, const std::vector<std::string>& baseNames, const std::string& fallback) {
        for (const auto& base : baseNames) {
            std::vector<std::string> cands = {
                base + ".wav",
                base + ".ogg",
                base + ".mp3"
            };
            std::string res = findFileInList(folder, files, cands);
            if (!res.empty()) return res;
        }
        return fallback;
    }

    static std::string findImageWithSubdirs(const std::string& folder, const std::vector<std::string>& files, const std::vector<std::string>& baseNames, const std::string& subDirName = "") {
        std::string res = findImageInFolder(folder, files, baseNames);
        if (!res.empty()) return res;
        if (!subDirName.empty()) {
            std::string subPath = folder + subDirName + "/";
            auto subFiles = getFilesInFolder(subPath);
            if (!subFiles.empty()) {
                res = findImageInFolder(subPath, subFiles, baseNames);
                if (!res.empty()) return res;
            }
        }
        return "";
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

        // Listar archivos del directorio de skin una sola vez en memoria
        std::vector<std::string> files = getFilesInFolder(folderPath);

        // Buscar skin.ini (insensible a mayúsculas)
        std::string iniPath = findFileInList(folderPath, files, {"skin.ini"});
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
                    // Quitar comentarios // antes de interpretar la línea
                    size_t commentPos = line.find("//");
                    if (commentPos != std::string::npos) {
                        line = line.substr(0, commentPos);
                    }
                    line = Beatmap::trim(line);
                    if (line.empty()) continue;

                    if (line.front() == '[' && line.back() == ']') {
                        section = Beatmap::toLower(Beatmap::trim(line.substr(1, line.size() - 2)));
                        continue;
                    }

                    size_t sep = line.find(':');
                    if (sep == std::string::npos) continue;
                    std::string key = Beatmap::toLower(Beatmap::trim(line.substr(0, sep)));
                    std::string val = Beatmap::trim(line.substr(sep + 1));

                    if (section == "general") {
                        if (key == "name" && !val.empty()) s.name = "[SD] " + val;
                        else if (key == "author" && !val.empty()) s.author = val;
                        else if (key == "cursorexpand") s.cursorExpand = (std::atoi(val.c_str()) != 0);
                        else if (key == "cursortrail")  s.cursorTrail  = (std::atoi(val.c_str()) != 0);
                        else if (key == "darkcirclecenter") s.darkCircleCenter = (std::atoi(val.c_str()) != 0);
                        else if (key == "hitcircleoverlayabovenumber") s.hitCircleOverlayAboveNumber = (std::atoi(val.c_str()) != 0);
                    } else if (section == "colours" || section == "colors") {
                        RGBColor rgb;
                        if (key.rfind("combo", 0) == 0 && parseRGB(val, rgb)) {
                            s.comboColors.push_back(rgb);
                        } else if (key == "sliderborder" && parseRGB(val, rgb)) {
                            s.sliderBorderColor = rgb;
                        } else if (key == "slidertrackoverride" && parseRGB(val, rgb)) {
                            s.hasSliderTrackOverride = true;
                            s.sliderTrackColor = rgb;
                        } else if (key == "cursor" && parseRGB(val, rgb)) {
                            s.cursorColor = rgb;
                        } else if (key == "cursortrail" && parseRGB(val, rgb)) {
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

        // Resolver hitsounds (.wav, .ogg o .mp3) del skin en la SD
        s.hitNormalWav = findAudioInFolder(folderPath, files, {
            "soft-hitnormal", "normal-hitnormal", "drum-hitnormal", "hitnormal", "soft-hitnormal2"
        }, "romfs:/drum-hitnormal2.wav");

        s.hitWhistleWav = findAudioInFolder(folderPath, files, {
            "soft-hitwhistle", "normal-hitwhistle", "drum-hitwhistle", "hitwhistle", "soft-hitwhistle2"
        }, "romfs:/drum-hitwhistle2.wav");

        s.hitFinishWav = findAudioInFolder(folderPath, files, {
            "soft-hitfinish", "normal-hitfinish", "drum-hitfinish", "hitfinish", "soft-hitclap"
        }, "romfs:/drum-hitclap2.wav");

        s.hitClapWav = findAudioInFolder(folderPath, files, {
            "soft-hitclap", "normal-hitclap", "drum-hitclap", "hitclap", "soft-hitclap2"
        }, "romfs:/drum-hitclap2.wav");

        s.comboBreakWav = findAudioInFolder(folderPath, files, {"combobreak"}, "");

        s.nightcoreKickWav = findAudioInFolder(folderPath, files, {"nightcore-kick"}, "");
        s.nightcoreClapWav = findAudioInFolder(folderPath, files, {"nightcore-clap"}, "");
        s.nightcoreHatWav  = findAudioInFolder(folderPath, files, {"nightcore-hat"}, "");

        // Sprites PNG/JPG de osu!standard (con soporte nativo para 1x y @2x)
        s.cursorPng           = findImageInFolder(folderPath, files, {"cursor"});
        s.cursorTrailPng      = findImageInFolder(folderPath, files, {"cursortrail"});
        s.hitCirclePng        = findImageInFolder(folderPath, files, {"hitcircle"});
        s.hitCircleOverlayPng = findImageInFolder(folderPath, files, {"hitcircleoverlay"});
        s.approachCirclePng   = findImageInFolder(folderPath, files, {"approachcircle"});
        s.comboBurstPng       = findImageInFolder(folderPath, files, {"comboburst", "comboburst-0", "comboburst-1"});
        s.sliderBallPng       = findImageInFolder(folderPath, files, {"sliderb0", "sliderb", "sliderball"});
        s.sliderFollowPng     = findImageInFolder(folderPath, files, {"sliderfollowcircle"});
        s.reverseArrowPng     = findImageInFolder(folderPath, files, {"reversearrow"});
        s.hit0Png             = findImageInFolder(folderPath, files, {"hit0-0", "hit0"});
        s.hit50Png            = findImageInFolder(folderPath, files, {"hit50-0", "hit50"});
        s.hit100Png           = findImageInFolder(folderPath, files, {"hit100-0", "hit100", "hit100k-0", "hit100k"});
        s.hit300Png           = findImageInFolder(folderPath, files, {"hit300-0", "hit300", "hit300g-0", "hit300g", "hit300k-0", "hit300k"});

        // Sprites de osu!taiko (busca en raíz y en subcarpeta taiko/)
        s.taikoDonPng         = findImageWithSubdirs(folderPath, files, {"taiko-normal", "don_note"}, "taiko");
        s.taikoKatPng         = findImageWithSubdirs(folderPath, files, {"taiko-clap", "kat_note"}, "taiko");
        s.taikoDonBigPng      = findImageWithSubdirs(folderPath, files, {"taiko-big-normal", "don_big"}, "taiko");
        s.taikoKatBigPng      = findImageWithSubdirs(folderPath, files, {"taiko-big-clap", "kat_big"}, "taiko");
        s.taikoHitCirclePng   = findImageWithSubdirs(folderPath, files, {"taiko-hit-circle", "taikohitcircle"}, "taiko");
        s.taikoHitCircleOverlayPng = findImageWithSubdirs(folderPath, files, {"taiko-hit-circle-overlay", "taikohitcircleoverlay"}, "taiko");
        s.taikoDrumPng        = findImageWithSubdirs(folderPath, files, {"taiko-drum", "taiko-bar-left", "drum_idle"}, "taiko");
        s.taikoDrumDonPng     = findImageWithSubdirs(folderPath, files, {"taiko-drum-inner", "drum_don"}, "taiko");
        s.taikoDrumKatPng     = findImageWithSubdirs(folderPath, files, {"taiko-drum-outer", "drum_kat"}, "taiko");

        // Sprites de osu!mania (busca en raíz y en subcarpeta mania/)
        s.maniaNote1Png       = findImageWithSubdirs(folderPath, files, {"mania-note1", "note1", "mania-note1h", "mania-key1"}, "mania");
        s.maniaNote2Png       = findImageWithSubdirs(folderPath, files, {"mania-note2", "note2", "mania-note2h", "mania-key2"}, "mania");
        s.maniaNoteSPng       = findImageWithSubdirs(folderPath, files, {"mania-notes", "notes", "mania-notesh", "mania-keys"}, "mania");
        s.maniaStageLightPng  = findImageWithSubdirs(folderPath, files, {"mania-stage-light", "stage-light", "lightingn"}, "mania");
        s.maniaStageBottomPng = findImageWithSubdirs(folderPath, files, {"mania-stage-bottom", "stage-bottom", "mania-stage-hint"}, "mania");

        // Sprites de osu!catch (busca en raíz y en subcarpeta catch/)
        s.catchFruitApplePng      = findImageWithSubdirs(folderPath, files, {"fruit-apple", "fruit-apple-overlay"}, "catch");
        s.catchFruitOrangePng     = findImageWithSubdirs(folderPath, files, {"fruit-orange", "fruit-orange-overlay"}, "catch");
        s.catchFruitGrapePng      = findImageWithSubdirs(folderPath, files, {"fruit-grapes", "fruit-grape"}, "catch");
        s.catchFruitStrawberryPng = findImageWithSubdirs(folderPath, files, {"fruit-strawberry"}, "catch");
        s.catchFruitBananaPng     = findImageWithSubdirs(folderPath, files, {"fruit-bananas", "fruit-banana"}, "catch");
        s.catchFruitDropPng       = findImageWithSubdirs(folderPath, files, {"fruit-drop", "fruit-droplet"}, "catch");
        s.catchPlatePng           = findImageWithSubdirs(folderPath, files, {"fruit-plate", "plate"}, "catch");
        s.catchCatcherPng         = findImageWithSubdirs(folderPath, files, {"fruit-catcher-idle", "catcher-idle", "fruit-ryuuta"}, "catch");

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
