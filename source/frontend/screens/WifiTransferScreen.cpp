#include "WifiTransferScreen.hpp"
#include "backend/i18n/Localization.hpp"

void runWifiTransferScreen(AppContext& ctx) {
    ctx.wifiServer.update();

    if (ctx.wifiServer.needsRescan) {
        ctx.wifiServer.needsRescan = false;
        ctx.songMgr.initAndScan(&ctx.scoreMgr);
        ctx.skinMgr.initAndScan();
        ctx.selectedSet = 0;
        ctx.selectedDiff = 0;
        ctx.audio.playHitSound(2);
    }

    bool exitWifi = (ctx.kDown & KEY_B) != 0;
    if (ctx.touchJustDown && ctx.touch.px >= 166 && ctx.touch.px <= 310 && ctx.touch.py >= 184 && ctx.touch.py <= 234) {
        exitWifi = true;
    }

    bool triggerRescan = (ctx.kDown & KEY_X) != 0;
    if (ctx.touchJustDown && ctx.touch.px >= 10 && ctx.touch.px <= 154 && ctx.touch.py >= 184 && ctx.touch.py <= 234) {
        triggerRescan = true;
    }

    if (triggerRescan) {
        ctx.audio.playHitSound(2);
        ctx.wifiServer.statusText = LOC(STR_WIFI_UPDATING_LIB);
        OszExtractor::extractAllPendingOsz("/osu/songs", "/osu/songs");
        OszExtractor::extractAllPendingOsk("/osu/skins", "/osu/skins");
        ctx.songMgr.initAndScan(&ctx.scoreMgr);
        ctx.skinMgr.initAndScan();
        ctx.selectedSet = 0;
        ctx.selectedDiff = 0;
        char rescanMsg[96];
        std::snprintf(rescanMsg, sizeof(rescanMsg), LOC(STR_WIFI_LIB_UPDATED), static_cast<int>(ctx.songMgr.sets.size()));
        ctx.wifiServer.statusText = rescanMsg;
        if (!ctx.wifiServer.isRunning()) {
            ctx.wifiServer.restart();
        }
    }

    if (exitWifi) {
        ctx.wifiServer.stop();
        ctx.songMgr.initAndScan(&ctx.scoreMgr);
        ctx.skinMgr.initAndScan();
        ctx.audio.playHitSound(0);
        ctx.appState = ctx.wifiReturnState;
        if (ctx.appState == STATE_SONG_SELECT) ctx.updateSongPreview();
        return;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // =========================================================================
    // PANTALLA SUPERIOR (400x240): ESTADO DEL SERVIDOR, PASOS Y PROGRESO
    // =========================================================================
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        float dCard = eyeSign * ctx.slider3D * 2.5f;

        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);

        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);

        // Cabecera superior estilo lazer
        Ui::gradV(0.0f, 0.0f, 400.0f, 26.0f, 0.10f, Ui::rgba(27, 20, 45, 255), Ui::rgba(20, 15, 34, 255));
        C2D_DrawRectSolid(0.0f, 25.0f, 0.15f, 400.0f, 1.0f, Ui::rgba(0, 229, 255));

        Ui::slantRectGrad(8.0f + dCard, 3.5f, 85.0f, 19.0f, -4.0f, 0.25f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawCenteredTextWithShadow(ctx.textBuf, 50.0f + dCard, 13.0f, 0.35f, ctx.clrWhite, "osu!3DS", 0.30f);

        drawTextWithShadow(ctx.textBuf, 102.0f + dCard, 6.0f, 0.38f, ctx.clrWhite, LOC(STR_WIFI_PORTAL_TITLE), 0.30f);

        bool hasIp = !ctx.wifiServer.serverUrl.empty();
        Ui::pill(306.0f + dCard, 4.0f, 86.0f, 18.0f, 0.25f, hasIp ? Ui::rgba(16, 50, 35) : Ui::rgba(45, 34, 20));
        drawRectOutline(306.0f + dCard, 4.0f, 86.0f, 18.0f, 1.0f, hasIp ? Ui::rgba(74, 222, 128) : Ui::rgba(255, 209, 102), 0.26f);
        drawCenteredText(ctx.textBuf, 349.0f + dCard, 13.0f, 0.29f, hasIp ? Ui::rgba(74, 222, 128) : Ui::rgba(255, 209, 102),
            hasIp ? LOC(STR_WIFI_ONLINE) : LOC(STR_WIFI_CONNECTING), 0.30f);

        // Tarjeta 1: Dirección Web / URL Destacada
        Ui::panel(10.0f + dCard, 32.0f, 380.0f, 62.0f, 0.20f, Ui::rgba(20, 34, 58, 255), Ui::rgba(0, 229, 255));
        drawTextWithShadow(ctx.textBuf, 22.0f + dCard, 38.0f, 0.32f, Ui::rgba(0, 229, 255), LOC(STR_WIFI_BROWSER_PROMPT), 0.25f);

        std::string ipDisplay = ctx.wifiServer.serverUrl.empty()
            ? (ctx.wifiServer.isConnecting() ? LOC(STR_WIFI_IP_CONNECTING) : LOC(STR_WIFI_NO_CONN_HINT))
            : ctx.wifiServer.serverUrl;
        drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dCard, 68.0f, 0.65f, Ui::rgba(255, 209, 102), ipDisplay.c_str(), 0.28f);

        // Tarjeta 2: Pasos rápidos de uso
        Ui::panel(10.0f + dCard, 100.0f, 380.0f, 44.0f, 0.20f, Ui::rgba(18, 22, 38, 240), Ui::rgba(45, 38, 65));
        drawText(ctx.textBuf, 22.0f + dCard, 106.0f, 0.34f, ctx.clrWhite, LOC(STR_WIFI_STEP1), 0.25f);
        drawText(ctx.textBuf, 22.0f + dCard, 122.0f, 0.34f, Ui::rgba(74, 222, 128), LOC(STR_WIFI_STEP2), 0.25f);

        // Tarjeta 3: Barra de progreso y recepción en tiempo real
        Ui::panel(10.0f + dCard, 150.0f, 380.0f, 56.0f, 0.20f, Ui::rgba(13, 17, 31, 255), Ui::rgba(0, 229, 255));
        drawTextWithShadow(ctx.textBuf, 22.0f + dCard, 156.0f, 0.32f, Ui::rgba(148, 163, 184), LOC(STR_WIFI_STATUS_TRANSFER), 0.25f);
        drawRightAlignedText(ctx.textBuf, 378.0f + dCard, 156.0f, 0.34f, Ui::rgba(74, 222, 128),
            truncateString(ctx.wifiServer.statusText, 38).c_str(), 0.25f);

        float prog = ctx.wifiServer.getProgressRatio();
        Ui::statBar(22.0f + dCard, 178.0f, 356.0f, 10.0f, 0.35f, prog, Ui::rgba(0, 229, 255));

        // Pie de pantalla superior
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.30f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        char footerWifi[96];
        std::snprintf(footerWifi, sizeof(footerWifi), LOC(STR_WIFI_FOOTER_FMT),
            ctx.wifiServer.uploadedCount, static_cast<int>(ctx.songMgr.sets.size()));
        drawCenteredTextWithShadow(ctx.textBuf, 200.0f, 222.0f, 0.36f, ctx.clrWhite, footerWifi);
        drawRightAlignedText(ctx.textBuf, 392.0f, 222.0f, 0.28f, Ui::rgba(135, 130, 165), "400x240", 0.32f);

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // =========================================================================
    // PANTALLA INFERIOR TÁCTIL (320x240): QR NÍTIDO, SEÑAL Y BOTONES
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.02f);

    drawTextWithShadow(ctx.textBuf, 12.0f, 6.0f, 0.40f, Ui::rgba(0, 229, 255), LOC(STR_WIFI_TITLE), 0.20f);
    drawRightAlignedText(ctx.textBuf, 308.0f, 6.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_WIFI_PORT_LABEL), 0.20f);

    // Panel Izquierdo: Código QR nítido o Estado de Descarga Activa
    if (ctx.wifiServer.isReceiving()) {
        Ui::panel(10.0f, 28.0f, 145.0f, 145.0f, 0.22f, Ui::rgba(18, 30, 52), Ui::rgba(0, 229, 255));
        drawCenteredTextWithShadow(ctx.textBuf, 82.0f, 62.0f, 0.48f, Ui::rgba(0, 229, 255), LOC(STR_WIFI_RECEIVING), 0.28f);
        drawCenteredText(ctx.textBuf, 82.0f, 90.0f, 0.34f, Ui::rgba(74, 222, 128), LOC(STR_WIFI_WRITING_SD), 0.28f);
        char mbBuf[32];
        float mbRecv = static_cast<float>(ctx.wifiServer.getBytesReceived()) / (1024.0f * 1024.0f);
        float mbTot = static_cast<float>(ctx.wifiServer.getContentLength()) / (1024.0f * 1024.0f);
        std::snprintf(mbBuf, sizeof(mbBuf), "%.1f / %.1f MB", mbRecv, mbTot);
        drawCenteredText(ctx.textBuf, 82.0f, 120.0f, 0.34f, ctx.clrWhite, mbBuf, 0.28f);
    } else if (!ctx.wifiServer.serverUrl.empty()) {
        renderQrCode(ctx.wifiServer.serverUrl.c_str(), 10.0f, 28.0f, 145.0f);
    } else {
        Ui::panel(10.0f, 28.0f, 145.0f, 145.0f, 0.22f, Ui::rgba(20, 25, 45), Ui::rgba(0, 229, 255));
        if (ctx.wifiServer.isConnecting()) {
            drawCenteredTextWithShadow(ctx.textBuf, 82.0f, 90.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_WIFI_CONNECTING_TO), 0.25f);
            drawCenteredText(ctx.textBuf, 82.0f, 110.0f, 0.36f, ctx.clrWhite, "Wi-Fi...", 0.25f);
        } else {
            drawCenteredTextWithShadow(ctx.textBuf, 82.0f, 90.0f, 0.38f, Ui::rgba(255, 209, 102), LOC(STR_WIFI_NO_CONNECTION), 0.25f);
            drawCenteredText(ctx.textBuf, 82.0f, 110.0f, 0.34f, ctx.clrWhite, LOC(STR_WIFI_CHECK_ROUTER), 0.25f);
        }
    }

    // Panel Derecho: Monitor de señal Wi-Fi e instrucciones rápidas
    Ui::panel(165.0f, 28.0f, 145.0f, 145.0f, 0.22f, Ui::rgba(18, 22, 38, 240), Ui::rgba(0, 229, 255));
    u8 wifiSig = osGetWifiStrength();
    renderWifiBars(175.0f, 36.0f, wifiSig, Ui::rgba(74, 222, 128), Ui::rgba(70, 65, 90));
    const char* sigText = (wifiSig == 3) ? LOC(STR_WIFI_SIG_EXCELLENT) : ((wifiSig == 2) ? LOC(STR_WIFI_SIG_GOOD) : ((wifiSig == 1) ? LOC(STR_WIFI_SIG_WEAK) : LOC(STR_WIFI_SIG_NONE)));
    drawTextWithShadow(ctx.textBuf, 198.0f, 35.0f, 0.34f, (wifiSig > 0) ? Ui::rgba(74, 222, 128) : Ui::rgba(235, 45, 65), sigText, 0.25f);

    drawTextWithShadow(ctx.textBuf, 175.0f, 56.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_WIFI_SCAN_MOBILE), 0.25f);
    drawText(ctx.textBuf, 175.0f, 74.0f, 0.30f, ctx.clrWhite, LOC(STR_WIFI_AIM_CAM_BULLET), 0.25f);
    drawText(ctx.textBuf, 175.0f, 90.0f, 0.30f, ctx.clrWhite, LOC(STR_WIFI_OPEN_LINK_BULLET), 0.25f);
    drawText(ctx.textBuf, 175.0f, 106.0f, 0.30f, ctx.clrWhite, LOC(STR_WIFI_DOWNLOAD_BULLET), 0.25f);

    char countBuf[48];
    std::snprintf(countBuf, sizeof(countBuf), LOC(STR_WIFI_TOTAL_SD_FMT), static_cast<int>(ctx.songMgr.sets.size()));
    drawText(ctx.textBuf, 175.0f, 140.0f, 0.28f, Ui::rgba(255, 209, 102), countBuf, 0.25f);

    // Botones de acción inferiores
    // Botón Izquierdo: Re-escanear canciones
    Ui::button(10.0f, 184.0f, 145.0f, 48.0f, 0.25f, Ui::rgba(20, 34, 58), Ui::rgba(0, 229, 255), false);
    Ui::pill(18.0f, 190.0f, 24.0f, 16.0f, 0.28f, Ui::rgba(0, 229, 255));
    drawCenteredText(ctx.textBuf, 30.0f, 198.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "X", 0.32f);
    drawTextWithShadow(ctx.textBuf, 48.0f, 190.0f, 0.36f, ctx.clrWhite, LOC(STR_WIFI_RESCAN_BTN), 0.30f);
    drawText(ctx.textBuf, 18.0f, 212.0f, 0.28f, Ui::rgba(74, 222, 128), LOC(STR_WIFI_EXTRACT_OSZ_SUB), 0.30f);

    // Botón Derecho: Volver al menú
    Ui::button(165.0f, 184.0f, 145.0f, 48.0f, 0.25f, Ui::rgba(35, 18, 28), Ui::rgba(255, 42, 133), false);
    Ui::pill(173.0f, 190.0f, 24.0f, 16.0f, 0.28f, Ui::rgba(255, 42, 133));
    drawCenteredText(ctx.textBuf, 185.0f, 198.0f, 0.30f, ctx.clrWhite, "B", 0.32f);
    drawTextWithShadow(ctx.textBuf, 203.0f, 190.0f, 0.38f, ctx.clrWhite, LOC(STR_WIFI_BACK_BTN), 0.30f);
    drawText(ctx.textBuf, 173.0f, 212.0f, 0.28f, Ui::rgba(255, 180, 215), LOC(STR_WIFI_CLOSE_SERVER_SUB), 0.30f);

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.95f, 10.0f, Ui::withAlpha(Ui::rgba(0, 229, 255), 130), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.96f, 4.0f, Ui::rgba(0, 229, 255));
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, Ui::rgba(0, 229, 255));

    C3D_FrameEnd(0);

    // Ejecutar un ciclo de bombeo de red extra tras esperar el fotograma VSync
    ctx.wifiServer.update();
}
