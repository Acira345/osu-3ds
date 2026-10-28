#include "MultiResultsScreen.hpp"
#include "backend/i18n/Localization.hpp"

void runMultiResultsScreen(AppContext& ctx) {
    ctx.audio.update();
    ctx.multiMgr.update(false);

    if (!ctx.multiMgr.inMultiplayer) {
        ctx.multiMgr.startBrowser(ctx.sysUser);
        ctx.appState = STATE_MULTI_BROWSER;
        return;
    }

    // Si el anfitrión envió la señal de retorno a la sala sincronizada
    if (!ctx.multiMgr.isHost() && ctx.multiMgr.shouldReturnToLobby) {
        ctx.multiMgr.shouldReturnToLobby = false;
        ctx.multiMgr.matchStarted = false;
        ctx.multiMgr.matchInProgressFlag = false;
        ctx.multiMgr.players[ctx.multiMgr.localSlot].status = STATUS_NOT_READY;
        ctx.multiMgr.sendPlayerUpdate();
        ctx.appState = STATE_MULTI_LOBBY;
        return;
    }

    // Salir de la sala con [B]
    if (ctx.kDown & KEY_B) {
        ctx.audio.playHitSound(0);
        if (ctx.multiMgr.isHost()) {
            ctx.multiMgr.notifyRoomClosed();
        } else {
            ctx.multiMgr.notifyPlayerLeave();
        }
        ctx.multiMgr.stop();
        ctx.multiMgr.startBrowser(ctx.sysUser);
        ctx.appState = STATE_MULTI_BROWSER;
        return;
    }

    bool allFinished = ctx.multiMgr.areAllPlayersFinished();
    bool touchedContinue = ctx.touchJustDown && (ctx.touch.px >= 10.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= 184.0f && ctx.touch.py <= 234.0f);

    if ((ctx.kDown & KEY_A) || touchedContinue) {
        if (!allFinished) {
            ctx.audio.playHitSound(4); // Bloquear avance si aún hay jugadores activos en partida
        } else if (ctx.multiMgr.isHost()) {
            ctx.audio.playHitSound(2);
            ctx.multiMgr.matchStarted = false;
            ctx.multiMgr.matchInProgressFlag = false;
            ctx.multiMgr.sendReturnToLobby();
            ctx.multiMgr.advanceQueueAndRotateHost();
            ctx.appState = STATE_MULTI_LOBBY;
            return;
        } else {
            ctx.audio.playHitSound(0);
        }
    }

    if (ctx.kDown & (KEY_X | KEY_Y)) {
        ctx.audio.playHitSound(0);
        ctx.multiResultsTab = (ctx.multiResultsTab + 1) % 4;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    auto ranked = ctx.multiMgr.getLeaderboard();

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dTop = depthShift(2.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);

        // Cabecera lazer
        Ui::slantRectGrad(8.0f + dTop, 3.5f, 78.0f, 19.0f, -4.0f, 0.25f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawCenteredTextWithShadow(ctx.textBuf, 47.0f + dTop, 13.0f, 0.34f, ctx.clrWhite, "osu!3DS", 0.30f);

        drawTextWithShadow(ctx.textBuf, 94.0f + dTop, 6.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_RESULTS_HEADER), 0.30f);

        std::string rSongInfo = ctx.multiMgr.activeBeatmap.title + " [" + ctx.multiMgr.activeBeatmap.version + "]";
        drawRightAlignedText(ctx.textBuf, 390.0f + dTop, 8.0f, 0.30f, Ui::rgba(148, 163, 184), truncateString(rSongInfo, 26).c_str(), 0.30f);

        // 4 Tiras horizontales de 40px para aprovechar los 400px de ancho y no saturar columnas
        for (size_t r = 0; r < ranked.size() && r < 4; ++r) {
            const auto& item = ranked[r];
            float cardY = 32.0f + r * 44.0f;
            float cardX = 10.0f + dTop;
            float cardW = 380.0f;
            float cardH = 40.0f;

            u32 medalClr = item.player->isFailed ? Ui::rgba(235, 45, 45) :
                           ((r == 0) ? Ui::rgba(255, 209, 102) :
                           ((r == 1) ? Ui::rgba(0, 229, 255) :
                           ((r == 2) ? Ui::rgba(180, 80, 240) : Ui::rgba(148, 163, 184))));

            u32 cardBg = item.player->isLocal ? Ui::rgba(20, 34, 58, 255) : Ui::rgba(18, 22, 38, 240);
            Ui::panel(cardX, cardY, cardW, cardH, 0.20f, cardBg, medalClr);
            drawRectOutline(cardX, cardY, cardW, cardH, 1.2f, medalClr, 0.22f);

            // Insignia de Ranking (#1, #2, FAIL)
            Ui::panel(cardX + 6.0f, cardY + 5.0f, 32.0f, 30.0f, 0.22f, medalClr);
            char rankTxt[8];
            if (item.player->isFailed) {
                std::snprintf(rankTxt, sizeof(rankTxt), "FAIL");
            } else {
                std::snprintf(rankTxt, sizeof(rankTxt), "#%u", static_cast<unsigned int>(r + 1));
            }
            drawCenteredText(ctx.textBuf, cardX + 22.0f, cardY + 20.0f, 0.32f, Ui::rgba(0, 0, 0, 250), rankTxt, 0.28f);

            // Nombre
            drawTextWithShadow(ctx.textBuf, cardX + 44.0f, cardY + 5.0f, 0.44f, ctx.clrWhite, item.player->name, 0.25f);
            if (item.player->isLocal) {
                Ui::pill(cardX + 130.0f, cardY + 6.0f, 28.0f, 13.0f, 0.26f, Ui::rgba(0, 229, 255));
                drawCenteredText(ctx.textBuf, cardX + 144.0f, cardY + 12.5f, 0.26f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
            }

            // Mods
            std::string mStr = (item.player->activeMods != MOD_NONE) ? ("+" + formatModsString(item.player->activeMods)) : LOC(STR_MULTI_NO_MODS);
            drawText(ctx.textBuf, cardX + 44.0f, cardY + 22.0f, 0.29f, Ui::rgba(148, 163, 184), mStr.c_str(), 0.25f);

            // Puntuación
            std::string scStr = formatScoreThousands(item.player->score);
            drawTextWithShadow(ctx.textBuf, cardX + 184.0f, cardY + 10.0f, 0.50f, ctx.clrWhite, scStr.c_str(), 0.25f);

            // Precisión y Combo
            char accComb[32];
            std::snprintf(accComb, sizeof(accComb), "%.2f%%", item.player->accuracy);
            drawText(ctx.textBuf, cardX + 280.0f, cardY + 6.0f, 0.34f, Ui::rgba(0, 229, 255), accComb, 0.25f);
            char cmb[16];
            std::snprintf(cmb, sizeof(cmb), "%dx combo", item.player->maxCombo);
            drawText(ctx.textBuf, cardX + 280.0f, cardY + 22.0f, 0.28f, Ui::rgba(148, 163, 184), cmb, 0.25f);

            // Calificación (Grado S/A/B/C/D)
            u32 gClr = getScoreGradeColor(item.player->grade, ctx.clrCursor);
            drawCenteredTextWithShadow(ctx.textBuf, cardX + cardW - 20.0f, cardY + 20.0f, 0.65f, gClr, item.player->grade, 0.28f);
        }

        // Pie de pantalla superior
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.30f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::glow(200.0f + dToast, 224.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
            Ui::pill(30.0f + dToast, 212.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
            drawRectOutline(30.0f + dToast, 212.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        } else if (!allFinished) {
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dTop, 222.0f, 0.34f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_BTN_WAIT_FINISH), 0.32f);
        } else {
            float byY = 220.0f;
            drawKeyBadge(ctx.textBuf, 12.0f + dTop, byY, "A", ctx.multiMgr.isHost() ? LOC(STR_MULTI_BADGE_RETURN_ROOM) : LOC(STR_MULTI_BADGE_WAIT_HOST), Ui::rgba(0, 229, 255));
            drawKeyBadge(ctx.textBuf, 180.0f + dTop, byY, "B", LOC(STR_MULTI_BADGE_LEAVE_ROOM), Ui::rgba(148, 163, 184));
            drawKeyBadge(ctx.textBuf, 296.0f + dTop, byY, "X", LOC(STR_MULTI_BADGE_TOGGLE_INFO), Ui::rgba(255, 209, 102));
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // PANTALLA INFERIOR TÁCTIL (320x240)
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.02f);

    const char* resTabs[4] = { LOC(STR_MULTI_TAB_BREAKDOWN), LOC(STR_MULTI_TAB_HITERROR), LOC(STR_MULTI_TAB_1V1), LOC(STR_MULTI_TAB_DETAILS) };
    for (int t = 0; t < 4; ++t) {
        float tx = 10.0f + t * 76.0f;
        bool activeT = (ctx.multiResultsTab == t);
        Ui::panel(tx, 6.0f, 72.0f, 22.0f, 0.20f, activeT ? Ui::rgba(0, 229, 255) : Ui::rgba(20, 25, 45));
        drawCenteredText(ctx.textBuf, tx + 36.0f, 17.0f, 0.30f, activeT ? Ui::rgba(0, 0, 0, 250) : ctx.clrMuted, resTabs[t], 0.25f);
    }

    const auto& winner = ranked.empty() ? ctx.multiMgr.players[ctx.multiMgr.localSlot] : *ranked[0].player;

    float boxW = 68.0f;
    float boxH = 46.0f;

    // Box 300
    Ui::panel(10.0f, 34.0f, boxW, boxH, 0.20f, Ui::rgba(20, 25, 45), Ui::rgba(0, 229, 255));
    drawCenteredText(ctx.textBuf, 10.0f + boxW * 0.5f, 44.0f, 0.30f, Ui::rgba(0, 229, 255), "300", 0.25f);
    char c3Str[16]; std::snprintf(c3Str, sizeof(c3Str), "%d", winner.count300);
    drawCenteredTextWithShadow(ctx.textBuf, 10.0f + boxW * 0.5f, 62.0f, 0.44f, ctx.clrWhite, c3Str, 0.25f);

    // Box 100
    Ui::panel(86.0f, 34.0f, boxW, boxH, 0.20f, Ui::rgba(20, 25, 45), Ui::rgba(74, 222, 128));
    drawCenteredText(ctx.textBuf, 86.0f + boxW * 0.5f, 44.0f, 0.30f, Ui::rgba(74, 222, 128), "100", 0.25f);
    char c1Str[16]; std::snprintf(c1Str, sizeof(c1Str), "%d", winner.count100);
    drawCenteredTextWithShadow(ctx.textBuf, 86.0f + boxW * 0.5f, 62.0f, 0.44f, ctx.clrWhite, c1Str, 0.25f);

    // Box 50
    Ui::panel(162.0f, 34.0f, boxW, boxH, 0.20f, Ui::rgba(20, 25, 45), Ui::rgba(255, 209, 102));
    drawCenteredText(ctx.textBuf, 162.0f + boxW * 0.5f, 44.0f, 0.30f, Ui::rgba(255, 209, 102), "50", 0.25f);
    char c5Str[16]; std::snprintf(c5Str, sizeof(c5Str), "%d", winner.count50);
    drawCenteredTextWithShadow(ctx.textBuf, 162.0f + boxW * 0.5f, 62.0f, 0.44f, ctx.clrWhite, c5Str, 0.25f);

    // Box MISS
    Ui::panel(238.0f, 34.0f, boxW, boxH, 0.20f, Ui::rgba(20, 25, 45), Ui::rgba(235, 45, 65));
    drawCenteredText(ctx.textBuf, 238.0f + boxW * 0.5f, 44.0f, 0.30f, Ui::rgba(235, 45, 65), "MISS", 0.25f);
    char cmStr[16]; std::snprintf(cmStr, sizeof(cmStr), "%d", winner.countMiss);
    drawCenteredTextWithShadow(ctx.textBuf, 238.0f + boxW * 0.5f, 62.0f, 0.44f, ctx.clrWhite, cmStr, 0.25f);

    // Tarjeta del Ganador / Jugador Destacado
    Ui::panel(10.0f, 88.0f, 300.0f, 52.0f, 0.20f, Ui::rgba(18, 22, 38, 240), Ui::rgba(0, 229, 255));
    drawText(ctx.textBuf, 18.0f, 94.0f, 0.28f, ctx.clrMuted, LOC(STR_MULTI_ROOM_WINNER), 0.25f);
    drawTextWithShadow(ctx.textBuf, 18.0f, 108.0f, 0.44f, ctx.clrWhite, winner.name, 0.25f);

    char cmbLabel[32];
    std::snprintf(cmbLabel, sizeof(cmbLabel), LOC(STR_MULTI_COMBO_FMT), winner.combo, winner.maxCombo);
    drawText(ctx.textBuf, 184.0f, 94.0f, 0.28f, ctx.clrMuted, cmbLabel, 0.25f);

    char pRel[32];
    std::snprintf(pRel, sizeof(pRel), LOC(STR_MULTI_ACC_FMT), winner.accuracy);
    drawText(ctx.textBuf, 184.0f, 108.0f, 0.32f, Ui::rgba(0, 229, 255), pRel, 0.25f);

    Ui::statBar(10.0f, 148.0f, 300.0f, 6.0f, 0.20f, winner.accuracy / 100.0f, Ui::rgba(0, 229, 255));

    if (!ctx.multiMgr.mapQueue.empty()) {
        char nextQMsg[64];
        std::snprintf(nextQMsg, sizeof(nextQMsg), LOC(STR_MULTI_NEXT_IN_QUEUE), ctx.multiMgr.mapQueue.front().title.c_str());
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 166.0f, 0.33f, Ui::rgba(255, 209, 102), truncateString(nextQMsg, 36).c_str(), 0.25f);
    } else {
        drawCenteredText(ctx.textBuf, 160.0f, 166.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_NO_MORE_QUEUE), 0.25f);
    }

    // Botón de continuar / retorno
    if (!allFinished) {
        Ui::panel(10.0f, 184.0f, 300.0f, 46.0f, 0.20f, Ui::rgba(45, 34, 20), Ui::rgba(255, 209, 102));
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 207.0f, 0.38f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_BTN_WAIT_FINISH), 0.25f);
    } else if (ctx.multiMgr.isHost()) {
        Ui::button(10.0f, 184.0f, 300.0f, 46.0f, 0.20f, Ui::rgba(16, 185, 129), Ui::rgba(200, 255, 220), true);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 207.0f, 0.44f, ctx.clrWhite, LOC(STR_MULTI_BTN_LOBBY_RETURN), 0.28f);
    } else {
        Ui::button(10.0f, 184.0f, 300.0f, 46.0f, 0.20f, Ui::rgba(20, 25, 45), Ui::rgba(0, 229, 255), false);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 207.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_BTN_FINISHED_WAIT), 0.25f);
    }

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
