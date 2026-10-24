#include "MultiSyncScreen.hpp"

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
        // Dibujar una pantalla de carga rapida antes de bloquear lectura de SD
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
            C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
            C2D_TargetClear(curTop, ctx.clrBgDark);
            C2D_SceneBegin(curTop);
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
            Ui::panel(50.0f, 80.0f, 300.0f, 80.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, 200.0f, 108.0f, 0.44f, ctx.clrWhite, LOC(STR_MULTI_LOADING_RESOURCES), 0.25f);
            drawCenteredText(ctx.textBuf, 200.0f, 134.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_PREP_MAP), 0.25f);
        }
        C2D_TargetClear(ctx.bottomScreen, ctx.clrBgDark);
        C2D_SceneBegin(ctx.bottomScreen);
        Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
        drawCenteredText(ctx.textBuf, 160.0f, 120.0f, 0.36f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_PREP_MAP), 0.25f);
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

    // Fase 2: Coordinacion del Host (comprueba si todos estan cargados)
    if (ctx.multiMgr.isHost() && ctx.multiSyncLoaded && !ctx.multiMgr.matchGo) {
        if (ctx.multiMgr.areAllPlayersLoaded()) {
            ctx.multiMgr.sendMatchGo(500); // 500 ms de margen para sincronizar el inicio simultaneo
        }
    }

    // Fase 3: Inicio simultaneo al recibir matchGo
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

    // Renderizado de la pantalla de espera de sincronizacion pre-partida
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dBg = depthShift(-3.0f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, 180));
        } else {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
        }

        float dPan = depthShift(2.5f);
        Ui::panel(40.0f + dPan, 35.0f, 320.0f, 150.0f, 0.20f, Ui::rgba(20, 16, 32, 240), Ui::rgba(40, 215, 235));
        drawCenteredText(ctx.textBuf, 200.0f + dPan, 52.0f, 0.44f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_WAIT_OTHER_CONSOLES), 0.25f);

        char syncTxt[64];
        std::snprintf(syncTxt, sizeof(syncTxt), "%s: %d / %d", LOC(STR_READY), ctx.multiMgr.getLoadedPlayerCount(), ctx.multiMgr.getActivePlayerCount());
        drawCenteredText(ctx.textBuf, 200.0f + dPan, 72.0f, 0.34f, ctx.clrWhite, syncTxt, 0.25f);

        // Ranuras de jugadores con su estado de carga
        for (int s = 0; s < MULTI_MAX_PLAYERS; ++s) {
            if (ctx.multiMgr.players[s].active) {
                float py = 92.0f + s * 20.0f;
                drawText(ctx.textBuf, 60.0f + dPan, py, 0.32f, ctx.clrWhite, ctx.multiMgr.players[s].name, 0.25f);
                bool isL = (ctx.multiMgr.players[s].status == STATUS_LOADED);
                Ui::pill(270.0f + dPan, py, 70.0f, 16.0f, 0.26f, isL ? Ui::rgba(30, 180, 80) : Ui::rgba(220, 140, 30));
                drawCenteredText(ctx.textBuf, 305.0f + dPan, py + 8.0f, 0.28f, ctx.clrWhite, isL ? LOC(STR_READY) : LOC(STR_MULTI_LOADING_RESOURCES), 0.30f);
            }
        }

        drawCenteredText(ctx.textBuf, 200.0f + dPan, 205.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_SAME_TIME), 0.25f);
        drawCenteredText(ctx.textBuf, 200.0f + dPan, 222.0f, 0.28f, Ui::rgba(255, 90, 90), LOC(STR_MULTI_BACK_TO_ROOM), 0.25f);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrBgDark);
    C2D_SceneBegin(ctx.bottomScreen);
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
    Ui::panel(16.0f, 40.0f, 288.0f, 160.0f, 0.20f, Ui::rgba(22, 18, 36, 240), Ui::rgba(85, 75, 110));
    drawCenteredText(ctx.textBuf, 160.0f, 65.0f, 0.44f, ctx.clrWhite, truncateString(ctx.multiMgr.activeBeatmap.title, 24).c_str(), 0.25f);
    drawCenteredText(ctx.textBuf, 160.0f, 85.0f, 0.34f, ctx.clrMuted, truncateString(ctx.multiMgr.activeBeatmap.artist, 26).c_str(), 0.25f);
    drawCenteredText(ctx.textBuf, 160.0f, 105.0f, 0.34f, Ui::rgba(255, 205, 50), truncateString(ctx.multiMgr.activeBeatmap.version, 24).c_str(), 0.25f);
    drawCenteredText(ctx.textBuf, 160.0f, 145.0f, 0.38f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_SYNC_CLOCK), 0.25f);
    C3D_FrameEnd(0);
}
