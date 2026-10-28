#include "MultiSyncScreen.hpp"
#include "backend/i18n/Localization.hpp"

void runMultiSyncScreen(AppContext& ctx) {
    ctx.audio.update();
    ctx.multiMgr.update(false);

    if (!ctx.multiMgr.inMultiplayer) {
        ctx.multiMgr.startBrowser(ctx.sysUser);
        ctx.appState = STATE_MULTI_BROWSER;
        return;
    }

    // Cancelar con B si el jugador decide abortar
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

    // Fase 1: Carga de recursos a memoria (una sola vez)
    if (!ctx.multiSyncLoaded) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
            C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
            C2D_TargetClear(curTop, ctx.clrBgDark);
            C2D_SceneBegin(curTop);
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);
            Ui::panel(45.0f, 65.0f, 310.0f, 110.0f, 0.20f, Ui::rgba(20, 16, 34, 245), Ui::rgba(0, 229, 255));
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f, 90.0f, 0.46f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_PREP_RES), 0.25f);
            drawCenteredText(ctx.textBuf, 200.0f, 116.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_SYNC_LOADING_SUB), 0.25f);
            Ui::slantRect(65.0f, 140.0f, 270.0f, 8.0f, -4.0f, 0.22f, Ui::rgba(25, 20, 42));
            Ui::slantRect(65.0f, 140.0f, 135.0f, 8.0f, -4.0f, 0.23f, Ui::rgba(0, 229, 255));
        }
        C2D_TargetClear(ctx.bottomScreen, ctx.clrBgDark);
        C2D_SceneBegin(ctx.bottomScreen);
        Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 120.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_LOADING_MAP), 0.25f);
        C3D_FrameEnd(0);

        std::string osuFileToLoad = ctx.findLocalMapFile(ctx.multiMgr.activeBeatmap.folderPath, ctx.multiMgr.activeBeatmap.osuFilename);
        if (osuFileToLoad.empty()) {
            osuFileToLoad = "sdmc:/3ds/osu-3ds/songs/" + ctx.multiMgr.activeBeatmap.folderPath + "/" + ctx.multiMgr.activeBeatmap.osuFilename;
        }

        if (ctx.beatmap.loadFromFile(osuFileToLoad, false)) {
            uint32_t matchMods = (ctx.multiMgr.modMode == MODS_HOST_ONLY)
                ? ctx.multiMgr.players[ctx.multiMgr.hostSlot].activeMods
                : ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods;
            ctx.beatmap.applyMods(matchMods);
            ctx.settings.activeMods = matchMods;

            ctx.bgLoader.cancel();
            ctx.previewDelay = 0;
            if (!ctx.beatmap.fullBackgroundPath.empty()) {
                ctx.bgTex.loadFromFile(ctx.beatmap.fullBackgroundPath, ctx.bgMaxW, ctx.bgMaxH);
            }
            if (ctx.settings.enableVideo && !ctx.beatmap.fullVideoPath.empty()) {
                ctx.videoPlayer.openVideo(ctx.beatmap.fullVideoPath, ctx.beatmap.videoOffsetMs);
            } else {
                ctx.videoPlayer.closeVideo();
            }

            ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
            ctx.applyActiveSkinAudio();
            if (ctx.settings.beatmapHitsounds && !ctx.beatmap.folderPath.empty()) {
                ctx.audio.loadBeatmapHitsounds(ctx.beatmap.folderPath, ctx.beatmap.sampleSetName);
            }

            bool isTaikoMatch = (ctx.settings.gameMode == MODE_TAIKO || ctx.beatmap.mode == 1);
            bool isCatchMatch = (ctx.settings.gameMode == MODE_CATCH || ctx.beatmap.mode == 2);
            bool isManiaMatch = (ctx.settings.gameMode == MODE_MANIA || ctx.beatmap.mode == 3);
            if (isTaikoMatch) {
                ctx.taikoEngine.initFromBeatmap(ctx.beatmap);
            } else if (isCatchMatch) {
                ctx.catchEngine.initFromBeatmap(ctx.beatmap);
            } else if (isManiaMatch) {
                ctx.maniaEngine.initFromBeatmap(ctx.beatmap);
            }

            ctx.multiSyncLeadIn = std::max(0, ctx.beatmap.audioLeadIn);
            if (isTaikoMatch) {
                if (!ctx.taikoEngine.notes.empty()) {
                    int firstObjTime = ctx.taikoEngine.notes.front().time;
                    int minStartWait = 1400;
                    if (firstObjTime < minStartWait) ctx.multiSyncLeadIn = std::max(ctx.multiSyncLeadIn, minStartWait - firstObjTime);
                }
            } else if (isCatchMatch) {
                if (!ctx.catchEngine.fruits.empty()) {
                    int firstObjTime = ctx.catchEngine.fruits.front().time;
                    int minStartWait = 1300;
                    if (firstObjTime < minStartWait) ctx.multiSyncLeadIn = std::max(ctx.multiSyncLeadIn, minStartWait - firstObjTime);
                }
            } else if (isManiaMatch) {
                if (!ctx.maniaEngine.notes.empty()) {
                    int firstObjTime = ctx.maniaEngine.notes.front().time;
                    int minStartWait = 1200;
                    if (firstObjTime < minStartWait) ctx.multiSyncLeadIn = std::max(ctx.multiSyncLeadIn, minStartWait - firstObjTime);
                }
            } else if (!ctx.beatmap.circles.empty()) {
                int firstObjTime = ctx.beatmap.circles.front().time;
                int minStartWait = static_cast<int>(ctx.beatmap.diff.preempt) + 500;
                if (firstObjTime < minStartWait) ctx.multiSyncLeadIn = std::max(ctx.multiSyncLeadIn, minStartWait - firstObjTime);
            }

            ctx.multiSyncLoaded = true;
            ctx.multiMgr.sendMatchLoaded();
        } else {
            ctx.multiMgr.matchStarted = false;
            ctx.multiMgr.players[ctx.multiMgr.localSlot].status = STATUS_MISSING_MAP;
            ctx.multiMgr.sendPlayerUpdate();
            ctx.appState = STATE_MULTI_LOBBY;
            return;
        }
    }

    // Fase 2: Coordinación del Host
    if (ctx.multiMgr.isHost() && ctx.multiSyncLoaded && !ctx.multiMgr.matchGo) {
        if (ctx.multiMgr.areAllPlayersLoaded()) {
            ctx.multiMgr.sendMatchGo(500);
        }
    }

    // Fase 3: Inicio simultáneo al recibir matchGo
    if (ctx.multiMgr.matchGo) {
        ctx.audio.playHitSound(2);
        float speedRate = getModsSpeedMultiplier(ctx.settings.activeMods);
        int totalLeadIn = ctx.multiSyncLeadIn + static_cast<int>(ctx.multiMgr.matchGoDelayMs);
        ctx.audio.startMusic(ctx.beatmap.fullAudioPath, totalLeadIn, speedRate, 0);
        ctx.gameplayFallbackStartTick = osGetTime() + totalLeadIn;

        ctx.combo = 0;
        ctx.prevCombo = 0;
        ctx.lastBrokenCombo = 0;
        ctx.maxCombo = 0;
        ctx.score = 0;
        ctx.count300 = ctx.count100 = ctx.count50 = ctx.countMiss = 0;
        ctx.k1PressCount = ctx.k2PressCount = 0;
        ctx.comboScale = 1.0f;
        ctx.comboBreakAnim = 0.0f;
        ctx.hitErrorTicks.clear();
        ctx.allHitErrors.clear();
        ctx.judgementHistory.clear();
        ctx.hitErrorNeedle = 0.0f;
        ctx.lastUnstableRate = 0.0f;
        ctx.lastMeanHitError = 0.0f;
        ctx.spinnerRPM = 0.0f;
        ctx.spinnerClearAnim = 0.0f;
        ctx.spinnerBonusAnim = 0.0f;
        ctx.lastSpinnerBonus = 0;
        ctx.shakeTimer = 0.0f;
        ctx.shakeIntensity = 0.0f;
        ctx.shakeX = 0.0f;
        ctx.shakeY = 0.0f;
        ctx.hp = 1.0f;
        ctx.stageFailed = false;
        ctx.newRecordAchieved = false;
        ctx.lastEarnedPP = 0.0f;
        ctx.isPaused = false;
        ctx.pauseOption = 0;
        ctx.cursorOsuX = 256.0f;
        ctx.cursorOsuY = 192.0f;
        ctx.prevCursorOsuX = 256.0f;
        ctx.prevCursorOsuY = 192.0f;
        ctx.cursorTrail.clear();
        ctx.smokeParticles.clear();
        ctx.popups.clear();
        ctx.particles.clear();
        ctx.overtakeFlashAnim = 0.0f;
        ctx.hasOvertakenBest = false;
        ctx.skipAvailable = false;
        ctx.skipTargetMs = 0;
        ctx.inBreakPeriod = false;
        ctx.breakRemainingMs = 0;
        ctx.breakTotalMs = 0;
        ctx.breakNextObjPos = !ctx.beatmap.circles.empty() ? Vec2(ctx.beatmap.circles.front().x, ctx.beatmap.circles.front().y) : Vec2(256.0f, 192.0f);
        ctx.inKiai = false;
        ctx.prevInKiai = false;
        ctx.kiaiGlow = 0.0f;
        ctx.kiaiBurstFlash = 0.0f;
        ctx.comboBurstTimer = 0;
        ctx.comboBurstSide = 0;
        ctx.comboBurstValue = 0;
        ctx.lastComboBurstMilestone = 0;
        ctx.lastNcStep = -999999;
        ctx.taikoGuideTimer = 240.0f;

        ctx.multiMgr.players[ctx.multiMgr.localSlot].resetForMatch();
        ctx.multiMgr.matchStarted = false;
        ctx.multiMgr.matchGo = false;

        ctx.appState = STATE_PLAYING;
        return;
    }

    // Renderizado de pantalla de espera y sincronización estilo osu!lazer
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dBg = depthShift(-3.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(12, 8, 22, 195));
        }

        float dPan = depthShift(2.5f);

        // Cabecera lazer
        Ui::slantRectGrad(8.0f + dPan, 4.0f, 85.0f, 20.0f, -4.0f, 0.20f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawCenteredTextWithShadow(ctx.textBuf, 50.0f + dPan, 14.0f, 0.35f, ctx.clrWhite, "osu!3DS", 0.22f);
        drawTextWithShadow(ctx.textBuf, 102.0f + dPan, 8.0f, 0.40f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_TITLE), 0.22f);

        char syncHeader[48];
        std::snprintf(syncHeader, sizeof(syncHeader), LOC(STR_MULTI_SYNC_READY_FMT), ctx.multiMgr.getLoadedPlayerCount(), ctx.multiMgr.getActivePlayerCount());
        Ui::pill(296.0f + dPan, 5.0f, 96.0f, 18.0f, 0.22f, Ui::rgba(20, 34, 58, 240));
        drawRectOutline(296.0f + dPan, 5.0f, 96.0f, 18.0f, 1.0f, Ui::rgba(0, 229, 255), 0.24f);
        drawCenteredText(ctx.textBuf, 344.0f + dPan, 14.0f, 0.31f, Ui::rgba(0, 229, 255), syncHeader, 0.25f);

        // 4 Ranuras horizontales de jugadores sincronizándose (estilo lobby actual)
        for (int s = 0; s < MULTI_MAX_PLAYERS; ++s) {
            float sy = 32.0f + s * 42.0f;
            float sx = 10.0f + dPan;
            float sw = 380.0f;
            float sh = 38.0f;

            if (ctx.multiMgr.players[s].active) {
                const auto& p = ctx.multiMgr.players[s];
                bool isLoaded = (p.status == STATUS_LOADED);
                u32 accentClr = isLoaded ? Ui::rgba(74, 222, 128) : Ui::rgba(255, 209, 102);

                Ui::panel(sx, sy, sw, sh, 0.20f, Ui::rgba(18, 22, 38, 240), accentClr);
                drawRectOutline(sx, sy, sw, sh, 1.2f, accentClr, 0.22f);

                drawTextWithShadow(ctx.textBuf, sx + 12.0f, sy + 4.0f, 0.42f, ctx.clrWhite, p.name, 0.25f);

                if (p.isHost) {
                    Ui::pill(sx + 104.0f, sy + 5.0f, 38.0f, 14.0f, 0.26f, Ui::rgba(255, 209, 102));
                    drawCenteredText(ctx.textBuf, sx + 123.0f, sy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_CARD_HOST), 0.30f);
                } else if (p.isLocal) {
                    Ui::pill(sx + 104.0f, sy + 5.0f, 28.0f, 14.0f, 0.26f, Ui::rgba(0, 229, 255));
                    drawCenteredText(ctx.textBuf, sx + 118.0f, sy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
                }

                char pInfo[48];
                std::snprintf(pInfo, sizeof(pInfo), "Ping: %ums", p.pingMs);
                drawText(ctx.textBuf, sx + 12.0f, sy + 21.0f, 0.28f, Ui::rgba(148, 163, 184), pInfo, 0.25f);

                Ui::pill(sx + sw - 120.0f, sy + 9.0f, 108.0f, 20.0f, 0.26f, isLoaded ? Ui::rgba(16, 50, 35) : Ui::rgba(45, 34, 20));
                drawRectOutline(sx + sw - 120.0f, sy + 9.0f, 108.0f, 20.0f, 1.0f, accentClr, 0.28f);
                drawCenteredText(ctx.textBuf, sx + sw - 66.0f, sy + 19.0f, 0.29f, accentClr,
                    isLoaded ? LOC(STR_MULTI_SYNC_RES_READY) : LOC(STR_MULTI_SYNC_LOADING_MAP), 0.30f);
            } else {
                Ui::panel(sx, sy, sw, sh, 0.18f, Ui::rgba(15, 12, 24, 180));
                drawRectOutline(sx, sy, sw, sh, 1.0f, Ui::rgba(35, 30, 50), 0.20f);
                drawCenteredText(ctx.textBuf, sx + sw * 0.5f, sy + 19.0f, 0.34f, Ui::rgba(90, 85, 115), LOC(STR_MULTI_EMPTY_SLOT), 0.25f);
            }
        }

        // Pie de pantalla superior
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.30f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dPan, 222.0f, 0.33f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_FOOTER), 0.32f);
        drawRightAlignedText(ctx.textBuf, 392.0f + dPan, 222.0f, 0.28f, Ui::rgba(135, 130, 165), "400x240", 0.32f);

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // PANTALLA INFERIOR TÁCTIL (320x240)
    C2D_TargetClear(ctx.bottomScreen, ctx.clrBgDark);
    C2D_SceneBegin(ctx.bottomScreen);
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);

    drawTextWithShadow(ctx.textBuf, 12.0f, 8.0f, 0.40f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_CLOCKS), 0.20f);

    // Tarjeta del mapa en preparación
    Ui::panel(10.0f, 32.0f, 300.0f, 150.0f, 0.20f, Ui::rgba(13, 17, 31, 255), Ui::rgba(0, 229, 255));
    drawTextWithShadow(ctx.textBuf, 20.0f, 40.0f, 0.30f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_SYNC_MAP_IN_PLAY), 0.25f);
    drawTextWithShadow(ctx.textBuf, 20.0f, 58.0f, 0.48f, ctx.clrWhite, truncateString(ctx.multiMgr.activeBeatmap.title, 22).c_str(), 0.25f);
    drawText(ctx.textBuf, 20.0f, 80.0f, 0.34f, Ui::rgba(148, 163, 184), truncateString(ctx.multiMgr.activeBeatmap.artist, 26).c_str(), 0.25f);

    char vBuf[64];
    std::snprintf(vBuf, sizeof(vBuf), LOC(STR_MULTI_DIFF_STARS_FMT), ctx.multiMgr.activeBeatmap.version.c_str(), ctx.multiMgr.activeBeatmap.starRating);
    drawTextWithShadow(ctx.textBuf, 20.0f, 100.0f, 0.34f, Ui::rgba(255, 209, 102), vBuf, 0.25f);

    float pulse = 0.5f + 0.5f * std::sin(ctx.animTick * 0.12f);
    Ui::pill(20.0f, 130.0f, 280.0f, 36.0f, 0.22f, Ui::rgba(20, 25, 45));
    drawRectOutline(20.0f, 130.0f, 280.0f, 36.0f, 1.2f, Ui::rgba(0, 229, 255), 0.24f);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 148.0f, 0.38f, Ui::lerpColor(Ui::rgba(0, 229, 255), ctx.clrWhite, pulse), LOC(STR_MULTI_SYNC_AUDIO_LEADIN), 0.26f);

    // Botón de cancelación
    Ui::button(10.0f, 194.0f, 300.0f, 38.0f, 0.20f, Ui::rgba(35, 18, 26), Ui::rgba(235, 45, 65), false);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 213.0f, 0.38f, ctx.clrWhite, LOC(STR_MULTI_SYNC_CANCEL_BTN), 0.25f);

    C3D_FrameEnd(0);
}
