#include "ResultsScreen.hpp"

void runResultsScreen(AppContext& ctx) {
    if (ctx.multiMgr.inMultiplayer) {
        ctx.appState = STATE_MULTI_RESULTS;
        return;
    }
    if (ctx.kDown & KEY_A) {
        ctx.audio.playHitSound(2);
        ctx.startSelectedBeatmap();
        return;
    }
    if (ctx.kDown & (KEY_B | KEY_START | KEY_SELECT)) {
        ctx.audio.playHitSound(0);
        ctx.applyActiveSkinAudio();
        ctx.appState = STATE_SONG_SELECT;
        ctx.updateSongPreview();
        return;
    }

    static int resultsBottomTab = 0;
    if (!ctx.stageFailed && (ctx.kDown & (KEY_X | KEY_Y))) {
        if (ctx.kDown & KEY_Y) {
            // Botón Y: Ver Replay de la partida actual
            if (ctx.currentReplay.isValid()) {
                ctx.audio.playHitSound(2);
                ctx.playReplay(ctx.currentReplay);
                return;
            }
        } else {
            resultsBottomTab = 1 - resultsBottomTab;
            ctx.audio.playHitSound(0);
        }
    }

    if (!ctx.stageFailed && (ctx.kDown & KEY_SELECT)) {
        // Botón SELECT: Guardar Replay en la tarjeta SD
        std::string err;
        std::string savedPath = ReplayManager::exportReplay(ctx.currentReplay, &err);
        if (!savedPath.empty()) {
            ctx.replayToastMessage = "Replay guardada en SD (/replays)";
            ctx.replayToastTimer = 180;
            ctx.audio.playHitSound(2);
        } else {
            ctx.replayToastMessage = err.empty() ? "Error al guardar replay" : err;
            ctx.replayToastTimer = 180;
            ctx.audio.playHitSound(0);
        }
    }

    if (ctx.touchJustDown) {
        if (ctx.stageFailed) {
            // Pantalla inferior de GAME OVER estilo osu!lazer:
            // Medallón izquierdo (cx=46, cy=120, r=38) -> Reintentar
            float dx = static_cast<float>(ctx.touch.px) - 46.0f;
            float dy = static_cast<float>(ctx.touch.py) - 120.0f;
            if ((dx * dx + dy * dy) <= (40.0f * 40.0f)) {
                ctx.audio.playHitSound(2);
                ctx.startSelectedBeatmap();
                return;
            }
            // Botones inclinados a la derecha (x = 90..314)
            if (ctx.touch.px >= 90 && ctx.touch.px <= 314) {
                if (ctx.touch.py >= 54 && ctx.touch.py <= 108) {
                    // Botón 1: REINTENTAR
                    ctx.audio.playHitSound(2);
                    ctx.startSelectedBeatmap();
                    return;
                } else if (ctx.touch.py >= 120 && ctx.touch.py <= 174) {
                    // Botón 2: CONTINUAR / SALIR A CANCIONES
                    ctx.audio.playHitSound(0);
                    ctx.applyActiveSkinAudio();
                    ctx.appState = STATE_SONG_SELECT;
                    ctx.updateSongPreview();
                    return;
                }
            }
        } else {
            // Pantalla inferior de RESULTADOS estilo osu!lazer:
            // Pestañas superiores en la tarjeta (y: 6..42, x >= 186)
            if (ctx.touch.py >= 6 && ctx.touch.py <= 42 && ctx.touch.px >= 186) {
                resultsBottomTab = 1 - resultsBottomTab;
                ctx.audio.playHitSound(0);
            }
            // Botones de acción de Replay o pulsación sobre un Récord
            else if (resultsBottomTab == 0 && ctx.touch.py >= 146 && ctx.touch.py <= 186) {
                if (ctx.touch.px >= 12 && ctx.touch.px <= 156) {
                    // Botón VER REPLAY
                    if (ctx.currentReplay.isValid()) {
                        ctx.audio.playHitSound(2);
                        ctx.playReplay(ctx.currentReplay);
                        return;
                    }
                } else if (ctx.touch.px >= 160 && ctx.touch.px <= 308) {
                    // Botón GUARDAR REPLAY
                    std::string err;
                    std::string savedPath = ReplayManager::exportReplay(ctx.currentReplay, &err);
                    if (!savedPath.empty()) {
                        ctx.replayToastMessage = "Replay guardada en SD (/replays)";
                        ctx.replayToastTimer = 180;
                        ctx.audio.playHitSound(2);
                    } else {
                        ctx.replayToastMessage = err.empty() ? "Error al guardar replay" : err;
                        ctx.replayToastTimer = 180;
                        ctx.audio.playHitSound(0);
                    }
                }
            } else if (resultsBottomTab == 1 && ctx.touch.py >= 48 && ctx.touch.py <= 180) {
                int row = static_cast<int>((ctx.touch.py - 48) / 26);
                if (row >= 0 && row < 3) {
                    int cMode = (ctx.settings.gameMode == MODE_TAIKO || ctx.beatmap.mode == 1) ? 1
                              : (ctx.settings.gameMode == MODE_CATCH || ctx.beatmap.mode == 2) ? 2
                              : (ctx.settings.gameMode == MODE_MANIA || ctx.beatmap.mode == 3) ? 3 : 0;
                    if (ReplayManager::hasTopReplay(ctx.beatmap.filePath, cMode, row + 1)) {
                        std::string repPath = ReplayManager::getTopReplayPath(ctx.beatmap.filePath, cMode, row + 1);
                        if (ctx.playReplayFromFile(repPath)) {
                            ctx.audio.playHitSound(2);
                            return;
                        }
                    }
                }
            }
            // Barra inferior de 3 botones inclinados (y: 190..236)
            else if (ctx.touch.py >= 190 && ctx.touch.py <= 236) {
                if (ctx.touch.px >= 4 && ctx.touch.px <= 98) {
                    resultsBottomTab = 1 - resultsBottomTab;
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 100 && ctx.touch.px <= 200) {
                    ctx.audio.playHitSound(2);
                    ctx.startSelectedBeatmap();
                    return;
                } else if (ctx.touch.px >= 202 && ctx.touch.px <= 316) {
                    ctx.audio.playHitSound(0);
                    ctx.applyActiveSkinAudio();
                    ctx.appState = STATE_SONG_SELECT;
                    ctx.updateSongPreview();
                    return;
                }
            }
        }
    }

    bool isTaikoPlay = (ctx.settings.gameMode == MODE_TAIKO || ctx.beatmap.mode == 1);
    bool isCatchPlay = (ctx.settings.gameMode == MODE_CATCH || ctx.beatmap.mode == 2);
    bool isManiaPlay = (ctx.settings.gameMode == MODE_MANIA || ctx.beatmap.mode == 3);
    int currentPlayMode = isTaikoPlay ? 1 : (isCatchPlay ? 2 : (isManiaPlay ? 3 : 0));

    int totalJudged = ctx.count300 + ctx.count100 + ctx.count50 + ctx.countMiss;
    float acc = (totalJudged > 0)
        ? ((ctx.count300 * 300.0f + ctx.count100 * 100.0f + ctx.count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
        : 100.0f;

    u32 lazerPink = Ui::rgba(255, 102, 171); // #FF66AB
    u32 lazerCyan = Ui::rgba(56, 210, 255);  // #38D2FF
    u32 lazerGold = Ui::rgba(255, 205, 55);  // #FFCD37
    u32 lazerRed  = Ui::rgba(255, 77, 109);  // #FF4D6D

    const char* grade = "D";
    u32 gradeClr = lazerRed;
    if (ctx.stageFailed) {
        grade = "F";
        gradeClr = lazerRed;
    } else if (acc >= 99.9f && ctx.countMiss == 0) {
        grade = "SS";
        gradeClr = lazerPink;
    } else if (acc >= 93.0f && ctx.countMiss == 0) {
        grade = "S";
        gradeClr = lazerGold;
    } else if (acc >= 85.0f) {
        grade = "A";
        gradeClr = Ui::rgba(46, 213, 115);
    } else if (acc >= 75.0f) {
        grade = "B";
        gradeClr = lazerCyan;
    } else if (acc >= 65.0f) {
        grade = "C";
        gradeClr = Ui::rgba(255, 165, 50);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    char buf[96];
    // =========================================================================
    // PANTALLA SUPERIOR (400x240, 3D Estereoscópico, Fondo del Beatmap Visible)
    // =========================================================================
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        C2D_TargetClear(curTop, Ui::rgba(12, 8, 24));
        C2D_SceneBegin(curTop);

        float dBg    = depthShift(-3.5f);
        float dGrade = depthShift(3.5f);
        float dPanel = depthShift(2.0f);

        // 1. Fondo del Beatmap visible con velo translúcido
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick),
                     ctx.stageFailed ? Ui::rgba(36, 12, 24) : Ui::rgba(24, 14, 42),
                     Ui::rgba(10, 7, 20), gradeClr, 0.01f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            u32 veilClr = ctx.stageFailed ? Ui::rgba(24, 8, 16, 140) : Ui::rgba(12, 8, 22, 115);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, veilClr);
        }

        // 2. Cabecera osu!lazer translúcida
        const char* headerTitle = ctx.stageFailed
            ? LOC(STR_RESULTS_FAILED)
            : (ctx.newRecordAchieved ? LOC(STR_RESULTS_NEW_RECORD_TITLE) : LOC(STR_RESULTS_MAP_COMPLETED));
        const char* headerBadge = ctx.stageFailed ? "GAME OVER" : LOC(STR_RESULTS_TITLE);
        Ui::lazerHeader(400.0f, 0.30f, headerTitle, headerBadge, ctx.stageFailed ? lazerRed : lazerPink, ctx.textBuf, true);

        // 3. Medallón Izquierdo de Grado / Game Over (cx = 74, cy = 118)
        float gPulse = 1.0f + std::sin(ctx.animTick * 0.08f) * 0.03f;
        float gIntro = std::min(1.0f, static_cast<float>(ctx.animTick) / 20.0f);
        float gScale = (gIntro < 1.0f) ? (1.0f + 0.30f * std::sin(gIntro * static_cast<float>(M_PI))) : 1.0f;
        float medX = 74.0f + dGrade;
        float medY = 114.0f;

        if (!isTaikoPlay || ctx.stageFailed) {
            if (!ctx.stageFailed && (std::string(grade) == "SS" || std::string(grade) == "S")) {
                for (int ray = 0; ray < 8; ++ray) {
                    float rAng = ray * (static_cast<float>(M_PI) / 4.0f) + ctx.animTick * 0.025f;
                    float rLen = 56.0f * gPulse;
                    C2D_DrawLine(medX, medY, Ui::rgba(255, 220, 60, 65),
                                 medX + std::cos(rAng) * rLen, medY + std::sin(rAng) * rLen,
                                 Ui::rgba(255, 220, 60, 0), 2.2f, 0.16f);
                }
            }

            Ui::glow(medX, medY, 0.15f, 68.0f * gPulse * gScale, Ui::withAlpha(gradeClr, 115), 5);
            C2D_DrawCircleSolid(medX, medY, 0.20f, 46.0f * gScale, Ui::rgba(16, 11, 28, 215));
            Ui::ring(medX, medY, 0.25f, 46.0f * gPulse * gScale, 3.5f, gradeClr, 36);
            Ui::ring(medX, medY, 0.26f, 40.0f * gScale, 1.2f, Ui::withAlpha(ctx.clrWhite, 90), 32);
            drawCenteredTextWithShadow(ctx.textBuf, medX, medY - 3.0f, 1.40f * gScale, gradeClr, grade, 0.28f);

            // Insignia inclinada debajo del medallón
            const char* subBadgeTxt = ctx.stageFailed
                ? "SIN ENERGIA"
                : ((ctx.countMiss == 0) ? "FULL COMBO!" : "COMPLETADO");
            u32 subBadgeClr = ctx.stageFailed
                ? lazerRed
                : ((ctx.countMiss == 0) ? lazerGold : lazerPink);
            u32 subTxtClr = (!ctx.stageFailed && ctx.countMiss == 0) ? Ui::rgba(20, 14, 28) : ctx.clrWhite;

            Ui::slantRect(medX - 48.0f, 172.0f, 96.0f, 18.0f, -4.0f, 0.28f, subBadgeClr);
            drawCenteredText(ctx.textBuf, medX, 181.0f, 0.34f, subTxtClr, subBadgeTxt, 0.30f);
        } else {
            // Corona oficial estilo Taiko no Tatsujin
            bool isFC = (ctx.countMiss == 0 && !ctx.stageFailed);
            bool isClear = (ctx.hp >= 0.70f && !ctx.stageFailed);
            u32 crownClr = isFC ? lazerGold : (isClear ? lazerCyan : lazerRed);
            Ui::glow(medX, 108.0f, 0.15f, 68.0f * gPulse, Ui::withAlpha(crownClr, 140), 5);

            float crownY = 104.0f;
            C2D_DrawRectSolid(medX - 30.0f, crownY + 8.0f, 0.20f, 60.0f, 12.0f, crownClr);
            for (int p = 0; p < 5; ++p) {
                float px = medX - 26.0f + p * 13.0f;
                float peakH = (p == 2) ? 22.0f : ((p == 1 || p == 3) ? 16.0f : 12.0f);
                C2D_DrawTriangle(px - 5.5f, crownY + 8.0f, crownClr, px + 5.5f, crownY + 8.0f, crownClr, px, crownY + 8.0f - peakH, crownClr, 0.21f);
                C2D_DrawCircleSolid(px, crownY + 8.0f - peakH, 0.22f, 2.5f, lazerPink);
            }
            Ui::slantRect(medX - 44.0f, 172.0f, 88.0f, 18.0f, -4.0f, 0.28f, crownClr);
            drawCenteredText(ctx.textBuf, medX, 181.0f, 0.33f, Ui::rgba(20, 14, 28), isFC ? "FULL COMBO!" : "CLEAR!", 0.30f);
        }

        // 4. Panel Derecho Translúcido estilo osu!lazer (x = 144, y = 34, w = 246, h = 172)
        float pX = 144.0f + dPanel;
        float pY = 34.0f;
        float pW = 246.0f;
        float pH = 172.0f;
        Ui::panel(pX, pY, pW, pH, 0.25f, Ui::rgba(16, 11, 28, 150), gradeClr);

        float rollProgress = std::min(1.0f, static_cast<float>(ctx.animTick) / 28.0f);
        int displayScore = static_cast<int>(ctx.score * rollProgress);

        // Bloque superior: Puntuación + Insignias inclinadas de PP y Estrellas
        Ui::slantRect(pX + 8.0f, pY + 7.0f, pW - 16.0f, 46.0f, -5.0f, 0.27f, Ui::rgba(22, 16, 38, 185));
        Ui::slantRect(pX + 8.0f, pY + 7.0f, 4.0f, 46.0f, -5.0f, 0.28f, gradeClr);
        drawText(ctx.textBuf, pX + 16.0f, pY + 10.0f, 0.32f, Ui::rgba(185, 195, 220), LOC(STR_RESULTS_FINAL_SCORE), 0.30f);

        std::snprintf(buf, sizeof(buf), "%07d", displayScore);
        drawTextWithShadow(ctx.textBuf, pX + 16.0f, pY + 22.0f, 0.66f, ctx.clrWhite, buf, 0.31f);

        float modStars = getModAdjustedStars(ctx.beatmap.starRating, ctx.settings.activeMods);
        u32 starClr = getStarRatingColor(modStars);
        std::snprintf(buf, sizeof(buf), "%.0f PP", ctx.stageFailed ? 0.0f : ctx.lastEarnedPP);
        Ui::slantRect(pX + pW - 76.0f, pY + 11.0f, 62.0f, 18.0f, -4.0f, 0.29f, lazerPink);
        drawCenteredText(ctx.textBuf, pX + pW - 45.0f, pY + 20.0f, 0.35f, ctx.clrWhite, buf, 0.32f);

        std::snprintf(buf, sizeof(buf), "* %.2f", modStars);
        Ui::slantRect(pX + pW - 76.0f, pY + 32.0f, 62.0f, 17.0f, -4.0f, 0.29f, starClr);
        drawCenteredText(ctx.textBuf, pX + pW - 45.0f, pY + 40.5f, 0.34f, Ui::rgba(18, 12, 28), buf, 0.32f);

        // Fila media: Precisión + Combo Máximo en cajas inclinadas
        float midY = pY + 58.0f;
        Ui::slantRect(pX + 8.0f, midY, 112.0f, 34.0f, -4.0f, 0.27f, Ui::rgba(22, 16, 38, 185));
        Ui::slantRect(pX + 8.0f, midY, 3.5f, 34.0f, -4.0f, 0.28f, lazerCyan);
        drawText(ctx.textBuf, pX + 16.0f, midY + 3.0f, 0.29f, Ui::rgba(180, 190, 215), LOC(STR_ACCURACY), 0.30f);
        std::snprintf(buf, sizeof(buf), "%.2f%%", acc);
        drawTextWithShadow(ctx.textBuf, pX + 16.0f, midY + 15.0f, 0.42f, lazerCyan, buf, 0.31f);

        Ui::slantRect(pX + 126.0f, midY, 112.0f, 34.0f, -4.0f, 0.27f, Ui::rgba(22, 16, 38, 185));
        Ui::slantRect(pX + 126.0f, midY, 3.5f, 34.0f, -4.0f, 0.28f, Ui::rgba(46, 213, 115));
        drawText(ctx.textBuf, pX + 134.0f, midY + 3.0f, 0.29f, Ui::rgba(180, 190, 215), LOC(STR_MAX_COMBO), 0.30f);
        std::snprintf(buf, sizeof(buf), "%dx / %dx", ctx.maxCombo, std::max(ctx.maxCombo, ctx.beatmap.maxPossibleCombo));
        drawTextWithShadow(ctx.textBuf, pX + 134.0f, midY + 15.0f, 0.38f, Ui::rgba(46, 213, 115), buf, 0.31f);

        // Cuadrícula de 4 contadores inclinados (300 / 100 / 50 / MISS)
        const char* hitLbl[4] = { "300", "100", "50", "MISS" };
        int hitCnt[4] = { ctx.count300, ctx.count100, ctx.count50, ctx.countMiss };
        u32 hitClr[4] = { lazerCyan, Ui::rgba(46, 213, 115), lazerGold, lazerRed };

        if (isTaikoPlay) {
            hitCnt[0] = ctx.count300;
            hitCnt[1] = ctx.count100;
            hitCnt[2] = ctx.countMiss;
            hitCnt[3] = ctx.taikoEngine.totalDrumrollHits;
            hitLbl[0] = "GOOD"; hitLbl[1] = "OK"; hitLbl[2] = "MISS"; hitLbl[3] = "ROLL";
        }

        float hitY = pY + 98.0f;
        for (int i = 0; i < 4; ++i) {
            float hx = pX + 8.0f + i * 58.5f;
            float hw = 54.5f;
            float hh = 44.0f;
            Ui::slantRect(hx, hitY, hw, hh, -4.0f, 0.27f, Ui::rgba(22, 16, 38, 190));
            Ui::slantRect(hx, hitY, hw, 3.5f, -4.0f, 0.29f, hitClr[i]);
            drawCenteredText(ctx.textBuf, hx + hw * 0.5f, hitY + 13.0f, 0.35f, hitClr[i], hitLbl[i], 0.31f);
            int rollCnt = static_cast<int>(hitCnt[i] * rollProgress);
            std::snprintf(buf, sizeof(buf), "%d", rollCnt);
            drawCenteredTextWithShadow(ctx.textBuf, hx + hw * 0.5f, hitY + 30.0f, 0.46f, ctx.clrWhite, buf, 0.31f);
        }

        // Línea inferior del panel: Mods activos y Unstable Rate (UR)
        std::snprintf(buf, sizeof(buf), "Mods: %s   •   UR: %.1f (%+.0f ms)",
                      formatModsString(ctx.settings.activeMods).c_str(), ctx.lastUnstableRate, ctx.lastMeanHitError);
        drawCenteredText(ctx.textBuf, pX + pW * 0.5f, pY + 156.0f, 0.33f, Ui::rgba(210, 215, 235), buf, 0.31f);

        // 5. Pie de pantalla superior con atajos claros
        float footY = 216.0f;
        C2D_DrawRectSolid(0.0f, footY, 0.25f, 400.0f, 24.0f, Ui::rgba(12, 8, 20, 185));
        C2D_DrawRectSolid(0.0f, footY, 0.26f, 400.0f, 1.0f, Ui::withAlpha(gradeClr, 140));

        drawKeyBadge(ctx.textBuf, 18.0f,  footY + 4.0f, "A", LOC(STR_RETRY), Ui::rgba(66, 144, 251));
        if (!ctx.stageFailed) {
            drawKeyBadge(ctx.textBuf, 142.0f, footY + 4.0f, "X", resultsBottomTab == 0 ? LOC(STR_RESULTS_VIEW_RECORDS) : LOC(STR_RESULTS_VIEW_GRAPH), lazerGold);
        }
        drawKeyBadge(ctx.textBuf, 272.0f, footY + 4.0f, "B", LOC(STR_RESULTS_NEXT_SONGS), lazerPink);

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, gradeClr);
    }

    // =========================================================================
    // PANTALLA INFERIOR (320x240): GAME OVER Ó RESULTADOS ESTILO OSU!LAZER
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, Ui::rgba(10, 7, 20));
    C2D_SceneBegin(ctx.bottomScreen);
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                 ctx.stageFailed ? Ui::rgba(28, 10, 22) : Ui::rgba(22, 14, 38),
                 Ui::rgba(10, 7, 18), gradeClr, 0.01f);
    if (ctx.bgTex.loaded) {
        ctx.bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.02f, 65);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 320.0f, 240.0f, Ui::rgba(12, 8, 22, 175));
    }

    if (ctx.stageFailed) {
        // --- MODO GAME OVER EN PANTALLA INFERIOR (Medallón Izquierdo + 2 Botones Inclinados) ---
        Ui::panel(8.0f, 8.0f, 304.0f, 36.0f, 0.18f, Ui::rgba(20, 12, 30, 195), lazerRed);
        drawTextWithShadow(ctx.textBuf, 16.0f, 11.0f, 0.42f, ctx.clrWhite,
                           truncateString(ctx.beatmap.title, 26).c_str(), 0.22f);
        std::snprintf(buf, sizeof(buf), LOC(STR_RESULTS_COMBO_REACHED_FMT),
                      truncateString(ctx.beatmap.version, 15).c_str(), ctx.maxCombo);
        drawText(ctx.textBuf, 16.0f, 27.0f, 0.32f, lazerCyan, buf, 0.22f);

        // Medallón ancla izquierdo (tocar para reintentar)
        float mCx = 46.0f;
        float mCy = 120.0f;
        float mR  = 34.0f;
        float pulse = 0.5f + 0.5f * std::sin(ctx.animTick * 0.12f);
        Ui::ring(mCx, mCy, 0.18f, mR + 5.0f + pulse * 3.0f, 2.0f, Ui::withAlpha(lazerRed, 95), 28);
        Ui::glow(mCx, mCy, 0.20f, mR + 6.0f, Ui::withAlpha(lazerRed, 115), 3);
        C2D_DrawCircleSolid(mCx, mCy, 0.25f, mR, 0xFFFFFFFF);
        C2D_DrawCircleSolid(mCx, mCy, 0.28f, mR - 2.5f, lazerRed);
        C2D_DrawCircleSolid(mCx, mCy, 0.30f, mR - 5.5f, Ui::rgba(110, 18, 42));

        // Icono Play / Retry (►) en el centro del medallón
        C2D_DrawTriangle(mCx - 6.0f, mCy - 11.0f, 0xFFFFFFFF,
                         mCx - 6.0f, mCy + 11.0f, 0xFFFFFFFF,
                         mCx + 11.0f, mCy,        0xFFFFFFFF, 0.35f);

        // 2 Botones Inclinados grandes estilo osu!lazer para reintentar o avanzar a la siguiente pantalla
        Ui::slantButton(96.0f, 58.0f, 214.0f, 50.0f, -6.0f, Ui::rgba(66, 144, 251),
                        LOC(STR_RETRY), LOC(STR_RESULTS_FAIL_RETRY_SUB), ctx.textBuf, true, 0.25f);

        Ui::slantButton(96.0f, 124.0f, 214.0f, 50.0f, -6.0f, lazerPink,
                        LOC(STR_RESULTS_CONTINUE), LOC(STR_RESULTS_FAIL_QUIT_SUB), ctx.textBuf, false, 0.25f);

        drawCenteredText(ctx.textBuf, 160.0f, 206.0f, 0.34f, Ui::rgba(210, 205, 230),
                         LOC(STR_RESULTS_FAIL_DONT_GIVE_UP), 0.25f);
    } else {
        // --- MODO FIN DE CANCIÓN / RESULTADOS EN PANTALLA INFERIOR ---
        // 1. Cabecera Translúcida con Título + Pestaña de Vista (Gráfica vs Récords)
        Ui::panel(8.0f, 6.0f, 304.0f, 34.0f, 0.15f, Ui::rgba(18, 13, 32, 185), gradeClr);
        drawTextWithShadow(ctx.textBuf, 15.0f, 8.0f, 0.40f, ctx.clrWhite,
                           truncateString(ctx.beatmap.title, 20).c_str(), 0.22f);
        std::snprintf(buf, sizeof(buf), "[%s]", truncateString(ctx.beatmap.version, 18).c_str());
        drawText(ctx.textBuf, 15.0f, 23.5f, 0.32f, lazerCyan, buf, 0.22f);

        // Interruptor inclinado GRAFICA | RECORDS en la esquina superior derecha
        Ui::slantRect(188.0f, 10.0f, 58.0f, 24.0f, -4.0f, 0.20f,
                      (resultsBottomTab == 0) ? lazerPink : Ui::rgba(28, 22, 46, 200));
        drawCenteredText(ctx.textBuf, 217.0f, 22.0f, 0.31f,
                         (resultsBottomTab == 0) ? ctx.clrWhite : Ui::rgba(180, 185, 210), LOC(STR_RESULTS_GRAPH_TAB), 0.24f);

        Ui::slantRect(248.0f, 10.0f, 58.0f, 24.0f, -4.0f, 0.20f,
                      (resultsBottomTab == 1) ? lazerGold : Ui::rgba(28, 22, 46, 200));
        drawCenteredText(ctx.textBuf, 277.0f, 22.0f, 0.31f,
                         (resultsBottomTab == 1) ? Ui::rgba(18, 12, 28) : Ui::rgba(180, 185, 210), LOC(STR_RESULTS_SCORES_TAB), 0.24f);

        // 2. Panel Central Translúcido (y = 44..186)
        Ui::panel(8.0f, 44.0f, 304.0f, 142.0f, 0.18f, Ui::rgba(16, 11, 28, 175),
                  (resultsBottomTab == 1) ? lazerGold : lazerPink);

        if (resultsBottomTab == 0) {
            // VISTA 0: Curva de Esfuerzo (Strain Graph) + Banner de Récord / Resumen
            drawText(ctx.textBuf, 16.0f, 49.0f, 0.32f, lazerCyan, LOC(STR_RESULTS_PERFORMANCE_TITLE), 0.22f);
            drawText(ctx.textBuf, 188.0f, 50.0f, 0.29f, Ui::rgba(46, 213, 115), "o 100", 0.22f);
            drawText(ctx.textBuf, 228.0f, 50.0f, 0.29f, lazerGold, "o 50", 0.22f);
            drawText(ctx.textBuf, 264.0f, 50.0f, 0.29f, lazerRed, "x Miss", 0.22f);

            float graphX = 14.0f, graphY = 66.0f, graphW = 292.0f, graphH = 74.0f;
            int firstT = ctx.beatmap.firstObjectTime;
            int lastT  = ctx.beatmap.lastObjectTime;
            if (lastT <= firstT) lastT = firstT + 1000;
            float totalSpan = static_cast<float>(lastT - firstT);

            Ui::strainGraph(graphX, graphY, graphW, graphH, 0.20f, ctx.beatmap.strainGraph, gradeClr, -1.0f, 0.85f);

            for (const auto& ev : ctx.judgementHistory) {
                if (ev.state == HIT_300) continue;
                float rx = graphX + (std::max(0.0f, std::min(totalSpan, static_cast<float>(ev.time - firstT))) / totalSpan) * (graphW - 2.0f);
                if (ev.state == HIT_MISS) {
                    float my = graphY + graphH - 5.0f;
                    C2D_DrawLine(rx, graphY + 2.0f, Ui::rgba(255, 77, 109, 65), rx, my, Ui::rgba(255, 77, 109, 130), 1.0f, 0.23f);
                    C2D_DrawLine(rx - 2.5f, my - 2.5f, lazerRed, rx + 2.5f, my + 2.5f, lazerRed, 1.8f, 0.26f);
                    C2D_DrawLine(rx - 2.5f, my + 2.5f, lazerRed, rx + 2.5f, my - 2.5f, lazerRed, 1.8f, 0.26f);
                } else if (ev.state == HIT_50) {
                    C2D_DrawCircleSolid(rx, graphY + graphH - 7.0f, 0.25f, 2.0f, lazerGold);
                } else if (ev.state == HIT_100) {
                    C2D_DrawCircleSolid(rx, graphY + graphH - 11.0f, 0.24f, 1.6f, Ui::rgba(46, 213, 115));
                }
            }

            // Botones de acción de Replay estilo osu!lazer
            Ui::slantButton(14.0f, 148.0f, 140.0f, 32.0f, -4.0f, lazerCyan,
                            LOC(STR_SONG_WATCH_REPLAY), LOC(STR_RESULTS_BTN_Y), ctx.textBuf, false, 0.22f);

            Ui::slantButton(162.0f, 148.0f, 144.0f, 32.0f, -4.0f, lazerGold,
                            LOC(STR_RESULTS_SAVE_REPLAY), "SELECT", ctx.textBuf, false, 0.22f);
        } else {
            // VISTA 1: Top 5 Récords Locales estilizados con cajas inclinadas osu!lazer
            auto topScores = ctx.scoreMgr.getTopRecords(ctx.beatmap.filePath, 5, currentPlayMode);
            if (topScores.empty()) {
                drawCenteredText(ctx.textBuf, 160.0f, 115.0f, 0.38f, ctx.clrMuted, LOC(STR_RESULTS_NO_SCORES_SD), 0.25f);
            } else {
                for (size_t s = 0; s < topScores.size() && s < 5; ++s) {
                    const auto& r = topScores[s];
                    float ry = 50.0f + s * 26.0f;
                    bool isCur = (r.bestScore == ctx.score && r.bestCombo == ctx.maxCombo && std::abs(r.bestAcc - acc) < 0.05f);
                    u32 rkClr = (s == 0) ? lazerGold : ((s == 1) ? lazerCyan : ((s == 2) ? Ui::rgba(235, 145, 75) : lazerPink));
                    u32 rowBg = isCur ? Ui::rgba(58, 34, 82, 225) : Ui::rgba(22, 16, 38, 190);

                    Ui::slantRect(14.0f, ry, 292.0f, 23.0f, -4.0f, 0.21f, rowBg);
                    Ui::slantRect(14.0f, ry, 3.5f, 23.0f, -4.0f, 0.23f, isCur ? lazerGold : rkClr);

                    char rkStr[8];
                    std::snprintf(rkStr, sizeof(rkStr), "#%u", static_cast<unsigned int>(s + 1));
                    drawTextWithShadow(ctx.textBuf, 22.0f, ry + 4.0f, 0.34f, rkClr, rkStr, 0.25f);

                    u32 gClr = getScoreGradeColor(r.bestGrade, ctx.clrCursor);
                    drawCenteredTextWithShadow(ctx.textBuf, 52.0f, ry + 11.5f, 0.38f, gClr, r.bestGrade.c_str(), 0.25f);

                    drawTextWithShadow(ctx.textBuf, 72.0f, ry + 3.5f, 0.36f, ctx.clrWhite,
                                       formatScoreThousands(r.bestScore).c_str(), 0.25f);

                    std::snprintf(buf, sizeof(buf), "%.1f%%  •  %dx", r.bestAcc, r.bestCombo);
                    drawText(ctx.textBuf, 164.0f, ry + 4.5f, 0.32f, Ui::rgba(46, 213, 115), buf, 0.25f);

                    bool hasRep = (s < 3) && ReplayManager::hasTopReplay(ctx.beatmap.filePath, currentPlayMode, static_cast<int>(s + 1));
                    if (hasRep) {
                        Ui::slantRect(242.0f, ry + 2.5f, 60.0f, 18.0f, -3.0f, 0.24f, lazerCyan);
                        drawCenteredText(ctx.textBuf, 272.0f, ry + 11.0f, 0.28f, Ui::rgba(14, 10, 24), "▶ REPLAY", 0.26f);
                    } else {
                        std::snprintf(buf, sizeof(buf), "%.0fpp", r.bestPP);
                        drawTextWithShadow(ctx.textBuf, 262.0f, ry + 4.5f, 0.32f, lazerPink, buf, 0.25f);
                    }
                }
            }
        }

        // 3. Barra Inferior de 3 Botones Inclinados estilo osu!lazer (y = 192, h = 40)
        Ui::slantButton(6.0f, 192.0f, 90.0f, 40.0f, -5.0f, Ui::rgba(147, 85, 232),
                        (resultsBottomTab == 0) ? LOC(STR_RESULTS_SCORES_TAB) : LOC(STR_RESULTS_GRAPH_TAB),
                        LOC(STR_RESULTS_BTN_X), ctx.textBuf, false, 0.22f);

        Ui::slantButton(100.0f, 192.0f, 98.0f, 40.0f, -5.0f, Ui::rgba(66, 144, 251),
                        LOC(STR_RETRY), LOC(STR_SONG_BTN_A_LABEL), ctx.textBuf, false, 0.22f);

        Ui::slantButton(202.0f, 192.0f, 112.0f, 40.0f, -5.0f, lazerPink,
                        LOC(STR_RESULTS_NEXT_BTN), LOC(STR_RESULTS_SONGS_B), ctx.textBuf, true, 0.22f);
    }

    // Notificación emergente estilo toast para guardado de repetición
    if (ctx.replayToastTimer > 0) {
        ctx.replayToastTimer--;
        Ui::pill(28.0f, 96.0f, 264.0f, 30.0f, 0.88f, Ui::rgba(14, 10, 24, 248));
        drawRectOutline(28.0f, 96.0f, 264.0f, 30.0f, 1.2f, lazerCyan, 0.90f);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 111.0f, 0.34f, Ui::rgba(46, 213, 115), ctx.replayToastMessage.c_str(), 0.92f);
    }

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, gradeClr);
    C3D_FrameEnd(0);
}
