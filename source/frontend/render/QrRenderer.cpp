#include "QrRenderer.hpp"
#include <citro2d.h>
#include <cmath>
#include "qrcodegen.h"

void renderQrCode(const char* text, float startX, float startY, float maxBoxSize) {
    if (!text || text[0] == '\0') return;
    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW, 1, 10, qrcodegen_Mask_AUTO, true)) {
        return;
    }
    int qrSize = qrcodegen_getSize(qrcode);
    int border = 2;
    int totalModules = qrSize + border * 2;
    float modSize = std::floor(maxBoxSize / static_cast<float>(totalModules));
    if (modSize < 1.0f) modSize = 1.0f;
    float actualSize = modSize * static_cast<float>(totalModules);
    float ox = startX + std::floor((maxBoxSize - actualSize) * 0.5f);
    float oy = startY + std::floor((maxBoxSize - actualSize) * 0.5f);

    // Fondo blanco del QR
    C2D_DrawRectSolid(ox, oy, 0.28f, actualSize, actualSize, C2D_Color32(255, 255, 255, 255));

    u32 clrBlack = C2D_Color32(18, 16, 26, 255);
    for (int y = 0; y < qrSize; ++y) {
        for (int x = 0; x < qrSize; ++x) {
            if (qrcodegen_getModule(qrcode, x, y)) {
                C2D_DrawRectSolid(ox + (x + border) * modSize,
                                  oy + (y + border) * modSize,
                                  0.30f, modSize, modSize, clrBlack);
            }
        }
    }
}

void renderWifiBars(float x, float y, u8 strength, u32 clrActive, u32 clrInactive) {
    for (int i = 0; i < 3; ++i) {
        float barH = 5.0f + i * 4.0f;
        float barY = y + (13.0f - barH);
        u32 c = (strength > i) ? clrActive : clrInactive;
        C2D_DrawRectSolid(x + i * 6.0f, barY, 0.35f, 4.0f, barH, c);
    }
}
