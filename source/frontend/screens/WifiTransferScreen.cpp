#include "WifiTransferScreen.hpp"

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
    if (ctx.touchJustDown && ctx.touch.px >= 166 && ctx.touch.px <= 306 && ctx.touch.py >= 180 && ctx.touch.py <= 228) {
        exitWifi = true;
    }

    bool triggerRescan = (ctx.kDown & KEY_X) != 0;
    if (ctx.touchJustDown && ctx.touch.px >= 14 && ctx.touch.px <= 154 && ctx.touch.py >= 180 && ctx.touch.py <= 228) {
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

    // --- PANTALLA SUPERIOR: URL DEL SERVIDOR Y PROGRESO EN VIVO ---
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        float dCard = eyeSign * ctx.slider3D * 2.5f;

        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);

        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), ctx.clr300, 0.02f);

        Ui::gradV(0.0f, 0.0f, 400.0f, 32.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
        C2D_DrawRectSolid(0.0f, 31.0f, 0.31f, 400.0f, 1.5f, ctx.clr300);
        drawText(ctx.textBuf, 12.0f, 7.0f, 0.50f, ctx.clrWhite, LOC(STR_WIFI_HEADER));

        Ui::panel(16.0f + dCard, 42.0f, 368.0f, 66.0f, 0.2f, Ui::rgba(26, 22, 42, 235), ctx.clrPrimary);
        drawCenteredText(ctx.textBuf, 200.0f + dCard, 56.0f, 0.42f, ctx.clrWhite,
            LOC(STR_WIFI_BROWSER_PROMPT));
        drawCenteredText(ctx.textBuf, 200.0f + dCard, 84.0f, 0.72f, ctx.clrCursor,
            ctx.wifiServer.serverUrl.empty() ? (ctx.wifiServer.isConnecting() ? LOC(STR_WIFI_IP_CONNECTING) : LOC(STR_WIFI_IP_NONE)) : ctx.wifiServer.serverUrl.c_str());

        Ui::panel(16.0f + dCard * 0.6f, 114.0f, 368.0f, 42.0f, 0.2f, Ui::rgba(22, 19, 36, 220));
        drawText(ctx.textBuf, 24.0f + dCard * 0.6f, 118.0f, 0.40f, ctx.clrWhite,
            LOC(STR_WIFI_STEP1));
        drawText(ctx.textBuf, 24.0f + dCard * 0.6f, 136.0f, 0.40f, ctx.clr100,
            LOC(STR_WIFI_STEP2));

        // Barra de progreso de recepción en tiempo real
        Ui::panel(16.0f + dCard, 162.0f, 368.0f, 42.0f, 0.2f, Ui::rgba(26, 22, 42, 235), ctx.clr300);
        drawCenteredText(ctx.textBuf, 200.0f + dCard, 174.0f, 0.41f, ctx.clr300,
            truncateString(ctx.wifiServer.statusText, 50).c_str());

        float prog = ctx.wifiServer.getProgressRatio();
        Ui::statBar(30.0f + dCard, 189.0f, 340.0f, 8.0f, 0.35f, prog, ctx.clr100);

        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
        char footerWifi[96];
        std::snprintf(footerWifi, sizeof(footerWifi),
            LOC(STR_WIFI_FOOTER_FMT),
            ctx.wifiServer.uploadedCount, static_cast<int>(ctx.songMgr.sets.size()));
        drawCenteredText(ctx.textBuf, 200.0f, 226.0f, 0.40f, ctx.clrWhite, footerWifi);
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clr300);
    }

    // --- PANTALLA INFERIOR: CÓDIGO QR, SEÑAL Y BOTONES INTERACTIVOS ---
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), ctx.clr300, 0.02f);

    // Cabecera superior
    Ui::gradV(0.0f, 0.0f, 320.0f, 26.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
    C2D_DrawRectSolid(0.0f, 25.0f, 0.31f, 320.0f, 1.5f, ctx.clr300);
    drawCenteredText(ctx.textBuf, 160.0f, 5.0f, 0.44f, ctx.clrWhite, LOC(STR_WIFI_TITLE));

    // Panel Izquierdo: Código QR nítido
    if (!ctx.wifiServer.serverUrl.empty()) {
        renderQrCode(ctx.wifiServer.serverUrl.c_str(), 14.0f, 32.0f, 138.0f);
    } else {
        Ui::panel(14.0f, 32.0f, 138.0f, 138.0f, 0.22f, Ui::rgba(26, 22, 42, 235), ctx.clrPrimary);
        if (ctx.wifiServer.isConnecting()) {
            drawCenteredText(ctx.textBuf, 83.0f, 85.0f, 0.40f, ctx.clrCursor, LOC(STR_WIFI_CONNECTING));
            drawCenteredText(ctx.textBuf, 83.0f, 105.0f, 0.40f, ctx.clrWhite, "Wi-Fi...");
        } else {
            drawCenteredText(ctx.textBuf, 83.0f, 85.0f, 0.40f, ctx.clrCursor, LOC(STR_WIFI_SEARCHING));
            drawCenteredText(ctx.textBuf, 83.0f, 105.0f, 0.40f, ctx.clrWhite, LOC(STR_WIFI_NETWORK));
        }
    }

    // Panel Derecho: Monitor de señal Wi-Fi e instrucciones rápidas
    Ui::panel(158.0f, 32.0f, 148.0f, 138.0f, 0.22f, Ui::rgba(24, 20, 40, 240), ctx.clr300);
    u8 wifiSig = osGetWifiStrength();
    renderWifiBars(166.0f, 40.0f, wifiSig, ctx.clr100, Ui::rgba(70, 65, 90));
    const char* sigText = (wifiSig == 3) ? LOC(STR_WIFI_SIG_EXCELLENT) : ((wifiSig == 2) ? LOC(STR_WIFI_SIG_GOOD) : ((wifiSig == 1) ? LOC(STR_WIFI_SIG_WEAK) : LOC(STR_WIFI_SIG_NONE)));
    drawText(ctx.textBuf, 188.0f, 39.0f, 0.36f, (wifiSig > 0) ? ctx.clr100 : ctx.clrMiss, sigText);

    drawText(ctx.textBuf, 166.0f, 58.0f, 0.44f, ctx.clrCursor, LOC(STR_WIFI_SCAN_MOBILE));
    drawText(ctx.textBuf, 166.0f, 78.0f, 0.34f, ctx.clrWhite, LOC(STR_WIFI_AIM_CAM));
    drawText(ctx.textBuf, 166.0f, 94.0f, 0.34f, ctx.clrWhite, LOC(STR_WIFI_OPEN_WEB));
    drawText(ctx.textBuf, 166.0f, 110.0f, 0.34f, ctx.clrWhite, LOC(STR_WIFI_DL_AUDIO));
    drawText(ctx.textBuf, 166.0f, 126.0f, 0.34f, ctx.clr100, LOC(STR_WIFI_STAR_FILTER));

    char countBuf[48];
    std::snprintf(countBuf, sizeof(countBuf), "%s: %d", LOC(STR_SONGS), static_cast<int>(ctx.songMgr.sets.size()));
    drawText(ctx.textBuf, 166.0f, 148.0f, 0.36f, ctx.clrWhite, countBuf);

    // Botón Izquierdo: Re-escanear canciones
    Ui::button(14.0f, 178.0f, 140.0f, 48.0f, 0.25f, Ui::rgba(22, 38, 62, 240), Ui::rgba(40, 180, 240), false);
    Ui::pill(22.0f, 184.0f, 26.0f, 16.0f, 0.28f, Ui::rgba(40, 180, 240));
    drawCenteredText(ctx.textBuf, 35.0f, 192.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "X", 0.32f);
    drawTextWithShadow(ctx.textBuf, 54.0f, 184.0f, 0.38f, ctx.clrWhite, LOC(STR_WIFI_RESCAN_BTN), 0.30f);
    drawTextWithShadow(ctx.textBuf, 22.0f, 206.0f, 0.30f, ctx.clr100, LOC(STR_WIFI_RESCAN_SUB), 0.30f);

    // Botón Derecho: Volver al menú
    Ui::button(166.0f, 178.0f, 140.0f, 48.0f, 0.25f, Ui::rgba(42, 20, 52, 240), ctx.clrPrimary, false);
    Ui::pill(174.0f, 184.0f, 26.0f, 16.0f, 0.28f, ctx.clrPrimary);
    drawCenteredText(ctx.textBuf, 187.0f, 192.0f, 0.30f, ctx.clrWhite, "B", 0.32f);
    drawTextWithShadow(ctx.textBuf, 206.0f, 184.0f, 0.40f, ctx.clrWhite, LOC(STR_WIFI_BACK_BTN), 0.30f);
    drawTextWithShadow(ctx.textBuf, 174.0f, 206.0f, 0.30f, Ui::rgba(255, 180, 215), LOC(STR_WIFI_SAVE_SUB), 0.30f);

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clr300);

    C3D_FrameEnd(0);
}
