#include "SettingsScreen.hpp"

void runSettingsScreen(AppContext& ctx) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    ctx.audio.update();
    if (!ctx.calibrationMode && ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }
    auto getTabForOption = [](int opt) -> int {
        if (opt >= 0 && opt <= 3) return 0;
        if (opt >= 4 && opt <= 7) return 1;
        if (opt >= 8 && opt <= 11) return 2;
        if ((opt >= 12 && opt <= 15) || opt == 20 || opt == 22) return 3;
        if ((opt >= 16 && opt <= 19) || opt == 21) return 4;
        return 0;
    };
    const int tabItems[5][6] = {
        { 0, 1, 2, 3, -1, -1 },
        { 4, 5, 6, 7, -1, -1 },
        { 8, 9, 10, 11, -1, -1 },
        { 22, 12, 13, 14, 15, 20 },
        { 16, 17, 18, 19, 21, -1 }
    };
    const int tabItemCounts[5] = { 4, 4, 4, 6, 5 };
    int activeTab = getTabForOption(ctx.settingsOption); // 0: TABLET, 1: CONTROLES, 2: AUDIO, 3: JUEGO, 4: VISUAL

    if (ctx.capturingKeyIndex != -1) {
        u32 anyKey = ctx.kDown & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_SELECT | KEY_START);
        if (ctx.touchJustDown) {
            u32 touchedKey = 0;
            if (activeTab == 1) {
                if (ctx.touch.px >= 28 && ctx.touch.px <= 70 && ctx.touch.py >= 68 && ctx.touch.py <= 84) touchedKey = KEY_L;
                else if (ctx.touch.px >= 250 && ctx.touch.px <= 292 && ctx.touch.py >= 68 && ctx.touch.py <= 84) touchedKey = KEY_R;
                else if (ctx.touch.px >= 72 && ctx.touch.px <= 100 && ctx.touch.py >= 68 && ctx.touch.py <= 82) touchedKey = KEY_ZL;
                else if (ctx.touch.px >= 220 && ctx.touch.px <= 248 && ctx.touch.py >= 68 && ctx.touch.py <= 82) touchedKey = KEY_ZR;
                else if (ctx.touch.px >= 53 && ctx.touch.px <= 67 && ctx.touch.py >= 135 && ctx.touch.py <= 150) touchedKey = KEY_DUP;
                else if (ctx.touch.px >= 53 && ctx.touch.px <= 67 && ctx.touch.py >= 155 && ctx.touch.py <= 170) touchedKey = KEY_DDOWN;
                else if (ctx.touch.px >= 42 && ctx.touch.px <= 55 && ctx.touch.py >= 145 && ctx.touch.py <= 162) touchedKey = KEY_DLEFT;
                else if (ctx.touch.px >= 63 && ctx.touch.px <= 78 && ctx.touch.py >= 145 && ctx.touch.py <= 162) touchedKey = KEY_DRIGHT;
                else if (std::hypot(ctx.touch.px - 258.0f, ctx.touch.py - 124.0f) <= 12.0f) touchedKey = KEY_X;
                else if (std::hypot(ctx.touch.px - 258.0f, ctx.touch.py - 156.0f) <= 12.0f) touchedKey = KEY_B;
                else if (std::hypot(ctx.touch.px - 242.0f, ctx.touch.py - 140.0f) <= 12.0f) touchedKey = KEY_Y;
                else if (std::hypot(ctx.touch.px - 274.0f, ctx.touch.py - 140.0f) <= 12.0f) touchedKey = KEY_A;
                else if (ctx.touch.px >= 118 && ctx.touch.px <= 140 && ctx.touch.py >= 174 && ctx.touch.py <= 188) touchedKey = KEY_SELECT;
                else if (ctx.touch.px >= 180 && ctx.touch.px <= 202 && ctx.touch.py >= 174 && ctx.touch.py <= 188) touchedKey = KEY_START;
            }

            if (touchedKey != 0) {
                anyKey = touchedKey;
            } else {
                ctx.capturingKeyIndex = -1;
                ctx.audio.playHitSound(0);
                return;
            }
        }

        if (anyKey) {
            if (anyKey & KEY_B) {
                ctx.capturingKeyIndex = -1;
                ctx.audio.playHitSound(0);
            } else {
                ctx.audio.playHitSound(2);
                int row = ctx.capturingKeyIndex - 4; // 0..3
                if (ctx.settingsControlsMode == MODE_STD) {
                    if (row == 0) ctx.settings.keyOsuK1 = anyKey;
                    else if (row == 1) ctx.settings.keyOsuK2 = anyKey;
                    else if (row == 2) ctx.settings.keyOsuSmoke = anyKey;
                    else if (row == 3) ctx.settings.keyQuickRetry = anyKey;
                } else if (ctx.settingsControlsMode == MODE_TAIKO) {
                    if (row == 0) ctx.settings.keyTaikoDonLeft = anyKey;
                    else if (row == 1) ctx.settings.keyTaikoDonRight = anyKey;
                    else if (row == 2) ctx.settings.keyTaikoKatsuLeft = anyKey;
                    else if (row == 3) ctx.settings.keyTaikoKatsuRight = anyKey;
                    ctx.settings.keyTaikoDon = ctx.settings.keyTaikoDonLeft | ctx.settings.keyTaikoDonRight;
                    ctx.settings.keyTaikoKatsu = ctx.settings.keyTaikoKatsuLeft | ctx.settings.keyTaikoKatsuRight;
                } else if (ctx.settingsControlsMode == MODE_CATCH) {
                    if (row == 0) ctx.settings.keyCatchLeft = anyKey;
                    else if (row == 1) ctx.settings.keyCatchRight = anyKey;
                    else if (row == 2) ctx.settings.keyCatchDash = anyKey;
                    else if (row == 3) ctx.settings.keyQuickRetry = anyKey;
                } else if (ctx.settingsControlsMode == MODE_MANIA) {
                    if (row == 0) ctx.settings.keyManiaCol1 = anyKey;
                    else if (row == 1) ctx.settings.keyManiaCol2 = anyKey;
                    else if (row == 2) ctx.settings.keyManiaCol3 = anyKey;
                    else if (row == 3) ctx.settings.keyManiaCol4 = anyKey;
                }
                ctx.capturingKeyIndex = -1;
                ctx.settings.save();
            }
        }
        return;
    } else if (ctx.kDown & KEY_B) {
        ctx.calibrationMode = false;
        ctx.settings.save();
        ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
        ctx.audio.playHitSound(0);
        ctx.appState = ctx.settingsReturnState;
        if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
        return;
    }

    if (ctx.capturingKeyIndex == -1 && (ctx.kDown & KEY_X)) {
        if (activeTab == 1) {
            ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
            ctx.audio.playHitSound(2);
        } else {
            ctx.calibrationMode = !ctx.calibrationMode;
            ctx.calibSamples.clear();
            ctx.calibStartTick = osGetTime();
            ctx.calibLastBeatIndex = -1;
            if (ctx.calibrationMode) ctx.audio.stopMusic();
            ctx.audio.playHitSound(2);
        }
    }

    if (ctx.capturingKeyIndex == -1 && (ctx.kDown & KEY_Y)) {
        if (activeTab == 1) {
            ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
            ctx.audio.playHitSound(2);
        } else {
            ctx.settings.resetTabletArea();
            ctx.audio.playHitSound(0);
        }
    }

    // Cambiar de categoría con gatillos L / R o ZL / ZR (5 categorías)
    if (activeTab != 1) {
        if (ctx.kDown & (KEY_L | KEY_ZL)) {
            activeTab = (activeTab + 4) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & (KEY_R | KEY_ZR)) {
            activeTab = (activeTab + 1) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ctx.audio.playHitSound(0);
        }
    } else {
        if (ctx.kDown & KEY_ZL) {
            activeTab = (activeTab + 4) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_ZR) {
            activeTab = (activeTab + 1) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ctx.audio.playHitSound(0);
        }
    }

    // Tocar pestañas superiores o botones inferiores en pantalla táctil
    if (ctx.touchJustDown && !ctx.calibrationMode && ctx.capturingKeyIndex == -1) {
        if (ctx.touch.py >= 6 && ctx.touch.py <= 30) {
            for (int t = 0; t < 5; ++t) {
                float tx = 7.0f + t * 62.0f;
                if (ctx.touch.px >= tx && ctx.touch.px <= tx + 58.0f) {
                    activeTab = t;
                    ctx.settingsOption = tabItems[t][0];
                    ctx.audio.playHitSound(0);
                    break;
                }
            }
        } else if (ctx.touch.py >= 204 && ctx.touch.py <= 234) {
            if (ctx.touch.px >= 10 && ctx.touch.px <= 104) { // CALIBRAR (X) / RESTABLECER (Tab Teclas)
                if (activeTab == 1) {
                    ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
                    ctx.audio.playHitSound(2);
                } else {
                    ctx.calibrationMode = !ctx.calibrationMode;
                    ctx.calibSamples.clear();
                    ctx.calibStartTick = osGetTime();
                    ctx.calibLastBeatIndex = -1;
                    if (ctx.calibrationMode) ctx.audio.stopMusic();
                    ctx.audio.playHitSound(2);
                }
            } else if (ctx.touch.px >= 110 && ctx.touch.px <= 204) { // RESET (Y)
                if (activeTab == 1) {
                    ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
                    ctx.audio.playHitSound(2);
                } else {
                    ctx.settings.resetTabletArea();
                    ctx.audio.playHitSound(0);
                }
            } else if (ctx.touch.px >= 210 && ctx.touch.px <= 304) { // GUARDAR (B)
                ctx.calibrationMode = false;
                ctx.capturingKeyIndex = -1;
                ctx.settings.save();
                ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                ctx.audio.playHitSound(0);
                ctx.appState = ctx.settingsReturnState;
                if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
                return;
            }
        }
    }

    if (ctx.calibrationMode) {
        u64 elapsed = osGetTime() - ctx.calibStartTick;
        int beatIdx = static_cast<int>(elapsed / 500);
        if (beatIdx > ctx.calibLastBeatIndex) {
            ctx.calibLastBeatIndex = beatIdx;
            ctx.audio.playHitSound((beatIdx % 4 == 0) ? 2 : 0);
        }

        u32 tapKeys = KEY_A | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
        if (ctx.kDown & tapKeys) {
            int mod = static_cast<int>(elapsed % 500);
            int err = (mod > 250) ? (mod - 500) : mod;
            if (std::abs(err) <= 150) {
                ctx.calibSamples.push_back(err);
                if (ctx.calibSamples.size() > 12) ctx.calibSamples.erase(ctx.calibSamples.begin());
                long sum = 0;
                for (int s : ctx.calibSamples) sum += s;
                ctx.settings.audioOffsetMs = static_cast<int>(sum / static_cast<long>(ctx.calibSamples.size()));
                ctx.settings.clampValues();
            }
        }
    } else if (ctx.capturingKeyIndex == -1) {
        if (activeTab == 1) {
            // En pestaña TECLAS: D-Pad Arriba / Abajo cicla únicamente entre las 4 acciones sin salirse de la pestaña
            if (ctx.kDown & KEY_DUP) {
                ctx.settingsOption = (ctx.settingsOption <= 4) ? 7 : (ctx.settingsOption - 1);
                ctx.audio.playHitSound(0);
            }
            if (ctx.kDown & KEY_DDOWN) {
                ctx.settingsOption = (ctx.settingsOption >= 7) ? 4 : (ctx.settingsOption + 1);
                ctx.audio.playHitSound(0);
            }
            // D-Pad Izquierda / Derecha cambia de modo (STD / TAIKO / CATCH / MANIA)
            if (ctx.kDown & KEY_DLEFT) {
                ctx.settingsControlsMode = (ctx.settingsControlsMode + 3) % 4;
                ctx.audio.playHitSound(2);
            }
            if (ctx.kDown & KEY_DRIGHT) {
                ctx.settingsControlsMode = (ctx.settingsControlsMode + 1) % 4;
                ctx.audio.playHitSound(2);
            }
        } else {
            int curIdx = 0;
            int count = tabItemCounts[activeTab];
            for (int i = 0; i < count; ++i) {
                if (tabItems[activeTab][i] == ctx.settingsOption) {
                    curIdx = i;
                    break;
                }
            }
            if (ctx.kDown & KEY_DUP) {
                curIdx = (curIdx - 1 + count) % count;
                ctx.settingsOption = tabItems[activeTab][curIdx];
                ctx.audio.playHitSound(0);
            }
            if (ctx.kDown & KEY_DDOWN) {
                curIdx = (curIdx + 1) % count;
                ctx.settingsOption = tabItems[activeTab][curIdx];
                ctx.audio.playHitSound(0);
            }

            int stepDir = 0;
            if (ctx.kDown & KEY_DLEFT)  stepDir = -1;
            if (ctx.kDown & KEY_DRIGHT) stepDir = +1;

            if (stepDir != 0) {
                int numSkins = std::max(1, static_cast<int>(ctx.skinMgr.skins.size()));
                switch (ctx.settingsOption) {
                    case 0: // Modo de Entrada
                        ctx.settings.cycleInputMode(stepDir);
                        break;
                    case 1: // Tamaño Área Tablet
                        ctx.settings.tabletScale += stepDir * 0.05f;
                        break;
                    case 2: // Posición Horizontal X
                        ctx.settings.tabletCenterX += stepDir * 8.0f;
                        break;
                    case 3: // Posición Vertical Y
                        ctx.settings.tabletCenterY += stepDir * 8.0f;
                        break;
                case 8: // Volumen Música
                    ctx.settings.musicVolume += stepDir * 5;
                    ctx.settings.clampValues();
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                    break;
                case 9: // Volumen Hitsounds
                    ctx.settings.sfxVolume += stepDir * 5;
                    ctx.settings.clampValues();
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                    ctx.audio.playHitSound(2);
                    break;
                case 10: // Offset de Audio
                    ctx.settings.audioOffsetMs += stepDir * ((ctx.kHeld & (KEY_L | KEY_R)) ? 5 : 1);
                    break;
                case 11: // Hitsounds Propios del Beatmap
                    ctx.settings.beatmapHitsounds = !ctx.settings.beatmapHitsounds;
                    break;
                case 22: // Idioma / Language
                    ctx.settings.language = (ctx.settings.language + stepDir + LANG_COUNT) % LANG_COUNT;
                    Localization::setLanguage(static_cast<AppLanguage>(ctx.settings.language));
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                    break;
                case 12: // Modo de Juego (STD / TAIKO / CATCH / MANIA)
                    ctx.settings.gameMode = static_cast<GameMode>((ctx.settings.gameMode + stepDir + 4) % 4);
                    break;
                case 13: // Skin Activo (.osk / Presets)
                    ctx.settings.skinIndex = (ctx.settings.skinIndex + stepDir + numSkins) % numSkins;
                    ctx.applyActiveSkinAudio();
                    ctx.audio.playHitSound(2);
                    break;
                case 14: // Sliders Serpiente (Snaking Sliders)
                    ctx.settings.snakingSliders = !ctx.settings.snakingSliders;
                    break;
                case 15: // Combo Bursts (Mascota)
                    ctx.settings.comboBursts = !ctx.settings.comboBursts;
                    break;
                case 20: // Efecto Visual Golpes 300
                    ctx.settings.showHit300 = !ctx.settings.showHit300;
                    break;
                case 16: // Fondo en Modo Taiko (Festivo / Imagen / Video)
                    ctx.settings.taikoBgMode = (ctx.settings.taikoBgMode + stepDir + 3) % 3;
                    break;
                case 17: // Tamaño del Cursor
                    ctx.settings.cursorScale += stepDir * 0.1f;
                    break;
                case 18: // Atenuación del Fondo (Dim %)
                    ctx.settings.bgDim += stepDir * 5;
                    break;
                case 19: // Videos de Fondo (.3dsv)
                    ctx.settings.enableVideo = !ctx.settings.enableVideo;
                    break;
                case 21: // Descargas en Multijugador
                    ctx.settings.multiDlOption = (ctx.settings.multiDlOption + stepDir + 2) % 2;
                    ctx.multiMgr.enableBgDownload = (ctx.settings.multiDlOption != 0);
                    break;
            }
            ctx.settings.clampValues();
        }

        if (ctx.kDown & KEY_A) {
            if (ctx.settingsOption == 0) ctx.settings.cycleInputMode(1);
            else if (ctx.settingsOption >= 4 && ctx.settingsOption <= 7) {
                ctx.capturingKeyIndex = ctx.settingsOption;
                ctx.audio.playHitSound(2);
            }
            else if (ctx.settingsOption == 10) { // Abrir calibrador rápido
                ctx.calibrationMode = true;
                ctx.calibSamples.clear();
                ctx.calibStartTick = osGetTime();
                ctx.calibLastBeatIndex = -1;
                ctx.audio.stopMusic();
            }
            else if (ctx.settingsOption == 11) ctx.settings.beatmapHitsounds = !ctx.settings.beatmapHitsounds;
            else if (ctx.settingsOption == 22) {
                ctx.settings.language = (ctx.settings.language + 1) % LANG_COUNT;
                Localization::setLanguage(static_cast<AppLanguage>(ctx.settings.language));
                ctx.settings.save();
                ctx.audio.playHitSound(2);
            }
            else if (ctx.settingsOption == 12) {
                ctx.settings.gameMode = static_cast<GameMode>((ctx.settings.gameMode + 1) % 4);
                ctx.settings.save();
            }
            else if (ctx.settingsOption == 13) {
                ctx.settings.skinIndex = (ctx.settings.skinIndex + 1) % std::max(1, static_cast<int>(ctx.skinMgr.skins.size()));
                ctx.applyActiveSkinAudio();
            }
            else if (ctx.settingsOption == 14) ctx.settings.snakingSliders = !ctx.settings.snakingSliders;
            else if (ctx.settingsOption == 15) ctx.settings.comboBursts = !ctx.settings.comboBursts;
            else if (ctx.settingsOption == 20) {
                ctx.settings.showHit300 = !ctx.settings.showHit300;
                ctx.settings.save();
            }
            else if (ctx.settingsOption == 16) {
                ctx.settings.taikoBgMode = (ctx.settings.taikoBgMode + 1) % 3;
                ctx.settings.save();
            }
            else if (ctx.settingsOption == 19) ctx.settings.enableVideo = !ctx.settings.enableVideo;
            else if (ctx.settingsOption == 21) {
                ctx.settings.multiDlOption = (ctx.settings.multiDlOption + 1) % 2;
                ctx.multiMgr.enableBgDownload = (ctx.settings.multiDlOption != 0);
                ctx.settings.save();
            }
            ctx.audio.playHitSound(0);
        }
    }
}

    if (std::abs(ctx.cpad.dx) > 20 || std::abs(ctx.cpad.dy) > 20) {
        ctx.settings.tabletCenterX += (ctx.cpad.dx / 156.0f) * 3.0f;
        ctx.settings.tabletCenterY -= (ctx.cpad.dy / 156.0f) * 3.0f;
        ctx.settings.clampValues();
    }

    if (ctx.isTouching) {
        if (!ctx.settings.playOnBottomScreen && (ctx.settingsOption == 2 || ctx.settingsOption == 3 || (ctx.kHeld & KEY_L))) {
            ctx.settings.tabletCenterX = ctx.touch.px;
            ctx.settings.tabletCenterY = ctx.touch.py;
            ctx.settings.clampValues();
        }
        ctx.settings.mapTouchToOsu(ctx.touch.px, ctx.touch.py, ctx.cursorOsuX, ctx.cursorOsuY);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    float rx, ry, rw, rh;
    ctx.settings.getTabletRect(rx, ry, rw, rh);
    activeTab = getTabForOption(ctx.settingsOption);

    // --- PANTALLA SUPERIOR: TARJETAS REDISEÑADAS POR CATEGORÍA ESTILO OSU!LAZER ---
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dBg = depthShift(-3.5f);
        if (ctx.bgTex.loaded) {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), ctx.clrPrimary, 0.01f);
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 220) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, dimAlpha));
        } else {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(28, 18, 52), Ui::rgba(10, 8, 22), ctx.clrPrimary, 0.02f);
        }

        // Barra superior con pestañas de categoría
        Ui::gradV(0.0f, 0.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 245), Ui::rgba(18, 15, 32, 160));
        C2D_DrawRectSolid(0.0f, 27.0f, 0.31f, 400.0f, 1.5f, ctx.clrPrimary);

        const char* tabNames[5] = {
            LOC(STR_TAB_TABLET),
            LOC(STR_TAB_CONTROLS),
            LOC(STR_TAB_AUDIO),
            LOC(STR_TAB_GAME),
            LOC(STR_TAB_VISUAL)
        };
        for (int t = 0; t < 5; ++t) {
            float tx = 6.0f + t * 62.0f;
            bool isTabActive = (activeTab == t);
            if (isTabActive) {
                Ui::pill(tx, 4.0f, 58.0f, 19.0f, 0.32f, ctx.clrPrimary);
                drawCenteredTextWithShadow(ctx.textBuf, tx + 29.0f, 13.5f, 0.33f, ctx.clrWhite, tabNames[t], 0.85f);
            } else {
                Ui::pill(tx, 4.0f, 58.0f, 19.0f, 0.32f, Ui::rgba(42, 36, 64, 240));
                drawCenteredTextWithShadow(ctx.textBuf, tx + 29.0f, 13.5f, 0.31f, Ui::rgba(225, 220, 245), tabNames[t], 0.85f);
            }
        }
        Ui::pill(320.0f, 4.0f, 44.0f, 19.0f, 0.32f, ctx.isNew3DS ? Ui::rgba(40, 75, 45, 220) : Ui::rgba(35, 30, 50, 220));
        drawCenteredTextWithShadow(ctx.textBuf, 342.0f, 13.5f, 0.31f, ctx.isNew3DS ? ctx.clr100 : Ui::rgba(210, 205, 230), ctx.isNew3DS ? "N3DS" : "3DS", 0.85f);

        // Si la categoría activa es CONTROLES (activeTab == 1), mostramos la PREVISUALIZACIÓN VIVA DEL MODO
        if (activeTab == 1) {
            float panX = 12.0f + depthShift(1.8f);
            float panY = 32.0f;
            float panW = 376.0f;
            float panH = 178.0f;
            Ui::panel(panX, panY, panW, panH, 0.14f, Ui::rgba(18, 14, 28, 240), Ui::rgba(65, 52, 95));

            const char* subNames[4] = { "osu! (Standard)", "osu!taiko", "osu!catch", "osu!mania (4K)" };
            char mTitleBuf[64];
            std::snprintf(mTitleBuf, sizeof(mTitleBuf), "%s: %s", LOC(STR_PREVIEW), subNames[ctx.settingsControlsMode]);
            u32 mTitleColors[4] = {
                Ui::rgba(255, 102, 171),
                Ui::rgba(255, 75, 80),
                Ui::rgba(45, 215, 115),
                Ui::rgba(60, 205, 255)
            };

            drawTextWithShadow(ctx.textBuf, panX + 10.0f, panY + 5.0f, 0.42f, mTitleColors[ctx.settingsControlsMode], mTitleBuf, 0.16f);
            drawTextWithShadow(ctx.textBuf, panX + 10.0f, panY + 19.0f, 0.31f, Ui::rgba(200, 195, 225), LOC(STR_CTRL_HELP_LIVE), 0.16f);

            if (ctx.capturingKeyIndex != -1) {
                Ui::pill(panX + 180.0f, panY + 4.0f, 186.0f, 20.0f, 0.90f, Ui::rgba(255, 180, 40, 240));
                drawCenteredTextWithShadow(ctx.textBuf, panX + 273.0f, panY + 14.0f, 0.32f, ctx.clrBgDark, LOC(STR_CTRL_ASSIGNING), 0.95f);
            }

            if (ctx.settingsControlsMode == MODE_STD) {
                float hudX = panX + 10.0f;
                float hudY = panY + 34.0f;
                float hudW = 110.0f;

                bool k1Active = (ctx.kHeld & ctx.settings.keyOsuK1) || (ctx.animTick % 120 >= 56 && ctx.animTick % 120 <= 64);
                bool k2Active = (ctx.kHeld & ctx.settings.keyOsuK2) || (ctx.animTick % 120 >= 114 && ctx.animTick % 120 <= 122);
                bool smokeActive = (ctx.kHeld & ctx.settings.keyOsuSmoke);
                bool retryActive = (ctx.kHeld & ctx.settings.keyQuickRetry);

                auto drawKeyBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                    u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                    Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                    drawTextWithShadow(ctx.textBuf, x + 8.0f, y + 4.0f, 0.33f, active ? ctx.clrWhite : ctx.clrCursor, name, 0.20f);
                    drawTextWithShadow(ctx.textBuf, x + 8.0f, y + 16.0f, 0.30f, active ? ctx.clrWhite : Ui::rgba(225, 220, 245), keyStr, 0.20f);
                    if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                };

                std::string sK1 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyOsuK1), 14);
                std::string sK2 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyOsuK2), 14);
                std::string sSm = truncateString(GameSettings::formatKeyMask(ctx.settings.keyOsuSmoke), 14);
                std::string sRt = truncateString(GameSettings::formatKeyMask(ctx.settings.keyQuickRetry), 14);

                drawKeyBadge(hudX, hudY,        LOC(STR_KEY_K1), sK1.c_str(), k1Active, Ui::rgba(180, 70, 240));
                drawKeyBadge(hudX, hudY + 33.0f, LOC(STR_KEY_K2), sK2.c_str(), k2Active, Ui::rgba(255, 95, 180));
                drawKeyBadge(hudX, hudY + 66.0f, LOC(STR_KEY_SMOKE),  sSm.c_str(), smokeActive, Ui::rgba(240, 200, 50));
                drawKeyBadge(hudX, hudY + 99.0f, LOC(STR_KEY_RETRY),     sRt.c_str(), retryActive, Ui::rgba(255, 75, 75));

                float pfX = panX + 128.0f;
                float pfY = panY + 34.0f;
                float pfW = 238.0f;
                float pfH = 132.0f;
                Ui::panel(pfX, pfY, pfW, pfH, 0.15f, Ui::rgba(12, 10, 20, 240), Ui::rgba(55, 45, 80));

                int t = ctx.animTick % 120;
                float c1X = pfX + 65.0f, c1Y = pfY + 66.0f;
                float c2X = pfX + 172.0f, c2Y = pfY + 56.0f;

                if (t < 60) {
                    float appR = 16.0f + (1.0f - (t / 60.0f)) * 32.0f;
                    drawRing(c1X, c1Y, appR, 1.5f, Ui::rgba(255, 102, 171), 22, 0.22f);
                    C2D_DrawCircleSolid(c1X, c1Y, 0.20f, 16.0f, Ui::rgba(255, 102, 171));
                    drawRing(c1X, c1Y, 16.0f, 2.0f, ctx.clrWhite, 22, 0.21f);
                    drawCenteredText(ctx.textBuf, c1X, c1Y - 6.0f, 0.40f, ctx.clrWhite, "1", 0.23f);
                } else if (t < 78) {
                    float burstR = 16.0f + (t - 60) * 1.5f;
                    u8 bAlpha = static_cast<u8>(255 * (1.0f - (t - 60) / 18.0f));
                    drawRing(c1X, c1Y, burstR, 2.0f, Ui::rgba(255, 215, 60, bAlpha), 20, 0.24f);
                    drawCenteredText(ctx.textBuf, c1X, c1Y - 10.0f - (t - 60) * 0.8f, 0.42f, Ui::rgba(100, 240, 255, bAlpha), "300", 0.25f);
                }

                if (t >= 30) {
                    float p2 = std::min(1.0f, (t - 30) / 90.0f);
                    float appR = 16.0f + (1.0f - p2) * 32.0f;
                    drawRing(c2X, c2Y, appR, 1.5f, Ui::rgba(100, 180, 255), 22, 0.22f);
                    C2D_DrawCircleSolid(c2X, c2Y, 0.20f, 16.0f, Ui::rgba(100, 180, 255));
                    drawRing(c2X, c2Y, 16.0f, 2.0f, ctx.clrWhite, 22, 0.21f);
                    drawCenteredText(ctx.textBuf, c2X, c2Y - 6.0f, 0.40f, ctx.clrWhite, "2", 0.23f);
                }

                float curX, curY;
                if (t < 60) {
                    float prog = t / 60.0f;
                    float smooth = prog * prog * (3.0f - 2.0f * prog);
                    curX = pfX + 20.0f + smooth * (c1X - (pfX + 20.0f));
                    curY = pfY + 100.0f + smooth * (c1Y - (pfY + 100.0f));
                } else {
                    float prog = (t - 60) / 60.0f;
                    float smooth = prog * prog * (3.0f - 2.0f * prog);
                    curX = c1X + smooth * (c2X - c1X);
                    curY = c1Y + smooth * (c2Y - c1Y);
                }

                Ui::glow(curX, curY, 0.26f, 12.0f, Ui::withAlpha(ctx.clrCursor, 140), 2);
                C2D_DrawCircleSolid(curX, curY, 0.27f, 6.0f, ctx.clrCursor);
                C2D_DrawCircleSolid(curX, curY, 0.28f, 2.0f, ctx.clrWhite);
            }
            else if (ctx.settingsControlsMode == MODE_TAIKO) {
                float trkX = panX + 10.0f;
                float trkY = panY + 34.0f;
                float trkW = 356.0f;
                float trkH = 70.0f;
                Ui::panel(trkX, trkY, trkW, trkH, 0.15f, Ui::rgba(22, 18, 32, 250), Ui::rgba(70, 55, 95));

                float drumX = trkX + 46.0f;
                float drumY = trkY + trkH * 0.5f;

                bool kLeftDown  = (ctx.kHeld & ctx.settings.keyTaikoKatsuLeft);
                bool kRightDown = (ctx.kHeld & ctx.settings.keyTaikoKatsuRight);
                bool dLeftDown  = (ctx.kHeld & ctx.settings.keyTaikoDonLeft);
                bool dRightDown = (ctx.kHeld & ctx.settings.keyTaikoDonRight);

                C2D_DrawCircleSolid(drumX, drumY, 0.17f, 28.0f, (kLeftDown || kRightDown) ? ctx.clrWhite : Ui::rgba(35, 120, 220));
                C2D_DrawCircleSolid(drumX, drumY, 0.19f, 18.0f, (dLeftDown || dRightDown) ? ctx.clrWhite : Ui::rgba(240, 55, 65));

                C2D_DrawCircleSolid(drumX - 6.0f, drumY - 2.0f, 0.21f, 2.2f, ctx.clrBgDark);
                C2D_DrawCircleSolid(drumX + 6.0f, drumY - 2.0f, 0.21f, 2.2f, ctx.clrBgDark);
                drawRing(drumX, drumY + 4.0f, 4.0f, 1.2f, ctx.clrBgDark, 12, 0.21f);

                float targetX = trkX + 115.0f;
                drawRing(targetX, drumY, 15.0f, 2.0f, ctx.clrWhite, 24, 0.20f);
                drawRing(targetX, drumY, 8.0f, 1.5f, Ui::rgba(180, 175, 210), 16, 0.19f);

                int loopT = ctx.animTick % 160;
                float speed = 2.4f;

                for (int n = 0; n < 4; ++n) {
                    int nOffset = n * 40;
                    float notePos = ((loopT + nOffset) % 160) * speed;
                    float nx = (trkX + trkW) - notePos;
                    if (nx >= trkX + 85.0f && nx <= trkX + trkW - 5.0f) {
                        bool isDon = (n % 2 == 0);
                        u32 noteClr = isDon ? Ui::rgba(245, 60, 65) : Ui::rgba(45, 135, 255);
                        C2D_DrawCircleSolid(nx, drumY, 0.22f, 12.0f, noteClr);
                        drawRing(nx, drumY, 12.0f, 1.8f, ctx.clrWhite, 20, 0.23f);

                        if (std::abs(nx - targetX) < 6.0f) {
                            Ui::glow(targetX, drumY, 0.25f, 22.0f, Ui::withAlpha(noteClr, 160), 2);
                            drawCenteredText(ctx.textBuf, targetX, drumY - 24.0f, 0.38f, Ui::rgba(255, 220, 80), "GREAT 300", 0.26f);
                        }
                    }
                }

                float rowY = panY + 110.0f;
                float cW = 86.0f;
                float cH = 56.0f;

                auto drawTaikoCard = [&](float cx, const char* name, const char* sub, uint32_t mask, u32 actClr, bool active) {
                    Ui::panel(cx, rowY, cW, cH, 0.16f, active ? Ui::rgba(45, 38, 70, 255) : Ui::rgba(24, 20, 36, 230), active ? ctx.clrWhite : actClr);
                    drawCenteredText(ctx.textBuf, cx + cW * 0.5f, rowY + 5.0f, 0.28f, actClr, name, 0.18f);
                    drawCenteredText(ctx.textBuf, cx + cW * 0.5f, rowY + 18.0f, 0.24f, ctx.clrMuted, sub, 0.18f);
                    std::string kStr = truncateString(GameSettings::formatKeyMask(mask), 12);
                    Ui::pill(cx + 6.0f, rowY + 32.0f, cW - 12.0f, 18.0f, 0.19f, active ? actClr : Ui::rgba(40, 32, 60));
                    drawCenteredText(ctx.textBuf, cx + cW * 0.5f, rowY + 41.0f, 0.26f, ctx.clrWhite, kStr.c_str(), 0.20f);
                };

                drawTaikoCard(trkX,               "DON IZQ",   LOC(STR_TAIKO_RED), ctx.settings.keyTaikoDonLeft,    Ui::rgba(245, 60, 65),  dLeftDown);
                drawTaikoCard(trkX + cW + 4.0f,   "DON DER",   LOC(STR_TAIKO_RED), ctx.settings.keyTaikoDonRight,   Ui::rgba(245, 60, 65),  dRightDown);
                drawTaikoCard(trkX + (cW+4.0f)*2, "KATSU IZQ", LOC(STR_TAIKO_BLUE), ctx.settings.keyTaikoKatsuLeft,  Ui::rgba(45, 135, 255), kLeftDown);
                drawTaikoCard(trkX + (cW+4.0f)*3, "KATSU DER", LOC(STR_TAIKO_BLUE), ctx.settings.keyTaikoKatsuRight, Ui::rgba(45, 135, 255), kRightDown);
            }
            else if (ctx.settingsControlsMode == MODE_CATCH) {
                float hudX = panX + 10.0f;
                float hudY = panY + 34.0f;
                float hudW = 110.0f;

                bool cLeftActive  = (ctx.kHeld & ctx.settings.keyCatchLeft);
                bool cRightActive = (ctx.kHeld & ctx.settings.keyCatchRight);
                bool cDashActive  = (ctx.kHeld & ctx.settings.keyCatchDash);
                bool cRetryActive = (ctx.kHeld & ctx.settings.keyQuickRetry);

                auto drawCatchBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                    u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                    Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                    drawText(ctx.textBuf, x + 8.0f, y + 4.0f, 0.30f, active ? ctx.clrWhite : ctx.clrCursor, name, 0.20f);
                    drawText(ctx.textBuf, x + 8.0f, y + 16.0f, 0.26f, active ? ctx.clrWhite : ctx.clrMuted, keyStr, 0.20f);
                    if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                };

                std::string sL = truncateString(GameSettings::formatKeyMask(ctx.settings.keyCatchLeft), 13);
                std::string sR = truncateString(GameSettings::formatKeyMask(ctx.settings.keyCatchRight), 13);
                std::string sD = truncateString(GameSettings::formatKeyMask(ctx.settings.keyCatchDash), 13);
                std::string sRt = truncateString(GameSettings::formatKeyMask(ctx.settings.keyQuickRetry), 13);

                drawCatchBadge(hudX, hudY,        LOC(STR_KEY_MOVE_L), sL.c_str(), cLeftActive, Ui::rgba(50, 210, 120));
                drawCatchBadge(hudX, hudY + 33.0f, LOC(STR_KEY_MOVE_R), sR.c_str(), cRightActive, Ui::rgba(50, 210, 120));
                drawCatchBadge(hudX, hudY + 66.0f, LOC(STR_KEY_DASH),   sD.c_str(), cDashActive, Ui::rgba(255, 175, 30));
                drawCatchBadge(hudX, hudY + 99.0f, LOC(STR_KEY_RETRY),  sRt.c_str(), cRetryActive, Ui::rgba(255, 75, 75));

                float stX = panX + 128.0f;
                float stY = panY + 34.0f;
                float stW = 238.0f;
                float stH = 132.0f;
                Ui::panel(stX, stY, stW, stH, 0.15f, Ui::rgba(12, 10, 20, 240), Ui::rgba(50, 150, 95));

                int cTick = ctx.animTick % 100;
                float f1Y = stY + (cTick * 1.3f);
                float f1X = stX + 70.0f;

                if (f1Y < stY + stH - 18.0f) {
                    C2D_DrawCircleSolid(f1X, f1Y, 0.20f, 9.0f, Ui::rgba(235, 50, 60));
                    C2D_DrawRectSolid(f1X - 1.0f, f1Y - 12.0f, 0.21f, 2.0f, 4.0f, Ui::rgba(80, 180, 50));
                }

                int cTick2 = (ctx.animTick + 50) % 100;
                float f2Y = stY + (cTick2 * 1.3f);
                float f2X = stX + 165.0f;
                if (f2Y < stY + stH - 18.0f) {
                    C2D_DrawCircleSolid(f2X, f2Y, 0.20f, 8.5f, Ui::rgba(255, 160, 30));
                    drawRing(f2X, f2Y, 5.0f, 1.2f, Ui::rgba(255, 210, 80), 12, 0.21f);
                }

                float catcherBaseX = stX + 70.0f + std::sin(ctx.animTick * 0.05f) * 60.0f;
                if (cLeftActive)  catcherBaseX = std::max(stX + 25.0f, catcherBaseX - 3.0f);
                if (cRightActive) catcherBaseX = std::min(stX + stW - 25.0f, catcherBaseX + 3.0f);

                float plateY = stY + stH - 16.0f;
                float plateW = 44.0f;

                if (cDashActive || (cTick > 40 && cTick < 70)) {
                    Ui::pill(catcherBaseX - plateW * 0.5f - 6.0f, plateY, plateW, 6.0f, 0.18f, Ui::rgba(255, 120, 40, 120));
                }
                Ui::pill(catcherBaseX - plateW * 0.5f, plateY, plateW, 6.0f, 0.22f, Ui::rgba(210, 215, 235));
                C2D_DrawCircleSolid(catcherBaseX, plateY + 8.0f, 0.21f, 6.0f, Ui::rgba(60, 180, 100));
            }
            else if (ctx.settingsControlsMode == MODE_MANIA) {
                float hudX = panX + 10.0f;
                float hudY = panY + 34.0f;
                float hudW = 110.0f;

                bool col1Active = (ctx.kHeld & ctx.settings.keyManiaCol1) || (ctx.animTick % 80 >= 18 && ctx.animTick % 80 <= 26);
                bool col2Active = (ctx.kHeld & ctx.settings.keyManiaCol2) || (ctx.animTick % 80 >= 38 && ctx.animTick % 80 <= 46);
                bool col3Active = (ctx.kHeld & ctx.settings.keyManiaCol3) || (ctx.animTick % 80 >= 58 && ctx.animTick % 80 <= 66);
                bool col4Active = (ctx.kHeld & ctx.settings.keyManiaCol4) || (ctx.animTick % 80 >= 72 && ctx.animTick % 80 <= 79);

                auto drawManiaBadge = [&](float x, float y, const char* name, const char* keyStr, bool active, u32 actClr) {
                    u32 bg = active ? actClr : Ui::rgba(28, 22, 44, 230);
                    Ui::pill(x, y, hudW, 29.0f, 0.18f, bg);
                    drawText(ctx.textBuf, x + 8.0f, y + 4.0f, 0.30f, active ? ctx.clrWhite : ctx.clrCursor, name, 0.20f);
                    drawText(ctx.textBuf, x + 8.0f, y + 16.0f, 0.26f, active ? ctx.clrWhite : ctx.clrMuted, keyStr, 0.20f);
                    if (active) Ui::glow(x + hudW * 0.5f, y + 14.0f, 0.17f, 16.0f, Ui::withAlpha(actClr, 130), 2);
                };

                std::string sC1 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyManiaCol1), 13);
                std::string sC2 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyManiaCol2), 13);
                std::string sC3 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyManiaCol3), 13);
                std::string sC4 = truncateString(GameSettings::formatKeyMask(ctx.settings.keyManiaCol4), 13);

                char colBuf[4][32];
                for (int c = 0; c < 4; ++c) {
                    std::snprintf(colBuf[c], sizeof(colBuf[c]), LOC(STR_KEY_COL_FMT), c + 1);
                }
                drawManiaBadge(hudX, hudY,        colBuf[0], sC1.c_str(), col1Active, Ui::rgba(50, 160, 255));
                drawManiaBadge(hudX, hudY + 33.0f, colBuf[1], sC2.c_str(), col2Active, Ui::rgba(235, 235, 245));
                drawManiaBadge(hudX, hudY + 66.0f, colBuf[2], sC3.c_str(), col3Active, Ui::rgba(235, 235, 245));
                drawManiaBadge(hudX, hudY + 99.0f, colBuf[3], sC4.c_str(), col4Active, Ui::rgba(50, 160, 255));

                float mStageX = panX + 158.0f;
                float mStageY = panY + 34.0f;
                float colW = 44.0f;
                float mStageW = colW * 4.0f;
                float mStageH = 132.0f;

                Ui::panel(mStageX, mStageY, mStageW, mStageH, 0.15f, Ui::rgba(14, 12, 22, 250), Ui::rgba(60, 120, 180));

                for (int c = 1; c < 4; ++c) {
                    float lx = mStageX + c * colW;
                    C2D_DrawLine(lx, mStageY, Ui::rgba(45, 40, 65), lx, mStageY + mStageH, Ui::rgba(45, 40, 65), 1.0f, 0.16f);
                }

                float judgeY = mStageY + mStageH - 24.0f;
                C2D_DrawRectSolid(mStageX, judgeY, 0.18f, mStageW, 2.5f, ctx.clrWhite);

                bool colActs[4] = { col1Active, col2Active, col3Active, col4Active };
                u32 colClrs[4] = { Ui::rgba(50, 160, 255), Ui::rgba(240, 240, 255), Ui::rgba(240, 240, 255), Ui::rgba(50, 160, 255) };

                for (int c = 0; c < 4; ++c) {
                    float cx = mStageX + c * colW;
                    if (colActs[c]) {
                        C2D_DrawRectSolid(cx + 2.0f, judgeY + 2.5f, 0.19f, colW - 4.0f, 20.0f, colClrs[c]);
                        Ui::glow(cx + colW * 0.5f, judgeY, 0.20f, 16.0f, Ui::withAlpha(colClrs[c], 140), 2);
                    } else {
                        C2D_DrawRectSolid(cx + 2.0f, judgeY + 2.5f, 0.19f, colW - 4.0f, 20.0f, Ui::rgba(30, 26, 45));
                    }
                }

                int mTick = ctx.animTick % 80;
                float noteSpd = 1.6f;

                auto drawManiaNote = [&](int col, int offset) {
                    float ny = mStageY + (((mTick + offset) % 80) * noteSpd);
                    if (ny < judgeY) {
                        float nx = mStageX + col * colW + 3.0f;
                        C2D_DrawRectSolid(nx, ny, 0.22f, colW - 6.0f, 8.0f, colClrs[col]);
                        drawRectOutline(nx, ny, colW - 6.0f, 8.0f, 1.0f, ctx.clrWhite, 0.23f);
                    }
                };

                drawManiaNote(0, 60);
                drawManiaNote(1, 40);
                drawManiaNote(2, 20);
                drawManiaNote(3, 5);
            }
        } else {
            // Renderizado de las tarjetas de la categoría activa (Tabs 0, 2, 3, 4)
            int count = tabItemCounts[activeTab];
            float ch = (count == 6) ? 27.0f : ((count == 5) ? 31.0f : 38.0f);
            float step = (count == 6) ? 29.5f : ((count == 5) ? 34.0f : 44.0f);
            float startY = (count == 6) ? 31.0f : ((count == 5) ? 32.0f : 34.0f);
            float titleYOff = (count == 6) ? 1.5f : ((count == 5) ? 2.5f : 4.0f);
            float descYOff  = (count == 6) ? 14.5f : ((count == 5) ? 16.5f : 20.0f);
            float ctrlYOff  = (count == 6) ? 4.0f : ((count == 5) ? 5.5f : 8.0f);
            float swYOff    = (count == 6) ? 4.5f : ((count == 5) ? 6.5f : 11.0f);
            float sldYOff   = (count == 6) ? 8.5f : ((count == 5) ? 10.5f : 14.0f);

            for (int i = 0; i < count; ++i) {
                int optIdx = tabItems[activeTab][i];
                bool sel = (!ctx.calibrationMode && ctx.settingsOption == optIdx);
                float dCard = depthShift(sel ? 3.5f : 1.2f);
                float cy = startY + i * step;
                float cw = 376.0f;
                float cx = 12.0f + dCard;

                u32 cardBg = sel ? Ui::rgba(38, 30, 64, 245) : Ui::rgba(22, 19, 36, 225);
                Ui::panel(cx, cy, cw, ch, 0.15f, cardBg, sel ? ctx.clrPrimary : 0);
                if (sel) {
                    C2D_DrawRectSolid(cx, cy, 0.22f, 3.5f, ch, ctx.clrCursor);
                    C2D_DrawLine(cx + 4.0f, cy, ctx.clrWhite, cx + cw, cy, ctx.clrWhite, 1.0f, 0.22f);
                }

                // Título y valor según opción
                if (optIdx == 0) { // Modo de Entrada
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_INPUT_MODE));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_INPUT_MODE_DESC));
                    Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, ctx.settings.playOnBottomScreen ? Ui::rgba(35, 110, 75) : Ui::rgba(55, 45, 95));
                    drawCenteredText(ctx.textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, ctx.clrWhite, ctx.settings.getInputModeLabel(), 0.85f);
                } else if (optIdx == 1) { // Tamaño Área Tablet
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_TABLET_SIZE));
                    char desc[64];
                    std::snprintf(desc, sizeof(desc), "%d%% (%dx%d px)", static_cast<int>(std::round(ctx.settings.tabletScale * 100.0f)), (int)rw, (int)rh);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = (ctx.settings.tabletScale - 0.30f) / 0.70f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 2) { // Posición X
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_POS_X));
                    char desc[64]; std::snprintf(desc, sizeof(desc), "X = %.0f px  (C-Pad / Stylus)", ctx.settings.tabletCenterX);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = ctx.settings.tabletCenterX / 320.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 3) { // Posición Y
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_POS_Y));
                    char desc[64]; std::snprintf(desc, sizeof(desc), "Y = %.0f px  (C-Pad / Stylus)", ctx.settings.tabletCenterY);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = ctx.settings.tabletCenterY / 240.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 8) { // Volumen Música
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_MUSIC_VOL));
                    char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.musicVolume);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = ctx.settings.musicVolume / 100.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 9) { // Volumen Hitsounds
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_SFX_VOL));
                    char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.sfxVolume);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = ctx.settings.sfxVolume / 100.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 10) { // Offset de Audio
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_OFFSET));
                    char desc[64]; std::snprintf(desc, sizeof(desc), LOC(STR_OFFSET_CALIB_HINT), ctx.settings.audioOffsetMs);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr100, desc);
                    float ratio = (ctx.settings.audioOffsetMs + 150) / 300.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clr300, sel);
                } else if (optIdx == 11) { // Hitsounds del Beatmap
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_MAP_HITSOUNDS));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_MAP_HITSOUNDS_DESC));
                    Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, ctx.settings.beatmapHitsounds, ctx.clr100);
                } else if (optIdx == 22) { // Idioma / Language
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_LANGUAGE));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_LANGUAGE_DESC));
                    u32 lBg = Ui::rgba(45, 115, 215);
                    Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, lBg);
                    drawCenteredText(ctx.textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, ctx.clrWhite, ctx.settings.getLanguageName(), 0.85f);
                } else if (optIdx == 12) { // Modo de Juego
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_MAIN_MODE));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_MAIN_MODE_DESC));
                    u32 mBg = (ctx.settings.gameMode == MODE_TAIKO) ? Ui::rgba(180, 45, 45) : ((ctx.settings.gameMode == MODE_CATCH) ? Ui::rgba(35, 160, 85) : ((ctx.settings.gameMode == MODE_MANIA) ? Ui::rgba(140, 35, 175) : Ui::rgba(65, 45, 115)));
                    Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, (count == 6) ? 19.0f : 22.0f, 0.25f, mBg);
                    drawCenteredText(ctx.textBuf, cx + 285.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.36f, ctx.clrWhite, ctx.settings.getGameModeName(), 0.85f);
                } else if (optIdx == 13) { // Skin Activo
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_ACTIVE_SKIN));
                    char desc[64]; std::snprintf(desc, sizeof(desc), "%s (%d/%d)", truncateString(activeSkin.name, 22).c_str(), ctx.settings.skinIndex + 1, (int)ctx.skinMgr.skins.size());
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    for (size_t c = 0; c < std::min((size_t)4, activeSkin.comboColors.size()); ++c) {
                        u32 cc = C2D_Color32(activeSkin.comboColors[c].r, activeSkin.comboColors[c].g, activeSkin.comboColors[c].b, 255);
                        C2D_DrawCircleSolid(cx + 276.0f + c * 20.0f, cy + ctrlYOff + ((count == 6) ? 9.5f : 11.0f), 0.25f, 6.5f, cc);
                    }
                } else if (optIdx == 14) { // Sliders Serpiente (Snaking)
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_SNAKING));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_SNAKING_DESC));
                    Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, ctx.settings.snakingSliders, ctx.clr100);
                } else if (optIdx == 15) { // Combo Bursts (Mascota)
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_BURSTS));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_BURSTS_DESC));
                    Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, ctx.settings.comboBursts, ctx.clr100);
                } else if (optIdx == 20) { // Efecto Visual Golpes 300
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_HIT300));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_HIT300_DESC));
                    Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, ctx.settings.showHit300, ctx.clr100);
                } else if (optIdx == 16) { // Fondo Taiko
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_TAIKO_BG));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_TAIKO_BG_DESC));
                    u32 bClr = (ctx.settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25) : ((ctx.settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(165, 55, 185));
                    Ui::pill(cx + 205.0f, cy + ctrlYOff, 160.0f, 22.0f, 0.25f, bClr);
                    drawCenteredText(ctx.textBuf, cx + 285.0f, cy + ctrlYOff + 11.0f, 0.34f, ctx.clrWhite, ctx.settings.getTaikoBgModeName(), 0.85f);
                } else if (optIdx == 17) { // Tamaño del Cursor
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_CURSOR_SIZE));
                    char desc[32]; std::snprintf(desc, sizeof(desc), "%.1fx", ctx.settings.cursorScale);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = (ctx.settings.cursorScale - 0.5f) / 1.5f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 18) { // Oscurecimiento Fondo
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_BG_DIM));
                    char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.bgDim);
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clr300, desc);
                    float ratio = (ctx.settings.bgDim - 20) / 80.0f;
                    Ui::sliderBar(cx + 230.0f, cy + sldYOff, 134.0f, 10.0f, 0.25f, ratio, sel ? ctx.clrCursor : ctx.clrPrimary, sel);
                } else if (optIdx == 19) { // Videos de Fondo
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_VIDEOS));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_VIDEOS_DESC));
                    Ui::toggleSwitch(cx + 312.0f, cy + swYOff, 48.0f, 18.0f, 0.25f, ctx.settings.enableVideo, ctx.clr100);
                } else if (optIdx == 21) { // Descargas Multijugador
                    drawText(ctx.textBuf, cx + 12.0f, cy + titleYOff, 0.44f, sel ? ctx.clrWhite : ctx.clrCursor, LOC(STR_SET_MULTI_DL));
                    drawText(ctx.textBuf, cx + 12.0f, cy + descYOff, 0.35f, ctx.clrMuted, LOC(STR_SET_MULTI_DL_DESC));
                    u32 dClr = (ctx.settings.multiDlOption == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(110, 95, 140);
                    Ui::pill(cx + 185.0f, cy + ctrlYOff, 180.0f, 22.0f, 0.25f, dClr);
                    drawCenteredText(ctx.textBuf, cx + 275.0f, cy + ctrlYOff + 11.0f, 0.34f, ctx.clrWhite, ctx.settings.getMultiDlOptionName(), 0.85f);
                }
            }
        }

        if (ctx.capturingKeyIndex != -1 && activeTab != 1) {
            float dCap = eyeSign * ctx.slider3D * 5.0f;
            C2D_DrawRectSolid(0.0f, 0.0f, 0.92f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 215));
            Ui::panel(40.0f + dCap, 50.0f, 320.0f, 130.0f, 0.94f, Ui::rgba(26, 20, 44, 250), ctx.clrCursor);
            int cIdx = std::max(0, std::min(3, ctx.capturingKeyIndex - 4));
            const char* capModeName = (ctx.settingsControlsMode == MODE_TAIKO) ? "TAIKO" : ((ctx.settingsControlsMode == MODE_CATCH) ? "CATCH" : ((ctx.settingsControlsMode == MODE_MANIA) ? "MANIA" : "STANDARD"));
            const char* capRowName = "CONTROL";
            char maniaCap[4][32];
            if (ctx.settingsControlsMode == MODE_STD) {
                const char* n[4] = { LOC(STR_KEY_K1), LOC(STR_KEY_K2), LOC(STR_KEY_SMOKE), LOC(STR_KEY_RETRY) };
                capRowName = n[cIdx];
            } else if (ctx.settingsControlsMode == MODE_TAIKO) {
                const char* n[4] = { LOC(STR_KEY_DON_L), LOC(STR_KEY_DON_R), LOC(STR_KEY_KAT_L), LOC(STR_KEY_KAT_R) };
                capRowName = n[cIdx];
            } else if (ctx.settingsControlsMode == MODE_CATCH) {
                const char* n[4] = { LOC(STR_KEY_MOVE_L), LOC(STR_KEY_MOVE_R), LOC(STR_KEY_DASH), LOC(STR_KEY_RETRY) };
                capRowName = n[cIdx];
            } else if (ctx.settingsControlsMode == MODE_MANIA) {
                for (int c = 0; c < 4; ++c) {
                    std::snprintf(maniaCap[c], sizeof(maniaCap[c]), LOC(STR_KEY_COL_FMT), c + 1);
                }
                capRowName = maniaCap[cIdx];
            }
            char capHead[64];
            std::snprintf(capHead, sizeof(capHead), "%s [%s]", LOC(STR_CTRL_ASSIGNING), capModeName);
            drawCenteredText(ctx.textBuf, 200.0f + dCap, 72.0f, 0.48f, ctx.clrCursor, capHead, 0.99f);
            drawCenteredText(ctx.textBuf, 200.0f + dCap, 96.0f, 0.44f, ctx.clrWhite, capRowName, 0.99f);
            drawCenteredText(ctx.textBuf, 200.0f + dCap, 126.0f, 0.38f, ctx.clr100, LOC(STR_CTRL_PRESS_ANY), 0.99f);
            drawCenteredText(ctx.textBuf, 200.0f + dCap, 152.0f, 0.32f, ctx.clrMuted, LOC(STR_CTRL_CANCEL_HINT), 0.99f);
        }

        if (ctx.calibrationMode) {
            float dCal = eyeSign * ctx.slider3D * 4.5f;
            C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 185));
            Ui::panel(30.0f + dCal, 52.0f, 340.0f, 128.0f, 0.90f, Ui::rgba(22, 18, 38, 250), ctx.clr300);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 70.0f, 0.50f, ctx.clrCursor, LOC(STR_CALIB_TITLE), 0.98f);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 94.0f, 0.40f, ctx.clrWhite, LOC(STR_CALIB_DESC), 0.98f);

            u64 elapsed = osGetTime() - ctx.calibStartTick;
            float beatPhase = (elapsed % 500) / 500.0f;
            float pulseR = 14.0f * (1.0f - beatPhase * 0.55f);
            Ui::ring(200.0f + dCal, 122.0f, 0.96f, pulseR, 2.0f, ctx.clr300, 20);

            char calStr[96];
            std::snprintf(calStr, sizeof(calStr), LOC(STR_CALIB_STATUS_FMT),
                static_cast<int>(ctx.calibSamples.size()), ctx.settings.audioOffsetMs);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 146.0f, 0.44f, ctx.clr100, calStr, 0.98f);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 166.0f, 0.38f, ctx.clr300, LOC(STR_CALIB_APPLY_HINT), 0.98f);
        }

        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(18, 15, 32, 170), Ui::rgba(18, 15, 32, 250));
        drawText(ctx.textBuf, 8.0f, 219.0f, 0.37f, ctx.clr300, LOC(STR_SET_HELP_BAR));

        float testR = 6.0f * ctx.settings.cursorScale;
        float dCur = eyeSign * ctx.slider3D * 4.5f;
        Ui::glow(osuToTopX(ctx.cursorOsuX) + dCur, osuToTopY(ctx.cursorOsuY), 0.94f, testR * 1.9f, Ui::withAlpha(ctx.clrCursor, 110), 3);
        C2D_DrawCircleSolid(osuToTopX(ctx.cursorOsuX) + dCur, osuToTopY(ctx.cursorOsuY), 0.95f, testR, ctx.clrCursor);
        C2D_DrawCircleSolid(osuToTopX(ctx.cursorOsuX) + dCur, osuToTopY(ctx.cursorOsuY), 0.97f, 2.0f, ctx.clrWhite);
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // --- PANTALLA INFERIOR: CONTROL TÁCTIL INTERACTIVO + EDITOR DE ÁREA ---
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), ctx.clrPrimary, 0.02f);
    if (ctx.bgTex.loaded) {
        ctx.bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 60);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 180));
    }

    // Botones de pestañas táctiles arriba (5 pestañas)
    const char* tabNames[5] = {
        LOC(STR_TAB_TABLET),
        LOC(STR_TAB_CONTROLS),
        LOC(STR_TAB_AUDIO),
        LOC(STR_TAB_GAME),
        LOC(STR_TAB_VISUAL)
    };
    for (int t = 0; t < 5; ++t) {
        float tx = 7.0f + t * 62.0f;
        float tw = 58.0f;
        float th = 22.0f;
        bool isTab = (activeTab == t);
        u32 tabBg = isTab ? ctx.clrPrimary : Ui::rgba(24, 20, 36, 235);
        Ui::panel(tx, 6.0f, tw, th, 0.20f, tabBg, isTab ? Ui::rgba(255, 255, 255) : 0);
        if (isTab) {
            Ui::glow(tx + tw * 0.5f, 17.0f, 0.18f, tw * 0.45f, Ui::withAlpha(ctx.clrPrimary, 90), 2);
        }
        const char* tLbl = tabNames[t];
        if (tLbl[0] >= '1' && tLbl[0] <= '5' && tLbl[1] == '.') tLbl += 3;
        drawCenteredTextWithShadow(ctx.textBuf, tx + tw * 0.5f, 17.0f, 0.32f, isTab ? ctx.clrWhite : Ui::rgba(180, 175, 205), tLbl, 0.25f);
    }

    // Contenido central interactivo
    if (activeTab == 0) { // Controles / Tablet
        if (ctx.settings.playOnBottomScreen) {
            Ui::panel(10.0f, 36.0f, 300.0f, 162.0f, 0.15f, Ui::rgba(28, 22, 48, 225), ctx.clr100);
            drawCenteredText(ctx.textBuf, 160.0f, 52.0f, 0.44f, ctx.clr100, LOC(STR_CTRL_TOUCH_1TO1));
            drawCenteredText(ctx.textBuf, 160.0f, 72.0f, 0.36f, ctx.clrWhite, LOC(STR_CTRL_TOUCH_1TO1_DESC));
            drawRing(160.0f, 122.0f, 22.0f, 2.5f, ctx.clrPrimary, 24, 0.3f);
            C2D_DrawCircleSolid(160.0f, 122.0f, 0.25f, 22.0f, Ui::withAlpha(ctx.clrPrimary, 140));
            drawCenteredText(ctx.textBuf, 160.0f, 122.0f, 0.55f, ctx.clrWhite, "1", 0.35f);
        } else {
            Ui::panel(rx, ry, rw, rh, 0.15f, Ui::rgba(42, 34, 70, 225), ctx.clrPrimary);
            u32 gridClr = C2D_Color32(110, 95, 160, 140);
            C2D_DrawLine(rx + rw * 0.5f, ry, gridClr, rx + rw * 0.5f, ry + rh, gridClr, 1.0f, 0.2f);
            C2D_DrawLine(rx, ry + rh * 0.5f, gridClr, rx + rw, ry + rh * 0.5f, gridClr, 1.0f, 0.2f);
            drawRectOutline(rx, ry, rw, rh, 2.0f, ctx.clrPrimary, 0.25f);
            drawRing(ctx.settings.tabletCenterX, ctx.settings.tabletCenterY, 7.0f, 1.5f, ctx.clrCursor, 18, 0.3f);

            char areaCaption[64];
            std::snprintf(areaCaption, sizeof(areaCaption), LOC(STR_TABLET_AREA_FMT),
                static_cast<int>(std::round(ctx.settings.tabletScale * 100.0f)), (int)rw, (int)rh);
            drawCenteredText(ctx.textBuf, ctx.settings.tabletCenterX, std::max(40.0f, ry + 14.0f), 0.38f, ctx.clrWhite, areaCaption);
        }
    } else if (activeTab == 1) { // Pestaña CONTROLES: Modo 3DS Interactivo
        // 1. Selector táctil de modo (STD / TAIKO / CATCH / MANIA)
        const char* subModeNames[4] = { "STANDARD", "TAIKO", "CATCH", "MANIA" };
        u32 subModeColors[4] = { Ui::rgba(255, 102, 171), Ui::rgba(255, 75, 80), Ui::rgba(45, 215, 115), Ui::rgba(60, 205, 255) };

        for (int m = 0; m < 4; ++m) {
            float sx = 6.0f + m * 77.0f;
            bool isCurM = (ctx.settingsControlsMode == m);
            u32 pillBg = isCurM ? Ui::withAlpha(subModeColors[m], 235) : Ui::rgba(32, 26, 48, 235);
            Ui::pill(sx, 31.0f, 74.0f, 17.0f, 0.22f, pillBg);
            if (isCurM) drawRectOutline(sx, 31.0f, 74.0f, 17.0f, 1.2f, ctx.clrWhite, 0.24f);
            drawCenteredTextWithShadow(ctx.textBuf, sx + 37.0f, 39.5f, 0.32f, isCurM ? ctx.clrWhite : Ui::rgba(220, 215, 240), subModeNames[m], 0.30f);

            if (ctx.touchJustDown && ctx.touch.px >= sx && ctx.touch.px <= sx + 74.0f && ctx.touch.py >= 29.0f && ctx.touch.py <= 49.0f) {
                ctx.settingsControlsMode = m;
                ctx.audio.playHitSound(2);
            }
        }

        // 2. 4 Pastillas de acción para el modo activo
        const char* kShortNames[4];
        uint32_t kMasks[4];
        u32 actColors[4];

        if (ctx.settingsControlsMode == MODE_STD) {
            kShortNames[0] = "K1";         kMasks[0] = ctx.settings.keyOsuK1;      actColors[0] = Ui::rgba(180, 70, 240);
            kShortNames[1] = "K2";         kMasks[1] = ctx.settings.keyOsuK2;      actColors[1] = Ui::rgba(255, 95, 180);
            kShortNames[2] = LOC(STR_KEY_SMOKE); kMasks[2] = ctx.settings.keyOsuSmoke;   actColors[2] = Ui::rgba(240, 200, 50);
            kShortNames[3] = LOC(STR_KEY_RETRY); kMasks[3] = ctx.settings.keyQuickRetry; actColors[3] = Ui::rgba(255, 75, 75);
        } else if (ctx.settingsControlsMode == MODE_TAIKO) {
            kShortNames[0] = LOC(STR_KEY_DON_L);    kMasks[0] = ctx.settings.keyTaikoDonLeft;    actColors[0] = Ui::rgba(245, 60, 65);
            kShortNames[1] = LOC(STR_KEY_DON_R);    kMasks[1] = ctx.settings.keyTaikoDonRight;   actColors[1] = Ui::rgba(245, 60, 65);
            kShortNames[2] = LOC(STR_KEY_KAT_L);  kMasks[2] = ctx.settings.keyTaikoKatsuLeft;  actColors[2] = Ui::rgba(45, 135, 255);
            kShortNames[3] = LOC(STR_KEY_KAT_R);  kMasks[3] = ctx.settings.keyTaikoKatsuRight; actColors[3] = Ui::rgba(45, 135, 255);
        } else if (ctx.settingsControlsMode == MODE_CATCH) {
            kShortNames[0] = LOC(STR_KEY_MOVE_L);  kMasks[0] = ctx.settings.keyCatchLeft;  actColors[0] = Ui::rgba(50, 210, 120);
            kShortNames[1] = LOC(STR_KEY_MOVE_R);  kMasks[1] = ctx.settings.keyCatchRight; actColors[1] = Ui::rgba(50, 210, 120);
            kShortNames[2] = LOC(STR_KEY_DASH);    kMasks[2] = ctx.settings.keyCatchDash;   actColors[2] = Ui::rgba(255, 175, 30);
            kShortNames[3] = LOC(STR_KEY_RETRY);   kMasks[3] = ctx.settings.keyQuickRetry;  actColors[3] = Ui::rgba(255, 75, 75);
        } else {
            kShortNames[0] = "Col 1";      kMasks[0] = ctx.settings.keyManiaCol1;  actColors[0] = Ui::rgba(50, 160, 255);
            kShortNames[1] = "Col 2";      kMasks[1] = ctx.settings.keyManiaCol2;  actColors[1] = Ui::rgba(235, 235, 245);
            kShortNames[2] = "Col 3";      kMasks[2] = ctx.settings.keyManiaCol3;  actColors[2] = Ui::rgba(235, 235, 245);
            kShortNames[3] = "Col 4";      kMasks[3] = ctx.settings.keyManiaCol4;  actColors[3] = Ui::rgba(50, 160, 255);
        }

        for (int i = 0; i < 4; ++i) {
            float bx = 6.0f + i * 77.0f;
            float by = 50.0f;
            float bw = 74.0f;
            float bh = 22.0f;
            int optIdx = 4 + i;
            bool sel = (ctx.settingsOption == optIdx);
            bool cap = (ctx.capturingKeyIndex == optIdx);

            u32 pillBg = cap ? Ui::rgba(255, 180, 40) : (sel ? Ui::rgba(52, 40, 85, 250) : Ui::rgba(24, 20, 38, 230));
            Ui::panel(bx, by, bw, bh, 0.15f, pillBg, cap ? ctx.clrWhite : (sel ? ctx.clrCursor : actColors[i]));
            drawCenteredTextWithShadow(ctx.textBuf, bx + bw * 0.5f, by + 4.5f, 0.31f, cap ? ctx.clrBgDark : (sel ? ctx.clrWhite : actColors[i]), kShortNames[i], 0.85f);
            std::string mStr = cap ? LOC(STR_PRESS_SHORT) : truncateString(GameSettings::formatKeyMask(kMasks[i]), 11);
            drawCenteredTextWithShadow(ctx.textBuf, bx + bw * 0.5f, by + 14.5f, 0.28f, cap ? ctx.clrBgDark : (sel ? ctx.clrCursor : Ui::rgba(235, 230, 255)), mStr.c_str(), 0.85f);

            if (ctx.touchJustDown && ctx.touch.px >= bx && ctx.touch.px <= bx + bw && ctx.touch.py >= by && ctx.touch.py <= by + bh) {
                ctx.settingsOption = optIdx;
                ctx.capturingKeyIndex = optIdx;
                ctx.audio.playHitSound(2);
            }
        }

        // 3. Dibujo estilizado de la consola Nintendo 3DS
        float chassisX = 22.0f, chassisY = 75.0f, chassisW = 276.0f, chassisH = 124.0f;
        Ui::panel(chassisX, chassisY, chassisW, chassisH, 0.12f, Ui::rgba(28, 24, 42, 250), Ui::rgba(65, 55, 90));
        C2D_DrawRectSolid(chassisX + 30.0f, chassisY + 1.0f, 0.13f, chassisW - 60.0f, 2.0f, Ui::rgba(45, 38, 65));

        // Pantalla interior de la 3DS
        float scX = 96.0f, scY = 85.0f, scW = 128.0f, scH = 86.0f;
        Ui::panel(scX, scY, scW, scH, 0.14f, Ui::rgba(10, 8, 16, 255), Ui::rgba(65, 55, 95));

        if (ctx.capturingKeyIndex != -1) {
            int cIdx = std::max(0, std::min(3, ctx.capturingKeyIndex - 4));
            Ui::glow(scX + scW * 0.5f, scY + scH * 0.5f, 0.15f, 32.0f, Ui::rgba(255, 180, 40, 60), 2);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 14.0f, 0.35f, ctx.clrCursor, LOC(STR_CTRL_REASSIGN_TITLE), 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 32.0f, 0.40f, ctx.clrWhite, kShortNames[cIdx], 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 50.0f, 0.33f, ctx.clr100, LOC(STR_CTRL_PRESS_ANY), 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 68.0f, 0.30f, ctx.clrWhite, LOC(STR_CTRL_CANCEL_HINT), 0.90f);
        } else {
            int selIdx = std::max(0, std::min(3, ctx.settingsOption - 4));
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 12.0f, 0.35f, subModeColors[ctx.settingsControlsMode], subModeNames[ctx.settingsControlsMode], 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 28.0f, 0.36f, ctx.clrWhite, kShortNames[selIdx], 0.90f);
            std::string curKeyFmt = GameSettings::formatKeyMask(kMasks[selIdx]);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 44.0f, 0.33f, ctx.clrCursor, curKeyFmt.c_str(), 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 62.0f, 0.32f, Ui::rgba(140, 240, 255), LOC(STR_CTRL_REASSIGN_HINT), 0.90f);
            drawCenteredTextWithShadow(ctx.textBuf, scX + scW * 0.5f, scY + 74.0f, 0.30f, ctx.clrWhite, LOC(STR_CTRL_REASSIGN_SUB), 0.90f);
        }

        auto getBtnClr = [&](uint32_t mask, u32 defClr) -> u32 {
            if (ctx.kHeld & mask) return Ui::rgba(255, 255, 255);
            for (int a = 0; a < 4; ++a) {
                if (kMasks[a] & mask) return actColors[a];
            }
            return defClr;
        };

        // Gatillos L / R / ZL / ZR
        u32 clrL = getBtnClr(KEY_L, Ui::rgba(55, 48, 75));
        u32 clrR = getBtnClr(KEY_R, Ui::rgba(55, 48, 75));
        u32 clrZL = getBtnClr(KEY_ZL, Ui::rgba(45, 40, 62));
        u32 clrZR = getBtnClr(KEY_ZR, Ui::rgba(45, 40, 62));

        Ui::pill(28.0f, 67.0f, 42.0f, 12.0f, 0.16f, clrL);
        drawCenteredTextWithShadow(ctx.textBuf, 49.0f, 73.0f, 0.33f, ctx.clrWhite, "L", 0.85f);

        Ui::pill(250.0f, 67.0f, 42.0f, 12.0f, 0.16f, clrR);
        drawCenteredTextWithShadow(ctx.textBuf, 271.0f, 73.0f, 0.33f, ctx.clrWhite, "R", 0.85f);

        Ui::pill(72.0f, 68.0f, 26.0f, 11.0f, 0.16f, clrZL);
        drawCenteredTextWithShadow(ctx.textBuf, 85.0f, 73.5f, 0.29f, ctx.clrWhite, "ZL", 0.85f);

        Ui::pill(222.0f, 68.0f, 26.0f, 11.0f, 0.16f, clrZR);
        drawCenteredTextWithShadow(ctx.textBuf, 235.0f, 73.5f, 0.29f, ctx.clrWhite, "ZR", 0.85f);

        // Circle Pad
        C2D_DrawCircleSolid(60.0f, 108.0f, 0.15f, 14.0f, Ui::rgba(40, 35, 55));
        C2D_DrawCircleSolid(60.0f, 108.0f, 0.16f, 11.0f, Ui::rgba(80, 75, 100));
        drawRing(60.0f, 108.0f, 7.0f, 1.2f, Ui::rgba(55, 50, 75), 16, 0.17f);
        C2D_DrawCircleSolid(60.0f, 108.0f, 0.18f, 2.0f, Ui::rgba(55, 50, 75));

        // D-Pad (Cruz direccional)
        u32 clrDUP = getBtnClr(KEY_DUP, Ui::rgba(55, 48, 75));
        u32 clrDDOWN = getBtnClr(KEY_DDOWN, Ui::rgba(55, 48, 75));
        u32 clrDLEFT = getBtnClr(KEY_DLEFT, Ui::rgba(55, 48, 75));
        u32 clrDRIGHT = getBtnClr(KEY_DRIGHT, Ui::rgba(55, 48, 75));

        C2D_DrawRectSolid(55.0f, 137.0f, 0.16f, 10.0f, 30.0f, Ui::rgba(35, 30, 50));
        C2D_DrawRectSolid(45.0f, 147.0f, 0.16f, 30.0f, 10.0f, Ui::rgba(35, 30, 50));

        C2D_DrawRectSolid(55.0f, 137.0f, 0.17f, 10.0f, 10.0f, clrDUP);
        C2D_DrawRectSolid(55.0f, 157.0f, 0.17f, 10.0f, 10.0f, clrDDOWN);
        C2D_DrawRectSolid(45.0f, 147.0f, 0.17f, 10.0f, 10.0f, clrDLEFT);
        C2D_DrawRectSolid(65.0f, 147.0f, 0.17f, 10.0f, 10.0f, clrDRIGHT);

        drawCenteredTextWithShadow(ctx.textBuf, 60.0f, 140.0f, 0.26f, ctx.clrWhite, "^", 0.85f);
        drawCenteredTextWithShadow(ctx.textBuf, 60.0f, 160.0f, 0.26f, ctx.clrWhite, "v", 0.85f);
        drawCenteredTextWithShadow(ctx.textBuf, 50.0f, 150.0f, 0.26f, ctx.clrWhite, "<", 0.85f);
        drawCenteredTextWithShadow(ctx.textBuf, 70.0f, 150.0f, 0.26f, ctx.clrWhite, ">", 0.85f);

        // C-Stick (N3DS)
        C2D_DrawCircleSolid(238.0f, 104.0f, 0.16f, 5.0f, Ui::rgba(85, 80, 105));

        // ABXY Diamond
        u32 clrX = getBtnClr(KEY_X, Ui::rgba(60, 100, 200));
        u32 clrB = getBtnClr(KEY_B, Ui::rgba(210, 175, 40));
        u32 clrY = getBtnClr(KEY_Y, Ui::rgba(60, 180, 80));
        u32 clrA = getBtnClr(KEY_A, Ui::rgba(220, 55, 65));

        C2D_DrawCircleSolid(258.0f, 124.0f, 0.17f, 8.0f, clrX);
        drawCenteredTextWithShadow(ctx.textBuf, 258.0f, 124.0f, 0.34f, ctx.clrWhite, "X", 0.85f);

        C2D_DrawCircleSolid(258.0f, 156.0f, 0.17f, 8.0f, clrB);
        drawCenteredTextWithShadow(ctx.textBuf, 258.0f, 156.0f, 0.34f, ctx.clrWhite, "B", 0.85f);

        C2D_DrawCircleSolid(242.0f, 140.0f, 0.17f, 8.0f, clrY);
        drawCenteredTextWithShadow(ctx.textBuf, 242.0f, 140.0f, 0.34f, ctx.clrWhite, "Y", 0.85f);

        C2D_DrawCircleSolid(274.0f, 140.0f, 0.17f, 8.0f, clrA);
        drawCenteredTextWithShadow(ctx.textBuf, 274.0f, 140.0f, 0.34f, ctx.clrWhite, "A", 0.85f);

        // START / SELECT / HOME
        u32 clrSel = getBtnClr(KEY_SELECT, Ui::rgba(50, 44, 70));
        u32 clrStr = getBtnClr(KEY_START, Ui::rgba(50, 44, 70));

        Ui::pill(116.0f, 176.0f, 24.0f, 9.0f, 0.16f, clrSel);
        drawCenteredTextWithShadow(ctx.textBuf, 128.0f, 180.5f, 0.28f, ctx.clrWhite, "SEL", 0.85f);

        Ui::pill(148.0f, 176.0f, 24.0f, 9.0f, 0.16f, Ui::rgba(40, 36, 58));
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 180.5f, 0.27f, Ui::rgba(210, 205, 230), "HOME", 0.85f);

        Ui::pill(180.0f, 176.0f, 24.0f, 9.0f, 0.16f, clrStr);
        drawCenteredTextWithShadow(ctx.textBuf, 192.0f, 180.5f, 0.28f, ctx.clrWhite, "STR", 0.85f);
    } else { // Pestañas Audio (2), Juego (3) o Visual (4): tarjetas táctiles para ajustar con stylus
        int count = tabItemCounts[activeTab];
        float bCardH = (count == 6) ? 25.0f : ((count == 5) ? 30.0f : 37.0f);
        float bCardStep = (count == 6) ? 28.0f : ((count == 5) ? 33.0f : 41.0f);
        float bStartY = (count == 6) ? 32.0f : ((count == 5) ? 33.0f : 36.0f);
        float bTitleYOff = (count == 6) ? 1.0f : ((count == 5) ? 2.0f : 5.0f);
        float bDescYOff  = (count == 6) ? 13.5f : ((count == 5) ? 15.5f : 20.0f);
        float bSwYOff    = (count == 6) ? 3.5f : ((count == 5) ? 6.0f : 10.0f);
        float bSldYOff   = (count == 6) ? 8.0f : ((count == 5) ? 10.0f : 14.0f);
        float bPillYOff  = (count == 6) ? 3.5f : ((count == 5) ? 5.0f : 8.0f);

        for (int i = 0; i < count; ++i) {
            int optIdx = tabItems[activeTab][i];
            float cy = bStartY + i * bCardStep;
            bool sel = (ctx.settingsOption == optIdx);
            Ui::panel(10.0f, cy, 300.0f, bCardH, 0.15f, sel ? Ui::rgba(42, 34, 70, 240) : Ui::rgba(22, 19, 36, 220), sel ? ctx.clrCursor : 0);

            // Detección de toque en la fila
            if (ctx.isTouching && ctx.touch.px >= 10 && ctx.touch.px <= 310 && ctx.touch.py >= cy && ctx.touch.py <= cy + bCardH) {
                ctx.settingsOption = optIdx;
                if (optIdx == 8) { // Música
                    ctx.settings.musicVolume = std::max(0, std::min(100, (int)(((ctx.touch.px - 180.0f) / 115.0f) * 100.0f)));
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                } else if (optIdx == 9) { // SFX
                    ctx.settings.sfxVolume = std::max(0, std::min(100, (int)(((ctx.touch.px - 180.0f) / 115.0f) * 100.0f)));
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                } else if (optIdx == 10) { // Offset
                    ctx.settings.audioOffsetMs = std::max(-150, std::min(150, (int)(((ctx.touch.px - 180.0f) / 115.0f) * 300.0f - 150.0f)));
                } else if (optIdx == 11 && ctx.touchJustDown) { // Hitsounds Beatmap
                    ctx.settings.beatmapHitsounds = !ctx.settings.beatmapHitsounds;
                } else if (optIdx == 22 && ctx.touchJustDown) { // Idioma / Language
                    ctx.settings.language = (ctx.settings.language + 1) % LANG_COUNT;
                    Localization::setLanguage(static_cast<AppLanguage>(ctx.settings.language));
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (optIdx == 12 && ctx.touchJustDown) { // Modo de Juego
                    ctx.settings.gameMode = static_cast<GameMode>((ctx.settings.gameMode + 1) % 4);
                    ctx.settings.save();
                } else if (optIdx == 13 && ctx.touchJustDown) { // Skin
                    ctx.settings.skinIndex = (ctx.settings.skinIndex + 1) % std::max(1, (int)ctx.skinMgr.skins.size());
                    ctx.applyActiveSkinAudio();
                } else if (optIdx == 14 && ctx.touchJustDown) { // Sliders Serpiente
                    ctx.settings.snakingSliders = !ctx.settings.snakingSliders;
                } else if (optIdx == 15 && ctx.touchJustDown) { // Combo Bursts
                    ctx.settings.comboBursts = !ctx.settings.comboBursts;
                } else if (optIdx == 20 && ctx.touchJustDown) { // Efecto Golpes 300
                    ctx.settings.showHit300 = !ctx.settings.showHit300;
                    ctx.audio.playHitSound(0);
                } else if (optIdx == 16 && ctx.touchJustDown) { // Fondo Taiko
                    ctx.settings.taikoBgMode = (ctx.settings.taikoBgMode + 1) % 3;
                    ctx.settings.save();
                } else if (optIdx == 17) { // Cursor
                    ctx.settings.cursorScale = std::max(0.5f, std::min(2.0f, 0.5f + ((ctx.touch.px - 180.0f) / 115.0f) * 1.5f));
                } else if (optIdx == 18) { // BgDim
                    ctx.settings.bgDim = std::max(20, std::min(100, (int)(20 + ((ctx.touch.px - 180.0f) / 115.0f) * 80.0f)));
                } else if (optIdx == 19 && ctx.touchJustDown) { // Videos
                    ctx.settings.enableVideo = !ctx.settings.enableVideo;
                } else if (optIdx == 21 && ctx.touchJustDown) { // Descargas Multijugador
                    ctx.settings.multiDlOption = (ctx.settings.multiDlOption + 1) % 2;
                    ctx.multiMgr.enableBgDownload = (ctx.settings.multiDlOption != 0);
                    ctx.settings.save();
                }
                ctx.settings.clampValues();
            }

            if (optIdx == 8) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_MUSIC_VOL));
                char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.musicVolume);
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, desc);
                Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, ctx.settings.musicVolume / 100.0f, ctx.clrCursor, sel);
            } else if (optIdx == 9) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_SFX_VOL));
                char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.sfxVolume);
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, desc);
                Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, ctx.settings.sfxVolume / 100.0f, ctx.clrCursor, sel);
            } else if (optIdx == 10) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_OFFSET));
                char desc[32]; std::snprintf(desc, sizeof(desc), "%+d ms", ctx.settings.audioOffsetMs);
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr100, desc);
                float r = (ctx.settings.audioOffsetMs + 150) / 300.0f;
                Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, ctx.clr300, sel);
            } else if (optIdx == 11) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_MAP_HITSOUNDS));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clrMuted, ctx.settings.beatmapHitsounds ? LOC(STR_ON) : LOC(STR_OFF));
                Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, ctx.settings.beatmapHitsounds, ctx.clr100);
            } else if (optIdx == 22) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_LANGUAGE));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, ctx.settings.getLanguageName());
                u32 lPill = Ui::rgba(45, 115, 215);
                Ui::pill(232.0f, cy + bPillYOff, 68.0f, (count == 6) ? 18.0f : 20.0f, 0.25f, lPill);
                drawCenteredText(ctx.textBuf, 266.0f, cy + bPillYOff + ((count == 6) ? 9.0f : 10.0f), 0.33f, ctx.clrWhite, ctx.settings.getLanguageName(), 0.85f);
            } else if (optIdx == 12) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_MAIN_MODE));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, ctx.settings.getGameModeName());
                u32 mPill = (ctx.settings.gameMode == MODE_TAIKO) ? Ui::rgba(180, 45, 45) : ((ctx.settings.gameMode == MODE_CATCH) ? Ui::rgba(35, 160, 85) : ((ctx.settings.gameMode == MODE_MANIA) ? Ui::rgba(140, 35, 175) : Ui::rgba(55, 40, 95)));
                Ui::pill(232.0f, cy + bPillYOff, 68.0f, 20.0f, 0.25f, mPill);
                drawCenteredText(ctx.textBuf, 266.0f, cy + bPillYOff + 10.0f, 0.33f, ctx.clrWhite, ctx.settings.getGameModeShortName(), 0.85f);
            } else if (optIdx == 13) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_ACTIVE_SKIN));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, truncateString(activeSkin.name, 20).c_str());
                for (size_t c = 0; c < std::min((size_t)4, activeSkin.comboColors.size()); ++c) {
                    u32 cc = C2D_Color32(activeSkin.comboColors[c].r, activeSkin.comboColors[c].g, activeSkin.comboColors[c].b, 255);
                    C2D_DrawCircleSolid(225.0f + c * 18.0f, cy + bPillYOff + 10.0f, 0.25f, 6.0f, cc);
                }
            } else if (optIdx == 14) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_SNAKING));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clrMuted, ctx.settings.snakingSliders ? LOC(STR_ON) : LOC(STR_OFF));
                Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, ctx.settings.snakingSliders, ctx.clr100);
            } else if (optIdx == 15) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_BURSTS));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clrMuted, ctx.settings.comboBursts ? LOC(STR_ON) : LOC(STR_OFF));
                Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, ctx.settings.comboBursts, ctx.clr100);
            } else if (optIdx == 20) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_HIT300));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clrMuted, ctx.settings.showHit300 ? LOC(STR_ON) : LOC(STR_OFF));
                Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, ctx.settings.showHit300, ctx.clr100);
            } else if (optIdx == 16) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_TAIKO_BG));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, ctx.settings.getTaikoBgModeName());
                u32 bClr = (ctx.settings.taikoBgMode == 0) ? Ui::rgba(225, 145, 25) : ((ctx.settings.taikoBgMode == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(165, 55, 185));
                Ui::pill(180.0f, cy + bPillYOff + 1.0f, 115.0f, 20.0f, 0.25f, bClr);
                drawCenteredText(ctx.textBuf, 237.5f, cy + bPillYOff + 11.0f, 0.32f, ctx.clrWhite, ctx.settings.getTaikoBgModeName(), 0.35f);
            } else if (optIdx == 17) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_CURSOR_SIZE));
                char desc[32]; std::snprintf(desc, sizeof(desc), "%.1fx", ctx.settings.cursorScale);
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, desc);
                float r = (ctx.settings.cursorScale - 0.5f) / 1.5f;
                Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, ctx.clrCursor, sel);
            } else if (optIdx == 18) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_BG_DIM));
                char desc[32]; std::snprintf(desc, sizeof(desc), "%d%%", ctx.settings.bgDim);
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, desc);
                float r = (ctx.settings.bgDim - 20) / 80.0f;
                Ui::sliderBar(180.0f, cy + bSldYOff, 115.0f, 9.0f, 0.25f, r, ctx.clrPrimary, sel);
            } else if (optIdx == 19) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_VIDEOS));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clrMuted, ctx.settings.enableVideo ? LOC(STR_ON) : LOC(STR_OFF));
                Ui::toggleSwitch(245.0f, cy + bSwYOff, 48.0f, 18.0f, 0.25f, ctx.settings.enableVideo, ctx.clr100);
            } else if (optIdx == 21) {
                drawText(ctx.textBuf, 20.0f, cy + bTitleYOff, 0.40f, ctx.clrWhite, LOC(STR_SET_MULTI_DL));
                drawText(ctx.textBuf, 20.0f, cy + bDescYOff, 0.33f, ctx.clr300, ctx.settings.getMultiDlOptionName());
                u32 dClr = (ctx.settings.multiDlOption == 1) ? Ui::rgba(45, 130, 215) : Ui::rgba(110, 95, 140);
                Ui::pill(175.0f, cy + bPillYOff + 1.0f, 120.0f, 20.0f, 0.25f, dClr);
                drawCenteredText(ctx.textBuf, 235.0f, cy + bPillYOff + 11.0f, 0.30f, ctx.clrWhite, ctx.settings.getMultiDlOptionName(), 0.35f);
            }
        }
    }

    if (ctx.capturingKeyIndex != -1 && activeTab != 1) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, 320.0f, 240.0f, Ui::rgba(8, 6, 16, 215));
        Ui::panel(20.0f, 50.0f, 280.0f, 140.0f, 0.90f, Ui::rgba(26, 20, 44, 250), ctx.clrCursor);
        const char* capNames[4] = { LOC(STR_KEY_K1), LOC(STR_KEY_K2), LOC(STR_KEY_SMOKE), LOC(STR_KEY_RETRY) };
        int cIdx = std::max(0, std::min(3, ctx.capturingKeyIndex - 4));
        drawCenteredText(ctx.textBuf, 160.0f, 72.0f, 0.48f, ctx.clrCursor, LOC(STR_CTRL_ASSIGN_KEY_TITLE), 0.95f);
        drawCenteredText(ctx.textBuf, 160.0f, 96.0f, 0.42f, ctx.clrWhite, capNames[cIdx], 0.95f);
        drawCenteredText(ctx.textBuf, 160.0f, 126.0f, 0.36f, ctx.clr100, LOC(STR_CTRL_PRESS_ANY), 0.95f);
        drawCenteredText(ctx.textBuf, 160.0f, 154.0f, 0.32f, ctx.clrMuted, LOC(STR_CTRL_CANCEL_HINT), 0.95f);
        if (ctx.touchJustDown) {
            ctx.capturingKeyIndex = -1;
            ctx.audio.playHitSound(0);
        }
    }

    // Barra inferior con 3 botones de acción estilo multijugador
    Ui::button(10.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(20, 36, 60, 240), Ui::rgba(40, 180, 240), false);
    Ui::pill(15.0f, 209.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(40, 180, 240));
    drawCenteredText(ctx.textBuf, 24.0f, 218.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "X", 0.28f);
    drawTextWithShadow(ctx.textBuf, 37.0f, 211.0f, 0.33f, ctx.clrWhite, (activeTab == 1) ? LOC(STR_DEFAULT) : LOC(STR_CALIBRATE), 0.28f);

    Ui::button(112.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(45, 35, 20, 240), Ui::rgba(255, 180, 40), false);
    Ui::pill(117.0f, 209.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(255, 180, 40));
    drawCenteredText(ctx.textBuf, 126.0f, 218.0f, 0.30f, Ui::rgba(0, 0, 0, 240), "Y", 0.28f);
    drawTextWithShadow(ctx.textBuf, 140.0f, 211.0f, 0.34f, ctx.clrWhite, LOC(STR_RESET), 0.28f);

    Ui::button(214.0f, 204.0f, 96.0f, 28.0f, 0.20f, Ui::rgba(42, 20, 52, 240), ctx.clrPrimary, true);
    Ui::pill(219.0f, 209.0f, 18.0f, 18.0f, 0.24f, ctx.clrPrimary);
    drawCenteredText(ctx.textBuf, 228.0f, 218.0f, 0.30f, ctx.clrWhite, "B", 0.28f);
    drawTextWithShadow(ctx.textBuf, 242.0f, 211.0f, 0.34f, ctx.clrWhite, LOC(STR_SAVE), 0.28f);

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.75f, 11.0f, Ui::withAlpha(ctx.clr300, 140), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.8f, 4.5f, ctx.clr300);
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
