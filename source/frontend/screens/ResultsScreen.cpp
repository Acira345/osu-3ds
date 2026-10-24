#include "ResultsScreen.hpp"

void runResultsScreen(AppContext& ctx) {
    if (ctx.multiMgr.inMultiplayer) {
        ctx.appState = STATE_MULTI_RESULTS;
        return;
    }
    if (ctx.kDown & KEY_A) {
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
    if (ctx.kDown & (KEY_X | KEY_Y)) {
        resultsBottomTab = 1 - resultsBottomTab;
        ctx.audio.playHitSound(0);
    }
    if (ctx.touchJustDown) {
        if (ctx.touch.py >= 46 && ctx.touch.py <= 68) {
            if (ctx.touch.px < 160) resultsBottomTab = 0;
            else resultsBottomTab = 1;
            ctx.audio.playHitSound(0);
        } else if (ctx.touch.py >= 200 && ctx.touch.py <= 236) {
            if (ctx.touch.px >= 8 && ctx.touch.px <= 106) {
                resultsBottomTab = 1 - resultsBottomTab;
                ctx.audio.playHitSound(0);
            } else if (ctx.touch.px >= 111 && ctx.touch.px <= 211) {
                ctx.audio.playHitSound(2);
                ctx.startSelectedBeatmap();
                return;
            } else if (ctx.touch.px >= 216 && ctx.touch.px <= 314) {
                ctx.audio.playHitSound(0);
                ctx.applyActiveSkinAudio();
                ctx.appState = STATE_SONG_SELECT;
                ctx.updateSongPreview();
                return;
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

    const char* grade = "D";
    u32 gradeClr = ctx.clrMiss;
    if (ctx.stageFailed) {
        grade = "F";
        gradeClr = ctx.clrMiss;
    } else if (acc >= 99.9f && ctx.countMiss == 0) {
        grade = "SS";
        gradeClr = ctx.clrCursor;
    } else if (acc >= 93.0f && ctx.countMiss == 0) {
        grade = "S";
        gradeClr = ctx.clrCursor;
    } else if (acc >= 85.0f) {
        grade = "A";
        gradeClr = ctx.clr100;
    } else if (acc >= 75.0f) {
        grade = "B";
        gradeClr = ctx.clr300;
    } else if (acc >= 65.0f) {
        grade = "C";
        gradeClr = ctx.clr50;
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    char buf[96];
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);

        float dBg    = depthShift(-3.5f);
        float dGrade = depthShift(4.2f);
        float dPanel = depthShift(2.0f);

        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, 190));
        } else {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), gradeClr, 0.02f);
        }

        Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 150));
        C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::withAlpha(gradeClr, 220));
        const char* resTitle = ctx.stageFailed ? LOC(STR_RESULTS_FAILED) : (ctx.newRecordAchieved ? LOC(STR_RESULTS_NEW_RECORD) : LOC(STR_RESULTS_TITLE));
        std::string taikoFailTitle = std::string(LOC(STR_RESULTS_FAILED)) + " (ざんねん...)";
        if (isTaikoPlay) {
            if (ctx.stageFailed || ctx.hp < 0.70f) resTitle = taikoFailTitle.c_str();
            else if (ctx.countMiss == 0) resTitle = "DON-DERFUL! FULL COMBO! (フルコンボ!)";
            else resTitle = "CLEAR! (クリア!)";
        }
        drawText(ctx.textBuf, 12.0f, 6.0f, 0.55f, ctx.clrWhite, resTitle);

        // Rango gigante animado o Corona oficial estilo Taiko
        float gPulse = 1.0f + std::sin(ctx.animTick * 0.08f) * 0.03f;
        float gIntro = std::min(1.0f, static_cast<float>(ctx.animTick) / 20.0f);
        float gScale = (gIntro < 1.0f) ? (1.0f + 0.35f * std::sin(gIntro * static_cast<float>(M_PI))) : 1.0f;

        if (!isTaikoPlay) {
            // Rayos de sol rotativos dorados para SS y S
            if (std::string(grade) == "SS" || std::string(grade) == "S") {
                for (int ray = 0; ray < 8; ++ray) {
                    float rAng = ray * (static_cast<float>(M_PI) / 4.0f) + ctx.animTick * 0.025f;
                    float rLen = 58.0f * gPulse;
                    C2D_DrawLine(88.0f + dGrade, 124.0f, Ui::rgba(255, 220, 60, 65),
                                 88.0f + dGrade + std::cos(rAng) * rLen, 124.0f + std::sin(rAng) * rLen,
                                 Ui::rgba(255, 220, 60, 0), 2.2f, 0.16f);
                }
            }

            Ui::glow(88.0f + dGrade, 124.0f, 0.15f, 78.0f * gPulse * gScale, Ui::withAlpha(gradeClr, 120), 6);
            C2D_DrawCircleSolid(90.0f + dGrade, 128.0f, 0.18f, 52.0f * gScale, Ui::rgba(0, 0, 0, 110));
            C2D_DrawCircleSolid(88.0f + dGrade, 124.0f, 0.2f, 52.0f * gScale, Ui::rgba(24, 20, 40, 245));
            Ui::ring(88.0f + dGrade, 124.0f, 0.25f, 52.0f * gPulse * gScale, 4.0f, gradeClr, 40);
            drawCenteredText(ctx.textBuf, 88.0f + dGrade, 120.0f, 1.55f * gScale, gradeClr, grade);

            // Badge de FULL COMBO debajo de la medalla de grado
            if (ctx.countMiss == 0 && !ctx.stageFailed) {
                Ui::pill(48.0f + dGrade, 182.0f, 80.0f, 15.0f, 0.28f, Ui::rgba(255, 215, 60, 230));
                drawCenteredText(ctx.textBuf, 88.0f + dGrade, 189.5f, 0.30f, Ui::rgba(20, 16, 28), "FULL COMBO", 0.30f);
            }
        } else {
            // Corona o Medalla oficial estilo Taiko no Tatsujin
            bool isFC = (ctx.countMiss == 0 && !ctx.stageFailed);
            bool isClear = (ctx.hp >= 0.70f && !ctx.stageFailed);
            u32 crownClr = isFC ? Ui::rgba(255, 218, 55) : (isClear ? Ui::rgba(215, 235, 255) : Ui::rgba(110, 30, 75));
            Ui::glow(88.0f + dGrade, 112.0f, 0.15f, 76.0f * gPulse, Ui::withAlpha(crownClr, 150), 6);

            // Base de la Corona
            float crownX = 88.0f + dGrade;
            float crownY = 108.0f;
            C2D_DrawRectSolid(crownX - 32.0f, crownY + 8.0f, 0.20f, 64.0f, 12.0f, crownClr);
            C2D_DrawRectSolid(crownX - 30.0f, crownY + 16.0f, 0.21f, 60.0f, 4.0f, Ui::rgba(255, 255, 255, 200));

            // 5 Picos dorados de la corona
            for (int p = 0; p < 5; ++p) {
                float px = crownX - 28.0f + p * 14.0f;
                float peakH = (p == 2) ? 24.0f : ((p == 1 || p == 3) ? 17.0f : 13.0f);
                C2D_DrawTriangle(px - 6.0f, crownY + 8.0f, crownClr, px + 6.0f, crownY + 8.0f, crownClr, px, crownY + 8.0f - peakH, crownClr, 0.21f);
                // Joya en la punta
                u32 gemClr = isFC ? Ui::rgba(255, 45, 60) : (isClear ? Ui::rgba(45, 175, 255) : Ui::rgba(140, 45, 120));
                C2D_DrawCircleSolid(px, crownY + 8.0f - peakH, 0.22f, 2.8f, gemClr);
            }

            // Cinta con título
            const char* crownTxt = isFC ? "フルコンボ!" : (isClear ? "クリア!" : "ざんねん...");
            Ui::pill(crownX - 40.0f, crownY + 24.0f, 80.0f, 17.0f, 0.24f, crownClr);
            drawCenteredText(ctx.textBuf, crownX, crownY + 32.0f, 0.32f, Ui::rgba(20, 14, 26), crownTxt, 0.26f);

            // Mini Don-chan de celebración animado bajo la corona
            int donResEmotion = isFC ? 1 : (isClear ? 0 : 2);
            float dMascotY = 175.0f;
            C2D_DrawCircleSolid(crownX, dMascotY, 0.22f, 16.0f, Ui::rgba(255, 55, 65));
            ModeTaiko::drawTaikoFace(crownX, dMascotY, 13.0f, false, true, 0.24f, donResEmotion);
        }

        // Panel de resultados
        Ui::panel(158.0f + dPanel, 38.0f, 234.0f, 62.0f, 0.3f, Ui::rgba(24, 21, 40, 230), gradeClr);
        drawText(ctx.textBuf, 168.0f + dPanel, 42.0f, 0.36f, ctx.clrMuted, LOC(STR_SCORE));

        float rollProgress = std::min(1.0f, static_cast<float>(ctx.animTick) / 30.0f);
        int displayScore = static_cast<int>(ctx.score * rollProgress);
        std::snprintf(buf, sizeof(buf), "%07d", displayScore);
        drawText(ctx.textBuf, 168.0f + dPanel, 54.0f, 0.78f, ctx.clrWhite, buf);
        float modStars = getModAdjustedStars(ctx.beatmap.starRating, ctx.settings.activeMods);
        std::snprintf(buf, sizeof(buf), "%.1f pp", ctx.lastEarnedPP);
        Ui::pill(318.0f + dPanel, 46.0f, 66.0f, 18.0f, 0.5f, Ui::rgba(70, 55, 110, 240));
        drawCenteredText(ctx.textBuf, 351.0f + dPanel, 55.0f, 0.42f, ctx.clr300, buf, 0.9f);
        std::snprintf(buf, sizeof(buf), "*%.2f", modStars);
        Ui::pill(318.0f + dPanel, 70.0f, 66.0f, 16.0f, 0.5f, getStarRatingColor(modStars));
        drawCenteredText(ctx.textBuf, 351.0f + dPanel, 78.0f, 0.36f, Ui::rgba(20, 16, 30), buf, 0.9f);

        // Precision con barra
        Ui::panel(158.0f + dPanel, 106.0f, 234.0f, 34.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
        std::snprintf(buf, sizeof(buf), "%s %.2f%%", LOC(STR_ACCURACY), acc);
        drawText(ctx.textBuf, 168.0f + dPanel, 109.0f, 0.40f, ctx.clr300, buf);
        std::snprintf(buf, sizeof(buf), "%s %dx", LOC(STR_MAX_COMBO), ctx.maxCombo);
        drawText(ctx.textBuf, 320.0f + dPanel, 109.0f, 0.40f, ctx.clrCursor, buf);
        Ui::statBar(168.0f + dPanel, 128.0f, 214.0f, 6.0f, 0.4f, (acc / 100.0f) * rollProgress, ctx.clr300);

        // Contadores de golpes adaptados a cada modo de juego
        const char* hitLbl[4] = { "300", "100", "50", "X" };
        const char* taikoHitLbl[4] = { "良 GOOD", "可 OK", "不可 MISS", "連打 ROLLS" };
        const char* catchHitLbl[4] = { LOC(STR_CATCH_FRUITS), LOC(STR_CATCH_DROPS), LOC(STR_CATCH_DROPLETS), "MISS" };
        const char* maniaHitLbl[4] = { "MAX 300", "100", "50", "MISS" };

        int hitCnt[4] = { ctx.count300, ctx.count100, ctx.count50, ctx.countMiss };
        u32 hitClr[4] = { ctx.clr300, ctx.clr100, ctx.clr50, ctx.clrMiss };
        u32 taikoHitClr[4] = { Ui::rgba(240, 50, 55), Ui::rgba(45, 160, 230), Ui::rgba(180, 50, 140), Ui::rgba(255, 215, 55) };
        u32 catchHitClr[4] = { ModeCatch::CLR_APPLE, ModeCatch::CLR_DROPLET, Ui::rgba(180, 220, 255), ctx.clrMiss };
        u32 maniaHitClr[4] = { ModeMania::CLR_COL_INNER, ModeMania::CLR_COL_OUTER, ctx.clr50, ctx.clrMiss };

        if (isTaikoPlay) {
            hitCnt[0] = ctx.count300;
            hitCnt[1] = ctx.count100;
            hitCnt[2] = ctx.countMiss;
            hitCnt[3] = ctx.taikoEngine.totalDrumrollHits;
        } else if (isCatchPlay) {
            hitCnt[0] = ctx.count300;
            hitCnt[1] = ctx.count100;
            hitCnt[2] = ctx.count50;
            hitCnt[3] = ctx.countMiss;
        }

        for (int i = 0; i < 4; ++i) {
            float hx = 158.0f + i * 59.5f + dPanel;
            const char* curLbl = isTaikoPlay ? taikoHitLbl[i] : (isCatchPlay ? catchHitLbl[i] : (isManiaPlay ? maniaHitLbl[i] : hitLbl[i]));
            u32 curClr = isTaikoPlay ? taikoHitClr[i] : (isCatchPlay ? catchHitClr[i] : (isManiaPlay ? maniaHitClr[i] : hitClr[i]));

            Ui::panel(hx, 146.0f, 55.0f, 44.0f, 0.3f, Ui::rgba(24, 21, 40, 230));
            C2D_DrawRectSolid(hx, 146.0f, 0.4f, 55.0f, 3.0f, curClr);
            drawCenteredText(ctx.textBuf, hx + 27.5f, 160.0f, (isTaikoPlay || isCatchPlay || isManiaPlay) ? 0.26f : 0.42f, curClr, curLbl, 0.9f);
            int rollCnt = static_cast<int>(hitCnt[i] * rollProgress);
            std::snprintf(buf, sizeof(buf), "%d", rollCnt);
            drawCenteredText(ctx.textBuf, hx + 27.5f, 177.0f, 0.50f, ctx.clrWhite, buf, 0.9f);
        }
        char urLine[96];
        std::snprintf(urLine, sizeof(urLine), "Mods: %s  |  UR: %.1f (%+.0f ms)",
            formatModsString(ctx.settings.activeMods).c_str(), ctx.lastUnstableRate, ctx.lastMeanHitError);
        drawText(ctx.textBuf, 160.0f + dPanel, 196.0f, 0.35f, ctx.clrMuted, urLine);

        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
        drawCenteredText(ctx.textBuf, 200.0f, 227.0f, 0.42f, ctx.clr300, LOC(STR_RESULTS_RETRY_HINT));
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, gradeClr);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), gradeClr, 0.02f);

    // 1. Cabecera con metadatos del mapa y PP ganado
    Ui::panel(8.0f, 6.0f, 304.0f, 38.0f, 0.15f, Ui::rgba(20, 16, 32, 245), gradeClr);
    drawText(ctx.textBuf, 16.0f, 8.0f, 0.42f, ctx.clrWhite, truncateString(ctx.beatmap.title, 26).c_str(), 0.22f);
    char subMeta[64];
    std::snprintf(subMeta, sizeof(subMeta), "[%s]  //  Por %s",
                  truncateString(ctx.beatmap.version, 16).c_str(), truncateString(ctx.beatmap.creator, 14).c_str());
    drawText(ctx.textBuf, 16.0f, 24.5f, 0.28f, ctx.clrMuted, subMeta, 0.22f);

    float modStars = getModAdjustedStars(ctx.beatmap.starRating, ctx.settings.activeMods);
    u32 stClr = getStarRatingColor(modStars);
    char stBadge[24];
    std::snprintf(stBadge, sizeof(stBadge), "* %.2f", modStars);
    Ui::pill(248.0f, 8.0f, 56.0f, 15.0f, 0.20f, stClr);
    drawCenteredText(ctx.textBuf, 276.0f, 15.5f, 0.33f, Ui::rgba(20, 16, 30), stBadge, 0.25f);

    std::snprintf(buf, sizeof(buf), "%.1f pp", ctx.lastEarnedPP);
    Ui::pill(248.0f, 25.0f, 56.0f, 15.0f, 0.20f, Ui::rgba(50, 40, 80, 230));
    drawCenteredText(ctx.textBuf, 276.0f, 32.5f, 0.33f, ctx.clr300, buf, 0.25f);

    // 2. Pestañas interactivas de Selección (Curva de Esfuerzo vs Ranking Local)
    float tabW = 148.0f;
    u32 tab0Bg = (resultsBottomTab == 0) ? Ui::rgba(52, 42, 88, 245) : Ui::rgba(20, 16, 32, 210);
    u32 tab1Bg = (resultsBottomTab == 1) ? Ui::rgba(52, 42, 88, 245) : Ui::rgba(20, 16, 32, 210);
    u32 tab0Brd = (resultsBottomTab == 0) ? ctx.clrCursor : Ui::rgba(55, 45, 80);
    u32 tab1Brd = (resultsBottomTab == 1) ? ctx.clrCursor : Ui::rgba(55, 45, 80);

    Ui::panel(8.0f, 48.0f, tabW, 19.0f, 0.18f, tab0Bg, tab0Brd);
    drawCenteredText(ctx.textBuf, 8.0f + tabW * 0.5f, 57.5f, 0.30f, (resultsBottomTab == 0) ? ctx.clrWhite : ctx.clrMuted, LOC(STR_RESULTS_TAB_STRAIN), 0.22f);

    Ui::panel(164.0f, 48.0f, tabW, 19.0f, 0.18f, tab1Bg, tab1Brd);
    drawCenteredText(ctx.textBuf, 164.0f + tabW * 0.5f, 57.5f, 0.30f, (resultsBottomTab == 1) ? ctx.clrWhite : ctx.clrMuted, LOC(STR_RESULTS_TAB_RANKING), 0.22f);

    if (resultsBottomTab == 0) {
        // VISTA 1: CURVA DE ESFUERZO (Strain Graph) + Historial de Fallos
        Ui::panel(8.0f, 70.0f, 304.0f, 96.0f, 0.18f, Ui::rgba(18, 14, 28, 250), ctx.clrPrimary);

        // Leyenda de marcas
        if (isTaikoPlay) {
            drawText(ctx.textBuf, 184.0f, 73.0f, 0.26f, Ui::rgba(45, 160, 230), "o 可", 0.22f);
            drawText(ctx.textBuf, 240.0f, 73.0f, 0.26f, ctx.clrMiss, "x 不可", 0.22f);
        } else {
            drawText(ctx.textBuf, 184.0f, 73.0f, 0.26f, ctx.clr100, "o 100", 0.22f);
            drawText(ctx.textBuf, 224.0f, 73.0f, 0.26f, ctx.clr50,  "o 50", 0.22f);
            drawText(ctx.textBuf, 260.0f, 73.0f, 0.26f, ctx.clrMiss,"x Miss", 0.22f);
        }

        float graphX = 12.0f, graphY = 82.0f, graphW = 296.0f, graphH = 68.0f;
        int firstT = ctx.beatmap.firstObjectTime;
        int lastT  = ctx.beatmap.lastObjectTime;
        if (lastT <= firstT) lastT = firstT + 1000;
        float totalSpan = static_cast<float>(lastT - firstT);

        // Tramos de descanso sombreados en el gráfico
        for (const auto& brk : ctx.beatmap.breaks) {
            if (brk.endTime > brk.startTime && brk.startTime >= firstT && brk.startTime <= lastT) {
                float bx0 = graphX + (std::max(0.0f, static_cast<float>(brk.startTime - firstT)) / totalSpan) * graphW;
                float bx1 = graphX + (std::min(totalSpan, static_cast<float>(brk.endTime - firstT)) / totalSpan) * graphW;
                if (bx1 > bx0) {
                    C2D_DrawRectSolid(bx0, graphY, 0.19f, bx1 - bx0, graphH, Ui::rgba(40, 32, 65, 140));
                    drawCenteredText(ctx.textBuf, (bx0 + bx1) * 0.5f, graphY + graphH * 0.5f, 0.24f, Ui::rgba(160, 150, 190), "BREAK", 0.20f);
                }
            }
        }

        // Gráfico Strain Graph relleno
        Ui::strainGraph(graphX, graphY, graphW, graphH, 0.20f, ctx.beatmap.strainGraph, gradeClr, -1.0f);

        // Dibujar marcas de fallos e imperfecciones sobre la línea temporal
        for (const auto& ev : ctx.judgementHistory) {
            if (ev.state == HIT_300) continue;
            float rx = graphX + (std::max(0.0f, std::min(totalSpan, static_cast<float>(ev.time - firstT))) / totalSpan) * (graphW - 2.0f);
            if (ev.state == HIT_MISS) {
                float my = graphY + graphH - 4.0f;
                C2D_DrawLine(rx, graphY + 1.0f, Ui::rgba(240, 70, 70, 50), rx, my, Ui::rgba(240, 70, 70, 100), 1.0f, 0.23f);
                C2D_DrawLine(rx - 2.5f, my - 2.5f, ctx.clrMiss, rx + 2.5f, my + 2.5f, ctx.clrMiss, 1.8f, 0.26f);
                C2D_DrawLine(rx - 2.5f, my + 2.5f, ctx.clrMiss, rx + 2.5f, my - 2.5f, ctx.clrMiss, 1.8f, 0.26f);
            } else if (ev.state == HIT_50) {
                C2D_DrawCircleSolid(rx, graphY + graphH - 6.0f, 0.25f, 1.8f, ctx.clr50);
            } else if (ev.state == HIT_100) {
                C2D_DrawCircleSolid(rx, graphY + graphH - 10.0f, 0.24f, 1.4f, ctx.clr100);
            }
        }

        // Marcas de tiempo al pie del gráfico (0:00 y duración final)
        drawText(ctx.textBuf, graphX + 2.0f, graphY + graphH - 8.0f, 0.22f, Ui::rgba(180, 175, 205), "0:00", 0.27f);
        char durStr[16];
        int totalSec = std::max(0, lastT / 1000);
        std::snprintf(durStr, sizeof(durStr), "%d:%02d", totalSec / 60, totalSec % 60);
        drawText(ctx.textBuf, graphX + graphW - 26.0f, graphY + graphH - 8.0f, 0.22f, Ui::rgba(180, 175, 205), durStr, 0.27f);
    } else {
        // VISTA 2: RANKING LOCAL (Top 5 en tarjeta SD)
        Ui::panel(8.0f, 70.0f, 304.0f, 96.0f, 0.18f, Ui::rgba(18, 14, 28, 250), ctx.clrPrimary);
        auto topScores = ctx.scoreMgr.getTopRecords(ctx.beatmap.filePath, 5, currentPlayMode);

        if (topScores.empty()) {
            drawCenteredText(ctx.textBuf, 160.0f, 118.0f, 0.36f, ctx.clrMuted, LOC(STR_RESULTS_NO_SCORES_SD), 0.25f);
        } else {
            for (size_t s = 0; s < topScores.size() && s < 5; ++s) {
                const auto& r = topScores[s];
                float ry = 73.0f + s * 18.2f;
                bool isCur = (r.bestScore == ctx.score && r.bestCombo == ctx.maxCombo && std::abs(r.bestAcc - acc) < 0.05f);
                u32 rowBg = isCur ? Ui::rgba(65, 45, 95, 230) : ((s % 2 == 0) ? Ui::rgba(24, 20, 36, 200) : Ui::rgba(18, 15, 28, 200));
                u32 rkClr = (s == 0) ? Ui::rgba(255, 215, 60) : ((s == 1) ? Ui::rgba(220, 230, 245) : ((s == 2) ? Ui::rgba(215, 145, 80) : ctx.clrMuted));

                Ui::pill(12.0f, ry, 296.0f, 16.5f, 0.20f, rowBg);
                if (isCur) {
                    drawRectOutline(12.0f, ry, 296.0f, 16.5f, 1.0f, Ui::rgba(255, 215, 60, 220), 0.22f);
                }

                char rkStr[8];
                std::snprintf(rkStr, sizeof(rkStr), "#%d", static_cast<int>(s + 1));
                drawText(ctx.textBuf, 16.0f, ry + 2.0f, 0.28f, rkClr, rkStr, 0.25f);

                char scStr[32];
                std::snprintf(scStr, sizeof(scStr), "%07d", r.bestScore);
                drawText(ctx.textBuf, 44.0f, ry + 2.0f, 0.30f, ctx.clrWhite, scStr, 0.25f);

                char cmbStr[24];
                std::snprintf(cmbStr, sizeof(cmbStr), "%dx", r.bestCombo);
                drawText(ctx.textBuf, 114.0f, ry + 2.0f, 0.27f, ctx.clrCursor, cmbStr, 0.25f);

                char accStr[24];
                std::snprintf(accStr, sizeof(accStr), "%.1f%%", r.bestAcc);
                drawText(ctx.textBuf, 166.0f, ry + 2.0f, 0.27f, ctx.clr300, accStr, 0.25f);

                if (r.bestMods != MOD_NONE) {
                    std::string mStr = formatModsString(r.bestMods);
                    drawText(ctx.textBuf, 218.0f, ry + 2.0f, 0.24f, ctx.clrMuted, mStr.c_str(), 0.25f);
                }

                Ui::pill(282.0f, ry + 1.5f, 20.0f, 13.5f, 0.22f, Ui::rgba(35, 28, 52));
                drawCenteredText(ctx.textBuf, 292.0f, ry + 8.0f, 0.28f, ctx.clrWhite, r.bestGrade.c_str(), 0.26f);
            }
        }
    }

    // 3. Tarjeta inferior de Logro / Estado
    if (ctx.newRecordAchieved) {
        float flash = 0.70f + 0.30f * std::sin(ctx.animTick * 0.18f);
        Ui::gradH(8.0f, 168.0f, 304.0f, 32.0f, 0.20f,
                  Ui::rgba(90, 32, 110, static_cast<u8>(240.0f * flash)),
                  Ui::rgba(30, 18, 55, 245));
        drawRectOutline(8.0f, 168.0f, 304.0f, 32.0f, 1.2f, Ui::withAlpha(ctx.clrCursor, static_cast<u8>(255.0f * flash)), 0.25f);
        Ui::pill(14.0f, 172.0f, 126.0f, 15.0f, 0.26f, Ui::rgba(255, 102, 171, 230));
        drawCenteredText(ctx.textBuf, 77.0f, 179.5f, 0.30f, ctx.clrWhite, LOC(STR_RESULTS_NEW_RECORD), 0.28f);

        char recSub[64];
        std::snprintf(recSub, sizeof(recSub), "%s: %d  •  UR: %.1f", LOC(STR_MAX_COMBO), ctx.maxCombo, ctx.lastUnstableRate);
        drawTextWithShadow(ctx.textBuf, 148.0f, 173.0f, 0.30f, ctx.clr100, recSub, 0.28f);
    } else {
        Ui::panel(8.0f, 168.0f, 304.0f, 32.0f, 0.20f, Ui::rgba(22, 18, 34, 240), gradeClr);
        char statSummary[96];
        std::snprintf(statSummary, sizeof(statSummary), "%s: %d / %d   •   UR: %.1f",
                      LOC(STR_MAX_COMBO), ctx.maxCombo, ctx.beatmap.maxPossibleCombo, ctx.lastUnstableRate);
        drawTextWithShadow(ctx.textBuf, 16.0f, 172.0f, 0.32f, ctx.clrWhite, statSummary, 0.25f);
        drawTextWithShadow(ctx.textBuf, 16.0f, 186.0f, 0.26f, ctx.clrMuted,
                 ctx.stageFailed ? LOC(STR_RESULTS_FAILED_DESC) : LOC(STR_RESULTS_CLEARED_DESC), 0.25f);
    }

    // 4. Tres botones de acción interactivos estilo multijugador (táctiles y con atajo físico)
    // Botón 1: Vista [X]
    Ui::button(8.0f, 204.0f, 96.0f, 30.0f, 0.22f, Ui::rgba(20, 36, 60, 240), Ui::rgba(40, 180, 240), false);
    Ui::pill(14.0f, 210.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(40, 180, 240));
    drawCenteredText(ctx.textBuf, 23.0f, 219.0f, 0.28f, Ui::rgba(0, 0, 0, 240), "X", 0.28f);
    drawTextWithShadow(ctx.textBuf, 36.0f, 211.0f, 0.32f, ctx.clrWhite, (resultsBottomTab == 0) ? LOC(STR_RESULTS_VIEW_TOP) : LOC(STR_RESULTS_STRAIN_CURVE), 0.28f);

    // Botón 2: Reintentar [A] (resaltado con brillo verde como en multijugador)
    Ui::button(111.0f, 204.0f, 98.0f, 30.0f, 0.22f, Ui::rgba(22, 52, 32, 245), Ui::rgba(30, 200, 85), true);
    Ui::pill(117.0f, 210.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(30, 200, 85));
    drawCenteredText(ctx.textBuf, 126.0f, 219.0f, 0.28f, Ui::rgba(0, 0, 0, 240), "A", 0.28f);
    drawTextWithShadow(ctx.textBuf, 140.0f, 211.0f, 0.32f, ctx.clrWhite, LOC(STR_RETRY), 0.28f);

    // Botón 3: Salir a Canciones [B]
    Ui::button(216.0f, 204.0f, 96.0f, 30.0f, 0.22f, Ui::rgba(42, 20, 52, 240), ctx.clrPrimary, false);
    Ui::pill(222.0f, 210.0f, 18.0f, 18.0f, 0.24f, ctx.clrPrimary);
    drawCenteredText(ctx.textBuf, 231.0f, 219.0f, 0.28f, ctx.clrWhite, "B", 0.28f);
    drawTextWithShadow(ctx.textBuf, 244.0f, 211.0f, 0.32f, ctx.clrWhite, LOC(STR_SONGS), 0.28f);

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, gradeClr);
    C3D_FrameEnd(0);
}
