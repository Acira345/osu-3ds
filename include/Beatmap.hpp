#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <dirent.h>

enum HitState {
    NOT_HIT,
    HIT_300,
    HIT_100,
    HIT_50,
    HIT_MISS
};

struct Vec2 {
    float x;
    float y;
    Vec2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
};

struct RGBColor {
    uint8_t r, g, b;
    RGBColor(uint8_t _r = 255, uint8_t _g = 255, uint8_t _b = 255) : r(_r), g(_g), b(_b) {}
};

struct TimingPoint {
    double time = 0.0;
    float beatLength = 333.33f;
    bool uninherited = true;
    int volume = 100;
};

struct SliderTick {
    float x = 0.0f;
    float y = 0.0f;
    int time = 0;
    bool hit = false;
    SliderTick(float _x = 0.0f, float _y = 0.0f, int _t = 0)
        : x(_x), y(_y), time(_t), hit(false) {}
};

struct HitCircle {
    float x = 0.0f;
    float y = 0.0f;
    int time = 0;
    int endTime = 0;
    int hitSound = 0;
    int comboNumber = 1;
    int colorIndex = 0;

    bool isSlider = false;
    bool isSpinner = false;
    bool sliderDone = false;
    bool sliderTracking = false;
    int repeats = 1;
    int lastRepeatProcessed = 0;
    float pixelLength = 0.0f;
    std::vector<Vec2> sliderPath;
    std::vector<int> edgeHitSounds;
    std::vector<SliderTick> sliderTicks;

    // Estado para Spinners
    float spinnerProgress = 0.0f;
    float spinnerRequiredRads = 12.0f;
    float lastSpinnerAngle = 0.0f;
    bool spinnerAngleInit = false;

    int stackCount = 0;
    HitState state = NOT_HIT;

    int getEdgeHitSound(int edgeIndex) const {
        if (edgeIndex >= 0 && static_cast<size_t>(edgeIndex) < edgeHitSounds.size()) {
            return edgeHitSounds[edgeIndex];
        }
        return hitSound;
    }

    Vec2 getSliderBallPos(int songTime) const {
        if (sliderPath.empty()) return Vec2(x, y);
        if (sliderPath.size() == 1 || endTime <= time) return sliderPath.front();

        float totalDuration = static_cast<float>(endTime - time);
        float slideDuration = totalDuration / static_cast<float>(repeats);
        float elapsed = static_cast<float>(songTime - time);

        if (elapsed <= 0.0f) return sliderPath.front();
        if (elapsed >= totalDuration) {
            return (repeats % 2 == 1) ? sliderPath.back() : sliderPath.front();
        }

        int currentRepeat = static_cast<int>(elapsed / slideDuration);
        if (currentRepeat >= repeats) currentRepeat = repeats - 1;
        float progress = (elapsed - (currentRepeat * slideDuration)) / slideDuration;
        if (currentRepeat % 2 == 1) {
            progress = 1.0f - progress;
        }

        float targetIndex = progress * static_cast<float>(sliderPath.size() - 1);
        size_t idx = static_cast<size_t>(targetIndex);
        float frac = targetIndex - static_cast<float>(idx);

        if (idx + 1 >= sliderPath.size()) return sliderPath.back();
        return Vec2(
            sliderPath[idx].x + (sliderPath[idx + 1].x - sliderPath[idx].x) * frac,
            sliderPath[idx].y + (sliderPath[idx + 1].y - sliderPath[idx].y) * frac
        );
    }

    Vec2 getEndPosition() const {
        if (isSlider && !sliderPath.empty()) {
            return (repeats % 2 == 1) ? sliderPath.back() : sliderPath.front();
        }
        return Vec2(x, y);
    }
};

struct BeatmapDifficulty {
    float hp = 5.0f;
    float cs = 4.0f;
    float ar = 5.0f;
    float od = 5.0f;
    bool hasExplicitAR = false;
    float sliderMultiplier = 1.4f;
    float sliderTickRate = 1.0f;
    float preempt = 1200.0f;
    float fadeIn = 800.0f;
    float hit50 = 150.0f;
    float hit100 = 100.0f;
    float hit300 = 50.0f;
    float radiusOsu = 36.48f;

    void calculateDerivedValues() {
        if (!hasExplicitAR) {
            ar = od;
        }
        radiusOsu = 54.4f - (4.48f * cs);
        if (radiusOsu < 10.0f) radiusOsu = 10.0f;

        if (ar < 5.0f) {
            preempt = 1200.0f + 600.0f * (5.0f - ar) / 5.0f;
            fadeIn  = 800.0f + 400.0f * (5.0f - ar) / 5.0f;
        } else if (ar == 5.0f) {
            preempt = 1200.0f;
            fadeIn  = 800.0f;
        } else {
            preempt = 1200.0f - 750.0f * (ar - 5.0f) / 5.0f;
            fadeIn  = 800.0f - 500.0f * (ar - 5.0f) / 5.0f;
        }

        hit300 = 80.0f - (6.0f * od);
        hit100 = 140.0f - (8.0f * od);
        hit50  = 200.0f - (10.0f * od);
    }
};

struct Beatmap {
    std::string filePath = "";
    std::string folderPath = "";
    std::string audioFilename = "audio.mp3";
    std::string fullAudioPath = "";
    int audioLeadIn = 0;
    int previewTime = -1;
    float stackLeniency = 0.7f;
    int mode = 0;

    std::string title = "Unknown";
    std::string artist = "Unknown Artist";
    std::string creator = "";
    std::string version = "Normal";

    BeatmapDifficulty diff;
    std::vector<TimingPoint> timingPoints;
    std::vector<RGBColor> customComboColors;
    std::vector<HitCircle> circles;

    int circleCount = 0;
    int sliderCount = 0;
    int spinnerCount = 0;

    static inline std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static inline std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
        return elems;
    }

    static inline std::string toLower(std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        return s;
    }

    static std::string extractFolder(const std::string& path) {
        size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return "";
        return path.substr(0, slash + 1);
    }

    // Resuelve la ruta del archivo de audio buscando coincidencia exacta o insensible a mayúsculas
    static std::string resolveAudioPath(const std::string& folder, const std::string& audioName) {
        std::string direct = folder + audioName;
        FILE* f = std::fopen(direct.c_str(), "rb");
        if (f) {
            std::fclose(f);
            return direct;
        }

        // Búsqueda insensible a mayúsculas/minúsculas dentro de la carpeta
        std::string dirPath = folder;
        if (dirPath.size() > 1 && dirPath.back() == '/' && dirPath.find(":/") != dirPath.size() - 2) {
            dirPath.pop_back();
        }
        DIR* dir = opendir(dirPath.c_str());
        if (dir) {
            std::string targetLower = toLower(audioName);
            std::string fallbackMp3 = "";
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string candidate = entry->d_name;
                std::string candLower = toLower(candidate);
                if (candLower == targetLower) {
                    closedir(dir);
                    return folder + candidate;
                }
                if (fallbackMp3.empty() && candLower.size() > 4 &&
                    candLower.substr(candLower.size() - 4) == ".mp3") {
                    fallbackMp3 = folder + candidate;
                }
            }
            closedir(dir);
            if (!fallbackMp3.empty()) return fallbackMp3;
        }

        // Fallback final al audio integrado en romfs
        return "romfs:/audio.mp3";
    }

    static Vec2 evalBezier(const std::vector<Vec2>& pts, float t) {
        std::vector<Vec2> tmp = pts;
        int n = static_cast<int>(tmp.size());
        for (int r = 1; r < n; ++r) {
            for (int i = 0; i < n - r; ++i) {
                tmp[i].x = (1.0f - t) * tmp[i].x + t * tmp[i + 1].x;
                tmp[i].y = (1.0f - t) * tmp[i].y + t * tmp[i + 1].y;
            }
        }
        return tmp[0];
    }

    static Vec2 evalCatmull(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        float x = 0.5f * ((2.0f * p1.x) +
                          (-p0.x + p2.x) * t +
                          (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                          (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
        float y = 0.5f * ((2.0f * p1.y) +
                          (-p0.y + p2.y) * t +
                          (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                          (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
        return Vec2(x, y);
    }

    // Genera un arco circular perfecto que pasa por p0 -> p1 -> p2
    static void appendCircularArc(const Vec2& p0, const Vec2& p1, const Vec2& p2, std::vector<Vec2>& outPath) {
        float d = 2.0f * (p0.x * (p1.y - p2.y) + p1.x * (p2.y - p0.y) + p2.x * (p0.y - p1.y));
        if (std::fabs(d) < 1e-3f) {
            // Puntos colineales: degradar a segmentos lineales
            outPath.push_back(p1);
            outPath.push_back(p2);
            return;
        }

        float p0Sq = p0.x * p0.x + p0.y * p0.y;
        float p1Sq = p1.x * p1.x + p1.y * p1.y;
        float p2Sq = p2.x * p2.x + p2.y * p2.y;

        float cx = (p0Sq * (p1.y - p2.y) + p1Sq * (p2.y - p0.y) + p2Sq * (p0.y - p1.y)) / d;
        float cy = (p0Sq * (p2.x - p1.x) + p1Sq * (p0.x - p2.x) + p2Sq * (p1.x - p0.x)) / d;
        float r = std::sqrt((p0.x - cx) * (p0.x - cx) + (p0.y - cy) * (p0.y - cy));

        if (r > 5000.0f) {
            outPath.push_back(p1);
            outPath.push_back(p2);
            return;
        }

        float a0 = std::atan2(p0.y - cy, p0.x - cx);
        float a1 = std::atan2(p1.y - cy, p1.x - cx);
        float a2 = std::atan2(p2.y - cy, p2.x - cx);

        const float twoPi = 2.0f * static_cast<float>(M_PI);
        // Producto cruzado para saber el sentido de giro de p0 -> p1 -> p2
        float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
        if (cross > 0.0f) {
            while (a1 < a0) a1 += twoPi;
            while (a2 < a1) a2 += twoPi;
        } else {
            while (a1 > a0) a1 -= twoPi;
            while (a2 > a1) a2 -= twoPi;
        }

        float totalAngle = std::fabs(a2 - a0);
        int steps = std::max(8, std::min(32, static_cast<int>((r * totalAngle) / 5.0f)));
        for (int i = 1; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float ang = a0 + (a2 - a0) * t;
            outPath.push_back(Vec2(cx + std::cos(ang) * r, cy + std::sin(ang) * r));
        }
    }

    // Re-muestrea y recorta una polilínea para que tenga exactamente pixelLength y pasos uniformes
    static std::vector<Vec2> resampleToLength(const std::vector<Vec2>& rawPath, float targetLength) {
        if (rawPath.size() < 2 || targetLength <= 0.0f) return rawPath;

        std::vector<float> cumDist(rawPath.size(), 0.0f);
        for (size_t i = 1; i < rawPath.size(); ++i) {
            float dx = rawPath[i].x - rawPath[i - 1].x;
            float dy = rawPath[i].y - rawPath[i - 1].y;
            cumDist[i] = cumDist[i - 1] + std::sqrt(dx * dx + dy * dy);
        }

        float totalRawLen = cumDist.back();
        if (totalRawLen < 0.01f) return rawPath;

        // Elegir número de segmentos equilibrado para precisión y rendimiento en 3DS (máx 28 segmentos)
        int numSegments = std::max(2, std::min(28, static_cast<int>(targetLength / 7.5f)));
        std::vector<Vec2> result;
        result.reserve(numSegments + 1);
        result.push_back(rawPath.front());

        size_t segIdx = 0;
        for (int s = 1; s <= numSegments; ++s) {
            float d = (targetLength * static_cast<float>(s)) / static_cast<float>(numSegments);
            if (d <= totalRawLen) {
                while (segIdx + 1 < rawPath.size() - 1 && cumDist[segIdx + 1] < d) {
                    segIdx++;
                }
                float segLen = cumDist[segIdx + 1] - cumDist[segIdx];
                float frac = (segLen > 1e-4f) ? ((d - cumDist[segIdx]) / segLen) : 0.0f;
                result.push_back(Vec2(
                    rawPath[segIdx].x + (rawPath[segIdx + 1].x - rawPath[segIdx].x) * frac,
                    rawPath[segIdx].y + (rawPath[segIdx + 1].y - rawPath[segIdx].y) * frac
                ));
            } else {
                // Extender en la dirección del último segmento si pixelLength supera la curva
                const Vec2& pA = rawPath[rawPath.size() - 2];
                const Vec2& pB = rawPath.back();
                float dx = pB.x - pA.x;
                float dy = pB.y - pA.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    float extra = d - totalRawLen;
                    result.push_back(Vec2(pB.x + (dx / len) * extra, pB.y + (dy / len) * extra));
                } else {
                    result.push_back(pB);
                }
            }
        }
        return result;
    }

    // Obtiene el beatLength base (ms por beat) y el multiplicador SV activos en timeMs
    void getTimingAt(int timeMs, float& outBeatLength, float& outSvMult) const {
        outBeatLength = 333.33f;
        outSvMult = 1.0f;
        if (timingPoints.empty()) return;

        // Buscar el primer uninherited como fallback inicial
        for (const auto& tp : timingPoints) {
            if (tp.uninherited && tp.beatLength > 0.0f) {
                outBeatLength = tp.beatLength;
                break;
            }
        }

        for (const auto& tp : timingPoints) {
            if (tp.time > static_cast<double>(timeMs) + 1.0) break;
            if (tp.uninherited) {
                if (tp.beatLength > 0.0f) outBeatLength = tp.beatLength;
                outSvMult = 1.0f;
            } else {
                if (tp.beatLength < 0.0f) {
                    float mult = -100.0f / tp.beatLength;
                    outSvMult = std::max(0.1f, std::min(10.0f, mult));
                }
            }
        }
    }

    void applyStacking() {
        if (circles.size() < 2 || stackLeniency <= 0.0f) return;

        const float stackDistance = 3.0f;
        const float stackThreshold = diff.preempt * stackLeniency;
        const float stackOffset = diff.radiusOsu / 10.0f;

        for (auto& c : circles) c.stackCount = 0;

        for (int i = static_cast<int>(circles.size()) - 1; i > 0; --i) {
            int n = i;
            HitCircle* objI = &circles[i];
            if (objI->stackCount != 0 || objI->isSpinner) continue;

            if (!objI->isSlider) {
                while (--n >= 0) {
                    HitCircle& objN = circles[n];
                    if (objN.isSpinner) continue;
                    if (objI->time - objN.endTime > stackThreshold) break;

                    if (objN.isSlider) {
                        Vec2 endPos = objN.getEndPosition();
                        float dxEnd = endPos.x - objI->x;
                        float dyEnd = endPos.y - objI->y;
                        if (std::sqrt(dxEnd * dxEnd + dyEnd * dyEnd) < stackDistance) {
                            int offset = objI->stackCount - objN.stackCount + 1;
                            for (int j = n + 1; j <= i; ++j) {
                                float dx = endPos.x - circles[j].x;
                                float dy = endPos.y - circles[j].y;
                                if (std::sqrt(dx * dx + dy * dy) < stackDistance) {
                                    circles[j].stackCount -= offset;
                                }
                            }
                            break;
                        }
                    }

                    float dx = objN.x - objI->x;
                    float dy = objN.y - objI->y;
                    if (std::sqrt(dx * dx + dy * dy) < stackDistance) {
                        objN.stackCount = objI->stackCount + 1;
                        objI = &objN;
                    }
                }
            } else {
                while (--n >= 0) {
                    HitCircle& objN = circles[n];
                    if (objN.isSpinner) continue;
                    if (objI->time - objN.time > stackThreshold) break;

                    Vec2 endPos = objN.getEndPosition();
                    float dx = endPos.x - objI->x;
                    float dy = endPos.y - objI->y;
                    if (std::sqrt(dx * dx + dy * dy) < stackDistance) {
                        objN.stackCount = objI->stackCount + 1;
                        objI = &objN;
                    }
                }
            }
        }

        for (auto& c : circles) {
            if (c.stackCount != 0 && !c.isSpinner) {
                float shift = -static_cast<float>(c.stackCount) * stackOffset;
                c.x += shift;
                c.y += shift;
                for (auto& pt : c.sliderPath) {
                    pt.x += shift;
                    pt.y += shift;
                }
            }
        }
    }

    bool loadFromFile(const std::string& path, bool metadataOnly = false) {
        std::ifstream file(path.c_str());
        if (!file.is_open()) return false;

        filePath = path;
        folderPath = extractFolder(path);
        audioFilename = "audio.mp3";
        audioLeadIn = 0;
        previewTime = -1;
        stackLeniency = 0.7f;
        mode = 0;
        title = "Unknown";
        artist = "Unknown Artist";
        creator = "";
        version = "Normal";
        diff = BeatmapDifficulty();

        timingPoints.clear();
        customComboColors.clear();
        circles.clear();
        if (!metadataOnly) circles.reserve(1024);

        circleCount = 0;
        sliderCount = 0;
        spinnerCount = 0;

        std::string line;
        std::string section = "";
        int currentComboNum = 1;
        int currentColorIdx = 0;

        while (std::getline(file, line)) {
            if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                line = line.substr(3);
            }

            line = trim(line);
            if (line.empty() || line.rfind("//", 0) == 0) continue;

            if (line.front() == '[' && line.back() == ']') {
                section = trim(line.substr(1, line.length() - 2));
                continue;
            }

            if (section == "General") {
                size_t sep = line.find(':');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    if (key == "AudioFilename")  audioFilename = val;
                    else if (key == "AudioLeadIn")   audioLeadIn = std::atoi(val.c_str());
                    else if (key == "PreviewTime")   previewTime = std::atoi(val.c_str());
                    else if (key == "StackLeniency") stackLeniency = std::strtof(val.c_str(), nullptr);
                    else if (key == "Mode")          mode = std::atoi(val.c_str());
                }
            } else if (section == "Metadata") {
                size_t sep = line.find(':');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    if (key == "Title" && !val.empty())   title = val;
                    else if (key == "Artist" && !val.empty())  artist = val;
                    else if (key == "Creator" && !val.empty()) creator = val;
                    else if (key == "Version" && !val.empty()) version = val;
                }
            } else if (section == "Difficulty") {
                size_t sep = line.find(':');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    if (key == "HPDrainRate")       diff.hp = std::strtof(val.c_str(), nullptr);
                    else if (key == "CircleSize")        diff.cs = std::strtof(val.c_str(), nullptr);
                    else if (key == "OverallDifficulty") diff.od = std::strtof(val.c_str(), nullptr);
                    else if (key == "ApproachRate") {
                        diff.ar = std::strtof(val.c_str(), nullptr);
                        diff.hasExplicitAR = true;
                    }
                    else if (key == "SliderMultiplier")  diff.sliderMultiplier = std::strtof(val.c_str(), nullptr);
                    else if (key == "SliderTickRate")    diff.sliderTickRate = std::strtof(val.c_str(), nullptr);
                }
            } else if (section == "Colours") {
                if (metadataOnly) continue;
                size_t sep = line.find(':');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    if (key.rfind("Combo", 0) == 0) {
                        std::vector<std::string> rgb = split(val, ',');
                        if (rgb.size() >= 3) {
                            customComboColors.push_back(RGBColor(
                                static_cast<uint8_t>(std::atoi(rgb[0].c_str())),
                                static_cast<uint8_t>(std::atoi(rgb[1].c_str())),
                                static_cast<uint8_t>(std::atoi(rgb[2].c_str()))
                            ));
                        }
                    }
                }
            } else if (section == "TimingPoints") {
                if (metadataOnly) continue;
                std::vector<std::string> tokens = split(line, ',');
                if (tokens.size() >= 2) {
                    TimingPoint tp;
                    tp.time = std::strtod(tokens[0].c_str(), nullptr);
                    tp.beatLength = std::strtof(tokens[1].c_str(), nullptr);
                    if (tokens.size() >= 7) {
                        tp.uninherited = (std::atoi(tokens[6].c_str()) != 0);
                    } else {
                        tp.uninherited = (tp.beatLength > 0.0f);
                    }
                    if (tokens.size() >= 6) {
                        tp.volume = std::atoi(tokens[5].c_str());
                    }
                    timingPoints.push_back(tp);
                }
            } else if (section == "HitObjects") {
                std::vector<std::string> tokens = split(line, ',');
                if (tokens.size() < 5) continue;

                int type = std::atoi(tokens[3].c_str());
                bool isCircle  = (type & 1) != 0;
                bool isSlider  = (type & 2) != 0;
                bool isSpinner = (type & 8) != 0;

                if (isCircle)  circleCount++;
                if (isSlider)  sliderCount++;
                if (isSpinner) spinnerCount++;

                if (metadataOnly) continue;

                if (isCircle || isSlider || isSpinner) {
                    int colorCount = customComboColors.empty() ? 4 : static_cast<int>(customComboColors.size());
                    if ((type & 4) || circles.empty() || isSpinner) {
                        currentComboNum = 1;
                        int skip = 1 + ((type >> 4) & 7);
                        currentColorIdx = (currentColorIdx + skip) % colorCount;
                    } else {
                        currentComboNum++;
                    }

                    HitCircle c;
                    c.x = std::strtof(tokens[0].c_str(), nullptr);
                    c.y = std::strtof(tokens[1].c_str(), nullptr);
                    c.time = std::atoi(tokens[2].c_str());
                    c.endTime = c.time;
                    c.hitSound = std::atoi(tokens[4].c_str());
                    c.comboNumber = currentComboNum;
                    c.colorIndex = currentColorIdx;
                    c.state = NOT_HIT;
                    c.isSlider = isSlider;
                    c.isSpinner = isSpinner;

                    if (isSpinner && tokens.size() >= 6) {
                        c.x = 256.0f;
                        c.y = 192.0f;
                        c.endTime = std::max(c.time + 100, std::atoi(tokens[5].c_str()));
                        float durSec = static_cast<float>(c.endTime - c.time) / 1000.0f;
                        // Aproximadamente 2.5 vueltas completas por segundo
                        c.spinnerRequiredRads = std::max(6.28f, durSec * 2.2f * 6.283185f);
                    } else if (isSlider && tokens.size() >= 8) {
                        std::vector<std::string> pts = split(tokens[5], '|');
                        char curveType = (!pts.empty() && !pts[0].empty()) ? pts[0][0] : 'B';

                        std::vector<Vec2> ctrlPts;
                        ctrlPts.push_back(Vec2(c.x, c.y));
                        for (size_t i = 1; i < pts.size(); ++i) {
                            std::vector<std::string> xy = split(pts[i], ':');
                            if (xy.size() == 2) {
                                ctrlPts.push_back(Vec2(
                                    std::strtof(xy[0].c_str(), nullptr),
                                    std::strtof(xy[1].c_str(), nullptr)
                                ));
                            }
                        }

                        std::vector<Vec2> rawPoly;
                        rawPoly.push_back(Vec2(c.x, c.y));

                        if (curveType == 'L') {
                            for (size_t i = 1; i < ctrlPts.size(); ++i) {
                                rawPoly.push_back(ctrlPts[i]);
                            }
                        } else if (curveType == 'P' && ctrlPts.size() == 3) {
                            appendCircularArc(ctrlPts[0], ctrlPts[1], ctrlPts[2], rawPoly);
                        } else if (curveType == 'C' && ctrlPts.size() >= 2) {
                            for (size_t i = 0; i + 1 < ctrlPts.size(); ++i) {
                                const Vec2& p0 = (i > 0) ? ctrlPts[i - 1] : ctrlPts[i];
                                const Vec2& p1 = ctrlPts[i];
                                const Vec2& p2 = ctrlPts[i + 1];
                                const Vec2& p3 = (i + 2 < ctrlPts.size()) ? ctrlPts[i + 2] : p2;
                                for (int s = 1; s <= 10; ++s) {
                                    rawPoly.push_back(evalCatmull(p0, p1, p2, p3, s / 10.0f));
                                }
                            }
                        } else {
                            // Curva Bézier (o fallback para P con != 3 puntos)
                            std::vector<Vec2> sub;
                            for (size_t i = 0; i < ctrlPts.size(); ++i) {
                                if (!sub.empty() && sub.back().x == ctrlPts[i].x && sub.back().y == ctrlPts[i].y) {
                                    if (sub.size() == 2) {
                                        rawPoly.push_back(sub.back());
                                    } else if (sub.size() > 2) {
                                        int steps = std::min(16, static_cast<int>(sub.size()) * 4);
                                        for (int s = 1; s <= steps; ++s) {
                                            rawPoly.push_back(evalBezier(sub, static_cast<float>(s) / steps));
                                        }
                                    }
                                    sub.clear();
                                }
                                sub.push_back(ctrlPts[i]);
                            }
                            if (sub.size() == 2) {
                                rawPoly.push_back(sub.back());
                            } else if (sub.size() > 2) {
                                int steps = std::min(16, static_cast<int>(sub.size()) * 4);
                                for (int s = 1; s <= steps; ++s) {
                                    rawPoly.push_back(evalBezier(sub, static_cast<float>(s) / steps));
                                }
                            }
                        }

                        c.repeats = std::max(1, std::atoi(tokens[6].c_str()));
                        c.pixelLength = std::strtof(tokens[7].c_str(), nullptr);
                        c.sliderPath = resampleToLength(rawPoly, c.pixelLength);

                        // Edge Hitsounds opcionales (tokens[8])
                        if (tokens.size() >= 9 && !tokens[8].empty()) {
                            std::vector<std::string> edges = split(tokens[8], '|');
                            for (const auto& e : edges) {
                                c.edgeHitSounds.push_back(std::atoi(e.c_str()));
                            }
                        }

                        float beatLen = 333.33f;
                        float svMult = 1.0f;
                        getTimingAt(c.time, beatLen, svMult);

                        float slideDuration = (c.pixelLength / (100.0f * diff.sliderMultiplier * svMult)) * beatLen;
                        c.endTime = c.time + static_cast<int>(slideDuration * c.repeats);

                        // Generar Slider Ticks intermedios según SliderTickRate
                        float tickIntervalMs = (diff.sliderTickRate > 0.1f) ? (beatLen / diff.sliderTickRate) : beatLen;
                        if (tickIntervalMs >= 40.0f && slideDuration > tickIntervalMs * 1.15f) {
                            for (int rep = 0; rep < c.repeats; ++rep) {
                                int spanStart = c.time + static_cast<int>(rep * slideDuration);
                                int spanEnd   = c.time + static_cast<int>((rep + 1) * slideDuration);
                                for (float tOffset = tickIntervalMs; tOffset < slideDuration - 18.0f; tOffset += tickIntervalMs) {
                                    int tickTime = (rep % 2 == 0)
                                        ? (spanStart + static_cast<int>(tOffset))
                                        : (spanEnd - static_cast<int>(tOffset));
                                    Vec2 pos = c.getSliderBallPos(tickTime);
                                    c.sliderTicks.push_back(SliderTick(pos.x, pos.y, tickTime));
                                }
                            }
                        }
                    }
                    circles.push_back(c);
                }
            }
        }

        diff.calculateDerivedValues();
        fullAudioPath = resolveAudioPath(folderPath, audioFilename);

        if (!metadataOnly) {
            applyStacking();
            return !circles.empty();
        }
        return (circleCount + sliderCount + spinnerCount) > 0;
    }

    // Aplica modificadores de juego (EZ = 2, HR = 8) sobre la dificultad y geometría
    void applyMods(uint32_t mods) {
        if (mods & (1u << 1)) { // MOD_EZ
            diff.cs *= 0.5f;
            diff.ar *= 0.5f;
            diff.od *= 0.5f;
            diff.hp *= 0.5f;
            diff.calculateDerivedValues();
        }
        if (mods & (1u << 3)) { // MOD_HR
            diff.cs = std::min(10.0f, diff.cs * 1.3f);
            diff.ar = std::min(10.0f, diff.ar * 1.4f);
            diff.od = std::min(10.0f, diff.od * 1.4f);
            diff.hp = std::min(10.0f, diff.hp * 1.4f);
            diff.calculateDerivedValues();

            for (auto& c : circles) {
                if (!c.isSpinner) {
                    c.y = 384.0f - c.y;
                    for (auto& pt : c.sliderPath) {
                        pt.y = 384.0f - pt.y;
                    }
                    for (auto& tk : c.sliderTicks) {
                        tk.y = 384.0f - tk.y;
                    }
                }
            }
        }
    }
};