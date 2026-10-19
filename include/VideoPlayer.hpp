#pragma once
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "TextureLoader.hpp"

// =============================================================================
// Reproductor de Video de Fondo (.3dsv / .mjpg) para Nintendo 3DS
// - Decodifica fotogramas JPEG (256x128 @ 15-20 FPS) en un hilo secundario de
//   baja prioridad (Core 1 / tiempo libre de VBlank).
// - Mantiene una unica textura persistente en VRAM (256x128 GPU_RGBA8 = 128 KB)
//   y una tabla Morton precalculada para subir fotogramas en < 0.1 ms sin
//   afectar los 60 FPS del gameplay.
// - Sincronizado por tiempo absoluto con el reloj de audio: si la consola (ej.
//   Old 3DS) tarda mas en un fotograma, salta automaticamente al fotograma
//   exacto de la musica sin acumular retraso.
// =============================================================================
class VideoPlayer {
public:
    static constexpr u32 VIDEO_TEX_W = 256;
    static constexpr u32 VIDEO_TEX_H = 128;
    static constexpr u32 VIDEO_PIXELS = VIDEO_TEX_W * VIDEO_TEX_H;
    static constexpr size_t VIDEO_BYTES = VIDEO_PIXELS * sizeof(u32);

    struct FrameEntry {
        uint32_t offset;
        uint32_t size;
    };

private:
    C3D_Tex tex;
    Tex3DS_SubTexture subtex;
    C2D_Image image;
    bool texAllocated = false;

    uint16_t mortonMap[VIDEO_PIXELS];
    u32* decodeBuf = nullptr;
    u32* readyBuf = nullptr;

    Thread worker = nullptr;
    LightLock lock;
    LightEvent wake;
    volatile bool quitThread = false;

    // Estado del archivo abierto (protegido por lock cuando cambia de video)
    FILE* videoFile = nullptr;
    std::string currentPath = "";
    std::vector<FrameEntry> frames;
    uint16_t fps100 = 1500; // 15.00 FPS por defecto
    int startOffsetMs = 0;
    uint32_t fileGeneration = 0;

    // Peticion de fotograma hacia el hilo secundario
    int reqFrameIdx = -1;
    uint32_t reqGen = 0;

    // Fotograma decodificado listo para subir a VRAM
    bool frameReady = false;
    int readyFrameIdx = -1;
    uint32_t readyGen = 0;

    // Estado en el hilo principal
    int displayedFrameIdx = -1;
    int lastRequestedFrameIdx = -1;
    bool hasValidFrameInTex = false;
    bool withinVideoTimeline = false;

    static uint16_t readU16LE(const uint8_t* p) {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }

    static uint32_t readU32LE(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    void buildMortonTable() {
        static const u8 mortonLUT[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
        const u32 tilesPerRow = VIDEO_TEX_W >> 3;
        for (u32 dy = 0; dy < VIDEO_TEX_H; ++dy) {
            u32 tileY = dy >> 3;
            u32 inTileY = static_cast<u32>(mortonLUT[dy & 7]) << 1;
            for (u32 dx = 0; dx < VIDEO_TEX_W; ++dx) {
                u32 tileX = dx >> 3;
                u32 inTileX = mortonLUT[dx & 7];
                u32 tileIndex = (tileY * tilesPerRow) + tileX;
                mortonMap[dy * VIDEO_TEX_W + dx] = static_cast<uint16_t>((tileIndex << 6) | inTileY | inTileX);
            }
        }
    }

    static void threadMain(void* arg) {
        static_cast<VideoPlayer*>(arg)->workerLoop();
    }

    void workerLoop() {
        std::vector<uint8_t> jpegBuf;
        jpegBuf.reserve(32768);

        while (!quitThread) {
            LightEvent_Wait(&wake);
            if (quitThread) break;

            while (!quitThread) {
                LightLock_Lock(&lock);
                int fIdx = reqFrameIdx;
                uint32_t gen = reqGen;
                FILE* fp = videoFile;
                FrameEntry entry = { 0, 0 };
                bool valid = (fp != nullptr && gen == fileGeneration &&
                              fIdx >= 0 && fIdx < static_cast<int>(frames.size()));
                if (valid) {
                    entry = frames[fIdx];
                    reqFrameIdx = -1; // Consumir peticion
                }
                LightLock_Unlock(&lock);

                if (!valid || entry.size == 0 || entry.size > 512 * 1024) break;

                if (jpegBuf.size() < entry.size) {
                    jpegBuf.resize(entry.size);
                }

                // Leer el bloque JPEG del archivo
                LightLock_Lock(&lock);
                bool readOk = false;
                if (videoFile == fp && gen == fileGeneration) {
                    if (std::fseek(fp, static_cast<long>(entry.offset), SEEK_SET) == 0) {
                        readOk = (std::fread(jpegBuf.data(), 1, entry.size, fp) == entry.size);
                    }
                }
                LightLock_Unlock(&lock);

                if (!readOk) break;

                // Decodificar JPEG en CPU (hilo secundario)
                int srcW = 0, srcH = 0, comp = 0;
                unsigned char* pixels = stbi_load_from_memory(
                    jpegBuf.data(), static_cast<int>(entry.size), &srcW, &srcH, &comp, 4
                );
                if (!pixels || srcW <= 0 || srcH <= 0) {
                    if (pixels) stbi_image_free(pixels);
                    break;
                }

                // Convertir a formato Morton de la PICA200
                if (static_cast<u32>(srcW) == VIDEO_TEX_W && static_cast<u32>(srcH) == VIDEO_TEX_H) {
                    // Ruta ultrarrapida 1:1 (256x128 exacto, sin reescalado)
                    const unsigned char* p = pixels;
                    for (u32 i = 0; i < VIDEO_PIXELS; ++i, p += 4) {
                        decodeBuf[mortonMap[i]] =
                            (static_cast<u32>(p[0]) << 24) |
                            (static_cast<u32>(p[1]) << 16) |
                            (static_cast<u32>(p[2]) << 8)  | 0xFFu;
                    }
                } else {
                    // Ruta con reescalado automatico si el .mjpg tiene otra resolucion
                    for (u32 dy = 0; dy < VIDEO_TEX_H; ++dy) {
                        u32 sy = (dy * static_cast<u32>(srcH)) / VIDEO_TEX_H;
                        const unsigned char* srcRow = pixels + (sy * srcW * 4);
                        const uint16_t* mapRow = mortonMap + (dy * VIDEO_TEX_W);
                        for (u32 dx = 0; dx < VIDEO_TEX_W; ++dx) {
                            u32 sx = (dx * static_cast<u32>(srcW)) / VIDEO_TEX_W;
                            const unsigned char* p = srcRow + (sx * 4);
                            decodeBuf[mapRow[dx]] =
                                (static_cast<u32>(p[0]) << 24) |
                                (static_cast<u32>(p[1]) << 16) |
                                (static_cast<u32>(p[2]) << 8)  | 0xFFu;
                        }
                    }
                }
                stbi_image_free(pixels);

                // Publicar el buffer listo intercambiando punteros en O(1)
                LightLock_Lock(&lock);
                if (gen == fileGeneration) {
                    std::swap(decodeBuf, readyBuf);
                    readyFrameIdx = fIdx;
                    readyGen = gen;
                    frameReady = true;
                }
                bool morePending = (reqFrameIdx >= 0 && reqGen == fileGeneration);
                LightLock_Unlock(&lock);

                if (!morePending) break;
            }
        }
    }

    // Indexa un archivo .3dsv (cabecera indexada instantanea) o un flujo .mjpg (escaneo JPEG)
    bool parseVideoContainer(FILE* fp) {
        frames.clear();
        fps100 = 1500;

        std::fseek(fp, 0, SEEK_END);
        long fileLen = std::ftell(fp);
        if (fileLen < 32) return false;
        std::fseek(fp, 0, SEEK_SET);

        uint8_t hdr[16];
        if (std::fread(hdr, 1, 16, fp) != 16) return false;

        // Formato 1: Contenedor indexado nativo "3DSV"
        if (hdr[0] == '3' && hdr[1] == 'D' && hdr[2] == 'S' && hdr[3] == 'V') {
            uint16_t f100 = readU16LE(&hdr[8]);
            uint32_t count = readU32LE(&hdr[12]);
            if (f100 >= 100 && f100 <= 6000) fps100 = f100;
            if (count == 0 || count > 36000) return false;

            size_t tableBytes = static_cast<size_t>(count) * 8;
            if (16 + static_cast<long>(tableBytes) > fileLen) return false;

            std::vector<uint8_t> rawTable(tableBytes);
            if (std::fread(rawTable.data(), 1, tableBytes, fp) != tableBytes) return false;

            frames.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t off = readU32LE(&rawTable[i * 8]);
                uint32_t sz  = readU32LE(&rawTable[i * 8 + 4]);
                if (off + sz <= static_cast<uint32_t>(fileLen) && sz > 16) {
                    frames.push_back({ off, sz });
                }
            }
            return !frames.empty();
        }

        // Formato 2: Flujo MJPEG crudo (.mjpg / .mjpeg) — escanear marcadores SOI (FF D8 FF) y EOI (FF D9)
        std::fseek(fp, 0, SEEK_SET);
        const size_t CHUNK = 65536;
        std::vector<uint8_t> buf(CHUNK);
        uint32_t filePos = 0;
        bool inFrame = false;
        uint32_t frameStart = 0;
        uint8_t prevByte = 0;

        frames.reserve(2048);
        while (filePos < static_cast<uint32_t>(fileLen)) {
            size_t n = std::fread(buf.data(), 1, CHUNK, fp);
            if (n == 0) break;

            for (size_t i = 0; i < n; ++i) {
                uint8_t b = buf[i];
                uint32_t absPos = filePos + static_cast<uint32_t>(i);
                if (!inFrame) {
                    if (prevByte == 0xFF && b == 0xD8 && absPos >= 1) {
                        inFrame = true;
                        frameStart = absPos - 1;
                    }
                } else {
                    if (prevByte == 0xFF && b == 0xD9) {
                        uint32_t frameEnd = absPos + 1;
                        uint32_t sz = frameEnd - frameStart;
                        if (sz >= 128) {
                            frames.push_back({ frameStart, sz });
                        }
                        inFrame = false;
                    }
                }
                prevByte = b;
            }
            filePos += static_cast<uint32_t>(n);
        }
        return !frames.empty();
    }

public:
    bool init() {
        buildMortonTable();
        decodeBuf = static_cast<u32*>(std::calloc(VIDEO_PIXELS, sizeof(u32)));
        readyBuf  = static_cast<u32*>(std::calloc(VIDEO_PIXELS, sizeof(u32)));
        if (!decodeBuf || !readyBuf) return false;

        if (!C3D_TexInit(&tex, VIDEO_TEX_W, VIDEO_TEX_H, GPU_RGBA8)) {
            return false;
        }
        C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        std::memset(tex.data, 0, VIDEO_BYTES);
        GSPGPU_FlushDataCache(tex.data, VIDEO_BYTES);

        subtex.width  = VIDEO_TEX_W;
        subtex.height = VIDEO_TEX_H;
        subtex.left   = 0.0f;
        subtex.top    = 1.0f;
        subtex.right  = 1.0f;
        subtex.bottom = 0.0f;

        image.tex    = &tex;
        image.subtex = &subtex;
        texAllocated = true;

        LightLock_Init(&lock);
        LightEvent_Init(&wake, RESET_ONESHOT);
        quitThread = false;

        s32 prio = 0x30;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        s32 workerPrio = std::min<s32>(0x3F, prio + 2);

        APT_SetAppCpuTimeLimit(30);
        worker = threadCreate(threadMain, this, 64 * 1024, workerPrio, 1, false);
        if (!worker) worker = threadCreate(threadMain, this, 64 * 1024, workerPrio, -2, false);
        return worker != nullptr;
    }

    void shutdown() {
        closeVideo();
        if (worker) {
            quitThread = true;
            LightEvent_Signal(&wake);
            threadJoin(worker, U64_MAX);
            threadFree(worker);
            worker = nullptr;
        }
        if (texAllocated) {
            C3D_TexDelete(&tex);
            texAllocated = false;
        }
        if (decodeBuf) { std::free(decodeBuf); decodeBuf = nullptr; }
        if (readyBuf)  { std::free(readyBuf);  readyBuf = nullptr; }
    }

    void closeVideo() {
        if (texAllocated) {
            LightLock_Lock(&lock);
            fileGeneration++;
            reqFrameIdx = -1;
            frameReady = false;
            if (videoFile) {
                std::fclose(videoFile);
                videoFile = nullptr;
            }
            frames.clear();
            currentPath.clear();
            LightLock_Unlock(&lock);
        }
        displayedFrameIdx = -1;
        lastRequestedFrameIdx = -1;
        hasValidFrameInTex = false;
        withinVideoTimeline = false;
    }

    bool openVideo(const std::string& path, int offsetMs = 0) {
        closeVideo();
        if (!texAllocated || !worker || path.empty()) return false;

        FILE* fp = std::fopen(path.c_str(), "rb");
        if (!fp) return false;

        LightLock_Lock(&lock);
        bool ok = parseVideoContainer(fp);
        if (!ok) {
            std::fclose(fp);
            LightLock_Unlock(&lock);
            return false;
        }

        videoFile = fp;
        currentPath = path;
        startOffsetMs = offsetMs;
        fileGeneration++;
        reqGen = fileGeneration;
        reqFrameIdx = 0; // Pre-cargar el primer fotograma inmediatamente
        lastRequestedFrameIdx = 0;
        frameReady = false;
        LightLock_Unlock(&lock);

        LightEvent_Signal(&wake);
        return true;
    }

    // Llamar una vez por fotograma en el hilo principal durante el gameplay
    void update(int songTimeMs, bool paused) {
        if (!texAllocated || !worker || frames.empty()) return;

        int relMs = songTimeMs - startOffsetMs;
        if (relMs < 0) {
            withinVideoTimeline = false;
        } else {
            int64_t num = static_cast<int64_t>(relMs) * static_cast<int64_t>(fps100);
            int targetIdx = static_cast<int>(num / 100000);
            if (targetIdx >= 0 && targetIdx < static_cast<int>(frames.size())) {
                withinVideoTimeline = true;
                if (!paused && targetIdx != displayedFrameIdx && targetIdx != lastRequestedFrameIdx) {
                    LightLock_Lock(&lock);
                    reqFrameIdx = targetIdx;
                    reqGen = fileGeneration;
                    lastRequestedFrameIdx = targetIdx;
                    LightLock_Unlock(&lock);
                    LightEvent_Signal(&wake);
                }
            } else {
                withinVideoTimeline = false;
            }
        }

        // Subir fotograma listo a VRAM si el hilo secundario termino (solo 128 KB memcpy)
        LightLock_Lock(&lock);
        if (frameReady && readyGen == fileGeneration) {
            std::memcpy(tex.data, readyBuf, VIDEO_BYTES);
            displayedFrameIdx = readyFrameIdx;
            hasValidFrameInTex = true;
            frameReady = false;
            LightLock_Unlock(&lock);
            GSPGPU_FlushDataCache(tex.data, VIDEO_BYTES);
        } else {
            LightLock_Unlock(&lock);
        }
    }

    bool isLoaded() const {
        return texAllocated && videoFile != nullptr && !frames.empty();
    }

    bool shouldRender() const {
        return isLoaded() && withinVideoTimeline && hasValidFrameInTex;
    }

    void drawRect(float x, float y, float w, float h, float z) const {
        if (!shouldRender()) return;
        float sx = w / static_cast<float>(VIDEO_TEX_W);
        float sy = h / static_cast<float>(VIDEO_TEX_H);
        C2D_DrawImageAt(image, x, y, z, nullptr, sx, sy);
    }
};
