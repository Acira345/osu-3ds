#pragma once
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_SIMD
#include "stb_image.h"

struct LoadedTexture {
    C3D_Tex tex;
    Tex3DS_SubTexture subtex;
    C2D_Image image;
    bool loaded = false;
    std::string sourcePath = "";
    int origW = 0;
    int origH = 0;
    bool is2x = false;

    float getNativeW() const {
        if (!loaded || origW <= 0) return 0.0f;
        return is2x ? (static_cast<float>(origW) * 0.5f) : static_cast<float>(origW);
    }

    float getNativeH() const {
        if (!loaded || origH <= 0) return 0.0f;
        return is2x ? (static_cast<float>(origH) * 0.5f) : static_cast<float>(origH);
    }

    static u32 nextPow2(u32 v) {
        u32 p = 8;
        while (p < v && p < 1024) p <<= 1;
        return p;
    }

    void unload() {
        if (loaded) {
            C3D_TexDelete(&tex);
            loaded = false;
            sourcePath = "";
            origW = 0;
            origH = 0;
            is2x = false;
        }
    }

    // Resultado de decodificacion en CPU (ya en formato Morton de la PICA200), listo para subir a GPU
    struct Decoded {
        u32* data = nullptr;
        u32 texW = 0, texH = 0, dstW = 0, dstH = 0;
        int origW = 0, origH = 0;
        void release() { if (data) { std::free(data); data = nullptr; } }
    };

    // Decodifica + escala + swizzle. Solo CPU/heap: seguro para ejecutarse en un hilo secundario.
    static bool decode(const std::string& path, int maxW, int maxH, Decoded& out) {
        out = Decoded();
        int srcW = 0, srcH = 0, channels = 0;
        unsigned char* pixels = stbi_load(path.c_str(), &srcW, &srcH, &channels, 4);
        if (!pixels || srcW <= 0 || srcH <= 0) {
            if (pixels) stbi_image_free(pixels);
            return false;
        }

        // Escalar proporcionalmente para ajustarse a los límites de memoria y GPU de la 3DS
        float scale = 1.0f;
        if (srcW > maxW || srcH > maxH) {
            float sx = static_cast<float>(maxW) / static_cast<float>(srcW);
            float sy = static_cast<float>(maxH) / static_cast<float>(srcH);
            scale = std::min(sx, sy);
        }

        u32 dstW = std::max<u32>(1, std::min<u32>(512, static_cast<u32>(std::round(srcW * scale))));
        u32 dstH = std::max<u32>(1, std::min<u32>(512, static_cast<u32>(std::round(srcH * scale))));
        u32 texW = nextPow2(dstW);
        u32 texH = nextPow2(dstH);

        u32* outBuf = static_cast<u32*>(std::calloc(texW * texH, sizeof(u32)));
        if (!outBuf) {
            stbi_image_free(pixels);
            return false;
        }

        // Tabla de entrelazado Morton para tiles de 8x8 de la GPU PICA200
        static const u8 mortonLUT[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
        u32 tilesPerRow = texW >> 3;

        for (u32 dy = 0; dy < dstH; ++dy) {
            u32 sy = std::min(static_cast<u32>(srcH - 1), (dy * static_cast<u32>(srcH)) / dstH);
            const unsigned char* srcRow = pixels + (sy * srcW * 4);

            // En PICA200 (igual que tex3ds), memY = 0 corresponde a subtex.top = 1.0f
            u32 memY = dy;
            u32 tileY = memY >> 3;
            u32 inTileY = mortonLUT[memY & 7] << 1;

            for (u32 dx = 0; dx < dstW; ++dx) {
                u32 sx = std::min(static_cast<u32>(srcW - 1), (dx * static_cast<u32>(srcW)) / dstW);
                const unsigned char* p = srcRow + (sx * 4);

                // Formato GPU_RGBA8 en PICA200: [A, B, G, R] en little-endian = (r<<24)|(g<<16)|(b<<8)|a
                u32 rgba8 = (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
                            (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]);

                u32 tileX = dx >> 3;
                u32 inTileX = mortonLUT[dx & 7];
                u32 tileIndex = (tileY * tilesPerRow) + tileX;
                outBuf[(tileIndex << 6) | inTileY | inTileX] = rgba8;
            }
        }

        stbi_image_free(pixels);
        out.data = outBuf;
        out.texW = texW; out.texH = texH;
        out.dstW = dstW; out.dstH = dstH;
        out.origW = srcW; out.origH = srcH;
        return true;
    }

    // Sube un resultado decodificado a la GPU (rapido: solo memcpy). Debe llamarse en el hilo principal.
    bool upload(const Decoded& d, const std::string& path) {
        if (!d.data) return false;
        unload();
        if (!C3D_TexInit(&tex, d.texW, d.texH, GPU_RGBA8)) return false;
        C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        std::memcpy(tex.data, d.data, d.texW * d.texH * sizeof(u32));
        GSPGPU_FlushDataCache(tex.data, d.texW * d.texH * sizeof(u32));

        subtex.width  = static_cast<u16>(d.dstW);
        subtex.height = static_cast<u16>(d.dstH);
        subtex.left   = 0.0f;
        subtex.top    = 1.0f;
        subtex.right  = static_cast<float>(d.dstW) / static_cast<float>(d.texW);
        subtex.bottom = 1.0f - (static_cast<float>(d.dstH) / static_cast<float>(d.texH));

        image.tex    = &tex;
        image.subtex = &subtex;
        loaded       = true;
        sourcePath   = path;
        origW        = d.origW;
        origH        = d.origH;
        std::string pLow = path;
        for (char& c : pLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        is2x         = (pLow.find("@2x") != std::string::npos);
        return true;
    }

    // Carga sincrona (usada para sprites de skin pequeños)
    bool loadFromFile(const std::string& path, int maxW = 256, int maxH = 192) {
        if (path.empty()) return false;
        if (loaded && sourcePath == path) return true;
        Decoded d;
        if (!decode(path, maxW, maxH, d)) { unload(); return false; }
        bool ok = upload(d, path);
        d.release();
        return ok;
    }

    // Dibuja la imagen cubriendo el rectángulo (x, y, w, h) con una opacidad/tinte
    void drawRect(float x, float y, float w, float h, float z, u8 alpha = 255) const {
        if (!loaded || subtex.width == 0 || subtex.height == 0) return;
        float sx = w / static_cast<float>(subtex.width);
        float sy = h / static_cast<float>(subtex.height);

        if (alpha >= 254) {
            C2D_DrawImageAt(image, x, y, z, nullptr, sx, sy);
        } else {
            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, static_cast<float>(alpha) / 255.0f);
            C2D_DrawImageAt(image, x, y, z, &tint, sx, sy);
        }
    }

    // Dibuja un sprite centrado en (cx, cy) con diámetro (size x size) y color/tinte multiplicativo
    void drawCenteredSprite(float cx, float cy, float size, float z, u32 color, float blend = 0.65f) const {
        if (!loaded || subtex.width == 0 || subtex.height == 0) return;
        float sx = size / static_cast<float>(subtex.width);
        float sy = size / static_cast<float>(subtex.height);

        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, color, blend);
        C2D_DrawImageAt(image, cx - size * 0.5f, cy - size * 0.5f, z, &tint, sx, sy);
    }

    // Dibuja un sprite centrado en (cx, cy) con diametro (size x size), rotacion (radianes) y color/tinte multiplicativo
    void drawCenteredSpriteRotated(float cx, float cy, float size, float z, float angleRad, u32 color = 0xFFFFFFFF, float blend = 0.0f) const {
        if (!loaded || subtex.width == 0 || subtex.height == 0) return;
        float sx = size / static_cast<float>(subtex.width);
        float sy = size / static_cast<float>(subtex.height);

        C2D_ImageTint tint;
        if (blend <= 0.001f) {
            u8 a = static_cast<u8>((color >> 24) & 0xFF);
            if (a >= 254 && color == 0xFFFFFFFF) {
                C2D_DrawImageAtRotated(image, cx, cy, z, angleRad, nullptr, sx, sy);
                return;
            }
            C2D_AlphaImageTint(&tint, static_cast<float>(a) / 255.0f);
        } else {
            C2D_PlainImageTint(&tint, color, blend);
        }
        C2D_DrawImageAtRotated(image, cx, cy, z, angleRad, &tint, sx, sy);
    }

    // Dibuja la imagen centrada en (cx, cy) con tamaño (w, h)
    void drawCentered(float cx, float cy, float w, float h, float z, u8 alpha = 255) const {
        if (!loaded || subtex.width == 0 || subtex.height == 0) return;
        float x = cx - w * 0.5f;
        float y = cy - h * 0.5f;
        drawRect(x, y, w, h, z, alpha);
    }
};

// =============================================================================
// Cargador asincrono de fondos: decodifica JPG/PNG en un hilo secundario para que
// el selector de canciones no se congele (~1s por imagen en el ARM11 de la 3DS).
// Siempre gana la peticion mas reciente: si el jugador pasa rapido de cancion,
// las imagenes intermedias se descartan.
// =============================================================================
class AsyncTextureLoader {
private:
    Thread worker = nullptr;
    LightLock lock;
    LightEvent wake;
    volatile bool quit = false;

    // Peticion pendiente (protegida por lock)
    std::string reqPath;
    int reqW = 256, reqH = 192;
    u32 reqId = 0;

    // Resultado listo (protegido por lock)
    LoadedTexture::Decoded result;
    std::string resultPath;
    u32 resultId = 0;
    bool resultReady = false;
    bool resultFailed = false;

    static void threadMain(void* arg) {
        static_cast<AsyncTextureLoader*>(arg)->run();
    }

    void run() {
        while (!quit) {
            LightEvent_Wait(&wake);
            if (quit) break;

            LightLock_Lock(&lock);
            std::string path = reqPath;
            int w = reqW, h = reqH;
            u32 id = reqId;
            LightLock_Unlock(&lock);

            if (path.empty()) continue;

            LoadedTexture::Decoded d;
            bool ok = LoadedTexture::decode(path, w, h, d);

            LightLock_Lock(&lock);
            if (id == reqId) {
                // Sigue siendo la peticion vigente: publicar
                result.release();
                result = d;
                resultPath = path;
                resultId = id;
                resultReady = ok;
                resultFailed = !ok;
            } else {
                // Llego una peticion mas nueva mientras decodificabamos: descartar
                d.release();
                LightEvent_Signal(&wake);
            }
            LightLock_Unlock(&lock);
        }
    }

public:
    bool start(bool isNew3DS = false) {
        LightLock_Init(&lock);
        LightEvent_Init(&wake, RESET_ONESHOT);
        quit = false;

        s32 prio = 0x30;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        s32 workerPrio = std::min<s32>(0x3F, prio + 1);

        APT_SetAppCpuTimeLimit(isNew3DS ? 70 : 30);
        if (isNew3DS) {
            // En New 3DS / New 2DS XL el núcleo 2 es exclusivo para hilos secundarios de la app a 804 MHz
            worker = threadCreate(threadMain, this, 64 * 1024, workerPrio, 2, false);
        }
        if (!worker) worker = threadCreate(threadMain, this, 64 * 1024, workerPrio, 1, false);
        if (!worker) worker = threadCreate(threadMain, this, 64 * 1024, workerPrio, -2, false);
        return worker != nullptr;
    }

    void stop() {
        if (!worker) return;
        quit = true;
        LightEvent_Signal(&wake);
        threadJoin(worker, U64_MAX);
        threadFree(worker);
        worker = nullptr;
        result.release();
    }

    // Solicita cargar una imagen (no bloquea)
    void request(const std::string& path, int maxW, int maxH) {
        LightLock_Lock(&lock);
        if (path != reqPath || resultFailed) {
            reqPath = path;
            reqW = maxW;
            reqH = maxH;
            reqId++;
            resultReady = false;
            resultFailed = false;
            result.release();
            LightEvent_Signal(&wake);
        }
        LightLock_Unlock(&lock);
    }

    // Cancela cualquier peticion pendiente
    void cancel() {
        LightLock_Lock(&lock);
        reqPath.clear();
        reqId++;
        resultReady = false;
        result.release();
        LightLock_Unlock(&lock);
    }

    bool isPending() {
        LightLock_Lock(&lock);
        bool p = !reqPath.empty() && !resultReady && !resultFailed;
        LightLock_Unlock(&lock);
        return p;
    }

    // Llamar cada frame en el hilo principal. Devuelve true si se subio una textura nueva.
    bool poll(LoadedTexture& dst) {
        if (!worker) return false;
        bool uploaded = false;
        LightLock_Lock(&lock);
        if (resultReady && resultId == reqId) {
            if (!(dst.loaded && dst.sourcePath == resultPath)) {
                uploaded = dst.upload(result, resultPath);
            }
            result.release();
            resultReady = false;
        }
        LightLock_Unlock(&lock);
        return uploaded;
    }
};
