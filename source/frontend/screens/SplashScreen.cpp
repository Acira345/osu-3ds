#include "SplashScreen.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

struct SplashWorkerContext {
    AppContext* ctx = nullptr;
    LightLock lock;
    volatile bool finished = false;
    volatile int songsFound = 0;
    volatile int scannedFiles = 0;
    char statusMsg[64] = "Iniciando...";
};

void onScanProgress(void* userdata, const char* statusMsg, int songsFound, int scannedFiles) {
    auto* data = static_cast<SplashWorkerContext*>(userdata);
    if (!data) return;

    LightLock_Lock(&data->lock);
    data->songsFound = songsFound;
    data->scannedFiles = scannedFiles;
    if (statusMsg) {
        std::strncpy(data->statusMsg, statusMsg, sizeof(data->statusMsg) - 1);
        data->statusMsg[sizeof(data->statusMsg) - 1] = '\0';
    }
    LightLock_Unlock(&data->lock);
}

void scanThreadEntry(void* arg) {
    auto* data = static_cast<SplashWorkerContext*>(arg);
    if (!data || !data->ctx) return;

    data->ctx->songMgr.initAndScan(&data->ctx->scoreMgr, onScanProgress, data);

    LightLock_Lock(&data->lock);
    data->finished = true;
    LightLock_Unlock(&data->lock);
}

// Dibuja un engranaje mecanico con dientes radiales, cuerpo biselado y eje perforado
void drawGear(float cx, float cy, float z, float outerR, float innerR, float holeR,
              int numTeeth, float angle, u32 gearColor, u32 holeColor, u32 shadowColor) {
    // Sombra suave proyectada
    C2D_DrawCircleSolid(cx + 2.0f, cy + 3.0f, z - 0.01f, innerR + 1.0f, shadowColor);

    float step = 6.2831853f / static_cast<float>(numTeeth);
    float halfBase = step * 0.28f;
    float halfTip  = step * 0.16f;

    // Dientes del engranaje
    for (int i = 0; i < numTeeth; ++i) {
        float a = angle + i * step;
        float c0 = std::cos(a - halfBase), s0 = std::sin(a - halfBase);
        float c1 = std::cos(a + halfBase), s1 = std::sin(a + halfBase);
        float ct0 = std::cos(a - halfTip), st0 = std::sin(a - halfTip);
        float ct1 = std::cos(a + halfTip), st1 = std::sin(a + halfTip);

        float p0x = cx + c0 * innerR, p0y = cy + s0 * innerR;
        float p1x = cx + c1 * innerR, p1y = cy + s1 * innerR;
        float p2x = cx + ct1 * outerR, p2y = cy + st1 * outerR;
        float p3x = cx + ct0 * outerR, p3y = cy + st0 * outerR;

        C2D_DrawTriangle(p0x, p0y, gearColor, p1x, p1y, gearColor, p2x, p2y, gearColor, z);
        C2D_DrawTriangle(p0x, p0y, gearColor, p2x, p2y, gearColor, p3x, p3y, gearColor, z);
    }

    // Cuerpo circular del engranaje
    C2D_DrawCircleSolid(cx, cy, z, innerR, gearColor);

    // Bisel / relieve 3D interior
    C2D_DrawCircleSolid(cx, cy, z + 0.001f, innerR * 0.72f, Ui::brighten(gearColor, 0.88f));

    // Agujero central hueco
    C2D_DrawCircleSolid(cx, cy, z + 0.002f, holeR, holeColor);

    // 4 orificios circulares decorativos de alivio
    float dotR = (innerR - holeR) * 0.18f;
    float ringR = (innerR + holeR) * 0.48f;
    for (int d = 0; d < 4; ++d) {
        float a = angle + d * 1.5707963f;
        C2D_DrawCircleSolid(cx + std::cos(a) * ringR, cy + std::sin(a) * ringR, z + 0.002f, dotR, holeColor);
    }
}

// Dibuja el medallón de osu! con pulso y anillos de onda expansiva
void drawOsuSplashMedallion(C2D_TextBuf textBuf, float cx, float cy, float radius,
                           float beat, int animTick, float textScale) {
    u32 pinkMain = Ui::rgba(255, 102, 171); // #FF66AB
    u32 pinkDeep = Ui::rgba(184, 23, 85);   // #B81755
    u32 whiteClr = Ui::rgba(255, 255, 255);

    // Anillos de onda expansiva sutiles
    for (int k = 0; k < 2; ++k) {
        float phase = std::fmod(animTick * 0.016f + k * 0.5f, 1.0f);
        u8 ringAlpha = static_cast<u8>((1.0f - phase) * 90.0f);
        Ui::ring(cx, cy, 0.18f, radius + 4.0f + phase * 28.0f, 1.8f, Ui::withAlpha(pinkMain, ringAlpha), 36);
    }

    // Resplandor exterior pulsante
    Ui::ring(cx, cy, 0.20f, radius + 6.0f + beat * 4.0f, 1.8f, Ui::withAlpha(pinkMain, 80), 36);
    Ui::glow(cx, cy, 0.22f, radius + 14.0f, Ui::withAlpha(pinkMain, static_cast<u8>(90 + beat * 45)), 4);

    // Sombra suave proyectada
    C2D_DrawCircleSolid(cx + 3.0f, cy + 5.0f, 0.23f, radius, Ui::rgba(0, 0, 0, 130));

    // Borde blanco + cuerpo rosa osu! + centro magenta
    float borderW = std::max(2.5f, radius * 0.07f);
    float innerOffset = std::max(5.0f, radius * 0.14f);
    C2D_DrawCircleSolid(cx, cy, 0.25f, radius, whiteClr);
    C2D_DrawCircleSolid(cx, cy, 0.28f, radius - borderW, pinkMain);
    C2D_DrawCircleSolid(cx, cy, 0.30f, radius - innerOffset, pinkDeep);

    // Reflejo superior estilo cristal
    C2D_DrawCircleSolid(cx - radius * 0.18f, cy - radius * 0.22f, 0.31f, radius * 0.48f, Ui::rgba(255, 255, 255, 26));

    // Texto central "osu!"
    drawCenteredTextWithShadow(textBuf, cx, cy - 2.0f, textScale, whiteClr, "osu!", 0.35f, Ui::rgba(0, 0, 0, 160));
}

} // namespace

void runSplashScreen(AppContext& ctx) {
    SplashWorkerContext workerData;
    workerData.ctx = &ctx;
    LightLock_Init(&workerData.lock);

    // Determinar consola y crear hilo secundario
    bool isNew3DS = ctx.isNew3DS;
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    s32 workerPrio = std::min<s32>(0x3F, prio + 1);

    APT_SetAppCpuTimeLimit(isNew3DS ? 70 : 30);
    Thread scanThread = nullptr;
    if (isNew3DS) {
        scanThread = threadCreate(scanThreadEntry, &workerData, 64 * 1024, workerPrio, 2, false);
    }
    if (!scanThread) {
        scanThread = threadCreate(scanThreadEntry, &workerData, 64 * 1024, workerPrio, 1, false);
    }
    if (!scanThread) {
        scanThread = threadCreate(scanThreadEntry, &workerData, 64 * 1024, workerPrio, -2, false);
    }

    // Si fallara la creación del hilo, ejecutar directamente
    if (!scanThread) {
        ctx.songMgr.initAndScan(&ctx.scoreMgr, onScanProgress, &workerData);
        workerData.finished = true;
    }

    float gearAngle = 0.0f;
    int tick = 0;
    float fadeOut = 0.0f;
    bool startingFade = false;

    // Paleta de colores de la pantalla de carga
    u32 clrTopBg1    = Ui::rgba(16, 14, 25);
    u32 clrTopBg2    = Ui::rgba(27, 22, 40);
    u32 clrBotBg1    = Ui::rgba(20, 18, 30);
    u32 clrBotBg2    = Ui::rgba(28, 24, 42);
    u32 clrGear1     = Ui::rgba(255, 102, 171); // Rosa osu!
    u32 clrGear2     = Ui::rgba(184, 30, 95);   // Magenta profundo
    u32 clrProgressBg= Ui::rgba(32, 28, 48);
    u32 clrProgressFg= Ui::rgba(255, 102, 171);
    u32 clrMutedTxt  = Ui::rgba(180, 175, 205);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            // Permitir continuar si ya terminó o cancelar
        }

        tick++;
        gearAngle += 0.055f;

        // Comprobar estado de finalización del hilo
        bool isDone = false;
        int currentFound = 0;
        char statusCopy[64] = {0};

        LightLock_Lock(&workerData.lock);
        isDone = workerData.finished;
        currentFound = workerData.songsFound;
        std::snprintf(statusCopy, sizeof(statusCopy), "%s", workerData.statusMsg);
        LightLock_Unlock(&workerData.lock);

        if (isDone && !startingFade) {
            startingFade = true;
        }

        if (startingFade) {
            fadeOut += 0.07f;
            if (fadeOut >= 1.0f) {
                fadeOut = 1.0f;
                break;
            }
        }

        float beat = 0.5f + 0.5f * std::sin(tick * 0.09f);

        // Iniciar dibujado con C3D
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(ctx.textBuf);

        // -------------------------------------------------------------
        // 1. PANTALLA SUPERIOR (400 x 240): Presentación Oficial de osu!
        // -------------------------------------------------------------
        int eyeCount = ctx.stereoActive ? 2 : 1;
        for (int eye = 0; eye < eyeCount; ++eye) {
            C3D_RenderTarget* target = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
            C2D_SceneBegin(target);
            C2D_TargetClear(target, clrTopBg1);

            // Fondo con degradado vertical y ambiente estético
            Ui::gradV(0.0f, 0.0f, 400.0f, 240.0f, 0.05f, clrTopBg1, clrTopBg2);

            // Triángulos flotantes sutiles estilo osu!lazer
            for (int i = 0; i < 8; ++i) {
                float seed = i * 14.3f;
                float size = 10.0f + std::fmod(seed * 6.0f, 22.0f);
                float speed = 0.35f + std::fmod(seed * 2.0f, 0.55f);
                float tx = std::fmod(seed * 43.0f, 400.0f);
                float ty = 240.0f + size - std::fmod(tick * speed + seed * 23.0f, 260.0f);
                u32 triClr = Ui::withAlpha(clrGear1, static_cast<u8>(14 + (i % 3) * 6));
                C2D_DrawTriangle(tx, ty - size, triClr, tx - size * 0.86f, ty + size * 0.5f, triClr, tx + size * 0.86f, ty + size * 0.5f, triClr, 0.08f);
            }

            // Separación 3D para la pantalla superior si el slider está activo
            float eyeOffset = 0.0f;
            if (ctx.stereoActive) {
                eyeOffset = (eye == 0) ? -3.0f * ctx.slider3D : 3.0f * ctx.slider3D;
            }

            float medallionX = 200.0f + eyeOffset;
            float medallionY = 100.0f;
            float medallionRadius = 46.0f + beat * 2.5f;

            // Galleta / Medallón de osu! pulsando
            drawOsuSplashMedallion(ctx.textBuf, medallionX, medallionY, medallionRadius, beat, tick, 0.88f);

            // Título "osu! 3DS"
            drawCenteredTextWithShadow(ctx.textBuf, medallionX, 172.0f, 0.52f, ctx.clrWhite, "osu! 3DS", 0.40f, Ui::rgba(0, 0, 0, 180));

            // Subtítulo
            drawCenteredTextWithShadow(ctx.textBuf, medallionX, 198.0f, 0.34f, clrMutedTxt, "Rhythm is just a click away!", 0.40f, Ui::rgba(0, 0, 0, 150));

            // Efecto de desvanecimiento de salida (fade out a negro)
            if (fadeOut > 0.0f) {
                u8 a = static_cast<u8>(fadeOut * 255.0f);
                C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 400.0f, 240.0f, Ui::rgba(0, 0, 0, a));
            }
        }

        // -------------------------------------------------------------
        // 2. PANTALLA INFERIOR (320 x 240): Mecanismo de Engranaje y Progreso
        // -------------------------------------------------------------
        C2D_SceneBegin(ctx.bottomScreen);
        C2D_TargetClear(ctx.bottomScreen, clrBotBg1);

        Ui::gradV(0.0f, 0.0f, 320.0f, 240.0f, 0.05f, clrBotBg1, clrBotBg2);

        // Tarjeta central contenedora con bordes sutiles
        Ui::panel(20.0f, 18.0f, 280.0f, 204.0f, 0.10f, Ui::rgba(24, 21, 35, 230), Ui::rgba(255, 102, 171, 90));

        // ENGRANAJES MECÁNICOS GIRATORIOS ENGRANADOS
        // Engranaje principal (Rosa, grande, gira horario)
        float g1X = 142.0f;
        float g1Y = 90.0f;
        drawGear(g1X, g1Y, 0.20f, 31.0f, 23.5f, 8.0f, 10, gearAngle, clrGear1, clrBotBg1, Ui::rgba(0, 0, 0, 110));

        // Engranaje secundario (Magenta, mediano, entrelazado, gira antihorario)
        float g2X = 181.0f;
        float g2Y = 111.0f;
        float ratio = 10.0f / 6.0f;
        float g2Angle = -gearAngle * ratio + 0.28f;
        drawGear(g2X, g2Y, 0.22f, 19.5f, 14.5f, 5.0f, 6, g2Angle, clrGear2, clrBotBg1, Ui::rgba(0, 0, 0, 95));

        // Texto de estado dinámico
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 148.0f, 0.40f, ctx.clrWhite, statusCopy, 0.35f, Ui::rgba(0, 0, 0, 160));

        // Contador de canciones en vivo
        char countStr[64];
        if (currentFound > 0) {
            std::snprintf(countStr, sizeof(countStr), "%d canciones listas", currentFound);
        } else {
            std::snprintf(countStr, sizeof(countStr), "Iniciando biblioteca...");
        }
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 168.0f, 0.33f, clrMutedTxt, countStr, 0.35f, Ui::rgba(0, 0, 0, 140));

        // Barra de progreso estética con brillo deslizante continuo
        float barX = 40.0f;
        float barY = 192.0f;
        float barW = 240.0f;
        float barH = 8.0f;

        // Fondo de la barra
        Ui::pill(barX, barY, barW, barH, 0.30f, clrProgressBg);

        // Efecto de brillo deslizante (onda que recorre la barra)
        float progressSweep = std::fmod(tick * 3.5f, barW + 50.0f) - 25.0f;
        float sweepW = 55.0f;
        float sweepStart = std::max(barX, barX + progressSweep);
        float sweepEnd   = std::min(barX + barW, barX + progressSweep + sweepW);
        if (sweepEnd > sweepStart) {
            Ui::pill(sweepStart, barY, sweepEnd - sweepStart, barH, 0.32f, clrProgressFg);
        }

        // Si ya terminó, llenar la barra de forma sólida
        if (isDone) {
            Ui::pill(barX, barY, barW, barH, 0.33f, clrProgressFg);
        }

        // Efecto de desvanecimiento en la pantalla inferior
        if (fadeOut > 0.0f) {
            u8 a = static_cast<u8>(fadeOut * 255.0f);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 320.0f, 240.0f, Ui::rgba(0, 0, 0, a));
        }

        C3D_FrameEnd(0);
    }

    // Esperar a que el hilo de escaneo finalice completamente
    if (scanThread) {
        threadJoin(scanThread, U64_MAX);
        threadFree(scanThread);
        scanThread = nullptr;
    }

    // Al concluir la carga, arrancar la música de bienvenida
    std::string themePath = "romfs:/circles.mp3";
    bool themeStarted = false;
    FILE* fTheme = std::fopen(themePath.c_str(), "rb");
    if (fTheme) {
        std::fclose(fTheme);
        themeStarted = ctx.audio.startMusic(themePath, 0, 1.0f, 0);
        if (themeStarted) {
            ctx.audio.setLoop(false);
            ctx.bgLoader.cancel();
            ctx.bgTex.unload();
        }
    }

    if (!themeStarted && !ctx.songMgr.sets.empty()) {
        ctx.selectedSet = std::rand() % static_cast<int>(ctx.songMgr.sets.size());
        ctx.selectedDiff = 0;
        ctx.carouselPos = static_cast<float>(ctx.selectedSet);
        ctx.updateSongPreview();
    }

    ctx.appState = STATE_MAIN_MENU;
    ctx.screenFade = 1.0f;
}
