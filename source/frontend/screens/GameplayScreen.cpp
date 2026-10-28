#include "GameplayScreen.hpp"
#include "StandardMode.hpp"

void runGameplayScreen(AppContext& ctx) {
    // =========================================================
    // 0. TRANSICIÓN DE PRESENTACIÓN Y CARGA ESCALONADA ESTILO OSU!LAZER
    //    Evita cualquier congelamiento o bajón de FPS al iniciar el mapa.
    // =========================================================
    if (ctx.inSongIntro) {
        ctx.songIntroTick++;

        // Permitir cancelar con B antes de que arranque la canción
        if (ctx.kDown & KEY_B) {
            ctx.inSongIntro = false;
            ctx.isReplayPlaying = false;
            ctx.audio.stopMusic();
            ctx.videoPlayer.closeVideo();
            ctx.applyActiveSkinAudio();
            ctx.appState = STATE_SONG_SELECT;
            ctx.updateSongPreview();
            return;
        }

        // Ejecutar 1 tarea pesada por fotograma tras completar la animación de entrada (tick >= 10)
        if (ctx.songIntroTick == 10 && ctx.songIntroLoadStage == 0) {
            ctx.songIntroLoadStage = 1;
            if (!ctx.beatmap.loadFromFile(ctx.songIntroOsuPath, false)) {
                ctx.inSongIntro = false;
                ctx.isReplayPlaying = false;
                ctx.appState = STATE_SONG_SELECT;
                ctx.updateSongPreview();
                return;
            }
            ctx.beatmap.applyMods(ctx.settings.activeMods);
        } else if (ctx.songIntroTick == 13 && ctx.songIntroLoadStage == 1) {
            ctx.songIntroLoadStage = 2;
            // Reutilizar la textura de fondo si ya fue cargada asíncronamente en el selector
            if (!ctx.beatmap.fullBackgroundPath.empty()) {
                if (!(ctx.bgTex.loaded && ctx.bgTex.sourcePath == ctx.beatmap.fullBackgroundPath)) {
                    ctx.bgTex.loadFromFile(ctx.beatmap.fullBackgroundPath, ctx.bgMaxW, ctx.bgMaxH);
                }
            }
            bool isTaiko = (ctx.songIntroMode == 1);
            bool isCatch = (ctx.songIntroMode == 2);
            bool isMania = (ctx.songIntroMode == 3);
            if (isTaiko)      ctx.taikoEngine.initFromBeatmap(ctx.beatmap);
            else if (isCatch) ctx.catchEngine.initFromBeatmap(ctx.beatmap);
            else if (isMania) ctx.maniaEngine.initFromBeatmap(ctx.beatmap);

            ctx.localRecords = ctx.scoreMgr.getTopRecords(ctx.songIntroOsuPath, 4, ctx.songIntroMode);
            ctx.breakNextObjPos = !ctx.beatmap.circles.empty()
                ? Vec2(ctx.beatmap.circles.front().x, ctx.beatmap.circles.front().y)
                : Vec2(256.0f, 192.0f);
        } else if (ctx.songIntroTick == 16 && ctx.songIntroLoadStage == 2) {
            ctx.songIntroLoadStage = 3;
            ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
            ctx.applyActiveSkinAudio();
            if (ctx.settings.beatmapHitsounds && !ctx.beatmap.folderPath.empty()) {
                ctx.audio.loadBeatmapHitsounds(ctx.beatmap.folderPath, ctx.beatmap.sampleSetName);
            }
            if (ctx.settings.enableVideo && !ctx.beatmap.fullVideoPath.empty()) {
                ctx.videoPlayer.openVideo(ctx.beatmap.fullVideoPath, ctx.beatmap.videoOffsetMs);
            } else {
                ctx.videoPlayer.closeVideo();
            }
        } else if (ctx.songIntroTick == 19 && ctx.songIntroLoadStage == 3) {
            ctx.songIntroLoadStage = 4;
            bool isTaiko = (ctx.songIntroMode == 1);
            bool isCatch = (ctx.songIntroMode == 2);
            bool isMania = (ctx.songIntroMode == 3);
            int leadIn = std::max(0, ctx.beatmap.audioLeadIn);
            if (isTaiko && !ctx.taikoEngine.notes.empty()) {
                int firstObjTime = ctx.taikoEngine.notes.front().time;
                if (firstObjTime < 1400) leadIn = std::max(leadIn, 1400 - firstObjTime);
            } else if (isCatch && !ctx.catchEngine.fruits.empty()) {
                int firstObjTime = ctx.catchEngine.fruits.front().time;
                if (firstObjTime < 1300) leadIn = std::max(leadIn, 1300 - firstObjTime);
            } else if (isMania && !ctx.maniaEngine.notes.empty()) {
                int firstObjTime = ctx.maniaEngine.notes.front().time;
                if (firstObjTime < 1200) leadIn = std::max(leadIn, 1200 - firstObjTime);
            } else if (!ctx.beatmap.circles.empty()) {
                int firstObjTime = ctx.beatmap.circles.front().time;
                int minStartWait = static_cast<int>(ctx.beatmap.diff.preempt) + 500;
                if (firstObjTime < minStartWait) leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
            ctx.songIntroLeadInMs = leadIn;
            float speedRate = getModsSpeedMultiplier(ctx.settings.activeMods);
            // Pre-carga y decodifica los 3 buffers de audio iniciales en RAM y congela el reloj
            ctx.audio.startMusic(ctx.beatmap.fullAudioPath, leadIn, speedRate, 0);
            ctx.audio.pauseMusic();
        } else if (ctx.songIntroTick == 21 && ctx.songIntroLoadStage == 4) {
            ctx.songIntroLoadStage = 5;
        }

        // Animación de entrada (ticks 1..10) y salida (ticks 24..38) a 60 FPS estables
        float inT = std::min(1.0f, static_cast<float>(ctx.songIntroTick) / 10.0f);
        float easeIn = std::sin(inT * (static_cast<float>(M_PI) * 0.5f));
        float outT = (ctx.songIntroTick > 24)
            ? std::min(1.0f, static_cast<float>(ctx.songIntroTick - 24) / 14.0f)
            : 0.0f;
        float easeOut = outT * outT;

        if (ctx.songIntroTick >= 38) {
            ctx.inSongIntro = false;
            ctx.audio.resumeMusic();
            ctx.gameplayFallbackStartTick = osGetTime() + ctx.songIntroLeadInMs;
            ctx.screenFade = 0.35f;
        } else {
            u32 lazerPink = Ui::rgba(255, 102, 171);
            u32 lazerCyan = Ui::rgba(56, 210, 255);
            u32 starClr   = getStarRatingColor(ctx.songIntroStars);

            float loadPct = (ctx.songIntroLoadStage == 0) ? 0.15f
                          : (ctx.songIntroLoadStage == 1) ? 0.38f
                          : (ctx.songIntroLoadStage == 2) ? 0.64f
                          : (ctx.songIntroLoadStage == 3) ? 0.86f : 1.0f;
            const char* stageTxt = (ctx.songIntroLoadStage <= 1) ? LOC(STR_GAME_LOAD_STAGE_1)
                                 : (ctx.songIntroLoadStage == 2) ? LOC(STR_GAME_LOAD_STAGE_2)
                                 : (ctx.songIntroLoadStage == 3) ? LOC(STR_GAME_LOAD_STAGE_3)
                                 : (ctx.songIntroLoadStage == 4) ? LOC(STR_GAME_LOAD_STAGE_4)
                                 : LOC(STR_GAME_LOAD_STAGE_READY);

            float slideOffset = (1.0f - easeIn) * -320.0f + easeOut * 360.0f;
            u8 panelAlpha = static_cast<u8>(255.0f * easeIn * (1.0f - easeOut));

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            // --- PANTALLA SUPERIOR: SPLASH DE PRESENTACIÓN DEL BEATMAP ESTILO OSU!LAZER ---
            for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
                C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
                float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
                auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

                C2D_TargetClear(curTop, Ui::rgba(10, 7, 20));
                C2D_SceneBegin(curTop);

                float dBg = depthShift(-3.0f);
                Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick),
                             Ui::rgba(22, 14, 38), Ui::rgba(10, 7, 20), lazerPink, 0.01f);
                if (ctx.bgTex.loaded) {
                    ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
                    u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 220) / 100);
                    C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(10, 7, 18, std::max<u8>(110, dimAlpha)));
                }

                // Franjas diagonales decorativas estilo osu!lazer
                float dCard = depthShift(2.5f) + slideOffset;
                Ui::slantRect(-30.0f + dCard * 0.6f, 0.0f, 95.0f, 240.0f, -18.0f, 0.15f,
                              Ui::withAlpha(lazerPink, static_cast<u8>(panelAlpha * 0.22f)));
                Ui::slantRect(335.0f - dCard * 0.6f, 0.0f, 95.0f, 240.0f, -18.0f, 0.15f,
                              Ui::withAlpha(lazerCyan, static_cast<u8>(panelAlpha * 0.20f)));

                // Tarjeta central translúcida de presentación del mapa
                float cX = 36.0f + dCard;
                float cY = 32.0f;
                float cW = 328.0f;
                float cH = 176.0f;
                Ui::panel(cX, cY, cW, cH, 0.25f, Ui::rgba(16, 11, 30, static_cast<u8>(panelAlpha * 0.86f)), lazerPink);

                // Insignia superior: Modo + Estrellas
                const char* modeName = (ctx.songIntroMode == 1) ? "osu!taiko"
                                     : (ctx.songIntroMode == 2) ? "osu!catch"
                                     : (ctx.songIntroMode == 3) ? "osu!mania"
                                     : "osu!standard";
                Ui::slantRect(cX + 14.0f, cY + 12.0f, 98.0f, 20.0f, -5.0f, 0.28f, lazerPink);
                drawCenteredText(ctx.textBuf, cX + 63.0f, cY + 22.0f, 0.35f, ctx.clrWhite, modeName, 0.31f);

                char starTxt[24];
                std::snprintf(starTxt, sizeof(starTxt), "* %.2f", ctx.songIntroStars);
                Ui::slantRect(cX + 118.0f, cY + 12.0f, 64.0f, 20.0f, -5.0f, 0.28f, starClr);
                drawCenteredText(ctx.textBuf, cX + 150.0f, cY + 22.0f, 0.35f, Ui::rgba(18, 12, 28), starTxt, 0.31f);

                if (ctx.settings.activeMods != MOD_NONE) {
                    std::string mStr = formatModsString(ctx.settings.activeMods);
                    Ui::slantRect(cX + 188.0f, cY + 12.0f, 76.0f, 20.0f, -5.0f, 0.28f, Ui::rgba(46, 34, 78, 225));
                    drawCenteredText(ctx.textBuf, cX + 226.0f, cY + 22.0f, 0.33f, Ui::rgba(255, 210, 60), mStr.c_str(), 0.31f);
                }

                // Título de la canción y Artista
                drawTextWithShadow(ctx.textBuf, cX + 14.0f, cY + 40.0f, 0.52f, ctx.clrWhite,
                                   truncateString(ctx.songIntroTitle, 25).c_str(), 0.31f);
                drawTextWithShadow(ctx.textBuf, cX + 14.0f, cY + 63.0f, 0.38f, Ui::rgba(215, 210, 240),
                                   truncateString(ctx.songIntroArtist, 32).c_str(), 0.31f);

                // Dificultad y Mapeador
                char diffCreator[96];
                std::snprintf(diffCreator, sizeof(diffCreator), LOC(STR_GAME_MAPPED_BY_FMT),
                              truncateString(ctx.songIntroVersion, 18).c_str(),
                              ctx.songIntroCreator.empty() ? "Unknown" : truncateString(ctx.songIntroCreator, 16).c_str());
                drawTextWithShadow(ctx.textBuf, cX + 14.0f, cY + 84.0f, 0.35f, lazerCyan, diffCreator, 0.31f);

                // Píldoras de CS / AR / OD / HP
                float gY = cY + 108.0f;
                Ui::metricGauge(cX + 14.0f,  gY, "CS", ctx.songIntroCS, 10.0f, ctx.textBuf, 0.29f);
                Ui::metricGauge(cX + 92.0f,  gY, "AR", ctx.songIntroAR, 10.0f, ctx.textBuf, 0.29f);
                Ui::metricGauge(cX + 170.0f, gY, "OD", ctx.songIntroOD, 10.0f, ctx.textBuf, 0.29f);
                Ui::metricGauge(cX + 248.0f, gY, "HP", ctx.songIntroHP, 10.0f, ctx.textBuf, 0.29f);

                // Barra de progreso inclinada estilo osu!lazer en la base de la tarjeta
                float barX = cX + 14.0f;
                float barY = cY + 140.0f;
                float barW = cW - 28.0f;
                float barH = 10.0f;
                Ui::slantRect(barX, barY, barW, barH, -4.0f, 0.28f, Ui::rgba(28, 20, 48, 220));
                Ui::slantRectGrad(barX, barY, std::max(8.0f, barW * loadPct), barH, -4.0f, 0.30f, lazerPink, lazerCyan);

                drawCenteredTextWithShadow(ctx.textBuf, cX + cW * 0.5f, cY + 161.0f, 0.34f,
                                           (ctx.songIntroLoadStage >= 5) ? Ui::rgba(46, 213, 115) : ctx.clrWhite,
                                           stageTxt, 0.31f);
            }

            // --- PANTALLA INFERIOR: MEDALLÓN OSU!LAZER PULSANTE CON ANILLO DE CARGA ---
            C2D_TargetClear(ctx.bottomScreen, Ui::rgba(10, 7, 20));
            C2D_SceneBegin(ctx.bottomScreen);
            Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                         Ui::rgba(20, 13, 36), Ui::rgba(10, 7, 18), lazerPink, 0.01f);

            float bCx = 160.0f;
            float bCy = 112.0f;
            float bR  = 42.0f + std::sin(ctx.songIntroTick * 0.22f) * 2.5f;

            Ui::glow(bCx, bCy, 0.18f, bR + 18.0f, Ui::withAlpha(lazerPink, 110), 4);
            Ui::ring(bCx, bCy, 0.20f, bR + 10.0f, 2.5f, Ui::withAlpha(lazerCyan, 180), 32);
            C2D_DrawCircleSolid(bCx, bCy, 0.25f, bR, 0xFFFFFFFF);
            C2D_DrawCircleSolid(bCx, bCy, 0.28f, bR - 3.0f, lazerPink);
            C2D_DrawCircleSolid(bCx, bCy, 0.30f, bR - 6.0f, Ui::rgba(85, 23, 184));
            drawCenteredTextWithShadow(ctx.textBuf, bCx, bCy - 2.0f, 0.82f, ctx.clrWhite, "osu!", 0.35f);

            char pctTxt[32];
            std::snprintf(pctTxt, sizeof(pctTxt), LOC(STR_GAME_LOADING_PCT_FMT), loadPct * 100.0f);
            Ui::slantRect(96.0f, 178.0f, 128.0f, 22.0f, -4.0f, 0.25f, Ui::rgba(24, 18, 44, 215));
            Ui::slantRect(96.0f, 178.0f, 3.5f, 22.0f, -4.0f, 0.27f, lazerPink);
            drawCenteredText(ctx.textBuf, 160.0f, 189.0f, 0.36f, ctx.clrWhite, pctTxt, 0.30f);

            C3D_FrameEnd(0);
            return;
        }
    }

    const SkinProfile& activeSkin = ctx.getActiveSkin();
    // =========================================================
    // 5. JUEGO ACTIVO + MENÚ DE PAUSA CORREGIDO
    // =========================================================
    bool isTaikoPlay = (ctx.settings.gameMode == MODE_TAIKO || ctx.beatmap.mode == 1);

    // En multijugador la pausa está totalmente deshabilitada para mantener la sincronía en tiempo real
    if (ctx.multiMgr.inMultiplayer) {
        ctx.isPaused = false;
    }

    if (ctx.isReplayPlaying && (ctx.kDown & KEY_B)) {
        ctx.audio.stopMusic();
        ctx.videoPlayer.closeVideo();
        ctx.isReplayPlaying = false;
        ctx.appState = STATE_RESULTS;
        return;
    }

    if (!ctx.multiMgr.inMultiplayer && (ctx.kDown & KEY_START)) {
        if (!ctx.isPaused) {
            ctx.isPaused = true;
            ctx.pauseOption = 0;
            ctx.audio.pauseMusic();
            ctx.audio.playHitSound(0);
        } else {
            ctx.isPaused = false;
            ctx.audio.resumeMusic();
            ctx.audio.playHitSound(0);
        }
    }

    if (ctx.isPaused) {
        if (ctx.kDown & KEY_DUP) {
            ctx.pauseOption = (ctx.pauseOption + 2) % 3;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_DDOWN) {
            ctx.pauseOption = (ctx.pauseOption + 1) % 3;
            ctx.audio.playHitSound(0);
        }
        if (isTaikoPlay) {
            bool touchBgToggle = ctx.touchJustDown && (ctx.touch.px >= 40 && ctx.touch.px <= 280 && ctx.touch.py >= 204 && ctx.touch.py <= 238);
            if ((ctx.kDown & KEY_X) || touchBgToggle) {
                ctx.settings.taikoBgMode = (ctx.settings.taikoBgMode + 1) % 3;
                ctx.settings.save();
                ctx.audio.playHitSound(2);
            }
        }

        bool triggerPauseAction = (ctx.kDown & KEY_A) != 0;

        // Botones táctiles en la pantalla inferior durante la pausa (Círculo izquierdo o 3 botones inclinados)
        if (ctx.touchJustDown) {
            float dx = static_cast<float>(ctx.touch.px) - 46.0f;
            float dy = static_cast<float>(ctx.touch.py) - 120.0f;
            if (dx * dx + dy * dy <= 40.0f * 40.0f) {
                ctx.pauseOption = 0;
                triggerPauseAction = true;
            } else if (ctx.touch.px >= 90 && ctx.touch.px <= 314) {
                if (ctx.touch.py >= 38 && ctx.touch.py <= 84) {
                    ctx.pauseOption = 0;
                    triggerPauseAction = true;
                } else if (ctx.touch.py >= 94 && ctx.touch.py <= 140) {
                    ctx.pauseOption = 1;
                    triggerPauseAction = true;
                } else if (ctx.touch.py >= 150 && ctx.touch.py <= 196) {
                    ctx.pauseOption = 2;
                    triggerPauseAction = true;
                }
            }
        }

        if (triggerPauseAction) {
            ctx.audio.playHitSound(2);
            if (ctx.pauseOption == 0) {
                ctx.isPaused = false;
                ctx.audio.resumeMusic();
            } else if (ctx.pauseOption == 1) {
                ctx.startSelectedBeatmap();
                return;
            } else if (ctx.pauseOption == 2) {
                ctx.audio.stopMusic();
                ctx.videoPlayer.closeVideo();
                ctx.applyActiveSkinAudio();
                if (ctx.multiMgr.inMultiplayer) {
                    ctx.multiMgr.stop();
                    ctx.appState = STATE_MAIN_MENU;
                } else {
                    ctx.appState = STATE_SONG_SELECT;
                    ctx.updateSongPreview();
                }
                return;
            }
        }
    } else {
        ctx.audio.update();
    }

    if (ctx.multiMgr.inMultiplayer) {
        ctx.multiMgr.players[ctx.multiMgr.localSlot].score = ctx.score;
        ctx.multiMgr.players[ctx.multiMgr.localSlot].combo = ctx.combo;
        ctx.multiMgr.players[ctx.multiMgr.localSlot].maxCombo = ctx.maxCombo;
        int totalJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
        ctx.multiMgr.players[ctx.multiMgr.localSlot].accuracy = (totalJudged > 0)
            ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
            : 100.0f;
        ctx.multiMgr.players[ctx.multiMgr.localSlot].hp = ctx.hp;
        ctx.multiMgr.update(true);

        if (!ctx.multiMgr.inMultiplayer) {
            ctx.audio.stopMusic();
            ctx.videoPlayer.closeVideo();
            ctx.multiMgr.startBrowser(ctx.sysUser);
            ctx.appState = STATE_MULTI_BROWSER;
            return;
        }
    }

    float speedRate    = getModsSpeedMultiplier(ctx.settings.activeMods);
    int songTime = 0;
    if (ctx.audio.getIsPlaying()) {
        songTime = ctx.audio.getTimeMs() - ctx.settings.audioOffsetMs;
    } else {
        // Reloj monotónico de respaldo para emuladores (Citra / Azahar) o fallos de audio
        u64 nowTick = osGetTime();
        if (nowTick < ctx.gameplayFallbackStartTick) {
            songTime = -static_cast<int>(ctx.gameplayFallbackStartTick - nowTick) - ctx.settings.audioOffsetMs;
        } else {
            songTime = static_cast<int>((nowTick - ctx.gameplayFallbackStartTick) * speedRate) - ctx.settings.audioOffsetMs;
        }
    }
    if (ctx.settings.enableVideo) {
        ctx.videoPlayer.update(songTime, ctx.isPaused);
    }
    bool isAutoplay    = (ctx.settings.activeMods & MOD_AUTO) != 0;
    bool isHidden      = (ctx.settings.activeMods & MOD_HD) != 0;
    bool isNoFail      = (ctx.settings.activeMods & MOD_NF) != 0;
    bool isSuddenDeath = (ctx.settings.activeMods & MOD_SD) != 0;
    bool isPerfect     = (ctx.settings.activeMods & MOD_PF) != 0;
    bool isFlashlight  = (ctx.settings.activeMods & MOD_FL) != 0;
    bool isRelax       = (ctx.settings.activeMods & MOD_RX) != 0;
    bool isAutoPilot   = (ctx.settings.activeMods & MOD_AP) != 0;
    bool isSpunOut     = (ctx.settings.activeMods & MOD_SO) != 0;
    float scoreMult    = getModsScoreMultiplier(ctx.settings.activeMods);
    isTaikoPlay        = (ctx.settings.gameMode == MODE_TAIKO || ctx.beatmap.mode == 1);
    bool isCatchPlay   = (ctx.settings.gameMode == MODE_CATCH || ctx.beatmap.mode == 2);
    bool isManiaPlay   = (ctx.settings.gameMode == MODE_MANIA || ctx.beatmap.mode == 3);

    // 1. Evaluar si estamos en período de Descanso (Break Time)
    ctx.inBreakPeriod = false;
    ctx.breakRemainingMs = 0;
    ctx.breakTotalMs = 0;
    for (const auto& brk : ctx.beatmap.breaks) {
        if (songTime >= brk.startTime && songTime <= brk.endTime) {
            ctx.inBreakPeriod = true;
            ctx.breakRemainingMs = brk.endTime - songTime;
            ctx.breakTotalMs = std::max(1, brk.endTime - brk.startTime);
            break;
        }
    }

    // Buscar coordenadas del siguiente objeto para el indicador de advertencia
    for (const auto& obj : ctx.beatmap.circles) {
        if (obj.time > songTime) {
            ctx.breakNextObjPos = {obj.x, obj.y};
            break;
        }
    }

    // 2. Evaluar disponibilidad de SKIP (Intro o Descanso largo, solo en Single Player)
    ctx.skipAvailable = false;
    ctx.skipTargetMs = 0;
    if (!ctx.multiMgr.inMultiplayer) {
        if (!ctx.beatmap.circles.empty()) {
            int firstNoteTime = ctx.beatmap.circles.front().time;
            int introWait = firstNoteTime - static_cast<int>(ctx.beatmap.diff.preempt) - 1000;
            if (introWait > songTime + 2200) {
                ctx.skipAvailable = true;
                ctx.skipTargetMs = introWait;
            }
        }
        if (!ctx.skipAvailable && ctx.inBreakPeriod && ctx.breakRemainingMs > 2600) {
            ctx.skipAvailable = true;
            ctx.skipTargetMs = songTime + ctx.breakRemainingMs - 1200;
        }
    }

    // 3. Procesar activación de SKIP (Toque táctil o botones ZL / ZR / START)
    // touchJustDown únicamente (isTouching repetitivo en cada frame provocaba lag y desync)
    bool touchOnSkip = ctx.touchJustDown && (ctx.touch.px >= 195 && ctx.touch.px <= 318 && ctx.touch.py >= 185 && ctx.touch.py <= 240);
    bool btnSkip = (ctx.kDown & (KEY_ZL | KEY_ZR | KEY_START)) != 0;
    if (ctx.skipAvailable && !ctx.isPaused && (touchOnSkip || btnSkip)) {
        ctx.audio.playHitSound(2);
        ctx.audio.seekToMs(ctx.skipTargetMs);
        songTime = ctx.audio.getTimeMs() - ctx.settings.audioOffsetMs;
        ctx.lastNcStep = -999999;
        if (ctx.settings.enableVideo) {
            ctx.videoPlayer.update(songTime, ctx.isPaused);
        }
        ctx.skipAvailable = false;
        ctx.kDown &= ~(KEY_ZL | KEY_ZR | KEY_START | KEY_TOUCH);
        ctx.touchJustDown = false;
        ctx.isTouching = false;
        ctx.cursorTrail.clear();
    }

    // 3b. Procesar activación de REINICIO RÁPIDO (Solo botón físico asignado como SELECT, deshabilitado en multijugador)
    bool btnRetry = (ctx.kDown & ctx.settings.keyQuickRetry) != 0;
    if (!ctx.isPaused && btnRetry && !ctx.multiMgr.inMultiplayer) {
        ctx.audio.playHitSound(2);
        ctx.startSelectedBeatmap();
        return;
    }

    // 4. Animación y detección de superación de récord en vivo (In-Game Leaderboard, solo en Single Player)
    if (!ctx.multiMgr.inMultiplayer && !ctx.localRecords.empty() && !ctx.hasOvertakenBest) {
        if (ctx.score > ctx.localRecords[0].bestScore) {
            ctx.hasOvertakenBest = true;
            ctx.overtakeFlashAnim = 1.0f;
            ctx.audio.playHitSound(2);
            ctx.spawnParticles(256.0f, 192.0f, ctx.clr100, 16, 1.2f, 3.6f, 2.6f, 22);
        }
    }
    if (ctx.overtakeFlashAnim > 0.01f) {
        ctx.overtakeFlashAnim = Ui::approach(ctx.overtakeFlashAnim, 0.0f, 0.035f);
    }

    if (ctx.taikoGuideTimer > 0.0f && !ctx.isPaused) {
        ctx.taikoGuideTimer -= 1.0f;
    }
    float taikoGuideAlpha = (ctx.taikoGuideTimer > 60.0f) ? 1.0f : (ctx.taikoGuideTimer / 60.0f);
    if (taikoGuideAlpha < 0.0f) taikoGuideAlpha = 0.0f;

    // 5. Gestion del Efecto Kiai Time (Climax de la cancion con particulas luminosas y resplandor ritmico)
    bool nowKiai = ctx.beatmap.isKiaiAt(songTime);
    if (nowKiai && !ctx.prevInKiai) {
        ctx.kiaiBurstFlash = 1.0f;
        // Estallido inicial de estrellas y chispas al entrar en Kiai Time
        for (int i = 0; i < 20; ++i) {
            float sx = 30.0f + static_cast<float>(std::rand() % 452);
            float sy = 370.0f;
            float spd = 2.2f + static_cast<float>(std::rand() % 100) / 35.0f;
            float ang = -static_cast<float>(M_PI) * (0.28f + 0.44f * (static_cast<float>(std::rand() % 100) / 100.0f));
            u32 pClr = (i % 2 == 0) ? Ui::rgba(255, 215, 60) : Ui::rgba(255, 110, 185);
            ctx.particles.push_back({
                sx * SCALE_FACTOR, sy * SCALE_FACTOR,
                std::cos(ang) * spd, std::sin(ang) * spd,
                3.2f, pClr, 38, 38
            });
        }
    }
    ctx.prevInKiai = nowKiai;
    ctx.inKiai = nowKiai;

    float targetKiai = ctx.inKiai ? 1.0f : 0.0f;
    ctx.kiaiGlow = Ui::approach(ctx.kiaiGlow, targetKiai, ctx.inKiai ? 0.08f : 0.035f);
    if (ctx.kiaiBurstFlash > 0.01f) {
        ctx.kiaiBurstFlash = Ui::approach(ctx.kiaiBurstFlash, 0.0f, 0.045f);
    }

    // 5b. Ritmo Nightcore (NC): Bombo (kick), caja (clap), charles (hat) y platillo sincronizados al compas
    if ((ctx.settings.activeMods & MOD_NC) && !ctx.isPaused && !ctx.stageFailed && !ctx.inBreakPeriod && songTime >= 0 && !ctx.audio.isSongFinished()) {
        double tpTime = 0.0;
        float beatLen = 500.0f;
        ctx.beatmap.getBeatInfoAt(songTime, tpTime, beatLen);
        if (beatLen > 10.0f) {
            double elapsed = static_cast<double>(songTime) - tpTime;
            int step = static_cast<int>(std::floor(elapsed / (beatLen * 0.5f)));
            if (step > ctx.lastNcStep) {
                if (ctx.lastNcStep == -999999 || (step - ctx.lastNcStep) > 8) {
                    ctx.lastNcStep = step;
                } else {
                    for (int s = ctx.lastNcStep + 1; s <= step; ++s) {
                        if ((s % 2) == 0) {
                            // Tiempo principal (compas 4/4)
                            int beatInBar = ((s / 2) % 4 + 4) % 4;
                            if (beatInBar == 0 || beatInBar == 2) {
                                ctx.audio.playNcKick();
                            } else {
                                ctx.audio.playNcClap();
                            }
                            // Cada 4 compases remate de platillo dance
                            if (((s / 2) % 16) == 0) {
                                ctx.audio.playNcCrash();
                            }
                        } else {
                            // Contratiempo (corchea): charles rapido
                            ctx.audio.playNcHat();
                        }
                    }
                    ctx.lastNcStep = step;
                }
            }
        }
    }

    // Generar chispas / estrellas flotantes continuas durante Kiai
    if (ctx.inKiai && !ctx.isPaused && (ctx.animTick % 5 == 0) && ctx.particles.size() < 48) {
        float sx = 20.0f + static_cast<float>(std::rand() % 472);
        float sy = 375.0f;
        float spd = 1.4f + static_cast<float>(std::rand() % 100) / 55.0f;
        float ang = -static_cast<float>(M_PI) * (0.32f + 0.36f * (static_cast<float>(std::rand() % 100) / 100.0f));
        u32 pClr = ((ctx.animTick / 5) % 2 == 0) ? Ui::rgba(255, 225, 70) : Ui::rgba(255, 115, 190);
        ctx.particles.push_back({
            sx * SCALE_FACTOR, sy * SCALE_FACTOR,
            std::cos(ang) * spd, std::sin(ang) * spd,
            2.5f, pClr, 32, 32
        });
    }


    bool tapWithTouch = ctx.settings.playOnBottomScreen || ctx.settings.tapWithStylus;
    u32 btnGroup1 = ctx.settings.keyOsuK1 | (tapWithTouch ? KEY_TOUCH : 0);
    u32 btnGroup2 = ctx.settings.keyOsuK2;
    bool k1Held = (ctx.kHeld & btnGroup1) != 0;
    bool k2Held = (ctx.kHeld & btnGroup2) != 0;
    bool anyHeld = (k1Held || k2Held);
    bool kActionDown = (ctx.kDown & (btnGroup1 | btnGroup2)) != 0;

    if (ctx.isReplayPlaying) {
        if (!ctx.currentReplay.frames.empty()) {
            while (ctx.replayPlaybackIndex + 1 < ctx.currentReplay.frames.size() &&
                   ctx.currentReplay.frames[ctx.replayPlaybackIndex + 1].timeMs <= songTime) {
                ctx.replayPlaybackIndex++;
            }
            const auto& fCur = ctx.currentReplay.frames[ctx.replayPlaybackIndex];
            static uint32_t s_prevKeys = 0;
            uint32_t curKeys = fCur.keys;
            uint32_t justDown = curKeys & ~s_prevKeys;
            s_prevKeys = curKeys;

            k1Held = (curKeys & REPLAY_KEY_K1) != 0;
            k2Held = (curKeys & REPLAY_KEY_K2) != 0;
            anyHeld = (k1Held || k2Held);
            kActionDown = ((justDown & (REPLAY_KEY_K1 | REPLAY_KEY_K2)) != 0);
            ctx.isTouching = (curKeys & REPLAY_KEY_TOUCH) != 0;
            if (justDown & REPLAY_KEY_K1) ctx.k1PressCount++;
            if (justDown & REPLAY_KEY_K2) ctx.k2PressCount++;
        }
    } else if (!ctx.isPaused && !isAutoplay) {
        if (ctx.kDown & btnGroup1) ctx.k1PressCount++;
        if (ctx.kDown & btnGroup2) ctx.k2PressCount++;
    }

    if (!ctx.isPaused) {
        if (ctx.isReplayPlaying) {
            if (!ctx.currentReplay.frames.empty()) {
                const auto& fCur = ctx.currentReplay.frames[ctx.replayPlaybackIndex];
                if (ctx.replayPlaybackIndex + 1 < ctx.currentReplay.frames.size()) {
                    const auto& fNext = ctx.currentReplay.frames[ctx.replayPlaybackIndex + 1];
                    float span = static_cast<float>(fNext.timeMs - fCur.timeMs);
                    if (span > 0.0f && songTime >= fCur.timeMs && songTime <= fNext.timeMs) {
                        float t = static_cast<float>(songTime - fCur.timeMs) / span;
                        ctx.cursorOsuX = fCur.x + (fNext.x - fCur.x) * t;
                        ctx.cursorOsuY = fCur.y + (fNext.y - fCur.y) * t;
                    } else {
                        ctx.cursorOsuX = fCur.x;
                        ctx.cursorOsuY = fCur.y;
                    }
                } else {
                    ctx.cursorOsuX = fCur.x;
                    ctx.cursorOsuY = fCur.y;
                }
            }
        } else if (isAutoplay) {
            // Bot Autoplay: calcula la posición ideal del cursor e inputs automáticos
            anyHeld = false;
            k1Held = false;
            kActionDown = false;
            for (size_t i = 0; i < ctx.beatmap.circles.size(); ++i) {
                const auto& obj = ctx.beatmap.circles[i];
                bool done = obj.isSlider ? obj.sliderDone :
                           (obj.isSpinner ? obj.sliderDone : (obj.state != NOT_HIT));
                if (done) continue;

                if (obj.isSpinner && songTime >= obj.time && songTime <= obj.endTime) {
                    float spinAng = songTime * 0.035f;
                    ctx.cursorOsuX = 256.0f + std::cos(spinAng) * 55.0f;
                    ctx.cursorOsuY = 192.0f + std::sin(spinAng) * 55.0f;
                    anyHeld = k1Held = true;
                    break;
                } else if (obj.isSlider && songTime >= obj.time && songTime <= obj.endTime) {
                    Vec2 ball = obj.getSliderBallPos(songTime);
                    ctx.cursorOsuX = ball.x;
                    ctx.cursorOsuY = ball.y;
                    anyHeld = k1Held = true;
                    if (obj.state == NOT_HIT) kActionDown = true;
                    break;
                } else {
                    // Interpolar suavemente hacia la siguiente nota
                    float dt = static_cast<float>(obj.time - songTime);
                    if (dt <= 0.0f) {
                        ctx.cursorOsuX = obj.x;
                        ctx.cursorOsuY = obj.y;
                        kActionDown = true;
                        k1Held = anyHeld = true;
                    } else if (dt < ctx.beatmap.diff.preempt) {
                        float lerpFactor = 0.28f;
                        ctx.cursorOsuX += (obj.x - ctx.cursorOsuX) * lerpFactor;
                        ctx.cursorOsuY += (obj.y - ctx.cursorOsuY) * lerpFactor;
                    }
                    break;
                }
            }
        } else if (isAutoPilot) {
            // Mod AutoPilot: apuntado automático a las notas mientras el jugador pulsa las teclas
            for (size_t i = 0; i < ctx.beatmap.circles.size(); ++i) {
                const auto& obj = ctx.beatmap.circles[i];
                bool done = obj.isSlider ? obj.sliderDone :
                           (obj.isSpinner ? obj.sliderDone : (obj.state != NOT_HIT));
                if (done) continue;

                if (obj.isSpinner && songTime >= obj.time && songTime <= obj.endTime) {
                    float spinAng = songTime * 0.035f;
                    ctx.cursorOsuX = 256.0f + std::cos(spinAng) * 55.0f;
                    ctx.cursorOsuY = 192.0f + std::sin(spinAng) * 55.0f;
                    break;
                } else if (obj.isSlider && songTime >= obj.time && songTime <= obj.endTime) {
                    Vec2 ball = obj.getSliderBallPos(songTime);
                    ctx.cursorOsuX = ball.x;
                    ctx.cursorOsuY = ball.y;
                    break;
                } else {
                    float dt = static_cast<float>(obj.time - songTime);
                    if (dt <= 0.0f) {
                        ctx.cursorOsuX = obj.x;
                        ctx.cursorOsuY = obj.y;
                    } else if (dt < ctx.beatmap.diff.preempt) {
                        float lerpFactor = 0.35f;
                        ctx.cursorOsuX += (obj.x - ctx.cursorOsuX) * lerpFactor;
                        ctx.cursorOsuY += (obj.y - ctx.cursorOsuY) * lerpFactor;
                    }
                    break;
                }
            }
        } else if (ctx.isTouching) {
            bool touchingSkip = ctx.skipAvailable && (ctx.touch.px >= 195 && ctx.touch.px <= 318 && ctx.touch.py >= 185 && ctx.touch.py <= 240);
            if (!touchingSkip) {
                ctx.settings.mapTouchToOsu(ctx.touch.px, ctx.touch.py, ctx.cursorOsuX, ctx.cursorOsuY);
            }
        }

        // Actualizar estela dinámica y fluida del cursor (Dynamic Cursor Trail con sub-interpolación)
        float cdx = ctx.cursorOsuX - ctx.prevCursorOsuX;
        float cdy = ctx.cursorOsuY - ctx.prevCursorOsuY;
        float cdist = std::sqrt(cdx * cdx + cdy * cdy);

        if (activeSkin.cursorTrail) {
            const int trailMaxLife = 18;
            if (cdist > 1.2f) {
                int subSteps = std::max(1, std::min(8, static_cast<int>(cdist / 4.5f)));
                for (int s = 1; s <= subSteps; ++s) {
                    float t = static_cast<float>(s) / static_cast<float>(subSteps);
                    float ix = ctx.prevCursorOsuX + cdx * t;
                    float iy = ctx.prevCursorOsuY + cdy * t;
                    ctx.cursorTrail.push_back({ix, iy, trailMaxLife, trailMaxLife});
                }
                if (ctx.cursorTrail.size() > 42) {
                    ctx.cursorTrail.erase(ctx.cursorTrail.begin(), ctx.cursorTrail.begin() + (ctx.cursorTrail.size() - 42));
                }
            } else if (ctx.cursorTrail.empty() || std::fabs(ctx.cursorTrail.back().x - ctx.cursorOsuX) > 0.5f || std::fabs(ctx.cursorTrail.back().y - ctx.cursorOsuY) > 0.5f) {
                ctx.cursorTrail.push_back({ctx.cursorOsuX, ctx.cursorOsuY, trailMaxLife, trailMaxLife});
                if (ctx.cursorTrail.size() > 42) ctx.cursorTrail.erase(ctx.cursorTrail.begin());
            }

            for (size_t t = 0; t < ctx.cursorTrail.size(); ) {
                ctx.cursorTrail[t].life--;
                if (ctx.cursorTrail[t].life <= 0) ctx.cursorTrail.erase(ctx.cursorTrail.begin() + t);
                else ++t;
            }
        } else {
            ctx.cursorTrail.clear();
        }
        ctx.prevCursorOsuX = ctx.cursorOsuX;
        ctx.prevCursorOsuY = ctx.cursorOsuY;

        // Actualizar Humo (Smoke) estilo osu! 'C' (activado manteniendo el botón asignado en Ajustes)
        bool smokeActive = (ctx.kHeld & ctx.settings.keyOsuSmoke) != 0;
        if (ctx.isReplayPlaying && !ctx.currentReplay.frames.empty()) {
            smokeActive = (ctx.currentReplay.frames[ctx.replayPlaybackIndex].keys & REPLAY_KEY_SMOKE) != 0;
        }
        if (smokeActive && !ctx.isPaused) {
            if (ctx.smokeParticles.size() < 240) {
                for (int sm = 0; sm < 2; ++sm) {
                    float rndX = ((std::rand() % 100) - 50) * 0.04f;
                    float rndY = ((std::rand() % 100) - 50) * 0.04f;
                    float driftX = ((std::rand() % 100) - 50) * 0.006f;
                    float driftY = -0.045f - (static_cast<float>(std::rand() % 100) * 0.0004f);
                    float initSize = 4.8f + (std::rand() % 20) * 0.12f;
                    int sLife = 170 + (std::rand() % 50); // ~3.5 segundos
                    ctx.smokeParticles.push_back({
                        ctx.cursorOsuX + rndX, ctx.cursorOsuY + rndY,
                        driftX, driftY,
                        initSize, sLife, sLife
                    });
                }
            }
        }

        for (size_t sm = 0; sm < ctx.smokeParticles.size(); ) {
            auto& sp = ctx.smokeParticles[sm];
            sp.life--;
            sp.x += sp.vx;
            sp.y += sp.vy;
            sp.vx *= 0.985f;
            sp.size += 0.042f; // El humo se expande suavemente al disiparse
            if (sp.life <= 0) {
                ctx.smokeParticles.erase(ctx.smokeParticles.begin() + sm);
            } else {
                ++sm;
            }
        }

        float hitRadius = ctx.beatmap.diff.radiusOsu;
        float hit50     = ctx.beatmap.diff.hit50;
        float hit100    = ctx.beatmap.diff.hit100;
        float hit300    = ctx.beatmap.diff.hit300;

        if (!ctx.inBreakPeriod && !ctx.beatmap.circles.empty() && songTime >= ctx.beatmap.circles.front().time &&
            songTime <= ctx.beatmap.circles.back().endTime) {
            ctx.hp -= (0.00018f * ctx.beatmap.diff.hp);
            if (ctx.hp <= 0.0f) {
                if (isNoFail || isAutoplay) {
                    ctx.hp = 0.05f;
                } else {
                    ctx.hp = 0.0f;
                    ctx.stageFailed = true;
                    ctx.lastEarnedPP = 0.0f;
                    ctx.audio.stopMusic();
                    ctx.videoPlayer.closeVideo();
                    int totalJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
                    float curAcc = (totalJudged > 0)
                        ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                        : 100.0f;
                    if (ctx.multiMgr.inMultiplayer) {
                        ctx.multiMgr.matchStarted = false;
                        ctx.multiMgr.matchInProgressFlag = false;
                        ctx.multiMgr.matchFinished = true;
                        ctx.multiMgr.sendLiveGameplay(ctx.score, ctx.combo, ctx.maxCombo, curAcc, 0.0f, "F", ctx.count300, ctx.count100, ctx.count50, ctx.countMiss, true, true);
                        ctx.appState = STATE_MULTI_RESULTS;
                    } else {
                        ctx.appState = STATE_RESULTS;
                    }
                    return;
                }
            }
        }

        bool allObjectsDone = true;
        bool anyActiveSpinner = false;

        if (isTaikoPlay) {
            float bpmPulse = ctx.getBpmPulse();
            ctx.taikoEngine.update(bpmPulse);

            // 1. Evaluar Misses de notas pasadas
            int tMisses = ctx.taikoEngine.checkMisses(songTime, hit50, isAutoplay);
            if (tMisses > 0) {
                ctx.countMiss += tMisses;
                ctx.combo = 0;
                ctx.hp = std::max(0.0f, ctx.hp - (0.10f * tMisses));
                ctx.triggerShake(3.0f, 10.0f);
                ctx.audio.playComboBreak();
                ctx.popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_MISS, 24.0f, ctx.clrMiss, 24));
                if (isSuddenDeath || isPerfect) {
                    ctx.triggerSuddenDeathFail();
                }
            }

            // 2. Procesar inputs de Don y Katsu
            bool donDown = false;
            bool katsuDown = false;

            if (ctx.isReplayPlaying) {
                if (!ctx.currentReplay.frames.empty()) {
                    const auto& fCur = ctx.currentReplay.frames[ctx.replayPlaybackIndex];
                    static uint32_t s_prevTaikoKeys = 0;
                    uint32_t justT = fCur.keys & ~s_prevTaikoKeys;
                    s_prevTaikoKeys = fCur.keys;

                    donDown = (justT & (REPLAY_KEY_TAIKO_DON_L | REPLAY_KEY_TAIKO_DON_R)) != 0;
                    katsuDown = (justT & (REPLAY_KEY_TAIKO_KATSU_L | REPLAY_KEY_TAIKO_KATSU_R)) != 0;
                    if (fCur.keys & REPLAY_KEY_TAIKO_DON_L) ctx.taikoEngine.drumHitDonLeft = 1.0f;
                    if (fCur.keys & REPLAY_KEY_TAIKO_DON_R) ctx.taikoEngine.drumHitDonRight = 1.0f;
                    if (fCur.keys & REPLAY_KEY_TAIKO_KATSU_L) ctx.taikoEngine.drumHitKatsuLeft = 1.0f;
                    if (fCur.keys & REPLAY_KEY_TAIKO_KATSU_R) ctx.taikoEngine.drumHitKatsuRight = 1.0f;
                }
            } else if (isAutoplay) {
                if (!ctx.taikoEngine.notes.empty()) {
                    for (size_t i = ctx.taikoEngine.nextNoteIndex; i < ctx.taikoEngine.notes.size(); ++i) {
                        auto& tn = ctx.taikoEngine.notes[i];
                        if (tn.state != NOT_HIT) continue;
                        if (tn.type == TAIKO_DRUMROLL || tn.type == TAIKO_DENDEN) {
                            if (songTime >= tn.time && songTime <= tn.endTime) {
                                if (ctx.animTick % 4 == 0) {
                                    donDown = true;
                                    if ((ctx.animTick / 4) % 2 == 0) ctx.taikoEngine.drumHitDonLeft = 1.0f;
                                    else ctx.taikoEngine.drumHitDonRight = 1.0f;
                                }
                                break;
                            }
                        } else {
                            if (songTime >= tn.time - 8) {
                                bool isDon = (tn.type == TAIKO_DON || tn.type == TAIKO_BIG_DON);
                                if (isDon) {
                                    donDown = true;
                                    if (i % 2 == 0) ctx.taikoEngine.drumHitDonLeft = 1.0f;
                                    else ctx.taikoEngine.drumHitDonRight = 1.0f;
                                } else {
                                    katsuDown = true;
                                    if (i % 2 == 0) ctx.taikoEngine.drumHitKatsuLeft = 1.0f;
                                    else ctx.taikoEngine.drumHitKatsuRight = 1.0f;
                                }
                                break;
                            } else {
                                break;
                            }
                        }
                    }
                }
            } else {
                // Controles físicos asignados con animación del tambor según el botón pulsado
                if (ctx.kDown & ctx.settings.keyTaikoDonLeft) {
                    donDown = true;
                    ctx.taikoEngine.drumHitDonLeft = 1.0f;
                }
                if (ctx.kDown & ctx.settings.keyTaikoDonRight) {
                    donDown = true;
                    ctx.taikoEngine.drumHitDonRight = 1.0f;
                }
                if (ctx.kDown & ctx.settings.keyTaikoKatsuLeft) {
                    katsuDown = true;
                    ctx.taikoEngine.drumHitKatsuLeft = 1.0f;
                }
                if (ctx.kDown & ctx.settings.keyTaikoKatsuRight) {
                    katsuDown = true;
                    ctx.taikoEngine.drumHitKatsuRight = 1.0f;
                }
                if (!donDown && (ctx.kDown & ctx.settings.keyTaikoDon)) {
                    donDown = true;
                    ctx.taikoEngine.drumHitDonLeft = 1.0f;
                }
                if (!katsuDown && (ctx.kDown & ctx.settings.keyTaikoKatsu)) {
                    katsuDown = true;
                    ctx.taikoEngine.drumHitKatsuLeft = 1.0f;
                }

                // Entrada táctil en el tambor interactivo
                if (ctx.touchJustDown) {
                    bool touchOnSkip = ctx.skipAvailable && (ctx.touch.px >= 195 && ctx.touch.px <= 318 && ctx.touch.py >= 185 && ctx.touch.py <= 240);
                    if (!touchOnSkip) {
                        int zone = ctx.taikoEngine.handleTouchDrum(ctx.touch.px, ctx.touch.py);
                        if (zone == 1 || zone == 2) donDown = true;
                        else if (zone == 3 || zone == 4) katsuDown = true;
                    }
                }
            }

            if (donDown || katsuDown) {
                if (donDown) ctx.k1PressCount++;
                if (katsuDown) ctx.k2PressCount++;

                bool isBigNote = false;
                if (ctx.taikoEngine.nextNoteIndex > 0 && ctx.taikoEngine.nextNoteIndex <= ctx.taikoEngine.notes.size()) {
                    const auto& lastN = ctx.taikoEngine.notes[ctx.taikoEngine.nextNoteIndex - 1];
                    isBigNote = (lastN.type == TAIKO_BIG_DON || lastN.type == TAIKO_BIG_KATSU);
                }

                auto playTaikoSfx = [&]() {
                    if (donDown) ctx.audio.playTaikoDon(isBigNote);
                    else if (katsuDown) ctx.audio.playTaikoKatsu(isBigNote);
                };

                int hitRes = ctx.taikoEngine.processHit(donDown, false, songTime, hit300, hit100, hit50, isAutoplay);
                if (hitRes == ModeTaiko::TAIKO_HIT_300) {
                    ctx.count300++;
                    ctx.combo++;
                    if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                    ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                    ctx.hp = std::min(1.0f, ctx.hp + 0.05f);
                    playTaikoSfx();
                    if (ctx.combo > 0 && (ctx.combo % 50 == 0)) ctx.audio.playTaikoBalloonPop();
                    if (ctx.settings.showHit300) {
                        ctx.popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_300, 24.0f, ctx.clr300, 24));
                        ctx.spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, donDown ? ModeTaiko::CLR_DON_INNER : ModeTaiko::CLR_KATSU_INNER, 6, 0.8f, 2.4f, 2.2f, 15);
                    }
                } else if (hitRes == ModeTaiko::TAIKO_HIT_100) {
                    ctx.count100++;
                    ctx.combo++;
                    if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                    ctx.score += static_cast<int>((100 + 100 * ctx.combo * 0.05f) * scoreMult);
                    ctx.hp = std::min(1.0f, ctx.hp + 0.02f);
                    playTaikoSfx();
                    if (ctx.combo > 0 && (ctx.combo % 50 == 0)) ctx.audio.playTaikoBalloonPop();
                    ctx.popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_100, 20.0f, ctx.clr100, 24));
                    ctx.spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, ctx.clr100, 4, 0.6f, 1.8f, 2.0f, 12);
                    if (isPerfect) ctx.triggerSuddenDeathFail();
                } else if (hitRes == ModeTaiko::TAIKO_HIT_TICK || hitRes == ModeTaiko::TAIKO_HIT_DENDEN) {
                    // Tick de redoble o denden estilo oficial Taiko: otorga puntos y combo, SIN penalizar la precisión
                    ctx.combo++;
                    if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                    ctx.score += static_cast<int>(300 * scoreMult);
                    ctx.hp = std::min(1.0f, ctx.hp + 0.015f);
                    if (ctx.taikoEngine.balloonPopTimer > 0.95f) {
                        ctx.audio.playTaikoBalloonPop();
                    } else {
                        playTaikoSfx();
                    }
                    ctx.spawnParticles(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, Ui::rgba(255, 220, 60), 4, 0.8f, 2.4f, 2.2f, 12);
                } else if (hitRes == ModeTaiko::TAIKO_HIT_MISS) {
                    ctx.countMiss++;
                    ctx.combo = 0;
                    ctx.hp = std::max(0.0f, ctx.hp - 0.12f);
                    ctx.triggerShake(3.0f, 10.0f);
                    ctx.audio.playComboBreak();
                    ctx.popups.push_back(HitPopup(ModeTaiko::TARGET_X, ModeTaiko::TRACK_Y, HIT_MISS, 24.0f, ctx.clrMiss, 26));
                    if (isSuddenDeath || isPerfect) ctx.triggerSuddenDeathFail();
                } else {
                    playTaikoSfx();
                }
            }

            allObjectsDone = (ctx.taikoEngine.nextNoteIndex >= ctx.taikoEngine.notes.size());
        } else if (isManiaPlay) {
            ctx.maniaEngine.update(0.016f);

            // 1. Evaluar Misses de notas pasadas
            int mMisses = ctx.maniaEngine.checkMisses(songTime, hit50);
            if (mMisses > 0) {
                ctx.countMiss += mMisses;
                ctx.combo = 0;
                ctx.hp = std::max(0.0f, ctx.hp - (0.08f * mMisses));
                ctx.triggerShake(3.0f, 10.0f);
                ctx.popups.push_back(HitPopup(ModeMania::TRACK_X + ModeMania::COL_WIDTH * 2.0f, ModeMania::HIT_Y - 20.0f, HIT_MISS, 24.0f, ctx.clrMiss, 24));
                if (isSuddenDeath || isPerfect) {
                    ctx.triggerSuddenDeathFail();
                }
            }

            // 2. Procesar inputs en las 4 columnas
            bool colDown[4] = { false, false, false, false };
            bool colUp[4]   = { false, false, false, false };

            if (ctx.isReplayPlaying) {
                if (!ctx.currentReplay.frames.empty()) {
                    const auto& fCur = ctx.currentReplay.frames[ctx.replayPlaybackIndex];
                    static uint32_t s_prevManiaKeys = 0;
                    for (int c = 0; c < 4; ++c) {
                        uint32_t mBit = (REPLAY_KEY_MANIA_0 << c);
                        bool isHeld = (fCur.keys & mBit) != 0;
                        bool wasHeld = (s_prevManiaKeys & mBit) != 0;
                        if (isHeld && !wasHeld) colDown[c] = true;
                        if (!isHeld && wasHeld) colUp[c] = true;
                        ctx.maniaEngine.colPressed[c] = isHeld;
                    }
                    s_prevManiaKeys = fCur.keys;
                }
            } else if (isAutoplay) {
                for (size_t i = ctx.maniaEngine.nextNoteIndex; i < ctx.maniaEngine.notes.size(); ++i) {
                    auto& mn = ctx.maniaEngine.notes[i];
                    if (mn.fullyCompleted) continue;
                    if (songTime >= mn.time && !mn.headJudged) {
                        colDown[mn.column] = true;
                    }
                    if (mn.isHold && mn.isHolding) {
                        ctx.maniaEngine.colPressed[mn.column] = true;
                        if (songTime >= mn.endTime) {
                            colUp[mn.column] = true;
                        }
                    }
                }
            } else {
                // Controles físicos por columna
                if (ctx.kDown & ctx.settings.keyManiaCol1) colDown[0] = true;
                if (ctx.kDown & ctx.settings.keyManiaCol2) colDown[1] = true;
                if (ctx.kDown & ctx.settings.keyManiaCol3) colDown[2] = true;
                if (ctx.kDown & ctx.settings.keyManiaCol4) colDown[3] = true;

                if (ctx.kUp & ctx.settings.keyManiaCol1) colUp[0] = true;
                if (ctx.kUp & ctx.settings.keyManiaCol2) colUp[1] = true;
                if (ctx.kUp & ctx.settings.keyManiaCol3) colUp[2] = true;
                if (ctx.kUp & ctx.settings.keyManiaCol4) colUp[3] = true;

                // Ajuste de Scroll Speed al vuelo en Mania
                if (ctx.kDown & (KEY_ZL | KEY_CPAD_DOWN)) {
                    ctx.maniaEngine.adjustScrollSpeed(-0.1f);
                    ctx.audio.playHitSound(0);
                } else if (ctx.kDown & (KEY_ZR | KEY_CPAD_UP)) {
                    ctx.maniaEngine.adjustScrollSpeed(+0.1f);
                    ctx.audio.playHitSound(0);
                } else if (ctx.kHeld & KEY_SELECT) {
                    if (ctx.kDown & KEY_DDOWN) {
                        ctx.maniaEngine.adjustScrollSpeed(-0.1f);
                        ctx.audio.playHitSound(0);
                    } else if (ctx.kDown & KEY_DUP) {
                        ctx.maniaEngine.adjustScrollSpeed(+0.1f);
                        ctx.audio.playHitSound(0);
                    }
                }

                // Entrada táctil en el teclado de la pantalla inferior
                if (ctx.isTouching) {
                    int tCol = ctx.maniaEngine.handleTouch(ctx.touch.px, ctx.touch.py, true);
                    if (tCol >= 0 && tCol <= 3) {
                        if (ctx.touchJustDown || ctx.maniaEngine.lastTouchCol != tCol) {
                            colDown[tCol] = true;
                        }
                    }
                }
                if (ctx.touchJustUp && ctx.maniaEngine.lastTouchCol >= 0) {
                    colUp[ctx.maniaEngine.lastTouchCol] = true;
                    ctx.maniaEngine.handleTouch(0.0f, 0.0f, false);
                }
            }

            // 3. Procesar liberaciones (Key Releases) para colas de Hold Notes
            for (int c = 0; c < 4; ++c) {
                if (colUp[c]) {
                    int relRes = ctx.maniaEngine.processKeyRelease(c, songTime, hit300, hit100, hit50);
                    float popX = ModeMania::TRACK_X + (c + 0.5f) * ModeMania::COL_WIDTH;
                    if (relRes == 300) {
                        ctx.count300++;
                        ctx.combo++;
                        if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                        ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                        ctx.hp = std::min(1.0f, ctx.hp + 0.04f);
                        ctx.audio.playHitSound(c % 2);
                        if (ctx.settings.showHit300) ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_300, 22.0f, ctx.clr300, 22));
                    } else if (relRes == 100) {
                        ctx.count100++;
                        ctx.combo++;
                        if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                        ctx.score += static_cast<int>((100 + 100 * ctx.combo * 0.05f) * scoreMult);
                        ctx.hp = std::min(1.0f, ctx.hp + 0.02f);
                        ctx.audio.playHitSound(c % 2);
                        ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_100, 20.0f, ctx.clr100, 22));
                        if (isPerfect) ctx.triggerSuddenDeathFail();
                    } else if (relRes == -1) {
                        ctx.countMiss++;
                        ctx.combo = 0;
                        ctx.hp = std::max(0.0f, ctx.hp - 0.08f);
                        ctx.triggerShake(3.0f, 10.0f);
                        ctx.audio.playHitSound(c % 2);
                        ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_MISS, 22.0f, ctx.clrMiss, 24));
                        if (isSuddenDeath || isPerfect) ctx.triggerSuddenDeathFail();
                    }
                }
            }

            // 4. Procesar pulsaciones (Key Presses)
            for (int c = 0; c < 4; ++c) {
                if (colDown[c]) {
                    ctx.k1PressCount++;
                    int hitRes = ctx.maniaEngine.processKeyPress(c, songTime, hit300, hit100, hit50);
                    float popX = ModeMania::TRACK_X + (c + 0.5f) * ModeMania::COL_WIDTH;
                    u32 pClr = (c == 0 || c == 3) ? ModeMania::CLR_COL_OUTER : ModeMania::CLR_COL_INNER;

                    if (hitRes == 300) {
                        ctx.count300++;
                        ctx.combo++;
                        if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                        ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                        ctx.hp = std::min(1.0f, ctx.hp + 0.04f);
                        ctx.audio.playHitSound(c % 2);
                        if (ctx.settings.showHit300) {
                            ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_300, 24.0f, ctx.clr300, 24));
                            ctx.spawnParticles(popX, ModeMania::HIT_Y, pClr, 6, 0.8f, 2.2f, 2.0f, 15);
                        }
                    } else if (hitRes == 100) {
                        ctx.count100++;
                        ctx.combo++;
                        if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                        ctx.score += static_cast<int>((100 + 100 * ctx.combo * 0.05f) * scoreMult);
                        ctx.hp = std::min(1.0f, ctx.hp + 0.02f);
                        ctx.audio.playHitSound(c % 2);
                        ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_100, 20.0f, ctx.clr100, 24));
                        ctx.spawnParticles(popX, ModeMania::HIT_Y, ctx.clr100, 4, 0.6f, 1.8f, 1.8f, 12);
                        if (isPerfect) ctx.triggerSuddenDeathFail();
                    } else if (hitRes == 50) {
                        ctx.count50++;
                        ctx.combo++;
                        if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                        ctx.score += static_cast<int>((50 + 50 * ctx.combo * 0.05f) * scoreMult);
                        ctx.hp = std::min(1.0f, ctx.hp + 0.01f);
                        ctx.audio.playHitSound(c % 2);
                        ctx.popups.push_back(HitPopup(popX, ModeMania::HIT_Y - 20.0f, HIT_50, 18.0f, ctx.clr50, 22));
                        if (isPerfect) ctx.triggerSuddenDeathFail();
                    } else {
                        ctx.audio.playHitSound(c % 2);
                    }
                }
            }

            // 5. Actualizar notas largas sostenidas y ticks
            int holdTicks = 0;
            int completeLN = ctx.maniaEngine.updateActiveHolds(songTime, holdTicks);
            if (holdTicks > 0) {
                ctx.combo += holdTicks;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>(10 * holdTicks * scoreMult);
                ctx.hp = std::min(1.0f, ctx.hp + 0.005f * holdTicks);
            }
            if (completeLN == 300) {
                ctx.count300++;
                ctx.combo++;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>((300 + 300 * ctx.combo * 0.05f) * scoreMult);
                ctx.hp = std::min(1.0f, ctx.hp + 0.04f);
                if (ctx.settings.showHit300) ctx.popups.push_back(HitPopup(ModeMania::TRACK_X + ModeMania::COL_WIDTH * 2.0f, ModeMania::HIT_Y - 20.0f, HIT_300, 24.0f, ctx.clr300, 24));
            }

            allObjectsDone = (ctx.maniaEngine.nextNoteIndex >= ctx.maniaEngine.notes.size());
        } else if (isCatchPlay) {
            // 1. Controles y Movimiento del Catcher
            float moveInput = 0.0f;
            bool dashInput = false;
            float analogX = 0.0f;

            if (isAutoplay) {
                // Autoplay de Catch: se desplaza automáticamente hacia la próxima fruta activa
                for (size_t i = ctx.catchEngine.nextFruitIndex; i < ctx.catchEngine.fruits.size(); ++i) {
                    const auto& f = ctx.catchEngine.fruits[i];
                    if (f.state != NOT_HIT) continue;
                    float diff = f.x - ctx.catchEngine.catcherX;
                    if (std::abs(diff) > 2.0f) {
                        moveInput = (diff > 0.0f) ? 1.0f : -1.0f;
                        if (std::abs(diff) > 40.0f) dashInput = true;
                    } else {
                        ctx.catchEngine.catcherX = f.x;
                    }
                    break;
                }
                ctx.catchEngine.update(16.0f, moveInput, dashInput, 0.0f, songTime, speedRate);
            } else {
                if (ctx.kHeld & ctx.settings.keyCatchLeft)  moveInput -= 1.0f;
                if (ctx.kHeld & ctx.settings.keyCatchRight) moveInput += 1.0f;
                if (ctx.kHeld & ctx.settings.keyCatchDash)  dashInput = true;

                // Soporte de palanca analógica Circle Pad para desplazamiento suave
                if (std::abs(ctx.cpad.dx) > 20) {
                    analogX = static_cast<float>(ctx.cpad.dx) / 150.0f;
                    if (std::abs(ctx.cpad.dx) > 130) dashInput = true;
                }

                // Soporte táctil en pantalla inferior
                if (ctx.isTouching) {
                    ctx.catchEngine.handleTouch(ctx.touch.px, ctx.touch.py, true);
                } else if (ctx.touchJustUp) {
                    ctx.catchEngine.handleTouch(0.0f, 0.0f, false);
                }

                ctx.catchEngine.update(16.0f, moveInput, dashInput, analogX, songTime, speedRate);
            }

            // 2. Comprobar capturas y fallos de frutas
            int scoreAdd = 0;
            int comboAdd = 0;
            float hpAdd = 0.0f;
            int hitResult = ctx.catchEngine.checkCatches(songTime, hit50, scoreAdd, comboAdd, hpAdd);

            if (hitResult > 0) {
                if (hitResult == 300 || hitResult == 1100) ctx.count300++;
                else if (hitResult == 100) ctx.count100++;
                else if (hitResult == 10) ctx.count50++;

                ctx.combo += comboAdd;
                if (ctx.combo > ctx.maxCombo) ctx.maxCombo = ctx.combo;
                ctx.score += static_cast<int>((scoreAdd + (scoreAdd * ctx.combo * 0.035f)) * scoreMult);
                ctx.hp = std::max(0.0f, std::min(1.0f, ctx.hp + hpAdd));

                // Sonido y feedback de captura
                ctx.audio.playHitSound((hitResult == 1100) ? 2 : (hitResult == 100 ? 1 : 0));
                float fruitScrX = ctx.catchEngine.toScreenX(ctx.catchEngine.catcherX);
                if (hitResult == 300) {
                    if (ctx.settings.showHit300) ctx.spawnParticles(fruitScrX, ModeCatch::PLATE_Y, Ui::rgba(255, 100, 140), 5, 0.8f, 2.2f, 2.0f, 14);
                } else if (hitResult == 1100) {
                    if (ctx.settings.showHit300) {
                        ctx.spawnParticles(fruitScrX, ModeCatch::PLATE_Y, ModeCatch::CLR_BANANA, 8, 1.0f, 2.8f, 2.2f, 18);
                        ctx.popups.push_back(HitPopup(fruitScrX, ModeCatch::PLATE_Y - 25.0f, HIT_300, 24.0f, ModeCatch::CLR_BANANA, 24));
                    }
                }
            } else if (hitResult == -1) {
                // ¡MISS de fruta grande!
                ctx.countMiss++;
                ctx.combo = 0;
                ctx.hp = std::max(0.0f, ctx.hp + hpAdd);
                ctx.triggerShake(3.5f, 12.0f);
                float fruitScrX = ctx.catchEngine.toScreenX(ctx.catchEngine.catcherX);
                ctx.popups.push_back(HitPopup(fruitScrX, ModeCatch::PLATE_Y - 20.0f, HIT_MISS, 24.0f, ctx.clrMiss, 24));
                if (isSuddenDeath || isPerfect) {
                    ctx.triggerSuddenDeathFail();
                }
            } else if (hpAdd != 0.0f) {
                ctx.hp = std::max(0.0f, std::min(1.0f, ctx.hp + hpAdd));
            }

            allObjectsDone = (ctx.catchEngine.nextFruitIndex >= ctx.catchEngine.fruits.size());
        } else {
            StandardMode::updateObjects(ctx, songTime, hitRadius, hit300, hit100, hit50, scoreMult, isAutoplay, isRelax, isSpunOut, isSuddenDeath, isPerfect, anyHeld, kActionDown, allObjectsDone, anyActiveSpinner);
    }

        if (ctx.appState == STATE_RESULTS) return;

        if (!anyActiveSpinner) {
            ctx.spinnerRPM = Ui::approach(ctx.spinnerRPM, 0.0f, 0.25f);
            ctx.lastSpinnerBonus = 0;
        }
        ctx.spinnerClearAnim = Ui::approach(ctx.spinnerClearAnim, 0.0f, 0.08f);
        ctx.spinnerBonusAnim = Ui::approach(ctx.spinnerBonusAnim, 0.0f, 0.07f);

        // Actualizar particulas de Sliders / Hits
        for (size_t p = 0; p < ctx.particles.size(); ) {
            ctx.particles[p].x += ctx.particles[p].vx;
            ctx.particles[p].y += ctx.particles[p].vy;
            ctx.particles[p].vy += 0.035f;
            ctx.particles[p].life--;
            if (ctx.particles[p].life <= 0) ctx.particles.erase(ctx.particles.begin() + p);
            else ++p;
        }

        // Envejecer marcas del Hit Error Meter
        for (size_t t = 0; t < ctx.hitErrorTicks.size(); ) {
            ctx.hitErrorTicks[t].life--;
            if (ctx.hitErrorTicks[t].life <= 0) ctx.hitErrorTicks.erase(ctx.hitErrorTicks.begin() + t);
            else ++t;
        }

        // Envejecer popups de calificacion (300 / 100 / 50 / X) una vez por frame logico
        for (size_t i = 0; i < ctx.popups.size(); ) {
            ctx.popups[i].timer--;
            if (ctx.popups[i].timer <= 0) ctx.popups.erase(ctx.popups.begin() + i);
            else ++i;
        }

        // Animaciones y sonido de Combo Pop, Combo Break, Combo Burst y Screen Shake
        if (ctx.combo > ctx.prevCombo) {
            ctx.comboScale = 1.45f;
            ctx.checkComboBurst(ctx.combo);
        } else if (ctx.combo < ctx.prevCombo) {
            ctx.lastComboBurstMilestone = 0;
            if (ctx.prevCombo >= 5) {
                ctx.comboBreakAnim = 1.0f;
                ctx.lastBrokenCombo = ctx.prevCombo;
                ctx.audio.playComboBreak();
                ctx.triggerShake(4.2f, 14.0f);
            }
        }
        ctx.prevCombo = ctx.combo;
        ctx.comboScale = Ui::approach(ctx.comboScale, 1.0f, 0.18f);
        ctx.comboBreakAnim = Ui::approach(ctx.comboBreakAnim, 0.0f, 0.06f);
        if (ctx.comboBurstTimer > 0) ctx.comboBurstTimer--;

        if (ctx.shakeTimer > 0.0f) {
            ctx.shakeTimer = std::max(0.0f, ctx.shakeTimer - 1.0f);
            float damp = ctx.shakeTimer / 14.0f;
            ctx.shakeX = std::sin(ctx.animTick * 2.3f) * ctx.shakeIntensity * damp;
            ctx.shakeY = std::cos(ctx.animTick * 2.9f) * ctx.shakeIntensity * damp;
        } else {
            ctx.shakeX = 0.0f;
            ctx.shakeY = 0.0f;
            ctx.shakeIntensity = 0.0f;
        }

        // 6. Grabación continua de la repetición (Replay) si es juego normal activo
        if (!ctx.isReplayPlaying && !ctx.isPaused && !ctx.stageFailed) {
            uint32_t rKeys = 0;
            if (k1Held) rKeys |= REPLAY_KEY_K1;
            if (k2Held) rKeys |= REPLAY_KEY_K2;
            if (ctx.isTouching) rKeys |= REPLAY_KEY_TOUCH;
            if (smokeActive) rKeys |= REPLAY_KEY_SMOKE;
            if (isTaikoPlay) {
                if (ctx.taikoEngine.drumHitDonLeft > 0.05f) rKeys |= REPLAY_KEY_TAIKO_DON_L;
                if (ctx.taikoEngine.drumHitDonRight > 0.05f) rKeys |= REPLAY_KEY_TAIKO_DON_R;
                if (ctx.taikoEngine.drumHitKatsuLeft > 0.05f) rKeys |= REPLAY_KEY_TAIKO_KATSU_L;
                if (ctx.taikoEngine.drumHitKatsuRight > 0.05f) rKeys |= REPLAY_KEY_TAIKO_KATSU_R;
            }
            if (isManiaPlay) {
                if (ctx.kHeld & ctx.settings.keyManiaCol1) rKeys |= REPLAY_KEY_MANIA_0;
                if (ctx.kHeld & ctx.settings.keyManiaCol2) rKeys |= REPLAY_KEY_MANIA_1;
                if (ctx.kHeld & ctx.settings.keyManiaCol3) rKeys |= REPLAY_KEY_MANIA_2;
                if (ctx.kHeld & ctx.settings.keyManiaCol4) rKeys |= REPLAY_KEY_MANIA_3;
            }
            if (isCatchPlay) {
                if (ctx.kHeld & ctx.settings.keyCatchLeft) rKeys |= REPLAY_KEY_CATCH_L;
                if (ctx.kHeld & ctx.settings.keyCatchRight) rKeys |= REPLAY_KEY_CATCH_R;
                if (ctx.kHeld & ctx.settings.keyCatchDash) rKeys |= REPLAY_KEY_CATCH_DASH;
            }
            ReplayFrame rf;
            rf.timeMs = songTime;
            rf.x = ctx.cursorOsuX;
            rf.y = ctx.cursorOsuY;
            rf.keys = rKeys;
            ctx.currentReplay.frames.push_back(rf);
        }

        int songEndTime = isTaikoPlay ? (!ctx.taikoEngine.notes.empty() ? ctx.taikoEngine.notes.back().endTime : 0)
                        : isCatchPlay ? (!ctx.catchEngine.fruits.empty() ? ctx.catchEngine.fruits.back().time : 0)
                        : isManiaPlay ? (!ctx.maniaEngine.notes.empty() ? ctx.maniaEngine.notes.back().endTime : 0)
                        : (!ctx.beatmap.circles.empty() ? ctx.beatmap.circles.back().endTime : 0);
        if ((allObjectsDone && songEndTime > 0 && songTime > songEndTime + 1000) ||
            ctx.audio.isSongFinished()) {
            ctx.audio.stopMusic();
            ctx.videoPlayer.closeVideo();

            int totalJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
            float acc = (totalJudged > 0)
                ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                : 100.0f;
            const char* grade = "D";
            if (acc >= 99.9f && ctx.countMiss == 0) grade = "SS";
            else if (acc >= 93.0f && ctx.countMiss == 0) grade = "S";
            else if (acc >= 85.0f) grade = "A";
            else if (acc >= 75.0f) grade = "B";
            else if (acc >= 65.0f) grade = "C";

            if (!ctx.allHitErrors.empty()) {
                float sum = 0.0f;
                for (float e : ctx.allHitErrors) sum += e;
                ctx.lastMeanHitError = sum / static_cast<float>(ctx.allHitErrors.size());
                float varSum = 0.0f;
                for (float e : ctx.allHitErrors) {
                    float d = e - ctx.lastMeanHitError;
                    varSum += d * d;
                }
                ctx.lastUnstableRate = std::sqrt(varSum / static_cast<float>(ctx.allHitErrors.size())) * 10.0f;
            } else {
                ctx.lastMeanHitError = 0.0f;
                ctx.lastUnstableRate = 0.0f;
            }

            ctx.lastEarnedPP = calculatePP(
                ctx.beatmap.starRating,
                ctx.beatmap.diff.ar,
                ctx.beatmap.diff.od,
                ctx.maxCombo,
                ctx.beatmap.maxPossibleCombo,
                ctx.count300, ctx.count100, ctx.count50, ctx.countMiss,
                ctx.settings.activeMods
            );

            int currentPlayMode = isTaikoPlay ? 1 : isCatchPlay ? 2 : isManiaPlay ? 3 : 0;
            if (ctx.isReplayPlaying) {
                ctx.isReplayPlaying = false;
                ctx.appState = STATE_RESULTS;
                return;
            }

            // Finalizar metadatos de la repetición grabada en la partida
            ctx.currentReplay.score = ctx.score;
            ctx.currentReplay.maxCombo = ctx.maxCombo;
            ctx.currentReplay.accuracy = acc;
            ctx.currentReplay.grade = grade;
            ctx.currentReplay.count300 = ctx.count300;
            ctx.currentReplay.count100 = ctx.count100;
            ctx.currentReplay.count50 = ctx.count50;
            ctx.currentReplay.countMiss = ctx.countMiss;
            ctx.currentReplay.pp = ctx.lastEarnedPP;
            ctx.currentReplay.gameMode = currentPlayMode;
            ctx.currentReplay.activeMods = ctx.settings.activeMods;
            ctx.currentReplay.title = ctx.beatmap.title;
            ctx.currentReplay.artist = ctx.beatmap.artist;
            ctx.currentReplay.difficulty = ctx.beatmap.version;
            ctx.currentReplay.playerName = ctx.sysUser;

            int achievedRank = -1;
            ctx.newRecordAchieved = ctx.scoreMgr.submitScore(
                ctx.beatmap.filePath, ctx.score, ctx.maxCombo, acc, grade, ctx.settings.activeMods, ctx.lastEarnedPP, currentPlayMode, &achievedRank
            );

            // Almacenar automáticamente la repetición de los 3 mejores récords locales
            if (achievedRank >= 1 && achievedRank <= 3 && !(ctx.settings.activeMods & MOD_AUTO)) {
                ReplayManager::saveTopReplay(ctx.beatmap.filePath, currentPlayMode, achievedRank, ctx.currentReplay);
            }

            if (ctx.multiMgr.inMultiplayer) {
                ctx.multiMgr.matchStarted = false;
                ctx.multiMgr.matchInProgressFlag = false;
                ctx.multiMgr.matchFinished = true;
                ctx.multiMgr.sendLiveGameplay(ctx.score, ctx.combo, ctx.maxCombo, acc, ctx.hp, grade, ctx.count300, ctx.count100, ctx.count50, ctx.countMiss, true, ctx.stageFailed);
                ctx.appState = STATE_MULTI_RESULTS;
            } else {
                ctx.appState = STATE_RESULTS;
            }
            return;
        }
    }

    // =========================================================
    // RENDERIZADO DE LA PARTIDA (SOPORTA 3D EN PANTALLA SUPERIOR Y JUEGO TÁCTIL 1:1 EN PANTALLA INFERIOR)
    // =========================================================
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    float hitRadius = ctx.beatmap.diff.radiusOsu;
    float preempt   = ctx.beatmap.diff.preempt;
    float fadeIn    = ctx.beatmap.diff.fadeIn;
    float hit50     = ctx.beatmap.diff.hit50;
    float rScreen   = hitRadius * SCALE_FACTOR;

    const std::vector<RGBColor>& activePalette =
        !ctx.beatmap.customComboColors.empty() ? ctx.beatmap.customComboColors : activeSkin.comboColors;

    auto renderPlayfield = [&](float screenW, float baseOffsetX, float baseOffsetY, float eyeSign, float slider3DVal, bool drawFullHud) {
        auto depthShift = [&](float px) { return eyeSign * slider3DVal * px; };

        float dBg        = depthShift(-4.0f);
        float dPlayfield = depthShift(0.4f);
        float dCircle    = depthShift(2.6f);
        float dFrontHud  = depthShift(3.6f);
        float dCursor    = depthShift(4.6f);

        auto playX = [&](float osuX, float dOffset = 0.0f) { return baseOffsetX + (osuX * SCALE_FACTOR) + ctx.shakeX + dOffset; };
        auto playY = [&](float osuY) { return baseOffsetY + (osuY * SCALE_FACTOR) + ctx.shakeY; };

        bool renderVideoNow = ctx.settings.enableVideo && ctx.videoPlayer.shouldRender();
        if (isTaikoPlay && ctx.settings.taikoBgMode == 1) {
            renderVideoNow = false; // Forzar imagen en vez de video
        }

        if (renderVideoNow) {
            ctx.videoPlayer.drawRect(ctx.shakeX * 0.5f + dBg, ctx.shakeY * 0.5f, screenW, 240.0f, 0.01f);
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 255) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.02f, screenW, 240.0f, C2D_Color32(10, 8, 18, dimAlpha));
        } else if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(ctx.shakeX * 0.5f + dBg, ctx.shakeY * 0.5f, screenW, 240.0f, 0.01f, 255);
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 255) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.02f, screenW, 240.0f, C2D_Color32(10, 8, 18, dimAlpha));
        } else {
            Ui::backdrop(screenW, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(24, 16, 42), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);
            C2D_DrawRectSolid(baseOffsetX + ctx.shakeX + dPlayfield, baseOffsetY + ctx.shakeY, 0.02f, PLAYFIELD_W, PLAYFIELD_H, Ui::withAlpha(ctx.clrPlayfield, 210));
        }
        // Marco sutil del playfield 4:3 en zona segura (Safe Area - solo en modo Standard)
        if (ctx.settings.showPlayfieldBorder && !isTaikoPlay && !isManiaPlay && !isCatchPlay) {
            drawRectOutline(baseOffsetX + ctx.shakeX + dPlayfield, baseOffsetY + ctx.shakeY, PLAYFIELD_W, PLAYFIELD_H, 1.0f, Ui::rgba(255, 255, 255, 38), 0.03f);
        }

        // Efectos de Kiai Time: Resplandor rítmico y viñetas de energía
        float bpmPulse = ctx.getBpmPulse();
        if (ctx.kiaiGlow > 0.01f) {
            u8 glowAlpha = static_cast<u8>(ctx.kiaiGlow * (45.0f + 110.0f * bpmPulse));
            if (!isTaikoPlay && !isManiaPlay && !isCatchPlay) {
                u32 kiaiBorderClr = Ui::rgba(255, 195, 75, glowAlpha);
                drawRectOutline(baseOffsetX + ctx.shakeX + dPlayfield - 1.0f, baseOffsetY + ctx.shakeY - 1.0f, PLAYFIELD_W + 2.0f, PLAYFIELD_H + 2.0f, 2.5f, kiaiBorderClr, 0.04f);
            }

            float edgeH = (6.0f + 6.0f * bpmPulse) * ctx.kiaiGlow;
            u8 vAlpha = static_cast<u8>(ctx.kiaiGlow * (35.0f + 45.0f * bpmPulse));
            Ui::gradV(0.0f, 0.0f, screenW, edgeH, 0.04f, Ui::rgba(255, 120, 180, vAlpha), Ui::rgba(255, 120, 180, 0));
            Ui::gradV(0.0f, 240.0f - edgeH, screenW, edgeH, 0.04f, Ui::rgba(255, 120, 180, 0), Ui::rgba(255, 120, 180, vAlpha));
        }

        if (isTaikoPlay) {
            int tJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
            float liveAcc = (tJudged > 0)
                ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                : 100.0f;
            ctx.taikoEngine.renderTopScreen(ctx.textBuf, songTime, speedRate, eyeSign, slider3DVal, ctx.inKiai, bpmPulse, ctx.combo, ctx.score, ctx.hp, liveAcc, ctx.beatmap.title.c_str(), ctx.beatmap.version.c_str(), ctx.settings.taikoBgMode, &ctx);
        } else if (isCatchPlay) {
            ctx.catchEngine.renderTopScreen(ctx.textBuf, songTime, speedRate, eyeSign, slider3DVal, ctx.inKiai, &ctx);
        } else if (isManiaPlay) {
            ctx.maniaEngine.renderTopScreen(ctx.textBuf, songTime, speedRate, eyeSign, slider3DVal, ctx.inKiai, bpmPulse, &ctx);
        } else {
                StandardMode::renderPlayfieldObjects(ctx, screenW, baseOffsetX, baseOffsetY, eyeSign, slider3DVal, songTime, preempt, fadeIn, hit50, rScreen, bpmPulse, isHidden, isFlashlight, activePalette);
        }

        // 2B. Chispas y particulas de Sliders / Hits (z = 0.54f..0.56f, debajo del cursor y overlays)
        for (const auto& pt : ctx.particles) {
            float lifeRatio = static_cast<float>(pt.life) / static_cast<float>(std::max(1, pt.maxLife));
            u8 pAlpha = static_cast<u8>(235.0f * lifeRatio);
            u32 pClr = Ui::withAlpha(pt.color, pAlpha);
            float pr = pt.size * (0.45f + 0.55f * lifeRatio);
            float px = baseOffsetX + pt.x + ctx.shakeX + dFrontHud;
            float py = baseOffsetY + pt.y + ctx.shakeY;
            C2D_DrawCircleSolid(px, py, 0.54f, pr * 1.6f, Ui::withAlpha(pt.color, static_cast<u8>(pAlpha * 0.35f)));
            C2D_DrawCircleSolid(px, py, 0.56f, pr, pClr);
        }

        // 3. Efecto Hit Burst y calificaciones flotantes (300 / 100 / 50 / X) (z = 0.58f..0.61f)
        float dHitPopup = depthShift(4.8f);
        for (size_t i = 0; i < ctx.popups.size(); ++i) {
            if (ctx.popups[i].state == HIT_300 && !ctx.settings.showHit300) continue;
            float px = (isTaikoPlay || isManiaPlay || isCatchPlay) ? (ctx.popups[i].x + dHitPopup) : playX(ctx.popups[i].x, dHitPopup);
            float py = (isTaikoPlay || isManiaPlay || isCatchPlay) ? (ctx.popups[i].y + ctx.shakeY) : playY(ctx.popups[i].y);
            float age = static_cast<float>(18 - ctx.popups[i].timer);

            if (ctx.popups[i].state != HIT_MISS && ctx.popups[i].timer > 6) {
                float burstR = ctx.popups[i].burstRadius * (1.0f + age * 0.045f);
                u8 bAlpha = static_cast<u8>(std::max(0, ctx.popups[i].timer * 10));
                u32 bClr = (ctx.popups[i].burstColor & 0x00FFFFFF) | (static_cast<u32>(bAlpha) << 24);
                drawRing(px, py, burstR, 1.8f, bClr, 18, 0.58f);
            }

            const char* txt = "300";
            u32 clr = ctx.clr300;
            const LoadedTexture* pJudgTex = nullptr;
            if (ctx.popups[i].state == HIT_300) {
                txt = "300"; clr = ctx.clr300;
                if (ctx.texSkinHit300.loaded) pJudgTex = &ctx.texSkinHit300;
            } else if (ctx.popups[i].state == HIT_100) {
                txt = "100"; clr = ctx.clr100;
                if (ctx.texSkinHit100.loaded) pJudgTex = &ctx.texSkinHit100;
            } else if (ctx.popups[i].state == HIT_50) {
                txt = "50";  clr = ctx.clr50;
                if (ctx.texSkinHit50.loaded) pJudgTex = &ctx.texSkinHit50;
            } else if (ctx.popups[i].state == HIT_MISS) {
                txt = "X";   clr = ctx.clrMiss;
                if (ctx.texSkinHit0.loaded) pJudgTex = &ctx.texSkinHit0;
            }

            if (pJudgTex && pJudgTex->loaded) {
                // Silenciar juicio si la skin usa un sprite dummy de 1x1 o 2x2 (ej. hit300 transparente en skins competitivas)
                if (pJudgTex->origW <= 2 && pJudgTex->origH <= 2) {
                    continue;
                }
                float nativeW = pJudgTex->getNativeW();
                float nativeH = pJudgTex->getNativeH();
                if (nativeW <= 0.0f || nativeH <= 0.0f) {
                    nativeW = 128.0f;
                    nativeH = 128.0f;
                }
                float drawW = nativeW * SCALE_FACTOR;
                float drawH = nativeH * SCALE_FACTOR;

                // Animación de pop-in/bounce idéntica a osu! stable
                float popScale = 1.0f;
                if (age < 3.0f) {
                    popScale = 0.88f + (age * 0.08f);
                } else if (age < 6.0f) {
                    popScale = 1.04f - ((age - 3.0f) * 0.02f);
                }

                u8 jAlpha = static_cast<u8>(std::min(255, ctx.popups[i].timer * 18));
                pJudgTex->drawCentered(px, py - age * 0.6f, drawW * popScale, drawH * popScale, 0.61f, jAlpha);
            } else if (!isTaikoPlay) {
                drawCenteredTextWithShadow(ctx.textBuf, px, py - age * 0.6f, 0.52f, clr, txt, 0.61f);
            }
        }

        // 4. Cursor, estela y humo de osu!standard (z = 0.62f..0.68f, debajo del HUD, Descanso y Pausa)
        if (!isTaikoPlay && !isManiaPlay && !isCatchPlay && !ctx.isPaused) {
            StandardMode::renderCursorAndSmoke(ctx, baseOffsetX, baseOffsetY, dCursor, anyHeld);
        }

        // 5. HUD In-Game estilo osu!lazer (z = 0.70f..0.78f)
        u32 lazerPink = Ui::rgba(255, 102, 171, 255);
        u32 lazerCyan = Ui::rgba(56, 210, 255, 255);
        u32 hpClr = (ctx.hp > 0.5f) ? Ui::rgba(46, 213, 115, 255) : ((ctx.hp > 0.25f) ? Ui::rgba(255, 195, 40, 255) : Ui::rgba(255, 77, 109, 255));
        if (!isTaikoPlay) {
            if (drawFullHud) {
                // --- Top-Left: Barra de Vida (HP Bar) inclinada estilo osu!lazer ---
                float hpX = 8.0f + dFrontHud;
                float hpY = 6.0f;
                float hpW = 156.0f;
                float hpH = 9.0f;
                Ui::slantRect(hpX - 1.5f, hpY - 1.5f, hpW + 3.0f, hpH + 3.0f, -5.0f, 0.70f, Ui::rgba(10, 7, 20, 185));
                Ui::slantRect(hpX, hpY, hpW, hpH, -5.0f, 0.71f, Ui::rgba(28, 22, 46, 210));
                if (ctx.hp > 0.01f) {
                    float fillW = std::max(4.0f, hpW * std::min(1.0f, ctx.hp));
                    Ui::slantRectGrad(hpX, hpY, fillW, hpH, -5.0f, 0.72f, Ui::brighten(hpClr, 0.45f), hpClr);
                }
                // Acento lateral izquierdo rosa
                Ui::slantRect(hpX - 1.5f, hpY - 1.5f, 4.0f, hpH + 3.0f, -5.0f, 0.73f, lazerPink);

                if (ctx.hp < 0.25f) {
                    float blink = 0.5f + 0.5f * std::sin(ctx.animTick * 0.35f);
                    Ui::slantRect(hpX - 1.5f, hpY - 1.5f, hpW + 3.0f, hpH + 3.0f, -5.0f, 0.74f, Ui::rgba(255, 60, 75, static_cast<int>(90 * blink)));
                }

                // Barra de progreso de la canción inclinada debajo de la vida
                if (!ctx.beatmap.circles.empty()) {
                    float endT = static_cast<float>(ctx.beatmap.circles.back().endTime + 1000);
                    float prog = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
                    Ui::slantRect(hpX - 2.0f, 18.5f, hpW, 3.0f, -3.0f, 0.71f, Ui::rgba(255, 255, 255, 45));
                    if (prog > 0.01f) {
                        Ui::slantRect(hpX - 2.0f, 18.5f, std::max(3.0f, hpW * prog), 3.0f, -3.0f, 0.72f, lazerPink);
                    }
                }

                // --- Top-Right: Panel inclinado translúcido de Score, Precisión y PP ---
                char hudStr[64];
                float scBoxX = 274.0f + dFrontHud;
                float scBoxY = 4.0f;
                float scBoxW = 120.0f;
                float scBoxH = 44.0f;
                Ui::slantRect(scBoxX, scBoxY, scBoxW, scBoxH, -6.0f, 0.70f, Ui::rgba(14, 10, 26, 165));
                Ui::slantRect(scBoxX + scBoxW - 4.0f, scBoxY, 4.0f, scBoxH, -6.0f, 0.72f, lazerPink);

                std::snprintf(hudStr, sizeof(hudStr), "%07d", ctx.score);
                drawTextWithShadow(ctx.textBuf, scBoxX + 10.0f, scBoxY + 2.0f, 0.56f, ctx.clrWhite, hudStr, 0.75f);

                int totalJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
                float acc = (totalJudged > 0)
                    ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
                    : 100.0f;
                std::snprintf(hudStr, sizeof(hudStr), "%.1f%%", acc);
                drawTextWithShadow(ctx.textBuf, scBoxX + 12.0f, scBoxY + 25.0f, 0.38f, lazerCyan, hudStr, 0.75f);

                float livePP = calculatePP(
                    ctx.beatmap.starRating,
                    ctx.beatmap.diff.ar,
                    ctx.beatmap.diff.od,
                    ctx.maxCombo,
                    std::max(1, totalJudged),
                    ctx.count300, ctx.count100, ctx.count50, ctx.countMiss,
                    ctx.settings.activeMods
                );
                std::snprintf(hudStr, sizeof(hudStr), "%.0fpp", livePP);
                drawTextWithShadow(ctx.textBuf, scBoxX + 68.0f, scBoxY + 26.0f, 0.36f, lazerPink, hudStr, 0.75f);

                if (ctx.settings.activeMods != MOD_NONE) {
                    std::string mTag = formatModsString(ctx.settings.activeMods);
                    Ui::slantRect(scBoxX + 18.0f, scBoxY + 46.0f, scBoxW - 22.0f, 14.0f, -4.0f, 0.71f, Ui::rgba(20, 14, 36, 175));
                    drawCenteredText(ctx.textBuf, scBoxX + 18.0f + (scBoxW - 22.0f) * 0.5f, scBoxY + 53.0f, 0.32f, Ui::rgba(255, 215, 70), mTag.c_str(), 0.75f);
                }

                // Leaderboard Multijugador en vivo (osu!lazer side panel)
                if (ctx.multiMgr.inMultiplayer) {
                    auto rankedList = ctx.multiMgr.getLeaderboard();
                    float mSlotX = 6.0f + dFrontHud;
                    float mSlotY = 40.0f;
                    float mSlotW = 86.0f;
                    float mSlotH = 26.0f;
                    float mSlotStep = 29.0f;

                    for (size_t r = 0; r < rankedList.size() && r < 4; ++r) {
                        const auto& item = rankedList[r];
                        float cardY = mSlotY + r * mSlotStep;
                        bool isMe = (item.player->isLocal);

                        u32 rankMedalClr = (r == 0) ? Ui::rgba(255, 209, 102) :
                                           ((r == 1) ? Ui::rgba(0, 229, 255) :
                                           ((r == 2) ? Ui::rgba(180, 100, 240) : Ui::rgba(148, 163, 184)));

                        u32 cardBg = item.player->isFailed ? Ui::rgba(38, 14, 20, 185) :
                                     (isMe ? Ui::rgba(16, 30, 48, 195) : Ui::rgba(16, 12, 28, 175));
                        u32 borderClr = item.player->isFailed ? Ui::rgba(235, 45, 45) :
                                        (isMe ? Ui::rgba(0, 229, 255) : rankMedalClr);

                        Ui::slantRect(mSlotX, cardY, mSlotW, mSlotH, -4.0f, 0.71f, cardBg);
                        Ui::slantRect(mSlotX, cardY, 3.5f, mSlotH, -4.0f, 0.72f, borderClr);

                        char rTag[8];
                        if (item.player->isFailed) {
                            std::snprintf(rTag, sizeof(rTag), "FAIL");
                        } else {
                            std::snprintf(rTag, sizeof(rTag), "#%u", static_cast<unsigned int>(r + 1));
                        }

                        drawText(ctx.textBuf, mSlotX + 6.0f, cardY + 2.5f, 0.31f,
                            item.player->isFailed ? Ui::rgba(235, 45, 45) : (isMe ? Ui::rgba(0, 229, 255) : rankMedalClr), rTag, 0.74f);

                        std::string pName = item.player->name;
                        if (pName.size() > 6) pName = pName.substr(0, 5) + ".";
                        drawText(ctx.textBuf, mSlotX + 24.0f, cardY + 2.5f, 0.30f,
                            isMe ? Ui::rgba(255, 255, 255) : Ui::rgba(215, 210, 235), pName.c_str(), 0.74f);

                        char statVal[24];
                        if (ctx.multiMgr.winCondition == WIN_ACCURACY) {
                            std::snprintf(statVal, sizeof(statVal), "%.1f%%", item.player->accuracy);
                        } else if (ctx.multiMgr.winCondition == WIN_COMBO) {
                            std::snprintf(statVal, sizeof(statVal), "%dx", item.player->combo);
                        } else {
                            if (item.player->score >= 1000000) {
                                std::snprintf(statVal, sizeof(statVal), "%.2fM", item.player->score / 1000000.0f);
                            } else {
                                std::snprintf(statVal, sizeof(statVal), "%ldk", item.player->score / 1000);
                            }
                        }
                        drawText(ctx.textBuf, mSlotX + 6.0f, cardY + 14.5f, 0.28f,
                            item.player->isFailed ? Ui::rgba(235, 90, 90) : Ui::rgba(255, 209, 102), statVal, 0.74f);

                        const char* gTxt = (item.player->grade[0] != '\0') ? item.player->grade : "A";
                        u32 gClr = (gTxt[0] == 'S') ? Ui::rgba(255, 209, 102) :
                                   ((gTxt[0] == 'A') ? ctx.clr100 : (gTxt[0] == 'F' ? Ui::rgba(235, 45, 45) : Ui::rgba(0, 229, 255)));
                        drawCenteredText(ctx.textBuf, mSlotX + mSlotW - 10.0f, cardY + 14.5f, 0.32f, gClr, gTxt, 0.74f);
                    }
                }

                // --- Bottom-Left: Insignia inclinada de Combo estilo osu!lazer ---
                std::snprintf(hudStr, sizeof(hudStr), "%dx", ctx.combo);
                float kiaiPulseScale = (ctx.kiaiGlow > 0.01f) ? (0.07f * ctx.kiaiGlow * bpmPulse) : 0.0f;
                float cScale = 0.56f * (ctx.comboScale + kiaiPulseScale);
                u32 comboClr = (ctx.comboBreakAnim > 0.1f) ? ctx.clrMiss :
                    ((ctx.kiaiGlow > 0.01f) ? Ui::lerpColor(ctx.clrWhite, Ui::rgba(255, 225, 90), ctx.kiaiGlow * bpmPulse * 0.75f) : ctx.clrWhite);

                float cbX = 8.0f + dFrontHud;
                float cbY = 204.0f;
                float cbW = 76.0f;
                float cbH = 28.0f;
                Ui::slantRect(cbX, cbY, cbW, cbH, -5.0f, 0.70f, Ui::rgba(14, 10, 26, 165));
                Ui::slantRect(cbX, cbY, 4.0f, cbH, -5.0f, 0.72f, (ctx.comboBreakAnim > 0.1f) ? ctx.clrMiss : lazerPink);
                drawTextWithShadow(ctx.textBuf, cbX + 10.0f, cbY + 14.0f - 16.0f * cScale, cScale, comboClr, hudStr, 0.75f);

                if (ctx.comboBreakAnim > 0.02f && ctx.lastBrokenCombo >= 5) {
                    char brkStr[32];
                    std::snprintf(brkStr, sizeof(brkStr), "%dx BREAK!", ctx.lastBrokenCombo);
                    u8 bAlpha = static_cast<u8>(ctx.comboBreakAnim * 235.0f);
                    float bLift = (1.0f - ctx.comboBreakAnim) * 14.0f;
                    drawTextWithShadow(ctx.textBuf, 10.0f + dFrontHud, 188.0f - bLift, 0.40f, Ui::rgba(255, 75, 95, bAlpha), brkStr, 0.76f);
                }

                // --- Bottom-Center: Hit Error Meter (barra de timing estilo osu!lazer) ---
                {
                    const float meterCX = 200.0f + dFrontHud;
                    const float meterY  = 231.0f;
                    const float halfW   = 60.0f;
                    float maxWin = std::max(1.0f, hit50);
                    float w100 = halfW * std::min(1.0f, ctx.beatmap.diff.hit100 / maxWin);
                    float w300 = halfW * std::min(1.0f, ctx.beatmap.diff.hit300 / maxWin);

                    Ui::pill(meterCX - halfW - 5.0f, meterY - 5.0f, (halfW + 5.0f) * 2.0f, 10.0f, 0.70f, Ui::rgba(10, 7, 20, 175));
                    C2D_DrawRectSolid(meterCX - halfW, meterY - 1.5f, 0.71f, halfW * 2.0f, 3.0f, Ui::withAlpha(ctx.clr50, 165));
                    C2D_DrawRectSolid(meterCX - w100,  meterY - 1.5f, 0.72f, w100 * 2.0f,  3.0f, Ui::withAlpha(ctx.clr100, 195));
                    C2D_DrawRectSolid(meterCX - w300,  meterY - 1.5f, 0.73f, w300 * 2.0f,  3.0f, Ui::withAlpha(lazerCyan, 230));

                    for (const auto& tk : ctx.hitErrorTicks) {
                        float norm = std::max(-1.0f, std::min(1.0f, tk.errMs / maxWin));
                        float tx = meterCX + norm * halfW;
                        u8 tAlpha = static_cast<u8>(std::min(255, tk.life * 2));
                        u32 baseC = (tk.state == HIT_300) ? lazerCyan : ((tk.state == HIT_100) ? ctx.clr100 : ctx.clr50);
                        u32 tClr = Ui::withAlpha(baseC, tAlpha);
                        C2D_DrawLine(tx, meterY - 4.0f, tClr, tx, meterY + 4.0f, tClr, 1.5f, 0.74f);
                    }

                    // Marca central (0 ms perfecto) y aguja de tendencia media
                    C2D_DrawLine(meterCX, meterY - 5.0f, ctx.clrWhite, meterCX, meterY + 5.0f, ctx.clrWhite, 1.8f, 0.75f);
                    float needleNorm = std::max(-1.0f, std::min(1.0f, ctx.hitErrorNeedle / maxWin));
                    float nx = meterCX + needleNorm * halfW;
                    C2D_DrawTriangle(
                        nx - 3.2f, meterY - 7.2f, lazerPink,
                        nx + 3.2f, meterY - 7.2f, lazerPink,
                        nx,        meterY - 2.8f, ctx.clrWhite,
                        0.76f
                    );
                }

                // --- Right-Center: Key Overlay inclinado K1 / K2 estilo osu!lazer ---
                for (int k = 0; k < 2; ++k) {
                    bool held = (k == 0) ? k1Held : k2Held;
                    int cnt   = (k == 0) ? ctx.k1PressCount : ctx.k2PressCount;
                    float kx  = 358.0f + dFrontHud;
                    float ky  = 90.0f + k * 28.0f;
                    u32 kAccent = (k == 0) ? lazerPink : lazerCyan;
                    u32 kBg = held ? Ui::withAlpha(kAccent, 230) : Ui::rgba(16, 12, 30, 175);
                    Ui::slantRect(kx, ky, 36.0f, 23.0f, -4.0f, 0.71f, kBg);
                    Ui::slantRect(kx + 32.5f, ky, 3.5f, 23.0f, -4.0f, 0.72f, held ? ctx.clrWhite : kAccent);
                    std::snprintf(hudStr, sizeof(hudStr), "%d", cnt);
                    drawCenteredText(ctx.textBuf, kx + 17.0f, ky + 11.5f, 0.37f, held ? Ui::rgba(14, 10, 24) : ctx.clrWhite, hudStr, 0.75f);
                }
            } else {
                // Mini HUD superior discreto en la pantalla táctil inferior (barra fina de HP + Progreso)
                C2D_DrawRectSolid(0.0f, 0.0f, 0.70f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
                if (ctx.hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * ctx.hp, 3.0f, 0.71f, Ui::brighten(hpClr, 0.5f), hpClr);
                if (!ctx.beatmap.circles.empty()) {
                    float endT = static_cast<float>(ctx.beatmap.circles.back().endTime + 1000);
                    float prog = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
                    C2D_DrawRectSolid(0.0f, 3.0f, 0.72f, 320.0f * prog, 1.5f, lazerPink);
                }
            }
        }

        if (drawFullHud && ctx.isReplayPlaying) {
            float banX = 85.0f + dFrontHud;
            float banY = 6.0f;
            float banW = 230.0f;
            float banH = 20.0f;
            Ui::slantRect(banX, banY, banW, banH, -4.0f, 0.72f, Ui::rgba(255, 42, 133, 215));
            Ui::slantRect(banX, banY, 3.5f, banH, -4.0f, 0.74f, Ui::rgba(0, 229, 255));
            char repMsg[64];
            std::snprintf(repMsg, sizeof(repMsg), LOC(STR_GAME_REPLAY_BANNER_FMT), ctx.currentReplay.playerName.c_str());
            drawCenteredTextWithShadow(ctx.textBuf, banX + banW * 0.5f, banY + 10.5f, 0.35f, ctx.clrWhite, repMsg, 0.76f);
        }

        // Marcador en vivo (Leaderboard) estilo osu!lazer en el margen izquierdo (z = 0.71f..0.75f)
        if (drawFullHud && !isTaikoPlay && !ctx.multiMgr.inMultiplayer) {
            float lbX = (6.0f + dFrontHud);
            float lbW = 76.0f;
            float lbH = 25.0f;
            float startY = 42.0f;

            int tJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
            float liveAcc = (tJudged > 0)
                ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                : 100.0f;
            const char* liveGrade = "SS";
            u32 liveGradeClr = lazerPink;
            if (ctx.countMiss > 0) {
                if (liveAcc >= 90.0f)      { liveGrade = "A"; liveGradeClr = ctx.clr100; }
                else if (liveAcc >= 80.0f) { liveGrade = "B"; liveGradeClr = lazerCyan; }
                else if (liveAcc >= 70.0f) { liveGrade = "C"; liveGradeClr = ctx.clr50; }
                else                       { liveGrade = "D"; liveGradeClr = ctx.clrMiss; }
            } else {
                if (liveAcc >= 99.9f)      { liveGrade = "SS"; liveGradeClr = lazerPink; }
                else if (liveAcc >= 93.0f) { liveGrade = "S";  liveGradeClr = Ui::rgba(255, 215, 70); }
                else if (liveAcc >= 85.0f) { liveGrade = "A";  liveGradeClr = ctx.clr100; }
                else if (liveAcc >= 75.0f) { liveGrade = "B";  liveGradeClr = lazerCyan; }
                else                       { liveGrade = "C";  liveGradeClr = ctx.clr50; }
            }

            struct LeaderEntry {
                const char* tag;
                const char* name;
                int pts;
                const char* gr;
                u32 grClr;
                bool isLive;
            };
            std::vector<LeaderEntry> entries;

            if (!ctx.localRecords.empty()) {
                int bestScore = ctx.localRecords[0].bestScore;
                const char* bGr = ctx.localRecords[0].bestGrade.c_str();
                u32 bGrClr = ctx.clrWhite;
                if (ctx.localRecords[0].bestGrade == "SS") bGrClr = lazerPink;
                else if (ctx.localRecords[0].bestGrade == "S") bGrClr = Ui::rgba(255, 215, 70);
                else if (ctx.localRecords[0].bestGrade == "A") bGrClr = ctx.clr100;
                else if (ctx.localRecords[0].bestGrade == "B") bGrClr = lazerCyan;
                else bGrClr = ctx.clrMiss;

                if (ctx.score >= bestScore) {
                    entries.push_back({ "#1", "TU", ctx.score, liveGrade, liveGradeClr, true });
                    entries.push_back({ "#2", "RECORD", bestScore, bGr, bGrClr, false });
                } else {
                    entries.push_back({ "#1", "RECORD", bestScore, bGr, bGrClr, false });
                    entries.push_back({ "#2", "TU", ctx.score, liveGrade, liveGradeClr, true });
                }
            } else {
                entries.push_back({ "#1", "TU", ctx.score, liveGrade, liveGradeClr, true });
            }

            for (size_t e = 0; e < entries.size(); ++e) {
                const auto& ent = entries[e];
                float ey = startY + e * (lbH + 4.0f);
                u32 cAccent = ent.isLive
                    ? (ctx.hasOvertakenBest ? Ui::rgba(255, 215, 60) : lazerPink)
                    : Ui::rgba(90, 80, 125);
                u32 cBg = ent.isLive
                    ? (ctx.hasOvertakenBest ? Ui::rgba(46, 36, 18, 175) : Ui::rgba(26, 18, 44, 175))
                    : Ui::rgba(14, 10, 24, 150);

                if (ent.isLive && ctx.overtakeFlashAnim > 0.05f) {
                    cBg = Ui::lerpColor(cBg, Ui::rgba(255, 215, 60), ctx.overtakeFlashAnim * 0.5f);
                    cAccent = Ui::rgba(255, 255, 220);
                }

                Ui::slantRect(lbX, ey, lbW, lbH, -4.0f, 0.71f, cBg);
                Ui::slantRect(lbX, ey, 3.0f, lbH, -4.0f, 0.72f, cAccent);
                drawText(ctx.textBuf, lbX + 6.0f, ey + 2.5f, 0.30f, ent.isLive ? lazerCyan : ctx.clrWhite, ent.tag, 0.74f);
                drawText(ctx.textBuf, lbX + 23.0f, ey + 3.0f, 0.28f, ent.isLive ? lazerPink : ctx.clrMuted, ent.name, 0.74f);
                drawCenteredText(ctx.textBuf, lbX + lbW - 11.0f, ey + 8.5f, 0.35f, ent.grClr, ent.gr, 0.74f);
                char sBuf[32];
                std::snprintf(sBuf, sizeof(sBuf), "%d", ent.pts);
                drawText(ctx.textBuf, lbX + 6.0f, ey + 13.5f, 0.28f, ctx.clrWhite, sBuf, 0.74f);
            }
        }

        // 6. Pantalla central de Descanso (Break / Wait Screen) en z = 0.80f..0.88f (SIEMPRE por encima del cursor)
        if (ctx.inBreakPeriod && !ctx.isPaused) {
            if (isTaikoPlay) {
                float bcX = 200.0f + dFrontHud;
                float bcY = 46.0f;
                Ui::panel(bcX - 65.0f, bcY - 18.0f, 130.0f, 38.0f, 0.80f, Ui::rgba(16, 11, 28, 195), lazerPink);
                drawCenteredText(ctx.textBuf, bcX, bcY - 7.0f, 0.36f, ctx.clrWhite, LOC(STR_GAME_BREAK), 0.84f);
                char cdStr[32];
                float sLeft = std::max(0.0f, ctx.breakRemainingMs / 1000.0f);
                std::snprintf(cdStr, sizeof(cdStr), "%.1f s", sLeft);
                drawCenteredText(ctx.textBuf, bcX, bcY + 8.0f, 0.35f, lazerCyan, cdStr, 0.84f);
            } else {
                float bcX = (screenW * 0.5f) + dFrontHud;
                float bcY = 114.0f;
                float cardW = 192.0f;
                float cardH = 100.0f;

                // Tarjeta translúcida estilo osu!lazer por encima del cursor (z = 0.80f..0.86f)
                Ui::panel(bcX - cardW * 0.5f, bcY - cardH * 0.5f, cardW, cardH, 0.80f, Ui::rgba(14, 10, 26, 205), lazerPink);
                Ui::slantRect(bcX - 52.0f, bcY - 43.0f, 104.0f, 16.0f, -4.0f, 0.82f, lazerPink);
                drawCenteredText(ctx.textBuf, bcX, bcY - 35.0f, 0.35f, ctx.clrWhite, LOC(STR_GAME_BREAK), 0.85f);

                int tJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
                float bAcc = (tJudged > 0)
                    ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                    : 100.0f;
                const char* bGrade = "SS";
                u32 bGradeClr = lazerPink;
                if (ctx.countMiss > 0) {
                    if (bAcc >= 90.0f)      { bGrade = "A"; bGradeClr = ctx.clr100; }
                    else if (bAcc >= 80.0f) { bGrade = "B"; bGradeClr = lazerCyan; }
                    else if (bAcc >= 70.0f) { bGrade = "C"; bGradeClr = ctx.clr50; }
                    else                    { bGrade = "D"; bGradeClr = ctx.clrMiss; }
                } else {
                    if (bAcc >= 99.9f)      { bGrade = "SS"; bGradeClr = lazerPink; }
                    else if (bAcc >= 93.0f) { bGrade = "S";  bGradeClr = Ui::rgba(255, 215, 70); }
                    else if (bAcc >= 85.0f) { bGrade = "A";  bGradeClr = ctx.clr100; }
                    else if (bAcc >= 75.0f) { bGrade = "B";  bGradeClr = lazerCyan; }
                    else                    { bGrade = "C";  bGradeClr = ctx.clr50; }
                }

                Ui::glow(bcX, bcY - 4.0f, 0.81f, 30.0f, Ui::withAlpha(bGradeClr, 105), 3);
                drawCenteredTextWithShadow(ctx.textBuf, bcX, bcY - 4.0f, 0.86f, bGradeClr, bGrade, 0.85f);

                char cdStr[64];
                float sLeft = std::max(0.0f, ctx.breakRemainingMs / 1000.0f);
                std::snprintf(cdStr, sizeof(cdStr), LOC(STR_GAME_CIRCLES_IN), sLeft);
                drawCenteredText(ctx.textBuf, bcX, bcY + 22.0f, 0.38f, ctx.clrWhite, cdStr, 0.85f);

                std::snprintf(cdStr, sizeof(cdStr), "%s: %.1f%%  |  %dx", LOC(STR_ACCURACY), bAcc, ctx.combo);
                drawCenteredText(ctx.textBuf, bcX, bcY + 37.0f, 0.32f, lazerCyan, cdStr, 0.85f);

                // Indicador de advertencia visual hacia la próxima nota (últimos 2.2 segundos)
                if (ctx.breakRemainingMs <= 2200) {
                    float warnPhase = static_cast<float>(2200 - ctx.breakRemainingMs) / 2200.0f;
                    float blink = 0.5f + 0.5f * std::sin(ctx.animTick * 0.45f);

                    Ui::slantRect(bcX - 48.0f, bcY - 68.0f, 96.0f, 16.0f, -4.0f, 0.84f, Ui::withAlpha(ctx.clrMiss, static_cast<u8>(190 + 65 * blink)));
                    drawCenteredText(ctx.textBuf, bcX, bcY - 60.0f, 0.35f, ctx.clrWhite, LOC(STR_GAME_WARNING), 0.86f);

                    float nx = playX(ctx.breakNextObjPos.x, dCircle);
                    float ny = playY(ctx.breakNextObjPos.y);
                    float targetR = rScreen * (1.1f + (1.0f - warnPhase) * 1.5f);

                    drawRing(nx, ny, targetR, 2.2f, Ui::withAlpha(lazerPink, static_cast<u8>(220 * blink)), 24, 0.86f);
                    drawRing(nx, ny, rScreen, 1.4f, Ui::withAlpha(ctx.clrWhite, 190), 20, 0.86f);

                    float aDist = targetR + 8.0f;
                    C2D_DrawTriangle(nx - aDist, ny, lazerPink, nx - aDist - 7.0f, ny - 6.0f, lazerPink, nx - aDist - 7.0f, ny + 6.0f, lazerPink, 0.87f);
                    C2D_DrawTriangle(nx + aDist, ny, lazerPink, nx + aDist + 7.0f, ny - 6.0f, lazerPink, nx + aDist + 7.0f, ny + 6.0f, lazerPink, 0.87f);
                }
            }
        }

        // 7. Botón SKIP >> inclinado estilo osu!lazer (z = 0.89f..0.91f)
        if (ctx.skipAvailable && !ctx.isPaused) {
            float skipX = drawFullHud ? (290.0f + dFrontHud) : 206.0f;
            float skipY = drawFullHud ? 198.0f : 196.0f;
            float skipW = 102.0f;
            float skipH = 32.0f;
            Ui::slantButton(skipX, skipY, skipW, skipH, -6.0f, lazerPink, "SKIP >>", LOC(STR_GAME_SKIP_SUB), ctx.textBuf, true, 0.89f);
        }

        // 8. Combo Burst (Aparición de personaje / mascota en hitos de combo)
        if (ctx.comboBurstTimer > 0 && drawFullHud && !ctx.isPaused) {
            float slideT = 1.0f;
            float alpha = 1.0f;
            if (ctx.comboBurstTimer > 32) {
                float t = static_cast<float>(48 - ctx.comboBurstTimer) / 16.0f;
                slideT = std::sin(t * (static_cast<float>(M_PI) * 0.5f));
                alpha = std::min(1.0f, t * 1.5f);
            } else if (ctx.comboBurstTimer <= 14) {
                float t = static_cast<float>(ctx.comboBurstTimer) / 14.0f;
                slideT = t;
                alpha = t;
            }

            float burstW = 120.0f;
            float burstH = 175.0f;
            float restX = (ctx.comboBurstSide == 0) ? (screenW - burstW + 15.0f) : -15.0f;
            float offX  = (ctx.comboBurstSide == 0) ? (screenW + 10.0f) : (-burstW - 10.0f);
            float dBurst = depthShift(4.4f);
            float burstX = offX + (restX - offX) * slideT + dBurst;
            float burstY = 240.0f - burstH + 5.0f;
            u8 aVal = static_cast<u8>(255.0f * alpha);

            if (ctx.texSkinComboBurst.loaded) {
                ctx.texSkinComboBurst.drawRect(burstX, burstY, burstW, burstH, 0.78f, aVal);
            } else {
                float cx = burstX + burstW * 0.5f;
                float cy = burstY + burstH * 0.55f;
                u32 pPink = Ui::withAlpha(lazerPink, aVal);
                u32 pGold = Ui::withAlpha(Ui::rgba(255, 215, 60), aVal);
                u32 pDark = Ui::withAlpha(Ui::rgba(26, 20, 42), static_cast<u8>(aVal * 0.90f));

                Ui::glow(cx, cy, 0.76f, 65.0f, Ui::withAlpha(pPink, static_cast<u8>(aVal * 0.45f)), 3);
                C2D_DrawCircleSolid(cx, cy - 14.0f, 0.77f, 32.0f, pPink);
                C2D_DrawCircleSolid(cx, cy - 14.0f, 0.78f, 28.0f, pDark);

                C2D_DrawTriangle(cx - 24.0f, cy - 30.0f, pPink, cx - 10.0f, cy - 42.0f, pPink, cx - 8.0f, cy - 28.0f, pPink, 0.78f);
                C2D_DrawTriangle(cx + 24.0f, cy - 30.0f, pPink, cx + 10.0f, cy - 42.0f, pPink, cx + 8.0f, cy - 28.0f, pPink, 0.78f);
                C2D_DrawTriangle(cx - 20.0f, cy - 30.0f, pGold, cx - 11.0f, cy - 38.0f, pGold, cx - 9.0f, cy - 28.0f, pGold, 0.79f);
                C2D_DrawTriangle(cx + 20.0f, cy - 30.0f, pGold, cx + 11.0f, cy - 38.0f, pGold, cx + 9.0f, cy - 28.0f, pGold, 0.79f);

                drawRing(cx - 10.0f, cy - 16.0f, 6.0f, 1.8f, pPink, 14, 0.79f);
                drawRing(cx + 10.0f, cy - 16.0f, 6.0f, 1.8f, pPink, 14, 0.79f);
                C2D_DrawCircleSolid(cx - 12.0f, cy - 10.0f, 0.79f, 4.0f, Ui::withAlpha(Ui::rgba(255, 140, 180), aVal));
                C2D_DrawCircleSolid(cx + 12.0f, cy - 10.0f, 0.79f, 4.0f, Ui::withAlpha(Ui::rgba(255, 140, 180), aVal));

                float sBob = std::sin(ctx.animTick * 0.2f) * 4.0f;
                Ui::glow(cx - 36.0f, cy - 35.0f + sBob, 0.79f, 10.0f, pGold, 2);
                C2D_DrawCircleSolid(cx - 36.0f, cy - 35.0f + sBob, 0.79f, 4.0f, pGold);
                Ui::glow(cx + 36.0f, cy - 25.0f - sBob, 0.79f, 10.0f, pGold, 2);
                C2D_DrawCircleSolid(cx + 36.0f, cy - 25.0f - sBob, 0.79f, 4.5f, pGold);
            }

            char cbStr[32];
            std::snprintf(cbStr, sizeof(cbStr), "%d COMBO!", ctx.comboBurstValue);
            float pillW = 96.0f;
            float pillH = 18.0f;
            float pillX = burstX + (burstW - pillW) * 0.5f;
            float pillY = burstY + burstH - 26.0f;

            Ui::slantRect(pillX, pillY, pillW, pillH, -4.0f, 0.79f, Ui::withAlpha(lazerPink, aVal));
            drawCenteredText(ctx.textBuf, pillX + pillW * 0.5f, pillY + pillH * 0.5f, 0.38f, Ui::withAlpha(ctx.clrWhite, aVal), cbStr, 0.80f);
        }

        // 9. Overlay de Pausa en pantalla superior (estilo osu!lazer con resumen de canción y estadísticas)
        if (drawFullHud && ctx.isPaused) {
            float dPause = depthShift(5.0f);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.91f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 185));

            Ui::lazerHeader(400.0f, 0.92f, LOC(STR_GAME_PAUSED), "PAUSE", lazerPink, ctx.textBuf, true);

            float pCardX = 36.0f + dPause;
            float pCardY = 38.0f;
            float pCardW = 328.0f;
            float pCardH = 162.0f;
            Ui::panel(pCardX, pCardY, pCardW, pCardH, 0.93f, Ui::rgba(16, 11, 30, 195), lazerPink);

            // Título y Dificultad de la canción en pausa
            std::string pTitle = ctx.beatmap.title.empty() ? "osu! 3DS Beatmap" : ctx.beatmap.title;
            if (pTitle.size() > 34) pTitle = pTitle.substr(0, 31) + "...";
            drawTextWithShadow(ctx.textBuf, pCardX + 14.0f, pCardY + 10.0f, 0.46f, ctx.clrWhite, pTitle.c_str(), 0.95f);

            char diffLine[96];
            std::snprintf(diffLine, sizeof(diffLine), "%s  •  [%s]",
                          ctx.beatmap.artist.empty() ? "Unknown" : ctx.beatmap.artist.c_str(),
                          ctx.beatmap.version.empty() ? "Normal" : ctx.beatmap.version.c_str());
            if (std::strlen(diffLine) > 42) diffLine[42] = '\0';
            drawText(ctx.textBuf, pCardX + 14.0f, pCardY + 30.0f, 0.36f, lazerCyan, diffLine, 0.95f);

            // Barra de progreso actual del mapa
            float pProg = 0.0f;
            if (!ctx.beatmap.circles.empty()) {
                float endT = static_cast<float>(ctx.beatmap.circles.back().endTime + 1000);
                pProg = std::max(0.0f, std::min(1.0f, static_cast<float>(songTime) / std::max(1.0f, endT)));
            }
            Ui::slantRect(pCardX + 14.0f, pCardY + 52.0f, pCardW - 28.0f, 8.0f, -3.0f, 0.94f, Ui::rgba(28, 22, 48, 220));
            if (pProg > 0.01f) {
                Ui::slantRectGrad(pCardX + 14.0f, pCardY + 52.0f, std::max(6.0f, (pCardW - 28.0f) * pProg), 8.0f, -3.0f, 0.95f, lazerPink, lazerCyan);
            }
            char progTxt[32];
            std::snprintf(progTxt, sizeof(progTxt), LOC(STR_GAME_PAUSE_PROGRESS_FMT), pProg * 100.0f);
            drawText(ctx.textBuf, pCardX + 14.0f, pCardY + 64.0f, 0.34f, Ui::rgba(200, 210, 230), progTxt, 0.95f);

            // 3 Cajas inclinadas con estadísticas en vivo (Score, Precisión, Combo)
            int tJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
            float pAcc = (tJudged > 0)
                ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (tJudged * 300.0f)) * 100.0f
                : 100.0f;

            float statY = pCardY + 84.0f;
            float statW = 94.0f;
            float statH = 38.0f;

            // Caja 1: Puntuación
            Ui::slantRect(pCardX + 14.0f, statY, statW, statH, -5.0f, 0.94f, Ui::rgba(24, 18, 44, 215));
            Ui::slantRect(pCardX + 14.0f, statY, 3.5f, statH, -5.0f, 0.95f, lazerPink);
            drawText(ctx.textBuf, pCardX + 22.0f, statY + 4.0f, 0.30f, Ui::rgba(170, 180, 205), LOC(STR_SCORE), 0.96f);
            char stVal[32];
            std::snprintf(stVal, sizeof(stVal), "%07d", ctx.score);
            drawTextWithShadow(ctx.textBuf, pCardX + 22.0f, statY + 18.0f, 0.40f, ctx.clrWhite, stVal, 0.96f);

            // Caja 2: Precisión
            Ui::slantRect(pCardX + 117.0f, statY, statW, statH, -5.0f, 0.94f, Ui::rgba(24, 18, 44, 215));
            Ui::slantRect(pCardX + 117.0f, statY, 3.5f, statH, -5.0f, 0.95f, lazerCyan);
            drawText(ctx.textBuf, pCardX + 125.0f, statY + 4.0f, 0.30f, Ui::rgba(170, 180, 205), LOC(STR_ACCURACY), 0.96f);
            std::snprintf(stVal, sizeof(stVal), "%.1f%%", pAcc);
            drawTextWithShadow(ctx.textBuf, pCardX + 125.0f, statY + 18.0f, 0.42f, lazerCyan, stVal, 0.96f);

            // Caja 3: Combo
            Ui::slantRect(pCardX + 220.0f, statY, statW, statH, -5.0f, 0.94f, Ui::rgba(24, 18, 44, 215));
            Ui::slantRect(pCardX + 220.0f, statY, 3.5f, statH, -5.0f, 0.95f, Ui::rgba(46, 213, 115));
            drawText(ctx.textBuf, pCardX + 228.0f, statY + 4.0f, 0.30f, Ui::rgba(170, 180, 205), LOC(STR_MAX_COMBO), 0.96f);
            std::snprintf(stVal, sizeof(stVal), "%dx", ctx.maxCombo);
            drawTextWithShadow(ctx.textBuf, pCardX + 228.0f, statY + 18.0f, 0.42f, Ui::rgba(46, 213, 115), stVal, 0.96f);

            // Desglose de aciertos inferior (300 / 100 / 50 / X)
            char hitsSummary[96];
            std::snprintf(hitsSummary, sizeof(hitsSummary), "300: %d   |   100: %d   |   50: %d   |   MISS: %d",
                          ctx.count300, ctx.count100, ctx.count50, ctx.countMiss);
            drawCenteredText(ctx.textBuf, pCardX + pCardW * 0.5f, pCardY + 141.0f, 0.36f, ctx.clrWhite, hitsSummary, 0.96f);

            if (isTaikoPlay) {
                u32 bClr = (ctx.settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25, 230) : ((ctx.settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215, 230) : Ui::rgba(165, 55, 185, 230));
                Ui::pill(90.0f + dPause, 208.0f, 220.0f, 18.0f, 0.94f, bClr);
                char bgInfo[64];
                std::snprintf(bgInfo, sizeof(bgInfo), "X: %s (%s)", LOC(STR_SET_TAIKO_BG), ctx.settings.getTaikoBgModeName());
                drawCenteredText(ctx.textBuf, 200.0f + dPause, 217.0f, 0.32f, ctx.clrWhite, bgInfo, 0.96f);
            } else {
                drawCenteredText(ctx.textBuf, 200.0f + dPause, 216.0f, 0.35f, Ui::rgba(200, 210, 230),
                                 LOC(STR_GAME_PAUSE_HINT), 0.95f);
            }
        }
        // Destello inicial de Kiai Time
        if (ctx.kiaiBurstFlash > 0.01f) {
            u8 fAlpha = static_cast<u8>(ctx.kiaiBurstFlash * 65.0f);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.78f, screenW, 240.0f, Ui::rgba(255, 240, 200, fAlpha));
        }

        // Borde de aviso al romper combo (Combo Break)
        if (ctx.comboBreakAnim > 0.01f) {
            u8 cbAlpha = static_cast<u8>(ctx.comboBreakAnim * 110.0f);
            drawRectOutline(0.0f, 0.0f, screenW, 240.0f, 4.0f, Ui::rgba(255, 60, 75, cbAlpha), 0.79f);
        }

        Ui::drawTransition(screenW, 240.0f, ctx.screenFade, ctx.clrPrimary);
    };

    // --- PANTALLA SUPERIOR (OJO IZQUIERDO Y OJO DERECHO EN 3D + HUD COMPLETO) ---
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        renderPlayfield(400.0f, PLAYFIELD_TOP_OFFSET_X, PLAYFIELD_TOP_OFFSET_Y, eyeSign, ctx.slider3D, true);
    }

    // --- PANTALLA INFERIOR: JUEGO TÁCTIL DIRECTO 1:1, SUPERFICIE DE TABLET O PAUSA ---
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    if (ctx.isPaused) {
        u32 lazerPink = Ui::rgba(255, 102, 171, 255);
        Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(20, 13, 36), Ui::rgba(10, 7, 20), lazerPink, 0.01f);

        // Medallón ancla izquierdo estilo osu!lazer (tocar para CONTINUAR)
        float mCx = 46.0f;
        float mCy = 118.0f;
        float mR  = 34.0f;
        float pulse = 0.5f + 0.5f * std::sin(ctx.animTick * 0.12f);
        Ui::ring(mCx, mCy, 0.18f, mR + 5.0f + pulse * 3.0f, 2.0f, Ui::withAlpha(lazerPink, 90), 28);
        Ui::glow(mCx, mCy, 0.20f, mR + 6.0f, Ui::withAlpha(lazerPink, 110), 3);
        C2D_DrawCircleSolid(mCx, mCy, 0.25f, mR, 0xFFFFFFFF);
        C2D_DrawCircleSolid(mCx, mCy, 0.28f, mR - 2.5f, lazerPink);
        C2D_DrawCircleSolid(mCx, mCy, 0.30f, mR - 5.5f, 0xFF5517B8);

        // Icono de Pausa (||) dentro del medallón
        C2D_DrawRectSolid(mCx - 8.5f, mCy - 11.0f, 0.35f, 5.5f, 22.0f, 0xFFFFFFFF);
        C2D_DrawRectSolid(mCx + 3.0f, mCy - 11.0f, 0.35f, 5.5f, 22.0f, 0xFFFFFFFF);

        // 3 Botones inclinados estilo osu!lazer a la derecha
        const char* pTitles[3] = {
            LOC(STR_PAUSE_RESUME),
            LOC(STR_PAUSE_RETRY),
            LOC(STR_PAUSE_QUIT)
        };
        const char* pSubs[3] = {
            LOC(STR_PAUSE_SUB_RESUME),
            LOC(STR_PAUSE_SUB_RETRY),
            LOC(STR_PAUSE_SUB_QUIT)
        };
        u32 pAccents[3] = {
            Ui::rgba(46, 213, 115, 255),  // Verde esmeralda
            Ui::rgba(66, 144, 251, 255),  // Azul lazer
            Ui::rgba(255, 77, 109, 255)   // Rosa/Rojo lazer
        };

        for (int i = 0; i < 3; ++i) {
            bool sel = (ctx.pauseOption == i);
            float bx = sel ? 92.0f : 98.0f;
            float by = 38.0f + i * 56.0f;
            float bw = 212.0f;
            float bh = 44.0f;
            Ui::slantButton(bx, by, bw, bh, -6.0f, pAccents[i], pTitles[i], pSubs[i], ctx.textBuf, sel, 0.25f);
        }

        if (isTaikoPlay) {
            u32 bClr = (ctx.settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25, 240) : ((ctx.settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215, 240) : Ui::rgba(165, 55, 185, 240));
            Ui::pill(40.0f, 206.0f, 240.0f, 24.0f, 0.25f, bClr);
            char bgInfo[64];
            std::snprintf(bgInfo, sizeof(bgInfo), LOC(STR_TAIKO_BG_TOUCH_HINT), LOC(STR_SET_TAIKO_BG), ctx.settings.getTaikoBgModeName());
            drawCenteredText(ctx.textBuf, 160.0f, 218.0f, 0.34f, ctx.clrWhite, bgInfo, 0.85f);
        }
        Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    } else if (isTaikoPlay) {
        // Modo osu!taiko: Tambor interactivo táctil completo en la pantalla inferior (estilo 3DS 1:1)
        ctx.taikoEngine.renderBottomDrum(ctx.textBuf, static_cast<float>(ctx.animTick), &ctx.settings, taikoGuideAlpha, &ctx);

        if (ctx.skipAvailable) {
            Ui::slantButton(206.0f, 196.0f, 104.0f, 34.0f, -6.0f, Ui::rgba(255, 102, 171), "SKIP >>", LOC(STR_SKIP_HINT), ctx.textBuf, true, 0.85f);
        }
        Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    } else if (isManiaPlay) {
        // Modo osu!mania: Teclado interactivo de 4 columnas en la pantalla inferior
        Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(8, 6, 14), ctx.clrPrimary, 0.01f);
        ctx.maniaEngine.renderBottomLanes(ctx.textBuf, static_cast<float>(ctx.animTick), &ctx);

        u32 hpClr = (ctx.hp > 0.5f) ? ctx.clr100 : ((ctx.hp > 0.25f) ? ctx.clr50 : ctx.clrMiss);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.29f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
        if (ctx.hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * ctx.hp, 3.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);

        if (ctx.skipAvailable) {
            Ui::slantButton(206.0f, 196.0f, 104.0f, 34.0f, -6.0f, Ui::rgba(255, 102, 171), "SKIP >>", LOC(STR_SKIP_HINT), ctx.textBuf, true, 0.85f);
        }
        Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    } else if (isCatchPlay) {
        // Modo osu!catch: Deck de control táctil con radar de frutas en la pantalla inferior
        Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(16, 22, 34), Ui::rgba(8, 12, 18), ctx.clrPrimary, 0.01f);
        ctx.catchEngine.renderBottomDeck(ctx.textBuf, static_cast<float>(ctx.animTick), songTime, &ctx);

        u32 hpClr = (ctx.hp > 0.5f) ? ctx.clr100 : ((ctx.hp > 0.25f) ? ctx.clr50 : ctx.clrMiss);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.29f, 320.0f, 4.0f, Ui::rgba(0, 0, 0, 150));
        if (ctx.hp > 0.0f) Ui::gradH(0.0f, 0.0f, 320.0f * ctx.hp, 3.0f, 0.35f, Ui::brighten(hpClr, 0.6f), hpClr);

        if (ctx.skipAvailable) {
            Ui::slantButton(206.0f, 196.0f, 104.0f, 34.0f, -6.0f, Ui::rgba(255, 102, 171), "SKIP >>", LOC(STR_SKIP_HINT), ctx.textBuf, true, 0.85f);
        }
        Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    } else if (ctx.settings.playOnBottomScreen) {
        // ¡Modo Táctil Directo! Renderiza el playfield completo 1:1 en la pantalla inferior (320x240)
        renderPlayfield(320.0f, PLAYFIELD_BOT_OFFSET_X, PLAYFIELD_BOT_OFFSET_Y, 0.0f, 0.0f, false);
    } else {
        StandardMode::renderBottomTablet(ctx);
    }

    C3D_FrameEnd(0);
}
