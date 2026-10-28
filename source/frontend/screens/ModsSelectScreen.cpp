#include "ModsSelectScreen.hpp"
#include "Draw.hpp"
#include "Localization.hpp"
#include "ScoreManager.hpp"
#include "Mods.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

struct ModCardInfo {
    int modTableIdx;
    const char* badge;
    const char* name;
    Localization::StringId multDescId;
    uint32_t bit;
    u32 accentColor;
};

static const ModCardInfo s_modCards[4][4] = {
    // Tab 0: DIFICULTAD (4 mods)
    {
        { 1, "NF", "No Fail",    Localization::STR_MOD_MULT_MINUS_50, MOD_NF,   Ui::rgba(0, 229, 255) },
        { 0, "EZ", "Easy",       Localization::STR_MOD_MULT_MINUS_50, MOD_EZ,   Ui::rgba(80, 200, 110) },
        { 8, "HD", "Hidden",     Localization::STR_MOD_MULT_PLUS_6,   MOD_HD,   Ui::rgba(255, 209, 102) },
        { 3, "HR", "Hard Rock",  Localization::STR_MOD_MULT_PLUS_6,   MOD_HR,   Ui::rgba(240, 70, 70) }
    },
    // Tab 1: VELOCIDAD (4 mods)
    {
        { 2, "HT", "Half Time",   Localization::STR_MOD_MULT_MINUS_70, MOD_HT,   Ui::rgba(45, 150, 140) },
        { 6, "DT", "Double Time", Localization::STR_MOD_MULT_PLUS_12,  MOD_DT,   Ui::rgba(180, 70, 240) },
        { 7, "NC", "Nightcore",   Localization::STR_MOD_MULT_PLUS_12,  MOD_NC,   Ui::rgba(220, 80, 255) },
        { 9, "FL", "Flashlight",  Localization::STR_MOD_MULT_PLUS_12,  MOD_FL,   Ui::rgba(255, 215, 60) }
    },
    // Tab 2: DESAFÍO (4 mods)
    {
        { 4, "SD", "Sudden Death", Localization::STR_MOD_MULT_PLUS_0,   MOD_SD,   Ui::rgba(255, 90, 50) },
        { 5, "PF", "Perfect",      Localization::STR_MOD_MULT_PLUS_0,   MOD_PF,   Ui::rgba(255, 180, 40) },
        { 12, "SO", "Spun Out",    Localization::STR_MOD_MULT_MINUS_10, MOD_SO,   Ui::rgba(130, 220, 110) },
        { 13, "MR", "Mirror",      Localization::STR_MOD_MULT_PLUS_0,   MOD_MR,   Ui::rgba(255, 145, 185) }
    },
    // Tab 3: ASISTENCIA (3 mods: RX, AP, AT)
    {
        { 10, "RX", "Relax",       Localization::STR_MOD_MULT_UNRANKED, MOD_RX,   Ui::rgba(0, 216, 240) },
        { 11, "AP", "Auto Pilot",  Localization::STR_MOD_MULT_UNRANKED, MOD_AP,   Ui::rgba(80, 180, 255) },
        { 14, "AT", "Autoplay",    Localization::STR_MOD_MULT_AUTO,     MOD_AUTO, Ui::rgba(100, 110, 255) },
        { -1, "",   "",            Localization::STR_COUNT,             0,        0 }
    }
};

static const int s_tabCardCount[4] = { 4, 4, 4, 3 };

static const Localization::StringId s_tabNames[4] = {
    Localization::STR_MOD_TAB_DIFF,
    Localization::STR_MOD_TAB_SPEED,
    Localization::STR_MOD_TAB_CHALLENGE,
    Localization::STR_MOD_TAB_ASSIST
};

static const Localization::StringId s_catHeaders[4][2] = {
    { Localization::STR_MODS_DIFF_REDUCTION, Localization::STR_MODS_DIFF_INCREASE },
    { Localization::STR_MOD_CAT_SPEED_RED,   Localization::STR_MOD_CAT_SPEED_INC },
    { Localization::STR_MOD_CAT_EXTREME,     Localization::STR_MOD_CAT_SPECIAL },
    { Localization::STR_MOD_CAT_ASSIST,      Localization::STR_MOD_CAT_AUTOMATION }
};

static void computeStats(float baseCS, float baseAR, float baseOD, float baseHP, uint32_t mods,
                         float& outCS, float& outAR, float& outOD, float& outHP) {
    outCS = (baseCS > 0.0f) ? baseCS : 4.0f;
    outAR = (baseAR > 0.0f) ? baseAR : 8.0f;
    outOD = (baseOD > 0.0f) ? baseOD : 7.0f;
    outHP = (baseHP > 0.0f) ? baseHP : 6.0f;

    if (mods & MOD_EZ) {
        outCS *= 0.5f;
        outAR *= 0.5f;
        outOD *= 0.5f;
        outHP *= 0.5f;
    }
    if (mods & MOD_HR) {
        outCS = std::min(10.0f, outCS * 1.3f);
        outAR = std::min(10.0f, outAR * 1.4f);
        outOD = std::min(10.0f, outOD * 1.4f);
        outHP = std::min(10.0f, outHP * 1.4f);
    }
    if (mods & (MOD_DT | MOD_NC)) {
        if (outAR <= 5.0f) {
            float ms = (1800.0f - outAR * 120.0f) / 1.5f;
            outAR = (1800.0f - ms) / 120.0f;
        } else {
            float ms = (1200.0f - (outAR - 5.0f) * 150.0f) / 1.5f;
            outAR = 5.0f + (1200.0f - ms) / 150.0f;
        }
        outAR = std::min(11.0f, outAR);
        outOD = std::min(11.0f, outOD * 1.2f);
    }
    if (mods & MOD_HT) {
        if (outAR <= 5.0f) {
            float ms = (1800.0f - outAR * 120.0f) / 0.75f;
            outAR = std::max(0.0f, (1800.0f - ms) / 120.0f);
        } else {
            float ms = (1200.0f - (outAR - 5.0f) * 150.0f) / 0.75f;
            outAR = std::max(0.0f, 5.0f + (1200.0f - ms) / 150.0f);
        }
        outOD = std::max(0.0f, outOD * 0.8f);
    }
}

} // namespace

void renderModsTop(AppContext& ctx, float eyeSign, float baseCS, float baseAR, float baseOD, float baseHP, const std::string& title, const std::string& version) {
    float dTop = eyeSign * ctx.slider3D * 2.0f;

    // Fondo medianoche
    Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick),
                 Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

    // Cabecera superior
    Ui::pill(12.0f + dTop, 8.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(255, 45, 117));
    drawCenteredText(ctx.textBuf, 39.0f + dTop, 17.0f, 0.36f, ctx.clrWhite, "MODS", 0.30f);

    drawText(ctx.textBuf, 72.0f + dTop, 9.0f, 0.44f, ctx.clrWhite, LOC(STR_MODS_IMPACT), 0.30f);

    float currentMult = getModsScoreMultiplier(ctx.settings.activeMods);
    char multStr[40];
    std::snprintf(multStr, sizeof(multStr), LOC(STR_MODS_MULTIPLIER), currentMult);
    drawRightAlignedText(ctx.textBuf, 388.0f + dTop, 10.0f, 0.38f, Ui::rgba(0, 229, 255), multStr, 0.30f);

    C2D_DrawRectSolid(12.0f + dTop, 28.0f, 0.20f, 376.0f, 1.0f, Ui::rgba(0, 229, 255, 90));

    // Panel Central
    Ui::panel(12.0f + dTop, 34.0f, 376.0f, 172.0f, 0.20f, Ui::rgba(15, 13, 26, 245), Ui::rgba(0, 229, 255, 140));

    std::string bTitle = title.empty() ? LOC(STR_MOD_NO_MAP_SELECTED) : (title + " [" + version + "]");
    drawText(ctx.textBuf, 24.0f + dTop, 42.0f, 0.46f, ctx.clrWhite, truncateString(bTitle, 36).c_str(), 0.25f);
    drawText(ctx.textBuf, 24.0f + dTop, 60.0f, 0.28f, Ui::rgba(148, 163, 184),
             LOC(STR_MOD_PANEL_DESC), 0.25f);

    float curCS, curAR, curOD, curHP;
    computeStats(baseCS, baseAR, baseOD, baseHP, ctx.settings.activeMods, curCS, curAR, curOD, curHP);

    auto drawBar = [&](float bx, float by, float bw, const char* name, float val, u32 fillClr) {
        drawText(ctx.textBuf, bx, by, 0.35f, Ui::rgba(220, 225, 240), name, 0.25f);
        char vbuf[16];
        std::snprintf(vbuf, sizeof(vbuf), "%.1f", val);
        drawRightAlignedText(ctx.textBuf, bx + bw, by, 0.38f, fillClr, vbuf, 0.25f);
        C2D_DrawRectSolid(bx, by + 14.0f, 0.22f, bw, 7.0f, Ui::rgba(22, 28, 45, 255));
        float fillW = bw * std::max(0.0f, std::min(1.0f, val / 10.0f));
        if (fillW > 0.0f) {
            C2D_DrawRectSolid(bx, by + 14.0f, 0.24f, fillW, 7.0f, fillClr);
        }
    };

    drawBar(24.0f + dTop, 82.0f, 166.0f, "Circle Size (CS)", curCS, Ui::rgba(0, 229, 255));
    drawBar(208.0f + dTop, 82.0f, 166.0f, "Approach Rate (AR)", curAR, Ui::rgba(255, 209, 102));
    drawBar(24.0f + dTop, 110.0f, 166.0f, "Overall Difficulty (OD)", curOD, Ui::rgba(147, 85, 232));
    drawBar(208.0f + dTop, 110.0f, 166.0f, "HP Drain Rate", curHP, Ui::rgba(74, 222, 128));

    // Fila Mods Equipados
    drawText(ctx.textBuf, 24.0f + dTop, 148.0f, 0.34f, Ui::rgba(148, 163, 184), LOC(STR_MODS_EQUIPPED), 0.25f);
    float px = 136.0f + dTop;
    bool anyMod = false;
    for (int m = 0; m < 15; ++m) {
        if (ctx.settings.activeMods & modTable[m].bit) {
            anyMod = true;
            Ui::pill(px, 144.0f, 32.0f, 18.0f, 0.25f, modTable[m].color);
            drawCenteredText(ctx.textBuf, px + 16.0f, 153.0f, 0.34f, Ui::rgba(10, 10, 16), modTable[m].code, 0.30f);
            px += 38.0f;
        }
    }
    if (!anyMod) {
        drawText(ctx.textBuf, 136.0f + dTop, 148.0f, 0.34f, Ui::rgba(100, 110, 130), LOC(STR_MOD_NONE_EQUIPPED), 0.25f);
    }

    // Pie
    Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
    drawText(ctx.textBuf, 14.0f, 221.0f, 0.33f, Ui::rgba(148, 163, 184), LOC(STR_MOD_BOTTOM_HINT), 0.35f);
}

void renderModsBottom(AppContext& ctx, bool isMultiplayer) {
    float currentMult = getModsScoreMultiplier(ctx.settings.activeMods);

    // Fondo
    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                 Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.02f);

    // Cabecera (y = 0..26)
    C2D_DrawRectSolid(0.0f, 0.0f, 0.10f, 320.0f, 26.0f, Ui::rgba(27, 20, 45, 255));
    C2D_DrawRectSolid(0.0f, 25.0f, 0.12f, 320.0f, 1.0f, Ui::rgba(0, 229, 255, 120));
    drawText(ctx.textBuf, 10.0f, 6.0f, 0.38f, ctx.clrWhite, LOC(STR_MOD_SELECT_TITLE), 0.25f);

    drawText(ctx.textBuf, 172.0f, 7.0f, 0.32f, Ui::rgba(180, 190, 210), LOC(STR_MOD_MULT_SHORT), 0.25f);
    char bMult[16];
    std::snprintf(bMult, sizeof(bMult), "%.2fx", currentMult);
    u32 multPillBg = (currentMult == 0.0f) ? Ui::rgba(100, 116, 139)
                   : (currentMult < 1.0f)  ? Ui::rgba(59, 130, 246)
                   : (currentMult == 1.0f) ? Ui::rgba(30, 41, 59)
                   : Ui::rgba(217, 119, 6);
    Ui::pill(252.0f, 4.0f, 58.0f, 18.0f, 0.25f, multPillBg);
    drawCenteredText(ctx.textBuf, 281.0f, 13.0f, 0.34f, ctx.clrWhite, bMult, 0.30f);

    // Barra de 4 Tabs (y = 28..44)
    int curTab = std::max(0, std::min(ctx.modTab, 3));
    for (int t = 0; t < 4; ++t) {
        float tx = 6.0f + t * 78.0f;
        bool isSel = (curTab == t);
        u32 tBg = isSel ? Ui::rgba(20, 40, 68, 250) : Ui::rgba(20, 18, 32, 220);
        u32 tBrd = isSel ? Ui::rgba(0, 229, 255) : Ui::rgba(45, 40, 60);
        Ui::panel(tx, 28.0f, 74.0f, 17.0f, 0.20f, tBg, tBrd);
        drawCenteredText(ctx.textBuf, tx + 37.0f, 36.5f, 0.27f, isSel ? ctx.clrWhite : Ui::rgba(148, 163, 184), Localization::get(s_tabNames[t]), 0.25f);
    }

    // Subtítulos de Categoría (y = 48)
    drawText(ctx.textBuf, 12.0f, 48.0f, 0.30f, Ui::rgba(180, 190, 210), Localization::get(s_catHeaders[curTab][0]), 0.22f);
    drawText(ctx.textBuf, 166.0f, 48.0f, 0.30f, Ui::rgba(180, 190, 210), Localization::get(s_catHeaders[curTab][1]), 0.22f);

    // Tarjetas de Mods de la pestaña actual
    int cardCount = s_tabCardCount[curTab];
    for (int i = 0; i < cardCount; ++i) {
        int col = i % 2;
        int row = i / 2;
        float cx = 10.0f + col * 155.0f;
        float cy = 60.0f + row * 64.0f;
        float cw = 145.0f;
        float ch = 58.0f;

        const auto& card = s_modCards[curTab][i];
        bool active = (ctx.settings.activeMods & card.bit) != 0;
        bool isCursor = (ctx.modCursor == i);

        u32 cardBg = active ? Ui::rgba(16, 36, 56, 250) : Ui::rgba(18, 22, 38, 240);
        u32 borderClr = active ? Ui::rgba(0, 229, 255, 255) : (isCursor ? Ui::rgba(255, 209, 102) : Ui::rgba(38, 46, 68, 220));

        Ui::panel(cx, cy, cw, ch, 0.20f, cardBg, borderClr);
        if (active) {
            drawRectOutline(cx - 1.0f, cy - 1.0f, cw + 2.0f, ch + 2.0f, 1.0f, Ui::rgba(0, 229, 255, 120), 0.22f);
        }

        // Badge cuadrado izquierdo (32x32)
        u32 badgeBg = active ? card.accentColor : Ui::rgba(28, 34, 52, 240);
        u32 badgeTxt = active ? Ui::rgba(12, 14, 22) : Ui::rgba(148, 163, 184);
        Ui::panel(cx + 8.0f, cy + 13.0f, 32.0f, 32.0f, 0.24f, badgeBg, active ? Ui::rgba(255, 255, 255) : Ui::rgba(45, 55, 80));
        drawCenteredText(ctx.textBuf, cx + 24.0f, cy + 29.0f, 0.42f, badgeTxt, card.badge, 0.28f);

        // Nombre y multiplicador
        drawTextWithShadow(ctx.textBuf, cx + 46.0f, cy + 13.0f, 0.38f, ctx.clrWhite, card.name, 0.26f);
        drawText(ctx.textBuf, cx + 46.0f, cy + 28.0f, 0.30f, active ? Ui::rgba(0, 229, 255) : Ui::rgba(148, 163, 184), Localization::get(card.multDescId), 0.26f);

        // Pill de estado
        if (active) {
            Ui::pill(cx + cw - 48.0f, cy + 34.0f, 42.0f, 16.0f, 0.25f, Ui::rgba(0, 229, 255));
            drawCenteredText(ctx.textBuf, cx + cw - 27.0f, cy + 42.0f, 0.28f, Ui::rgba(10, 12, 18), LOC(STR_ON), 0.30f);
        } else {
            Ui::pill(cx + cw - 42.0f, cy + 34.0f, 36.0f, 16.0f, 0.25f, Ui::rgba(28, 34, 52));
            drawCenteredText(ctx.textBuf, cx + cw - 24.0f, cy + 42.0f, 0.28f, Ui::rgba(100, 116, 139), LOC(STR_OFF), 0.30f);
        }
    }

    // Botones de acción inferiores (y = 192..232)
    // Botón Desactivar Todos
    Ui::panel(10.0f, 192.0f, 145.0f, 40.0f, 0.20f, Ui::rgba(30, 24, 42, 240), Ui::rgba(255, 209, 102));
    Ui::pill(16.0f, 198.0f, 24.0f, 24.0f, 0.24f, Ui::rgba(255, 209, 102));
    drawCenteredText(ctx.textBuf, 28.0f, 210.0f, 0.34f, Ui::rgba(14, 10, 20), "Y", 0.28f);
    drawTextWithShadow(ctx.textBuf, 46.0f, 204.0f, 0.34f, ctx.clrWhite, LOC(STR_MOD_RESET_ALL), 0.26f);

    // Botón Volver / Listo
    Ui::panel(165.0f, 192.0f, 145.0f, 40.0f, 0.20f, Ui::rgba(20, 38, 52, 240), Ui::rgba(0, 229, 255));
    Ui::pill(171.0f, 198.0f, 24.0f, 24.0f, 0.24f, Ui::rgba(0, 229, 255));
    drawCenteredText(ctx.textBuf, 183.0f, 210.0f, 0.34f, Ui::rgba(14, 10, 20), "B", 0.28f);
    const char* bLabel = isMultiplayer ? LOC(STR_MOD_BACK_TO_ROOM) : LOC(STR_DONE);
    drawTextWithShadow(ctx.textBuf, 201.0f, 204.0f, 0.34f, ctx.clrWhite, bLabel, 0.26f);
}

void handleModsInput(AppContext& ctx, bool isMultiplayer) {
    if (ctx.kDown & KEY_L) {
        ctx.modTab = (ctx.modTab + 3) % 4;
        ctx.modCursor = std::min(ctx.modCursor, s_tabCardCount[ctx.modTab] - 1);
        ctx.audio.playHitSound(0);
    }
    if (ctx.kDown & KEY_R) {
        ctx.modTab = (ctx.modTab + 1) % 4;
        ctx.modCursor = std::min(ctx.modCursor, s_tabCardCount[ctx.modTab] - 1);
        ctx.audio.playHitSound(0);
    }
    int curTab = std::max(0, std::min(ctx.modTab, 3));
    int numCards = s_tabCardCount[curTab];

    if (ctx.kDown & (KEY_DLEFT | KEY_DRIGHT)) {
        ctx.modCursor ^= 1;
        if (ctx.modCursor >= numCards) ctx.modCursor = numCards - 1;
        ctx.audio.playHitSound(0);
    }
    if (ctx.kDown & (KEY_DUP | KEY_DDOWN)) {
        ctx.modCursor ^= 2;
        if (ctx.modCursor >= numCards) ctx.modCursor = numCards - 1;
        ctx.audio.playHitSound(0);
    }
    if (ctx.kDown & KEY_A) {
        if (ctx.modCursor >= 0 && ctx.modCursor < numCards) {
            int mIdx = s_modCards[curTab][ctx.modCursor].modTableIdx;
            if (mIdx >= 0) {
                ctx.toggleModByIndex(mIdx);
                ctx.audio.playHitSound(2);
                if (!isMultiplayer) ctx.updateSongPreview();
            }
        }
    }
    if (ctx.kDown & KEY_Y) {
        ctx.settings.activeMods = MOD_NONE;
        if (ctx.multiMgr.inMultiplayer) {
            ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods = MOD_NONE;
            if (!ctx.multiMgr.isHost()) ctx.multiMgr.sendPlayerUpdate();
            else ctx.multiMgr.broadcastLobbyState();
        }
        ctx.settings.save();
        ctx.audio.playHitSound(0);
        if (!isMultiplayer) ctx.updateSongPreview();
    }
    if (ctx.kDown & (KEY_B | KEY_START)) {
        ctx.audio.playHitSound(0);
        if (isMultiplayer) {
            ctx.appState = STATE_MULTI_LOBBY;
        } else {
            ctx.modMenuOpen = false;
            ctx.updateSongPreview();
        }
        return;
    }

    if (ctx.touchJustDown) {
        // 1. Selector de 4 Tabs (y = 26..48)
        if (ctx.touch.py >= 26.0f && ctx.touch.py <= 48.0f) {
            for (int t = 0; t < 4; ++t) {
                float tx = 6.0f + t * 78.0f;
                if (ctx.touch.px >= tx && ctx.touch.px <= tx + 74.0f) {
                    ctx.modTab = t;
                    ctx.modCursor = std::min(ctx.modCursor, s_tabCardCount[t] - 1);
                    ctx.audio.playHitSound(0);
                    return;
                }
            }
        }

        // 2. Tarjetas de Mods de la pestaña activa
        for (int i = 0; i < numCards; ++i) {
            int col = i % 2;
            int row = i / 2;
            float cx = 10.0f + col * 155.0f;
            float cy = 60.0f + row * 64.0f;
            if (ctx.touch.px >= cx && ctx.touch.px <= cx + 145.0f &&
                ctx.touch.py >= cy && ctx.touch.py <= cy + 58.0f) {
                ctx.modCursor = i;
                int mIdx = s_modCards[curTab][i].modTableIdx;
                if (mIdx >= 0) {
                    ctx.toggleModByIndex(mIdx);
                    ctx.audio.playHitSound(2);
                    if (!isMultiplayer) ctx.updateSongPreview();
                }
                return;
            }
        }

        // 3. Botones inferiores
        if (ctx.touch.py >= 190.0f && ctx.touch.py <= 236.0f) {
            if (ctx.touch.px >= 10.0f && ctx.touch.px <= 155.0f) {
                // Desactivar Todos
                ctx.settings.activeMods = MOD_NONE;
                if (ctx.multiMgr.inMultiplayer) {
                    ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods = MOD_NONE;
                    if (!ctx.multiMgr.isHost()) ctx.multiMgr.sendPlayerUpdate();
                    else ctx.multiMgr.broadcastLobbyState();
                }
                ctx.settings.save();
                ctx.audio.playHitSound(0);
                if (!isMultiplayer) ctx.updateSongPreview();
                return;
            } else if (ctx.touch.px >= 165.0f && ctx.touch.px <= 310.0f) {
                // Volver / Listo
                ctx.audio.playHitSound(0);
                if (isMultiplayer) {
                    ctx.appState = STATE_MULTI_LOBBY;
                } else {
                    ctx.modMenuOpen = false;
                    ctx.updateSongPreview();
                }
                return;
            }
        }
    }
}

void runModsSelectScreen(AppContext& ctx) {
    ctx.audio.update();
    if (ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }
    ctx.multiMgr.update(false);

    handleModsInput(ctx, true);
    if (ctx.appState != STATE_MODS_SELECT) return;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    std::string title = ctx.multiMgr.activeBeatmap.title;
    std::string version = ctx.multiMgr.activeBeatmap.version;
    float baseCS = ctx.beatmap.diff.cs > 0.0f ? ctx.beatmap.diff.cs : 4.0f;
    float baseAR = ctx.beatmap.diff.ar > 0.0f ? ctx.beatmap.diff.ar : 8.0f;
    float baseOD = ctx.beatmap.diff.od > 0.0f ? ctx.beatmap.diff.od : 7.0f;
    float baseHP = ctx.beatmap.diff.hp > 0.0f ? ctx.beatmap.diff.hp : 6.0f;

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);

        renderModsTop(ctx, eyeSign, baseCS, baseAR, baseOD, baseHP, title, version);
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    renderModsBottom(ctx, true);
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
