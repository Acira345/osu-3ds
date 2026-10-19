#pragma once
#include <3ds.h>
#include <string>
#include <vector>
#include <cstdio>
#include <functional>
#include "OszExtractor.hpp"

namespace DirectDownloader {

// Helper para parsear un ID numérico desde texto o URL
inline u32 parseSetId(const std::string& input) {
    if (input.empty()) return 0;
    // Buscar "/s/" o "/beatmapsets/"
    size_t pos = input.find("/beatmapsets/");
    if (pos != std::string::npos) {
        pos += 13;
        u32 id = 0;
        while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') {
            id = id * 10 + (input[pos] - '0');
            pos++;
        }
        return id;
    }
    pos = input.find("/s/");
    if (pos != std::string::npos) {
        pos += 3;
        u32 id = 0;
        while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') {
            id = id * 10 + (input[pos] - '0');
            pos++;
        }
        return id;
    }
    // O comprobar si es puramente numérico
    u32 id = 0;
    for (char c : input) {
        if (c >= '0' && c <= '9') {
            id = id * 10 + (c - '0');
        } else if (c != ' ' && c != '#') {
            return 0; // Contiene letras no reconocidas
        }
    }
    return id;
}

// Descarga directa a la tarjeta SD usando httpc con seguimiento de progreso
inline bool downloadBeatmapSet(u32 setId,
                               std::string& statusOut,
                               std::function<void(float ratio, size_t received, size_t total)> onProgress) {
    if (setId == 0) {
        statusOut = "ID de Beatmap inválido";
        return false;
    }

    Result res = httpcInit(0);
    if (R_FAILED(res)) {
        statusOut = "Error al iniciar servicio HTTP (3DS)";
        return false;
    }

    std::string currentUrl = "https://osu.direct/api/d/" + std::to_string(setId);
    httpcContext context;
    u32 statusCode = 0;
    int maxRedirects = 4;

    while (maxRedirects-- > 0) {
        res = httpcOpenContext(&context, HTTPC_METHOD_GET, currentUrl.c_str(), 1);
        if (R_FAILED(res)) {
            httpcExit();
            statusOut = "Error al conectar con el servidor";
            return false;
        }

        httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
        httpcAddRequestHeaderField(&context, "User-Agent", "Mozilla/5.0 (Nintendo 3DS)");
        httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");

        res = httpcBeginRequest(&context);
        if (R_FAILED(res)) {
            httpcCloseContext(&context);
            httpcExit();
            statusOut = "Servidor requiere SSL moderno (Cloudflare). Pega el link en la web o QR";
            return false;
        }

        statusCode = 0;
        res = httpcGetResponseStatusCode(&context, &statusCode);
        if (R_FAILED(res)) {
            httpcCloseContext(&context);
            httpcExit();
            statusOut = "Servidor requiere SSL moderno (Cloudflare). Pega el link en la web o QR";
            return false;
        }

        if ((statusCode >= 301 && statusCode <= 303) || (statusCode >= 307 && statusCode <= 308)) {
            char newUrlBuf[1024] = "";
            res = httpcGetResponseHeader(&context, "Location", newUrlBuf, sizeof(newUrlBuf));
            httpcCloseContext(&context);
            if (R_SUCCEEDED(res) && newUrlBuf[0] != '\0') {
                currentUrl = newUrlBuf;
                continue;
            }
            httpcExit();
            statusOut = "Fallo al seguir redireccion del servidor";
            return false;
        }
        break;
    }

    if (statusCode != 200) {
        httpcCloseContext(&context);
        httpcExit();
        char errBuf[64];
        std::snprintf(errBuf, sizeof(errBuf), "Servidor respondio HTTP %lu", (unsigned long)statusCode);
        statusOut = errBuf;
        return false;
    }

    u32 contentSize = 0;
    httpcGetDownloadSizeState(&context, NULL, &contentSize);

    char outOszPath[128];
    std::snprintf(outOszPath, sizeof(outOszPath), "/osu/songs/%lu_direct.osz", (unsigned long)setId);

    FILE* fp = std::fopen(outOszPath, "wb");
    if (!fp) {
        httpcCloseContext(&context);
        httpcExit();
        statusOut = "Error al crear archivo en SD";
        return false;
    }

    std::vector<u8> chunk(16384);
    u32 totalDownloaded = 0;

    while (true) {
        u32 bytesRead = 0;
        res = httpcDownloadData(&context, chunk.data(), chunk.size(), &bytesRead);
        if (bytesRead > 0) {
            std::fwrite(chunk.data(), 1, bytesRead, fp);
            totalDownloaded += bytesRead;
            if (onProgress && contentSize > 0) {
                float r = static_cast<float>(totalDownloaded) / static_cast<float>(contentSize);
                onProgress(std::min(1.0f, r), totalDownloaded, contentSize);
            }
        }
        if (res == static_cast<Result>(HTTPC_RESULTCODE_DOWNLOADPENDING)) {
            continue;
        }
        break;
    }

    std::fclose(fp);
    httpcCloseContext(&context);
    httpcExit();

    if (totalDownloaded < 1024) {
        std::remove(outOszPath);
        statusOut = "Archivo descargado vacio o corrupto";
        return false;
    }

    // Descomprimir automáticamente en /osu/songs/
    int extracted = OszExtractor::extractAllPendingOsz("/osu/songs", "/osu/songs");
    if (extracted > 0) {
        char succBuf[64];
        std::snprintf(succBuf, sizeof(succBuf), "✓ Cancion #%lu instalada con exito!", (unsigned long)setId);
        statusOut = succBuf;
        return true;
    } else {
        statusOut = "Descargado, pero fallo la descompresion";
        return false;
    }
}

} // namespace DirectDownloader
