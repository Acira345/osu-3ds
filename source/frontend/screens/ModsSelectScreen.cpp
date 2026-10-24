#include "ModsSelectScreen.hpp"

void runModsSelectScreen(AppContext& ctx) {
    ctx.audio.update();
    if (ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }
    ctx.multiMgr.update(false);

    if (ctx.kDown & (KEY_B | KEY_START)) {
        ctx.audio.playHitSound(0);
        ctx.appState = STATE_MULTI_LOBBY;
        return;
    }

    float currentMult = getModsScoreMultiplier(ctx.settings.activeMods);

    if (ctx.touchJustDown) {
        // Fila 1: Reducción de Dificultad (NF, EZ)
        if (ctx.touch.py >= 54.0f && ctx.touch.py <= 114.0f) {
            if (ctx.touch.px >= 14.0f && ctx.touch.px <= 152.0f) {
                ctx.settings.activeMods ^= MOD_NOFAIL;
                ctx.audio.playHitSound(2);
            } else if (ctx.touch.px >= 168.0f && ctx.touch.px <= 306.0f) {
                ctx.settings.activeMods ^= MOD_EASY;
                if (ctx.settings.activeMods & MOD_EASY) ctx.settings.activeMods &= ~MOD_HARDROCK;
                ctx.audio.playHitSound(2);
            }
        }
        // Fila 2: Aumento de Dificultad (HD, HR)
        if (ctx.touch.py >= 132.0f && ctx.touch.py <= 192.0f) {
            if (ctx.touch.px >= 14.0f && ctx.touch.px <= 152.0f) {
                ctx.settings.activeMods ^= MOD_HIDDEN;
                ctx.audio.playHitSound(2);
            } else if (ctx.touch.px >= 168.0f && ctx.touch.px <= 306.0f) {
                ctx.settings.activeMods ^= MOD_HARDROCK;
                if (ctx.settings.activeMods & MOD_HARDROCK) ctx.settings.activeMods &= ~MOD_EASY;
                ctx.audio.playHitSound(2);
            }
        }

        if (ctx.touch.py >= 198.0f && ctx.touch.py <= 234.0f) {
            ctx.audio.playHitSound(0);
            ctx.appState = STATE_MULTI_LOBBY;
            return;
        }

        ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods = ctx.settings.activeMods;
        ctx.multiMgr.sendPlayerUpdate();
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dTop = depthShift(2.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

        Ui::pill(12.0f + dTop, 10.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
        drawCenteredText(ctx.textBuf, 39.0f + dTop, 18.0f, 0.36f, ctx.clrWhite, "MODS", 0.30f);

        drawText(ctx.textBuf, 72.0f + dTop, 11.0f, 0.44f, ctx.clrWhite, LOC(STR_MODS_IMPACT), 0.30f);

        char multStr[32];
        std::snprintf(multStr, sizeof(multStr), LOC(STR_MODS_MULTIPLIER), currentMult);
        drawText(ctx.textBuf, 230.0f + dTop, 12.0f, 0.36f, Ui::rgba(40, 215, 235), multStr, 0.30f);

        Ui::panel(12.0f + dTop, 36.0f, 376.0f, 150.0f, 0.20f, Ui::rgba(20, 18, 34, 240), Ui::rgba(40, 215, 235));

        std::string bTitle = ctx.multiMgr.activeBeatmap.title + " [" + ctx.multiMgr.activeBeatmap.version + "]";
        drawText(ctx.textBuf, 22.0f + dTop, 44.0f, 0.46f, ctx.clrWhite, truncateString(bTitle, 36).c_str(), 0.25f);
        drawText(ctx.textBuf, 22.0f + dTop, 62.0f, 0.32f, ctx.clrMuted, LOC(STR_MODS_INFO), 0.25f);

        float curCS = ctx.beatmap.diff.cs;
        float curAR = ctx.beatmap.diff.ar;
        float curOD = ctx.beatmap.diff.od;
        float curHP = ctx.beatmap.diff.hp;

        if (ctx.settings.activeMods & MOD_EASY) {
            curCS *= 0.5f; curAR *= 0.5f; curOD *= 0.5f; curHP *= 0.5f;
        }
        if (ctx.settings.activeMods & MOD_HARDROCK) {
            curCS = std::min(10.0f, curCS * 1.3f);
            curAR = std::min(10.0f, curAR * 1.4f);
            curOD = std::min(10.0f, curOD * 1.4f);
            curHP = std::min(10.0f, curHP * 1.4f);
        }

        drawStatBar(ctx.textBuf, 22.0f + dTop, 80.0f, "Circle Size (CS)", curCS, Ui::rgba(40, 215, 235), ctx.clrBgDark, ctx.clrWhite);
        drawStatBar(ctx.textBuf, 206.0f + dTop, 80.0f, "Approach Rate (AR)", curAR, Ui::rgba(255, 185, 40), ctx.clrBgDark, ctx.clrWhite);

        drawStatBar(ctx.textBuf, 22.0f + dTop, 108.0f, "Overall Diff (OD)", curOD, Ui::rgba(180, 80, 240), ctx.clrBgDark, ctx.clrWhite);
        drawStatBar(ctx.textBuf, 206.0f + dTop, 108.0f, "HP Drain Rate", curHP, Ui::rgba(60, 215, 120), ctx.clrBgDark, ctx.clrWhite);

        drawText(ctx.textBuf, 22.0f + dTop, 142.0f, 0.34f, ctx.clrMuted, LOC(STR_MODS_EQUIPPED), 0.25f);
        float pillX = 120.0f + dTop;
        if (ctx.settings.activeMods & MOD_HIDDEN) {
            Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(255, 205, 50));
            drawCenteredText(ctx.textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "HD", 0.30f);
            pillX += 34.0f;
        }
        if (ctx.settings.activeMods & MOD_NOFAIL) {
            Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "NF", 0.30f);
            pillX += 34.0f;
        }
        if (ctx.settings.activeMods & MOD_HARDROCK) {
            Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(240, 70, 70));
            drawCenteredText(ctx.textBuf, pillX + 14.0f, 146.0f, 0.30f, ctx.clrWhite, "HR", 0.30f);
            pillX += 34.0f;
        }
        if (ctx.settings.activeMods & MOD_EASY) {
            Ui::pill(pillX, 138.0f, 28.0f, 16.0f, 0.25f, Ui::rgba(80, 200, 110));
            drawCenteredText(ctx.textBuf, pillX + 14.0f, 146.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "EZ", 0.30f);
            pillX += 34.0f;
        }

        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        drawText(ctx.textBuf, 12.0f, 221.0f, 0.34f, ctx.clrMuted, LOC(STR_MODS_CONTROLS_HINT));
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.02f);

    drawTextWithShadow(ctx.textBuf, 14.0f, 8.0f, 0.40f, ctx.clrWhite, LOC(STR_MODS_TITLE), 0.20f);
    char bMult[16];
    std::snprintf(bMult, sizeof(bMult), "%.2fx", currentMult);
    Ui::pill(252.0f, 6.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(30, 80, 150));
    drawCenteredText(ctx.textBuf, 279.0f, 14.0f, 0.34f, ctx.clrWhite, bMult, 0.30f);

    drawTextWithShadow(ctx.textBuf, 14.0f, 36.0f, 0.32f, Ui::rgba(215, 210, 235), LOC(STR_MODS_DIFF_REDUCTION), 0.20f);

    bool nfOn = (ctx.settings.activeMods & MOD_NOFAIL) != 0;
    Ui::button(14.0f, 54.0f, 138.0f, 58.0f, 0.20f, nfOn ? Ui::rgba(22, 42, 60, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235), nfOn);
    Ui::pill(22.0f, 62.0f, 32.0f, 32.0f, 0.25f, Ui::rgba(40, 215, 235));
    drawCenteredText(ctx.textBuf, 38.0f, 78.0f, 0.40f, Ui::rgba(0, 0, 0, 240), "NF", 0.30f);
    drawTextWithShadow(ctx.textBuf, 60.0f, 60.0f, 0.38f, ctx.clrWhite, "No Fail", 0.25f);
    drawTextWithShadow(ctx.textBuf, 60.0f, 74.0f, 0.30f, Ui::rgba(215, 210, 235), ("-50% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
    Ui::pill(60.0f, 88.0f, 48.0f, 16.0f, 0.26f, nfOn ? Ui::rgba(40, 215, 235) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 84.0f, 96.0f, 0.28f, nfOn ? Ui::rgba(0, 0, 0, 240) : ctx.clrMuted, nfOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

    bool ezOn = (ctx.settings.activeMods & MOD_EASY) != 0;
    Ui::button(168.0f, 54.0f, 138.0f, 58.0f, 0.20f, ezOn ? Ui::rgba(22, 48, 36, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(80, 200, 110), ezOn);
    Ui::pill(176.0f, 62.0f, 32.0f, 32.0f, 0.25f, ezOn ? Ui::rgba(80, 200, 110) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 192.0f, 78.0f, 0.40f, ezOn ? Ui::rgba(0, 0, 0, 240) : ctx.clrMuted, "EZ", 0.30f);
    drawTextWithShadow(ctx.textBuf, 214.0f, 60.0f, 0.38f, ctx.clrWhite, "Easy", 0.25f);
    drawTextWithShadow(ctx.textBuf, 214.0f, 74.0f, 0.30f, Ui::rgba(215, 210, 235), ("-50% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
    Ui::pill(214.0f, 88.0f, 48.0f, 16.0f, 0.26f, ezOn ? Ui::rgba(80, 200, 110) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 238.0f, 96.0f, 0.28f, ezOn ? Ui::rgba(0, 0, 0, 240) : ctx.clrMuted, ezOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

    drawTextWithShadow(ctx.textBuf, 14.0f, 118.0f, 0.32f, Ui::rgba(215, 210, 235), LOC(STR_MODS_DIFF_INCREASE), 0.20f);

    bool hdOn = (ctx.settings.activeMods & MOD_HIDDEN) != 0;
    Ui::button(14.0f, 132.0f, 138.0f, 58.0f, 0.20f, hdOn ? Ui::rgba(48, 40, 22, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(255, 205, 50), hdOn);
    Ui::pill(22.0f, 140.0f, 32.0f, 32.0f, 0.25f, hdOn ? Ui::rgba(255, 205, 50) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 38.0f, 156.0f, 0.40f, hdOn ? Ui::rgba(0, 0, 0, 240) : ctx.clrMuted, "HD", 0.30f);
    drawTextWithShadow(ctx.textBuf, 60.0f, 138.0f, 0.38f, ctx.clrWhite, "Hidden", 0.25f);
    drawTextWithShadow(ctx.textBuf, 60.0f, 152.0f, 0.30f, Ui::rgba(215, 210, 235), ("+6% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
    Ui::pill(60.0f, 166.0f, 48.0f, 16.0f, 0.26f, hdOn ? Ui::rgba(255, 205, 50) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 84.0f, 174.0f, 0.28f, hdOn ? Ui::rgba(0, 0, 0, 240) : ctx.clrMuted, hdOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

    bool hrOn = (ctx.settings.activeMods & MOD_HARDROCK) != 0;
    Ui::button(168.0f, 132.0f, 138.0f, 58.0f, 0.20f, hrOn ? Ui::rgba(52, 24, 28, 245) : Ui::rgba(22, 19, 36, 240), Ui::rgba(240, 70, 70), hrOn);
    Ui::pill(176.0f, 140.0f, 32.0f, 32.0f, 0.25f, hrOn ? Ui::rgba(240, 70, 70) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 192.0f, 156.0f, 0.40f, ctx.clrWhite, "HR", 0.30f);
    drawTextWithShadow(ctx.textBuf, 214.0f, 138.0f, 0.38f, ctx.clrWhite, "Hard Rock", 0.25f);
    drawTextWithShadow(ctx.textBuf, 214.0f, 152.0f, 0.30f, Ui::rgba(215, 210, 235), ("+6% " + std::string(LOC(STR_POINTS))).c_str(), 0.25f);
    Ui::pill(214.0f, 166.0f, 48.0f, 16.0f, 0.26f, hrOn ? Ui::rgba(240, 70, 70) : Ui::rgba(45, 40, 60));
    drawCenteredText(ctx.textBuf, 238.0f, 174.0f, 0.28f, hrOn ? ctx.clrWhite : ctx.clrMuted, hrOn ? LOC(STR_ON) : LOC(STR_OFF), 0.30f);

    Ui::button(20.0f, 198.0f, 280.0f, 34.0f, 0.20f, Ui::rgba(42, 20, 52, 240), ctx.clrPrimary, true);
    Ui::pill(28.0f, 204.0f, 22.0f, 22.0f, 0.25f, ctx.clrPrimary);
    drawCenteredText(ctx.textBuf, 39.0f, 215.0f, 0.32f, ctx.clrWhite, "B", 0.28f);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 215.0f, 0.42f, ctx.clrWhite, LOC(STR_MULTI_BACK_TO_ROOM), 0.28f);

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
