#include "MultiResultsScreen.hpp"

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
    bool touchedContinue = ctx.touchJustDown && (ctx.touch.px >= 12.0f && ctx.touch.px <= 308.0f && ctx.touch.py >= 188.0f && ctx.touch.py <= 232.0f);

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
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

        Ui::pill(12.0f + dTop, 10.0f, 52.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
        drawCenteredText(ctx.textBuf, 38.0f + dTop, 18.0f, 0.36f, ctx.clrWhite, "osu!", 0.30f);

        drawText(ctx.textBuf, 70.0f + dTop, 11.0f, 0.44f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_RESULTS_TITLE), 0.30f);

        std::string rSongInfo = ctx.multiMgr.activeBeatmap.title + " [" + ctx.multiMgr.activeBeatmap.version + "]";
        drawText(ctx.textBuf, 250.0f + dTop, 12.0f, 0.32f, ctx.clrMuted, truncateString(rSongInfo, 22).c_str(), 0.30f);

        for (size_t r = 0; r < ranked.size() && r < 4; ++r) {
            const auto& item = ranked[r];
            float cardY = 34.0f + r * 44.0f;
            float cardX = 12.0f + dTop;
            float cardW = 376.0f;
            float cardH = 40.0f;

            u32 medalClr = item.player->isFailed ? Ui::rgba(235, 45, 45) :
                           ((r == 0) ? Ui::rgba(255, 195, 45) :
                           ((r == 1) ? Ui::rgba(40, 215, 235) :
                           ((r == 2) ? Ui::rgba(180, 80, 240) : Ui::rgba(100, 100, 130))));

            Ui::panel(cardX, cardY, cardW, cardH, 0.20f, Ui::rgba(22, 19, 36, 240), medalClr);

            Ui::panel(cardX + 6.0f, cardY + 6.0f, 26.0f, 28.0f, 0.22f, medalClr);
            char rankTxt[8];
            if (item.player->isFailed) {
                std::snprintf(rankTxt, sizeof(rankTxt), "FAIL");
            } else {
                std::snprintf(rankTxt, sizeof(rankTxt), "#%u", static_cast<unsigned int>(r + 1));
            }
            drawCenteredText(ctx.textBuf, cardX + 19.0f, cardY + 20.0f, 0.32f, Ui::rgba(0, 0, 0, 250), rankTxt, 0.28f);

            drawText(ctx.textBuf, cardX + 38.0f, cardY + 6.0f, 0.44f, ctx.clrWhite, item.player->name, 0.25f);
            if (item.player->isFailed) {
                Ui::pill(cardX + 105.0f, cardY + 7.0f, 32.0f, 13.0f, 0.26f, Ui::rgba(235, 45, 45));
                drawCenteredText(ctx.textBuf, cardX + 121.0f, cardY + 13.0f, 0.26f, ctx.clrWhite, LOC(STR_MULTI_FAIL), 0.30f);
            }
            if (item.player->isLocal) {
                float tuX = item.player->isFailed ? (cardX + 142.0f) : (cardX + 140.0f);
                Ui::pill(tuX, cardY + 7.0f, 24.0f, 13.0f, 0.26f, Ui::rgba(40, 215, 235));
                drawCenteredText(ctx.textBuf, tuX + 12.0f, cardY + 13.0f, 0.26f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
            }

            std::string mStr = (item.player->activeMods != MOD_NONE) ? ("+" + formatModsString(item.player->activeMods)) : "No Mod";
            drawText(ctx.textBuf, cardX + 38.0f, cardY + 22.0f, 0.30f, Ui::rgba(40, 215, 235), mStr.c_str(), 0.25f);

            std::string scStr = formatScoreThousands(item.player->score);
            drawText(ctx.textBuf, cardX + 180.0f, cardY + 10.0f, 0.52f, ctx.clrWhite, scStr.c_str(), 0.25f);

            char accComb[32];
            std::snprintf(accComb, sizeof(accComb), "%.2f%%", item.player->accuracy);
            drawText(ctx.textBuf, cardX + 276.0f, cardY + 6.0f, 0.34f, Ui::rgba(40, 215, 235), accComb, 0.25f);
            char cmb[16];
            std::snprintf(cmb, sizeof(cmb), "%dx", item.player->maxCombo);
            drawText(ctx.textBuf, cardX + 282.0f, cardY + 22.0f, 0.30f, ctx.clrMuted, cmb, 0.25f);

            u32 gClr = getScoreGradeColor(item.player->grade, ctx.clrCursor);
            drawCenteredText(ctx.textBuf, cardX + cardW - 20.0f, cardY + 20.0f, 0.70f, gClr, item.player->grade, 0.28f);
        }

        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::glow(200.0f + dToast, 224.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
            Ui::pill(30.0f + dToast, 212.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
            drawRectOutline(30.0f + dToast, 212.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        } else if (!allFinished) {
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dTop, 222.0f, 0.33f, Ui::rgba(255, 205, 50), LOC(STR_MULTI_WAITING_ALL), 0.32f);
        } else {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 12.0f + dTop, byY, "A", ctx.multiMgr.isHost() ? LOC(STR_MULTI_RETURN_LOBBY) : LOC(STR_MULTI_WAIT_HOST), Ui::rgba(255, 75, 80));
            drawKeyBadge(ctx.textBuf, 160.0f + dTop, byY, "B", LOC(STR_BACK), Ui::rgba(255, 195, 45));
            drawKeyBadge(ctx.textBuf, 240.0f + dTop, byY, "X", LOC(STR_MULTI_TABS), Ui::rgba(50, 165, 255));
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.02f);

    const char* resTabs[4] = { LOC(STR_MULTI_TAB_HITS), "Hit Error", "1v1 vs #1", "Chat" };
    for (int t = 0; t < 4; ++t) {
        float tx = 10.0f + t * 74.0f;
        bool activeT = (ctx.multiResultsTab == t);
        Ui::panel(tx, 8.0f, 70.0f, 22.0f, 0.20f, activeT ? Ui::rgba(40, 215, 235) : Ui::rgba(24, 20, 36, 220));
        drawCenteredText(ctx.textBuf, tx + 35.0f, 19.0f, 0.30f, activeT ? Ui::rgba(0, 0, 0, 250) : ctx.clrMuted, resTabs[t], 0.25f);
    }

    const auto& winner = ranked.empty() ? ctx.multiMgr.players[ctx.multiMgr.localSlot] : *ranked[0].player;

    float boxW = 68.0f;
    float boxH = 50.0f;

    // Box 300
    Ui::panel(12.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(40, 215, 235));
    drawCenteredText(ctx.textBuf, 12.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(40, 215, 235), "300", 0.25f);
    char c3Str[16]; std::snprintf(c3Str, sizeof(c3Str), "%d", winner.count300);
    drawCenteredText(ctx.textBuf, 12.0f + boxW * 0.5f, 68.0f, 0.48f, ctx.clrWhite, c3Str, 0.25f);

    // Box 100
    Ui::panel(86.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(60, 215, 120));
    drawCenteredText(ctx.textBuf, 86.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(60, 215, 120), "100", 0.25f);
    char c1Str[16]; std::snprintf(c1Str, sizeof(c1Str), "%d", winner.count100);
    drawCenteredText(ctx.textBuf, 86.0f + boxW * 0.5f, 68.0f, 0.48f, ctx.clrWhite, c1Str, 0.25f);

    // Box 50
    Ui::panel(160.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(255, 185, 40));
    drawCenteredText(ctx.textBuf, 160.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(255, 185, 40), "50", 0.25f);
    char c5Str[16]; std::snprintf(c5Str, sizeof(c5Str), "%d", winner.count50);
    drawCenteredText(ctx.textBuf, 160.0f + boxW * 0.5f, 68.0f, 0.48f, ctx.clrWhite, c5Str, 0.25f);

    // Box MISS
    Ui::panel(234.0f, 38.0f, boxW, boxH, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(240, 70, 70));
    drawCenteredText(ctx.textBuf, 234.0f + boxW * 0.5f, 48.0f, 0.32f, Ui::rgba(240, 70, 70), "MISS", 0.25f);
    char cmStr[16]; std::snprintf(cmStr, sizeof(cmStr), "%d", winner.countMiss);
    drawCenteredText(ctx.textBuf, 234.0f + boxW * 0.5f, 68.0f, 0.48f, ctx.clrWhite, cmStr, 0.25f);

    Ui::panel(12.0f, 96.0f, 296.0f, 54.0f, 0.20f, Ui::rgba(22, 19, 36, 240));
    drawText(ctx.textBuf, 20.0f, 102.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_SEL_PLAYER), 0.25f);
    drawText(ctx.textBuf, 20.0f, 116.0f, 0.42f, ctx.clrWhite, winner.name, 0.25f);

    char cmbLabel[32];
    std::snprintf(cmbLabel, sizeof(cmbLabel), "Combo: %d / %dx", winner.combo, winner.maxCombo);
    drawText(ctx.textBuf, 190.0f, 102.0f, 0.30f, ctx.clrMuted, cmbLabel, 0.25f);

    char pRel[32];
    std::snprintf(pRel, sizeof(pRel), "%.2f%%", winner.accuracy);
    drawText(ctx.textBuf, 20.0f, 134.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_REL_ACC), 0.25f);
    drawText(ctx.textBuf, 250.0f, 134.0f, 0.32f, Ui::rgba(40, 215, 235), pRel, 0.25f);

    Ui::statBar(12.0f, 154.0f, 296.0f, 6.0f, 0.20f, winner.accuracy / 100.0f, Ui::rgba(40, 215, 235));

    if (!ctx.multiMgr.mapQueue.empty()) {
        char nextQMsg[64];
        std::snprintf(nextQMsg, sizeof(nextQMsg), LOC(STR_MULTI_NEXT_IN_QUEUE), ctx.multiMgr.mapQueue.front().title.c_str());
        drawCenteredText(ctx.textBuf, 160.0f, 172.0f, 0.34f, Ui::rgba(255, 205, 50), truncateString(nextQMsg, 36).c_str(), 0.25f);
    } else {
        drawCenteredText(ctx.textBuf, 160.0f, 172.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_QUEUE_DONE), 0.25f);
    }

    if (!allFinished) {
        Ui::panel(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(50, 35, 70, 240), Ui::rgba(130, 80, 200));
        drawCenteredText(ctx.textBuf, 160.0f, 203.0f, 0.38f, Ui::rgba(255, 215, 60), LOC(STR_MULTI_WAITING_ALL), 0.25f);
        drawCenteredText(ctx.textBuf, 160.0f, 219.0f, 0.28f, ctx.clrWhite, LOC(STR_MULTI_WAITING_ALL), 0.25f);
    } else if (ctx.multiMgr.isHost()) {
        Ui::button(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(30, 165, 80, 240), Ui::rgba(100, 255, 160), true);
        Ui::pill(20.0f, 198.0f, 24.0f, 24.0f, 0.24f, Ui::rgba(100, 255, 160));
        drawCenteredText(ctx.textBuf, 32.0f, 210.0f, 0.34f, Ui::rgba(0, 0, 0, 240), "A", 0.28f);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 210.0f, 0.44f, ctx.clrWhite, LOC(STR_MULTI_RETURN_LOBBY), 0.28f);
    } else {
        Ui::button(12.0f, 188.0f, 296.0f, 44.0f, 0.20f, Ui::rgba(28, 38, 62, 240), Ui::rgba(60, 140, 230), false);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 203.0f, 0.38f, Ui::rgba(100, 200, 255), LOC(STR_MULTI_MATCH_FINISHED), 0.25f);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 219.0f, 0.30f, ctx.clrWhite, LOC(STR_MULTI_WAIT_HOST), 0.25f);
    }

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
