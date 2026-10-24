#include "MainMenuScreen.hpp"

void runMainMenuScreen(AppContext& ctx) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    ctx.audio.update();
    if (ctx.bgLoader.poll(ctx.bgTex)) ctx.bgFade = 0.0f;
    ctx.bgFade = Ui::approach(ctx.bgFade, 255.0f, 0.10f);
    // Al terminar la canción del menú (sea Click the circles o una descargada),
    // continuar con otra de las canciones descargadas continuamente sin quedarse en silencio
    if (ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && (ctx.audio.isSongFinished() || !ctx.audio.getIsPlaying())) {
        ctx.rotateMenuSong();
    }
    if (ctx.kDown & KEY_START) { ctx.shouldQuit = true; return; }

    if (ctx.kDown & KEY_DUP) {
        ctx.mainMenuOption = (ctx.mainMenuOption + 4) % 5;
        ctx.audio.playHitSound(0);
    }
    if (ctx.kDown & KEY_DDOWN) {
        ctx.mainMenuOption = (ctx.mainMenuOption + 1) % 5;
        ctx.audio.playHitSound(0);
    }
    // Cambiar canción del menú manualmente con L y R o cruceta Izq/Der
    if (!ctx.songMgr.sets.empty() && (ctx.kDown & (KEY_L | KEY_R | KEY_DLEFT | KEY_DRIGHT))) {
        int dir = (ctx.kDown & (KEY_R | KEY_DRIGHT)) ? 1 : -1;
        int nSets = static_cast<int>(ctx.songMgr.sets.size());
        ctx.selectedSet = (ctx.selectedSet + dir + nSets) % nSets;
        ctx.selectedDiff = 0;
        ctx.carouselPos = static_cast<float>(ctx.selectedSet);
        ctx.audio.playHitSound(0);
        ctx.updateSongPreview();
    }

    bool activateOption = (ctx.kDown & KEY_A) != 0;

    if (ctx.touchJustDown) {
        if (ctx.touch.px >= 18 && ctx.touch.px <= 302) {
            for (int i = 0; i < 5; ++i) {
                float by = 12.0f + i * 34.0f;
                if (ctx.touch.py >= by && ctx.touch.py <= by + 30.0f) {
                    ctx.mainMenuOption = i;
                    activateOption = true;
                    break;
                }
            }
        }
    }

    if (activateOption) {
        ctx.audio.playHitSound(2);
        if (ctx.mainMenuOption == 0) {
            ctx.appState = STATE_SONG_SELECT;
            ctx.updateSongPreview();
        } else if (ctx.mainMenuOption == 1) {
            // Abrir Navegador de Salas Multijugador Local (LAN)
            ctx.multiMgr.startBrowser(ctx.sysUser);
            ctx.appState = STATE_MULTI_BROWSER;
        } else if (ctx.mainMenuOption == 2) {
            ctx.audio.stopMusic();
            ctx.multiMgr.stop();
            ctx.wifiReturnState = STATE_MAIN_MENU;
            ctx.wifiServer.start();
            ctx.appState = STATE_WIFI_TRANSFER;
        } else if (ctx.mainMenuOption == 3) {
            ctx.settingsReturnState = STATE_MAIN_MENU;
            ctx.calibrationMode = false;
            ctx.appState = STATE_SETTINGS;
        } else if (ctx.mainMenuOption == 4) {
            { ctx.shouldQuit = true; return; }
        }
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    float beat = ctx.getBpmPulse();
    const char* menuLabels[5] = {
        LOC(STR_MENU_PLAY),
        LOC(STR_MENU_MULTI),
        LOC(STR_MENU_WIFI),
        LOC(STR_MENU_SETTINGS),
        LOC(STR_MENU_EXIT)
    };
    const char* menuSubPills[5] = {
        LOC(STR_MENU_PILL_SOLO),
        LOC(STR_MENU_PILL_MULTI),
        LOC(STR_MENU_PILL_WIFI),
        LOC(STR_MENU_PILL_SETTINGS),
        LOC(STR_MENU_PILL_EXIT)
    };
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dBg = depthShift(-3.5f);
        if (ctx.bgTex.loaded) {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(34, 14, 56), Ui::rgba(10, 7, 22), ctx.clrPrimary, 0.01f);
            ctx.bgTex.drawRect(dBg - beat * 2.0f, -beat * 1.5f, 400.0f + beat * 4.0f, 240.0f + beat * 3.0f, 0.02f, static_cast<u8>(ctx.bgFade));
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 180) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, dimAlpha));
        } else {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(34, 14, 56), Ui::rgba(10, 7, 22), ctx.clrPrimary, 0.02f);
        }

        // Barra superior de canción en reproducción (Estilo osu! PC / Multijugador)
        if (!ctx.songMgr.sets.empty() && ctx.selectedSet >= 0 && ctx.selectedSet < static_cast<int>(ctx.songMgr.sets.size())) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            Ui::gradV(0.0f, 0.0f, 400.0f, 26.0f, 0.28f, Ui::rgba(14, 10, 24, 240), Ui::rgba(14, 10, 24, 60));

            // Pastilla indicadora de música estilo multijugador (sin signos de interrogación)
            Ui::pill(10.0f, 5.0f, 56.0f, 16.0f, 0.32f, Ui::rgba(255, 102, 171));
            drawCenteredText(ctx.textBuf, 38.0f, 12.0f, 0.28f, ctx.clrWhite, LOC(STR_MENU_MUSIC_NOW), 0.35f);

            std::string nowPlaying = curSet.artist + " - " + curSet.title;
            drawTextWithShadow(ctx.textBuf, 72.0f, 5.0f, 0.38f, ctx.clrWhite, truncateString(nowPlaying, 34).c_str(), 0.35f);

            Ui::pill(322.0f, 5.0f, 70.0f, 16.0f, 0.32f, Ui::rgba(30, 24, 46, 230));
            drawCenteredText(ctx.textBuf, 357.0f, 12.0f, 0.29f, ctx.isNew3DS ? ctx.clr100 : ctx.clrMuted, ctx.isNew3DS ? "New3DS 804M" : "3DS 268M", 0.35f);
        }

        // Logo que late sincronizado al BPM real y flota hacia adelante en 3D
        float dLogo = depthShift(3.5f + beat * 1.2f);
        float logoX = 118.0f + dLogo;
        float logoY = 126.0f;
        float logoR = 55.0f + beat * 8.0f;

        for (int k = 0; k < 3; ++k) {
            float phase = std::fmod(ctx.animTick * 0.012f + k / 3.0f, 1.0f);
            Ui::ring(logoX, logoY, 0.15f, logoR + phase * 60.0f, 2.0f, Ui::withAlpha(ctx.clrPrimary, static_cast<u8>((1.0f - phase) * 95.0f)), 40);
        }
        Ui::glow(logoX, logoY, 0.16f, logoR * 1.9f, Ui::rgba(255, 102, 170, static_cast<u8>(115 + beat * 45)), 6);
        C2D_DrawCircleSolid(logoX + 3.0f, logoY + 5.0f, 0.18f, logoR, Ui::rgba(0, 0, 0, 120));
        C2D_DrawCircleSolid(logoX, logoY, 0.2f, logoR, Ui::rgba(255, 98, 168, 255));
        C2D_DrawCircleSolid(logoX - logoR * 0.22f, logoY - logoR * 0.28f, 0.21f, logoR * 0.6f, Ui::rgba(255, 255, 255, 30));
        Ui::ring(logoX, logoY, 0.25f, logoR, 3.5f, ctx.clrWhite, 40);
        drawCenteredText(ctx.textBuf, logoX, logoY - 6.0f, 1.10f + beat * 0.06f, ctx.clrWhite, "osu!");
        drawCenteredText(ctx.textBuf, logoX, logoY + 24.0f, 0.46f, Ui::rgba(255, 235, 245), "3DS Edition");

        for (int i = 0; i < 5; ++i) {
            bool sel = (ctx.mainMenuOption == i);
            float dBtn = depthShift(sel ? 4.2f : 1.5f);
            float bx = (sel ? (202.0f - beat * 3.5f) : 218.0f) + dBtn;
            float by = 28.0f + i * 36.0f;
            float bw = 178.0f;
            float bh = 30.0f;

            const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
            u32 base = C2D_Color32(c.r, c.g, c.b, 255);
            u32 bg = sel ? Ui::rgba(42, 34, 70, 250) : Ui::rgba(22, 19, 36, 235);
            Ui::button(bx, by, bw, bh, 0.20f, bg, base, sel);

            // Pastilla indicadora de selección [>] o [A]
            Ui::pill(bx + 8.0f, by + 6.0f, 20.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(38, 32, 55, 240));
            drawCenteredText(ctx.textBuf, bx + 18.0f, by + 14.0f, 0.28f, sel ? Ui::rgba(0, 0, 0, 245) : ctx.clrWhite, sel ? "A" : ">", 0.28f);

            // Título de la opción del menú
            drawTextWithShadow(ctx.textBuf, bx + 34.0f, by + 6.5f, 0.38f, ctx.clrWhite, menuLabels[i], 0.28f);

            // Pastilla de categoría / modo a la derecha estilo multijugador
            Ui::pill(bx + bw - 58.0f, by + 6.0f, 52.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(32, 26, 48, 220));
            drawCenteredText(ctx.textBuf, bx + bw - 32.0f, by + 14.0f, 0.26f, sel ? Ui::rgba(0, 0, 0, 245) : Ui::rgba(215, 210, 235), menuSubPills[i], 0.28f);
        }

        // Barra inferior de atajos estilo multijugador con v4.0 3D
        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(14, 11, 26, 140), Ui::rgba(14, 11, 26, 250));
        float byY = 219.0f;
        drawKeyBadge(ctx.textBuf, 12.0f, byY, "A", LOC(STR_SELECT_ACTION), Ui::rgba(50, 165, 255));
        drawKeyBadge(ctx.textBuf, 88.0f, byY, "DPAD", LOC(STR_MOVE_ACTION), Ui::rgba(255, 180, 40));
        drawKeyBadge(ctx.textBuf, 164.0f, byY, "TOUCH", LOC(STR_TOUCH_ACTION), Ui::rgba(50, 215, 115));

        Ui::pill(328.0f, byY - 1.0f, 62.0f, 16.0f, 0.30f, Ui::rgba(25, 40, 65, 230));
        drawCenteredText(ctx.textBuf, 359.0f, byY + 6.5f, 0.30f, Ui::rgba(100, 215, 255), "v4.0 3D", 0.32f);
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), ctx.clrPrimary, 0.02f);
    if (ctx.bgTex.loaded) {
        ctx.bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 75);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 160));
    }

    for (int i = 0; i < 5; ++i) {
        bool sel = (ctx.mainMenuOption == i);
        float bx = 20.0f;
        float by = 12.0f + i * 34.0f;
        float bw = 280.0f;
        float bh = 30.0f;

        const RGBColor& c = activeSkin.comboColors[i % activeSkin.comboColors.size()];
        u32 base = C2D_Color32(c.r, c.g, c.b, 255);
        u32 bg = sel ? Ui::rgba(45, 36, 72, 250) : Ui::rgba(22, 19, 36, 235);
        Ui::button(bx, by, bw, bh, 0.20f, bg, base, sel);

        // Pastilla indicadora de atajo [A]
        Ui::pill(bx + 8.0f, by + 6.0f, 22.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(38, 32, 55, 240));
        drawCenteredText(ctx.textBuf, bx + 19.0f, by + 14.5f, 0.28f, sel ? Ui::rgba(0, 0, 0, 245) : ctx.clrWhite, "A", 0.28f);

        // Titulo de la opcion del menu con sombra nitida
        drawTextWithShadow(ctx.textBuf, bx + 36.0f, by + 6.0f, 0.42f, ctx.clrWhite, menuLabels[i], 0.28f);

        // Pastilla de categoria / modo a la derecha estilo multijugador
        Ui::pill(bx + bw - 74.0f, by + 6.0f, 66.0f, 18.0f, 0.24f, sel ? base : Ui::rgba(32, 26, 48, 220));
        drawCenteredText(ctx.textBuf, bx + bw - 41.0f, by + 14.5f, 0.27f, sel ? Ui::rgba(0, 0, 0, 245) : Ui::rgba(215, 210, 235), menuSubPills[i], 0.28f);
    }

    Ui::panel(10.0f, 186.0f, 300.0f, 46.0f, 0.20f, Ui::rgba(20, 18, 34, 240), ctx.clr300);
    char summary1[96];
    std::snprintf(summary1, sizeof(summary1), "%s: %s  •  Area: %d%%  •  Offset: %+dms",
        LOC(STR_MODE),
        ctx.settings.getShortInputModeLabel(),
        static_cast<int>(std::round(ctx.settings.tabletScale * 100.0f)),
        ctx.settings.audioOffsetMs);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 199.0f, 0.35f, ctx.clr300, summary1, 0.25f);

    std::string skinLine = "Skin: " + truncateString(activeSkin.name, 22) + "  •  Mods: " + formatModsString(ctx.settings.activeMods);
    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 217.0f, 0.35f, ctx.clrCursor, skinLine.c_str(), 0.25f);
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
