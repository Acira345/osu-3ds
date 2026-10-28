#include "SettingsScreen.hpp"

namespace {

enum TouchInteractMode {
    TOUCH_NONE = 0,
    TOUCH_CANDIDATE_TAP = 1,
    TOUCH_SCROLL_LIST = 2,
    TOUCH_SLIDER = 3,
    TOUCH_SCROLLBAR = 4
};

static float s_scrollY[5]       = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static float s_targetScrollY[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static TouchInteractMode s_touchMode = TOUCH_NONE;
static float s_touchStartX = 0.0f;
static float s_touchStartY = 0.0f;
static float s_touchStartScroll = 0.0f;
static int   s_touchCandidateIdx = -1;
static int   s_touchSliderOpt = -1;
static bool  s_tabletEditorOpen = false;
static int   s_liveTapCountK1 = 0;
static int   s_liveTapCountK2 = 0;
static int   s_liveBurstTimer = 0;
static int   s_liveLastHitKey = 0; // 1 = K1, 2 = K2

inline bool isEsp() {
    return Localization::getLanguage() == LANG_ES;
}

} // namespace

void runSettingsScreen(AppContext& ctx) {
    const SkinProfile& activeSkin = ctx.getActiveSkin();
    ctx.audio.update();
    if (!ctx.calibrationMode && ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }

    // Organización de apartados por sección (6 por sección, 5 en Teclas):
    // Tab 0 (PANTALLA): 0=Dónde Jugar, 23=Toque Stylus, 24=Editor Visual Táctil, 1=Tamaño Área, 2=Posición X, 3=Posición Y
    // Tab 1 (TECLAS):   4=Acción 1, 5=Acción 2, 6=Acción 3, 7=Acción 4, 25=Restablecer Modo
    // Tab 2 (AUDIO):    8=Vol Música, 9=Vol Efectos, 10=Offset Audio, 26=Calibrador Rítmico, 11=Hitsounds Mapa, 27=Música en Menú
    // Tab 3 (JUEGO):    12=Modo Principal, 22=Idioma, 14=Sliders Serpiente, 15=Combo Bursts, 20=Mostrar 300s, 21=Descargas Multi
    // Tab 4 (VISUAL):   13=Skin Activo, 17=Tamaño Cursor, 18=Oscurecer Fondo, 28=Marco Playfield, 19=Videos Fondo, 16=Fondo Taiko
    const int tabItems[5][6] = {
        { 0, 23, 24, 1, 2, 3 },
        { 4, 5, 6, 7, 25, -1 },
        { 8, 9, 10, 26, 11, 27 },
        { 12, 22, 14, 15, 20, 21 },
        { 13, 17, 18, 28, 19, 16 }
    };
    const int tabItemCounts[5] = { 6, 5, 6, 6, 6 };

    auto getTabForOption = [&](int opt) -> int {
        for (int t = 0; t < 5; ++t) {
            for (int i = 0; i < tabItemCounts[t]; ++i) {
                if (tabItems[t][i] == opt) return t;
            }
        }
        return 0;
    };

    auto getRowInTab = [&](int tab, int opt) -> int {
        for (int i = 0; i < tabItemCounts[tab]; ++i) {
            if (tabItems[tab][i] == opt) return i;
        }
        return 0;
    };

    int activeTab = getTabForOption(ctx.settingsOption);

    // Geometría del área desplazable en pantalla inferior (320x240)
    auto getViewportMetrics = [&](int tab, float& outTop, float& outBottom, float& outCardH, float& outStep, float& outMaxScroll) {
        outTop = (tab == 1) ? 55.0f : 32.0f;
        outBottom = 198.0f;
        outCardH = 38.0f;
        outStep = 43.0f;
        int cnt = tabItemCounts[tab];
        float contentH = (cnt > 0) ? ((cnt - 1) * outStep + outCardH + 6.0f) : 0.0f;
        float viewH = outBottom - outTop;
        outMaxScroll = std::max(0.0f, contentH - viewH);
    };

    auto ensureSelectedVisible = [&](int tab, int opt) {
        float vTop, vBot, cH, step, maxSc;
        getViewportMetrics(tab, vTop, vBot, cH, step, maxSc);
        int idx = getRowInTab(tab, opt);
        float itemTop = idx * step;
        float itemBot = itemTop + cH;
        float viewH = vBot - vTop;
        if (itemTop < s_targetScrollY[tab] + 4.0f) {
            s_targetScrollY[tab] = std::max(0.0f, itemTop - 4.0f);
        } else if (itemBot > s_targetScrollY[tab] + viewH - 4.0f) {
            s_targetScrollY[tab] = std::min(maxSc, itemBot - viewH + 4.0f);
        }
    };

    // Contadores de prueba en vivo para la pestaña TECLAS
    if (s_liveBurstTimer > 0) s_liveBurstTimer--;
    if (activeTab == 1 && ctx.capturingKeyIndex == -1) {
        uint32_t m1 = ctx.settings.keyOsuK1;
        uint32_t m2 = ctx.settings.keyOsuK2;
        if (ctx.settingsControlsMode == MODE_TAIKO) {
            m1 = ctx.settings.keyTaikoDonLeft | ctx.settings.keyTaikoDonRight;
            m2 = ctx.settings.keyTaikoKatsuLeft | ctx.settings.keyTaikoKatsuRight;
        } else if (ctx.settingsControlsMode == MODE_CATCH) {
            m1 = ctx.settings.keyCatchLeft | ctx.settings.keyCatchRight;
            m2 = ctx.settings.keyCatchDash;
        } else if (ctx.settingsControlsMode == MODE_MANIA) {
            m1 = ctx.settings.keyManiaCol1 | ctx.settings.keyManiaCol2;
            m2 = ctx.settings.keyManiaCol3 | ctx.settings.keyManiaCol4;
        }
        if (ctx.kDown & m1) {
            s_liveTapCountK1++;
            s_liveBurstTimer = 16;
            s_liveLastHitKey = 1;
        }
        if (ctx.kDown & m2) {
            s_liveTapCountK2++;
            s_liveBurstTimer = 16;
            s_liveLastHitKey = 2;
        }
    }

    // -------------------------------------------------------------------------
    // MODO 1: EDITOR VISUAL DE ÁREA TÁCTIL EN PANTALLA COMPLETA (Tab 0)
    // -------------------------------------------------------------------------
    if (s_tabletEditorOpen) {
        if (ctx.kDown & (KEY_B | KEY_START)) {
            s_tabletEditorOpen = false;
            ctx.settings.save();
            ctx.audio.playHitSound(0);
            return;
        }
        if (ctx.kDown & KEY_Y) {
            ctx.settings.resetTabletArea();
            ctx.audio.playHitSound(2);
        }
        int scaleStep = 0;
        if (ctx.kDown & (KEY_DLEFT | KEY_DDOWN | KEY_L)) scaleStep = -1;
        if (ctx.kDown & (KEY_DRIGHT | KEY_DUP | KEY_R))  scaleStep = +1;
        if (scaleStep != 0) {
            ctx.settings.tabletScale += scaleStep * 0.05f;
            ctx.settings.clampValues();
            ctx.audio.playHitSound(0);
        }
        if (std::abs(ctx.cpad.dx) > 18 || std::abs(ctx.cpad.dy) > 18) {
            ctx.settings.tabletCenterX += (ctx.cpad.dx / 156.0f) * 3.0f;
            ctx.settings.tabletCenterY -= (ctx.cpad.dy / 156.0f) * 3.0f;
            ctx.settings.clampValues();
        }
        if (ctx.touchJustDown && ctx.touch.py >= 204 && ctx.touch.px >= 90 && ctx.touch.px <= 230) {
            s_tabletEditorOpen = false;
            ctx.settings.save();
            ctx.audio.playHitSound(0);
            return;
        }
        if (ctx.isTouching && ctx.touch.py < 200) {
            ctx.settings.tabletCenterX = ctx.touch.px;
            ctx.settings.tabletCenterY = ctx.touch.py;
            ctx.settings.clampValues();
            ctx.settings.mapTouchToOsu(ctx.touch.px, ctx.touch.py, ctx.cursorOsuX, ctx.cursorOsuY);
        }
    }
    // -------------------------------------------------------------------------
    // MODO 2: CAPTURA DE BOTÓN EN PESTAÑA TECLAS
    // -------------------------------------------------------------------------
    else if (ctx.capturingKeyIndex != -1) {
        u32 anyKey = ctx.kDown & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_SELECT | KEY_START);
        bool fromTouchBtn = false;

        if (ctx.touchJustDown) {
            // Botones táctiles en el modal de captura (Permite asignar incluso el botón B tocándolo en pantalla)
            struct TouchKeyChip { float x, y, w, h; u32 mask; };
            const TouchKeyChip chips[] = {
                { 18.0f,  86.0f, 42.0f, 22.0f, KEY_L },
                { 64.0f,  86.0f, 42.0f, 22.0f, KEY_ZL },
                { 214.0f, 86.0f, 42.0f, 22.0f, KEY_ZR },
                { 260.0f, 86.0f, 42.0f, 22.0f, KEY_R },
                { 18.0f,  116.0f, 42.0f, 22.0f, KEY_DUP },
                { 64.0f,  116.0f, 42.0f, 22.0f, KEY_DDOWN },
                { 18.0f,  144.0f, 42.0f, 22.0f, KEY_DLEFT },
                { 64.0f,  144.0f, 42.0f, 22.0f, KEY_DRIGHT },
                { 214.0f, 116.0f, 42.0f, 22.0f, KEY_X },
                { 260.0f, 116.0f, 42.0f, 22.0f, KEY_Y },
                { 214.0f, 144.0f, 42.0f, 22.0f, KEY_A },
                { 260.0f, 144.0f, 42.0f, 22.0f, KEY_B },
                { 114.0f, 144.0f, 44.0f, 22.0f, KEY_SELECT },
                { 162.0f, 144.0f, 44.0f, 22.0f, KEY_START }
            };
            u32 touchedKey = 0;
            for (const auto& ch : chips) {
                if (ctx.touch.px >= ch.x && ctx.touch.px <= ch.x + ch.w &&
                    ctx.touch.py >= ch.y && ctx.touch.py <= ch.y + ch.h) {
                    touchedKey = ch.mask;
                    break;
                }
            }
            if (touchedKey != 0) {
                anyKey = touchedKey;
                fromTouchBtn = true;
            } else if (ctx.touch.py >= 178 && ctx.touch.py <= 206 && ctx.touch.px >= 90 && ctx.touch.px <= 230) {
                ctx.capturingKeyIndex = -1;
                ctx.audio.playHitSound(0);
                return;
            }
        }

        if (anyKey) {
            if ((anyKey & KEY_B) && !fromTouchBtn) {
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
    }
    // -------------------------------------------------------------------------
    // MODO 3: NAVEGACIÓN NORMAL Y DESPLAZAMIENTO DEL MENÚ
    // -------------------------------------------------------------------------
    else {
        if (ctx.kDown & KEY_B) {
            if (ctx.calibrationMode) {
                ctx.calibrationMode = false;
                ctx.settings.save();
                ctx.audio.playHitSound(0);
                if (ctx.settings.songPreview) ctx.updateSongPreview();
                return;
            }
            ctx.calibrationMode = false;
            ctx.settings.save();
            ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
            ctx.audio.playHitSound(0);
            ctx.appState = ctx.settingsReturnState;
            if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
            return;
        }

        if (ctx.kDown & KEY_START) {
            ctx.calibrationMode = false;
            ctx.settings.save();
            ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
            ctx.audio.playHitSound(2);
            ctx.appState = ctx.settingsReturnState;
            if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
            return;
        }

        if (ctx.kDown & KEY_X) {
            if (activeTab == 1) {
                ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
                ctx.settings.save();
                ctx.audio.playHitSound(2);
            } else if (activeTab == 0) {
                s_tabletEditorOpen = true;
                ctx.audio.playHitSound(2);
            } else {
                ctx.calibrationMode = !ctx.calibrationMode;
                ctx.calibSamples.clear();
                ctx.calibStartTick = osGetTime();
                ctx.calibLastBeatIndex = -1;
                if (ctx.calibrationMode) ctx.audio.stopMusic();
                else if (ctx.settings.songPreview) ctx.updateSongPreview();
                ctx.audio.playHitSound(2);
            }
        }

        if (ctx.kDown & KEY_Y) {
            if (activeTab == 1) {
                ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
                ctx.settings.save();
                ctx.audio.playHitSound(2);
            } else {
                ctx.settings.resetTabletArea();
                ctx.settings.save();
                ctx.audio.playHitSound(0);
            }
        }

        // Cambiar de pestaña con L / R / ZL / ZR
        if (ctx.kDown & (KEY_L | KEY_ZL)) {
            activeTab = (activeTab + 4) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ensureSelectedVisible(activeTab, ctx.settingsOption);
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & (KEY_R | KEY_ZR)) {
            activeTab = (activeTab + 1) % 5;
            ctx.settingsOption = tabItems[activeTab][0];
            ensureSelectedVisible(activeTab, ctx.settingsOption);
            ctx.audio.playHitSound(0);
        }

        // Calibrador de audio activo
        if (ctx.calibrationMode) {
            u64 elapsed = osGetTime() - ctx.calibStartTick;
            int beatIdx = static_cast<int>(elapsed / 500);
            if (beatIdx > ctx.calibLastBeatIndex) {
                ctx.calibLastBeatIndex = beatIdx;
                ctx.audio.playHitSound((beatIdx % 4 == 0) ? 2 : 0);
            }

            u32 tapKeys = KEY_A | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_DLEFT | KEY_DRIGHT | KEY_DUP | KEY_DDOWN;
            if ((ctx.kDown & tapKeys) || ctx.touchJustDown) {
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
        } else {
            // Función auxiliar para modificar o activar una opción
            auto applyOptionStep = [&](int opt, int stepDir, bool isActivate) {
                int numSkins = std::max(1, static_cast<int>(ctx.skinMgr.skins.size()));
                switch (opt) {
                    case 0: // Pantalla de Juego (Abajo Táctil vs Arriba 3D)
                        ctx.settings.playOnBottomScreen = !ctx.settings.playOnBottomScreen;
                        ctx.audio.playHitSound(2);
                        break;
                    case 23: // Función del Stylus (Apuntar+Golpear vs Solo Apuntar)
                        ctx.settings.tapWithStylus = !ctx.settings.tapWithStylus;
                        ctx.audio.playHitSound(2);
                        break;
                    case 24: // Abrir Editor Visual de Área en Pantalla
                        if (isActivate || stepDir != 0) {
                            s_tabletEditorOpen = true;
                            ctx.audio.playHitSound(2);
                        }
                        break;
                    case 1: // Tamaño Área Tablet
                        ctx.settings.tabletScale += (stepDir != 0 ? stepDir : 1) * 0.05f;
                        if (isActivate && stepDir == 0 && ctx.settings.tabletScale > 1.0f) ctx.settings.tabletScale = 0.50f;
                        ctx.audio.playHitSound(0);
                        break;
                    case 2: // Posición Horizontal X
                        ctx.settings.tabletCenterX += (stepDir != 0 ? stepDir : 1) * 8.0f;
                        ctx.audio.playHitSound(0);
                        break;
                    case 3: // Posición Vertical Y
                        ctx.settings.tabletCenterY += (stepDir != 0 ? stepDir : 1) * 8.0f;
                        ctx.audio.playHitSound(0);
                        break;
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                        if (isActivate) {
                            ctx.capturingKeyIndex = opt;
                            ctx.audio.playHitSound(2);
                        } else if (stepDir != 0) {
                            ctx.settingsControlsMode = (ctx.settingsControlsMode + (stepDir > 0 ? 1 : 3)) % 4;
                            ctx.audio.playHitSound(2);
                        }
                        break;
                    case 25: // Restablecer controles del modo actual
                        if (isActivate || stepDir != 0) {
                            ctx.settings.resetControlsToDefault(ctx.settingsControlsMode);
                            ctx.settings.save();
                            ctx.audio.playHitSound(2);
                        }
                        break;
                    case 8: // Volumen Música
                        ctx.settings.musicVolume += (stepDir != 0 ? stepDir : 1) * 5;
                        ctx.settings.clampValues();
                        ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                        ctx.audio.playHitSound(0);
                        break;
                    case 9: // Volumen Efectos
                        ctx.settings.sfxVolume += (stepDir != 0 ? stepDir : 1) * 5;
                        ctx.settings.clampValues();
                        ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                        ctx.audio.playHitSound(2);
                        break;
                    case 10: // Offset de Audio
                        if (isActivate && stepDir == 0) {
                            ctx.calibrationMode = true;
                            ctx.calibSamples.clear();
                            ctx.calibStartTick = osGetTime();
                            ctx.calibLastBeatIndex = -1;
                            ctx.audio.stopMusic();
                            ctx.audio.playHitSound(2);
                        } else {
                            ctx.settings.audioOffsetMs += stepDir * ((ctx.kHeld & (KEY_L | KEY_R)) ? 5 : 1);
                            ctx.audio.playHitSound(0);
                        }
                        break;
                    case 26: // Asistente de Calibración
                        if (isActivate || stepDir != 0) {
                            ctx.calibrationMode = true;
                            ctx.calibSamples.clear();
                            ctx.calibStartTick = osGetTime();
                            ctx.calibLastBeatIndex = -1;
                            ctx.audio.stopMusic();
                            ctx.audio.playHitSound(2);
                        }
                        break;
                    case 11: // Hitsounds del Beatmap
                        ctx.settings.beatmapHitsounds = !ctx.settings.beatmapHitsounds;
                        ctx.audio.playHitSound(0);
                        break;
                    case 27: // Preescucha en Menú
                        ctx.settings.songPreview = !ctx.settings.songPreview;
                        if (!ctx.settings.songPreview) ctx.audio.stopMusic();
                        else ctx.updateSongPreview();
                        ctx.audio.playHitSound(0);
                        break;
                    case 12: // Modo de Juego Principal
                        ctx.settings.gameMode = static_cast<GameMode>((ctx.settings.gameMode + (stepDir != 0 ? stepDir : 1) + 4) % 4);
                        ctx.audio.playHitSound(2);
                        break;
                    case 22: // Idioma
                        ctx.settings.language = (ctx.settings.language + (stepDir != 0 ? stepDir : 1) + LANG_COUNT) % LANG_COUNT;
                        Localization::setLanguage(static_cast<AppLanguage>(ctx.settings.language));
                        ctx.audio.playHitSound(2);
                        break;
                    case 14: // Sliders Serpiente
                        ctx.settings.snakingSliders = !ctx.settings.snakingSliders;
                        ctx.audio.playHitSound(0);
                        break;
                    case 15: // Combo Bursts
                        ctx.settings.comboBursts = !ctx.settings.comboBursts;
                        ctx.audio.playHitSound(0);
                        break;
                    case 20: // Mostrar 300s
                        ctx.settings.showHit300 = !ctx.settings.showHit300;
                        ctx.audio.playHitSound(0);
                        break;
                    case 21: // Descargas Multijugador
                        ctx.settings.multiDlOption = (ctx.settings.multiDlOption + 1) % 2;
                        ctx.multiMgr.enableBgDownload = (ctx.settings.multiDlOption != 0);
                        ctx.audio.playHitSound(0);
                        break;
                    case 13: // Skin Activo
                        ctx.settings.skinIndex = (ctx.settings.skinIndex + (stepDir != 0 ? stepDir : 1) + numSkins) % numSkins;
                        ctx.applyActiveSkinAudio();
                        ctx.audio.playHitSound(2);
                        break;
                    case 17: // Tamaño del Cursor
                        ctx.settings.cursorScale += (stepDir != 0 ? stepDir : 1) * 0.1f;
                        ctx.audio.playHitSound(0);
                        break;
                    case 18: // Oscurecimiento de Fondo
                        ctx.settings.bgDim += (stepDir != 0 ? stepDir : 1) * 5;
                        ctx.audio.playHitSound(0);
                        break;
                    case 28: // Marco del Área de Juego (Safe Area Border)
                        ctx.settings.showPlayfieldBorder = !ctx.settings.showPlayfieldBorder;
                        ctx.audio.playHitSound(0);
                        break;
                    case 19: // Videos de Fondo
                        ctx.settings.enableVideo = !ctx.settings.enableVideo;
                        ctx.audio.playHitSound(0);
                        break;
                    case 16: // Estilo de Fondo en Taiko
                        ctx.settings.taikoBgMode = (ctx.settings.taikoBgMode + (stepDir != 0 ? stepDir : 1) + 3) % 3;
                        ctx.audio.playHitSound(0);
                        break;
                }
                ctx.settings.clampValues();
                ctx.settings.save();
            };

            // Navegación con D-Pad Arriba / Abajo dentro de la sección actual (auto-scroll)
            int count = tabItemCounts[activeTab];
            int curIdx = getRowInTab(activeTab, ctx.settingsOption);

            if (ctx.kDown & KEY_DUP) {
                curIdx = (curIdx - 1 + count) % count;
                ctx.settingsOption = tabItems[activeTab][curIdx];
                ensureSelectedVisible(activeTab, ctx.settingsOption);
                ctx.audio.playHitSound(0);
            }
            if (ctx.kDown & KEY_DDOWN) {
                curIdx = (curIdx + 1) % count;
                ctx.settingsOption = tabItems[activeTab][curIdx];
                ensureSelectedVisible(activeTab, ctx.settingsOption);
                ctx.audio.playHitSound(0);
            }

            int stepDir = 0;
            if (ctx.kDown & KEY_DLEFT)  stepDir = -1;
            if (ctx.kDown & KEY_DRIGHT) stepDir = +1;
            if (stepDir != 0) {
                applyOptionStep(ctx.settingsOption, stepDir, false);
            }
            if (ctx.kDown & KEY_A) {
                applyOptionStep(ctx.settingsOption, 0, true);
            }

            // Interacción Táctil en Pantalla Inferior (Pestañas, Sub-modos, Scroll, Sliders y Botones)
            float vTop, vBot, cH, step, maxSc;
            getViewportMetrics(activeTab, vTop, vBot, cH, step, maxSc);

            auto updateSliderFromTouch = [&](int opt, float touchX) {
                float norm = std::max(0.0f, std::min(1.0f, (touchX - 182.0f) / 110.0f));
                if (opt == 1) {
                    ctx.settings.tabletScale = 0.30f + norm * 0.70f;
                } else if (opt == 2) {
                    ctx.settings.tabletCenterX = norm * 320.0f;
                } else if (opt == 3) {
                    ctx.settings.tabletCenterY = norm * 240.0f;
                } else if (opt == 8) {
                    ctx.settings.musicVolume = static_cast<int>(std::round(norm * 100.0f));
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                } else if (opt == 9) {
                    ctx.settings.sfxVolume = static_cast<int>(std::round(norm * 100.0f));
                    ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                } else if (opt == 10) {
                    ctx.settings.audioOffsetMs = static_cast<int>(std::round(norm * 300.0f - 150.0f));
                } else if (opt == 17) {
                    ctx.settings.cursorScale = std::round((0.5f + norm * 1.5f) * 10.0f) / 10.0f;
                } else if (opt == 18) {
                    ctx.settings.bgDim = static_cast<int>(std::round(20.0f + norm * 80.0f));
                }
                ctx.settings.clampValues();
                if (!ctx.settings.playOnBottomScreen) {
                    ctx.cursorOsuX = 256.0f;
                    ctx.cursorOsuY = 192.0f;
                }
            };

            auto isSliderOpt = [](int opt) -> bool {
                return (opt == 1 || opt == 2 || opt == 3 || opt == 8 || opt == 9 || opt == 10 || opt == 17 || opt == 18);
            };

            if (ctx.touchJustDown) {
                s_touchMode = TOUCH_NONE;
                s_touchCandidateIdx = -1;
                s_touchSliderOpt = -1;

                // 1. Pestañas superiores (y = 4..28)
                if (ctx.touch.py >= 2 && ctx.touch.py <= 29) {
                    for (int t = 0; t < 5; ++t) {
                        float tx = 6.0f + t * 62.0f;
                        if (ctx.touch.px >= tx && ctx.touch.px <= tx + 59.0f) {
                            activeTab = t;
                            ctx.settingsOption = tabItems[t][0];
                            ensureSelectedVisible(t, ctx.settingsOption);
                            ctx.audio.playHitSound(0);
                            break;
                        }
                    }
                }
                // 2. Botones inferiores: [<- Volver al Menú] y [Guardar Cambios] (y = 202..236)
                else if (ctx.touch.py >= 202 && ctx.touch.py <= 236) {
                    if (ctx.touch.px >= 8 && ctx.touch.px <= 154) {
                        ctx.calibrationMode = false;
                        ctx.settings.save();
                        ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                        ctx.audio.playHitSound(0);
                        ctx.appState = ctx.settingsReturnState;
                        if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
                        return;
                    } else if (ctx.touch.px >= 166 && ctx.touch.px <= 312) {
                        ctx.calibrationMode = false;
                        ctx.settings.save();
                        ctx.audio.setVolumes(ctx.settings.musicVolume, ctx.settings.sfxVolume);
                        ctx.audio.playHitSound(2);
                        ctx.appState = ctx.settingsReturnState;
                        if (ctx.appState == STATE_SONG_SELECT || ctx.appState == STATE_MAIN_MENU) ctx.updateSongPreview();
                        return;
                    }
                }
                // 3. Selector de modo en Pestaña TECLAS (y = 31..52)
                else if (activeTab == 1 && ctx.touch.py >= 30 && ctx.touch.py <= 53) {
                    for (int m = 0; m < 4; ++m) {
                        float sx = 8.0f + m * 76.0f;
                        if (ctx.touch.px >= sx && ctx.touch.px <= sx + 72.0f) {
                            ctx.settingsControlsMode = m;
                            ctx.audio.playHitSound(2);
                            break;
                        }
                    }
                }
                // 4. Barra de desplazamiento derecha (x >= 304)
                else if (ctx.touch.px >= 304 && ctx.touch.py >= vTop && ctx.touch.py <= vBot && maxSc > 0.0f) {
                    s_touchMode = TOUCH_SCROLLBAR;
                    float ratio = std::max(0.0f, std::min(1.0f, (ctx.touch.py - vTop) / (vBot - vTop)));
                    s_targetScrollY[activeTab] = ratio * maxSc;
                }
                // 5. Filas dentro de la lista desplazable
                else if (ctx.touch.py >= vTop && ctx.touch.py <= vBot) {
                    float relY = (ctx.touch.py - vTop) + s_scrollY[activeTab];
                    int rowIdx = static_cast<int>(relY / step);
                    if (rowIdx >= 0 && rowIdx < tabItemCounts[activeTab]) {
                        int opt = tabItems[activeTab][rowIdx];
                        ctx.settingsOption = opt;
                        if (isSliderOpt(opt) && ctx.touch.px >= 174 && ctx.touch.px <= 302) {
                            s_touchMode = TOUCH_SLIDER;
                            s_touchSliderOpt = opt;
                            updateSliderFromTouch(opt, ctx.touch.px);
                        } else {
                            s_touchMode = TOUCH_CANDIDATE_TAP;
                            s_touchCandidateIdx = rowIdx;
                            s_touchStartX = ctx.touch.px;
                            s_touchStartY = ctx.touch.py;
                            s_touchStartScroll = s_targetScrollY[activeTab];
                        }
                    } else {
                        s_touchMode = TOUCH_SCROLL_LIST;
                        s_touchStartX = ctx.touch.px;
                        s_touchStartY = ctx.touch.py;
                        s_touchStartScroll = s_targetScrollY[activeTab];
                    }
                }
            } else if (ctx.isTouching) {
                if (s_touchMode == TOUCH_SLIDER && s_touchSliderOpt != -1) {
                    updateSliderFromTouch(s_touchSliderOpt, ctx.touch.px);
                } else if (s_touchMode == TOUCH_SCROLLBAR && maxSc > 0.0f) {
                    float ratio = std::max(0.0f, std::min(1.0f, (ctx.touch.py - vTop) / (vBot - vTop)));
                    s_targetScrollY[activeTab] = ratio * maxSc;
                } else if (s_touchMode == TOUCH_CANDIDATE_TAP) {
                    if (std::fabs(ctx.touch.py - s_touchStartY) > 6.0f) {
                        s_touchMode = TOUCH_SCROLL_LIST;
                    }
                }
                if (s_touchMode == TOUCH_SCROLL_LIST) {
                    float dy = s_touchStartY - ctx.touch.py;
                    s_targetScrollY[activeTab] = std::max(0.0f, std::min(maxSc, s_touchStartScroll + dy));
                }
            } else if (ctx.touchJustUp) {
                if (s_touchMode == TOUCH_CANDIDATE_TAP && s_touchCandidateIdx >= 0 && s_touchCandidateIdx < tabItemCounts[activeTab]) {
                    int opt = tabItems[activeTab][s_touchCandidateIdx];
                    ctx.settingsOption = opt;
                    applyOptionStep(opt, 1, true);
                }
                s_touchMode = TOUCH_NONE;
                s_touchCandidateIdx = -1;
                s_touchSliderOpt = -1;
            }
        }
    }

    // Suavizado del desplazamiento vertical en cada pestaña
    for (int t = 0; t < 5; ++t) {
        float vTop, vBot, cH, step, maxSc;
        getViewportMetrics(t, vTop, vBot, cH, step, maxSc);
        s_targetScrollY[t] = std::max(0.0f, std::min(maxSc, s_targetScrollY[t]));
        s_scrollY[t] = Ui::approach(s_scrollY[t], s_targetScrollY[t], 0.32f);
    }

    // Actualizar cursor de previsualización con Circle Pad si no estamos en el editor
    if (!s_tabletEditorOpen && (std::abs(ctx.cpad.dx) > 20 || std::abs(ctx.cpad.dy) > 20)) {
        if (activeTab == 0 && (ctx.settingsOption == 2 || ctx.settingsOption == 3)) {
            ctx.settings.tabletCenterX += (ctx.cpad.dx / 156.0f) * 3.0f;
            ctx.settings.tabletCenterY -= (ctx.cpad.dy / 156.0f) * 3.0f;
            ctx.settings.clampValues();
        }
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    float rx, ry, rw, rh;
    ctx.settings.getTabletRect(rx, ry, rw, rh);
    activeTab = getTabForOption(ctx.settingsOption);
    bool esp = isEsp();

    const char* tabShortNames[5] = {
        esp ? "PANTALLA" : "SCREEN",
        esp ? "TECLAS"   : "KEYS",
        "AUDIO",
        esp ? "JUEGO"    : "GAME",
        "VISUAL"
    };
    const char* tabSubtitles[5] = {
        esp ? "Pantalla de Juego y Área Táctil"        : "Gameplay Screen & Touch Area",
        esp ? "Asignación de Teclas y Prueba en Vivo"  : "Key Bindings & Live Input Test",
        esp ? "Volumen, Latencia y Hitsounds"          : "Volume, Offset & Hitsounds",
        esp ? "Modo Principal, Idioma y Jugabilidad"   : "Main Mode, Language & Rules",
        esp ? "Skin, Cursor, Fondo y Gráficos"         : "Skin, Cursor, Dim & Graphics"
    };

    // Helper para obtener título, subtítulo, valor e información detallada de cada opción
    auto getOptionMeta = [&](int opt, std::string& outTitle, std::string& outSub, std::string& outVal, std::string& outDetail1, std::string& outDetail2) {
        char buf[96];
        switch (opt) {
            case 0:
                outTitle = esp ? "Pantalla de Juego" : "Gameplay Screen";
                outSub   = esp ? "Dónde aparecen las notas al jugar" : "Where hit circles appear";
                outVal   = ctx.settings.playOnBottomScreen ? (esp ? "Inferior (Táctil)" : "Bottom (Touch)") : (esp ? "Superior (3D)" : "Top (3D)");
                outDetail1 = ctx.settings.playOnBottomScreen
                    ? (esp ? "Tocas los círculos directamente en la pantalla táctil inferior (1:1)." : "Tap circles directly on the bottom touch screen (1:1).")
                    : (esp ? "Las notas se ven en la pantalla superior y abajo usas el stylus como tablet." : "Notes appear on the top screen while the bottom screen acts as a tablet.");
                outDetail2 = esp ? "Pulsa A o toca la fila para alternar entre pantalla Inferior y Superior." : "Press A or tap row to switch between Bottom and Top screen.";
                break;
            case 23:
                outTitle = esp ? "Toque del Stylus (Modo Superior)" : "Stylus Tap (Top Screen Mode)";
                outSub   = esp ? "Al tocar la tablet: golpear o solo apuntar" : "Touch taps notes or only aims cursor";
                outVal   = ctx.settings.tapWithStylus ? (esp ? "Apunta + Golpea" : "Aim + Tap") : (esp ? "Solo Apunta (PC)" : "Aim Only (PC)");
                outDetail1 = ctx.settings.tapWithStylus
                    ? (esp ? "Al apoyar el stylus en la pantalla inferior también se registra un golpe." : "Touching the bottom screen both moves the cursor and taps notes.")
                    : (esp ? "Estilo PC: el stylus solo mueve el cursor y golpeas con botones K1/K2." : "PC Hybrid style: stylus only aims the cursor; use K1/K2 buttons to tap.");
                outDetail2 = ctx.settings.playOnBottomScreen
                    ? (esp ? "Nota: Este ajuste aplica cuando 'Pantalla de Juego' está en Superior (3D)." : "Note: Applies when 'Gameplay Screen' is set to Top (3D).")
                    : (esp ? "Activo ahora mismo en tu modo de Pantalla Superior." : "Currently active in your Top Screen tablet mode.");
                break;
            case 24:
                outTitle = esp ? "Calibrar Área en Pantalla Táctil" : "Interactive Touch Area Editor";
                outSub   = esp ? "Mover y ver el cuadro de tablet a pantalla completa" : "Drag and preview tablet area full-screen";
                outVal   = esp ? "Abrir Editor (A)" : "Open Editor (A)";
                outDetail1 = esp ? "Abre un lienzo interactivo en toda la pantalla inferior para colocar el área." : "Opens a full-screen canvas on the bottom screen to position your tablet area.";
                outDetail2 = esp ? "Arrastra con el stylus para mover el centro y usa L/R para cambiar tamaño." : "Drag with stylus to move the center and use L/R to resize.";
                break;
            case 1:
                outTitle = esp ? "Tamaño del Área Táctil" : LOC(STR_SET_TABLET_SIZE);
                std::snprintf(buf, sizeof(buf), "%d%% (%dx%d px)", static_cast<int>(std::round(ctx.settings.tabletScale * 100.0f)), (int)rw, (int)rh);
                outSub   = esp ? "Escala de sensibilidad en modo Tablet" : "Active tablet area dimensions";
                outVal   = buf;
                outDetail1 = esp ? "Un área más pequeña (ej. 65%) permite llegar a las esquinas moviendo menos la mano." : "A smaller area lets you reach corners with less hand movement.";
                outDetail2 = esp ? "Usa Izquierda/Derecha o desliza la barra para ajustar entre 30% y 100%." : "Use Left/Right or drag the slider between 30% and 100%.";
                break;
            case 2:
                outTitle = esp ? "Posición Horizontal (Eje X)" : LOC(STR_SET_POS_X);
                std::snprintf(buf, sizeof(buf), "X = %.0f px", ctx.settings.tabletCenterX);
                outSub   = esp ? "Desplaza el área hacia izquierda o derecha" : "Horizontal center of tablet area";
                outVal   = buf;
                outDetail1 = esp ? "Ajusta el centro horizontal del cuadro activo para zurdos o diestros." : "Adjust horizontal center of active area for left or right hand.";
                outDetail2 = esp ? "También puedes moverlo con el Circle Pad o en el Editor Visual (X)." : "You can also move it with the Circle Pad or Visual Editor (X).";
                break;
            case 3:
                outTitle = esp ? "Posición Vertical (Eje Y)" : LOC(STR_SET_POS_Y);
                std::snprintf(buf, sizeof(buf), "Y = %.0f px", ctx.settings.tabletCenterY);
                outSub   = esp ? "Desplaza el área hacia arriba o abajo" : "Vertical center of tablet area";
                outVal   = buf;
                outDetail1 = esp ? "Ajusta la altura del cuadro activo en la pantalla táctil inferior (320x240)." : "Adjust vertical position of active area on the 320x240 bottom screen.";
                outDetail2 = esp ? "Pulsa Y en cualquier momento para centrar el área al 100%." : "Press Y anytime to reset tablet area to 100% centered.";
                break;
            case 25:
                outTitle = esp ? "Restablecer Teclas del Modo" : "Reset Mode Controls";
                outSub   = esp ? "Vuelve a los botones originales de este modo" : "Restore default keys for current mode";
                outVal   = esp ? "Restaurar (A/X)" : "Reset (A/X)";
                outDetail1 = esp ? "Restaura la configuración de botones por defecto sólo para el modo elegido." : "Restores default button mappings for the currently selected mode.";
                outDetail2 = esp ? "Pulsa A, X o toca esta tarjeta para restablecer al instante." : "Press A, X or tap this card to reset immediately.";
                break;
            case 8:
                outTitle = LOC(STR_SET_MUSIC_VOL);
                std::snprintf(buf, sizeof(buf), "%d%%", ctx.settings.musicVolume);
                outSub   = esp ? "Volumen general de las canciones" : "Master music playback volume";
                outVal   = buf;
                outDetail1 = esp ? "Controla el volumen de la pista MP3/OGG durante el menú y las partidas." : "Controls MP3/OGG music volume in menus and gameplay.";
                outDetail2 = esp ? "Desliza la barra táctil o usa la cruceta Izquierda/Derecha." : "Drag the touch slider or use D-Pad Left/Right.";
                break;
            case 9:
                outTitle = LOC(STR_SET_SFX_VOL);
                std::snprintf(buf, sizeof(buf), "%d%%", ctx.settings.sfxVolume);
                outSub   = esp ? "Volumen de golpes (hitsounds) e interfaz" : "Hitsounds and UI effects volume";
                outVal   = buf;
                outDetail1 = esp ? "Controla la intensidad de los sonidos al golpear círculos, sliders y menús." : "Controls the volume of hitcircle sounds, sliders, and menu clicks.";
                outDetail2 = esp ? "Recomendado entre 80% y 100% para escuchar claramente el ritmo." : "Recommended between 80% and 100% for clear rhythm feedback.";
                break;
            case 10:
                outTitle = LOC(STR_SET_OFFSET);
                std::snprintf(buf, sizeof(buf), "%+d ms", ctx.settings.audioOffsetMs);
                outSub   = esp ? "Compensación de latencia de audio" : "Audio latency compensation";
                outVal   = buf;
                outDetail1 = esp ? "Si tus golpes salen tarde, usa un valor negativo; si salen temprano, positivo." : "If you hit late, lower offset; if you hit early, raise offset.";
                outDetail2 = esp ? "Mantén L/R + Izq/Der para avanzar de 5 en 5 ms, o usa el Calibrador." : "Hold L/R + Left/Right to step by 5 ms, or launch the Calibrator.";
                break;
            case 26:
                outTitle = esp ? "Asistente de Calibración Rítmica" : "Rhythm Calibration Wizard";
                outSub   = esp ? "Calcula tu offset tocando al pulso de 120 BPM" : "Tap to a 120 BPM metronome to set offset";
                outVal   = esp ? "Iniciar (A / X)" : "Start (A / X)";
                outDetail1 = esp ? "Escucha 4 pulsos de metrónomo y presiona cualquier botón al ritmo." : "Listen to the 120 BPM beat and tap any button in rhythm.";
                outDetail2 = esp ? "El juego promedia tus últimas 12 pulsaciones automáticamente." : "Automatically averages your last 12 taps into the ideal offset.";
                break;
            case 11:
                outTitle = LOC(STR_SET_MAP_HITSOUNDS);
                outSub   = LOC(STR_SET_MAP_HITSOUNDS_DESC);
                outVal   = ctx.settings.beatmapHitsounds ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Usa los archivos WAV/OGG incluidos dentro de cada beatmap." : "Uses custom WAV/OGG hitsound samples bundled with each beatmap.";
                outDetail2 = esp ? "Desactívalo si prefieres escuchar siempre los sonidos limpios de tu Skin." : "Disable to always use your active Skin's hitsounds instead.";
                break;
            case 27:
                outTitle = esp ? "Música de Preescucha en Menú" : "Menu Song Preview";
                outSub   = esp ? "Reproducir canciones al navegar en los menús" : "Play song previews while browsing menus";
                outVal   = ctx.settings.songPreview ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Reproduce automáticamente la canción seleccionada en el menú y selector." : "Automatically plays the selected beatmap's audio in menus.";
                outDetail2 = esp ? "Puedes desactivarlo si prefieres silencio mientras navegas por la lista." : "Disable if you prefer silence while browsing your beatmaps.";
                break;
            case 12:
                outTitle = LOC(STR_SET_MAIN_MODE);
                outSub   = LOC(STR_SET_MAIN_MODE_DESC);
                outVal   = ctx.settings.getGameModeName();
                outDetail1 = esp ? "Define el modo activo (Standard, Taiko, Catch o Mania) en el selector." : "Sets the active gameplay ruleset (Standard, Taiko, Catch, or Mania).";
                outDetail2 = esp ? "También puedes cambiarlo rápidamente desde las pestañas del selector." : "Can also be switched anytime from the top bar in Song Select.";
                break;
            case 22:
                outTitle = LOC(STR_SET_LANGUAGE);
                outSub   = LOC(STR_SET_LANGUAGE_DESC);
                outVal   = ctx.settings.getLanguageName();
                outDetail1 = esp ? "Cambia al instante el idioma de todos los menús y textos del juego." : "Instantly switches the language of all menus and labels.";
                outDetail2 = esp ? "Idiomas incluidos: Español, English, Deutsch, Русский y Polski." : "Supported languages: Español, English, Deutsch, Русский, and Polski.";
                break;
            case 14:
                outTitle = LOC(STR_SET_SNAKING);
                outSub   = LOC(STR_SET_SNAKING_DESC);
                outVal   = ctx.settings.snakingSliders ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Los sliders se despliegan suavemente desde su inicio estilo osu!lazer." : "Sliders smoothly snake out from their start point like osu!lazer.";
                outDetail2 = esp ? "Desactivarlo dibuja el cuerpo del slider estático desde el primer instante." : "Disabling draws the full slider body statically right away.";
                break;
            case 15:
                outTitle = LOC(STR_SET_BURSTS);
                outSub   = LOC(STR_SET_BURSTS_DESC);
                outVal   = ctx.settings.comboBursts ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Muestra el personaje del Skin en los bordes al alcanzar rachas de combo." : "Displays Skin character bursts on screen edges at combo milestones.";
                outDetail2 = esp ? "Desactívalo para una pantalla completamente limpia sin distracciones." : "Disable for a distraction-free playfield.";
                break;
            case 20:
                outTitle = LOC(STR_SET_HIT300);
                outSub   = LOC(STR_SET_HIT300_DESC);
                outVal   = ctx.settings.showHit300 ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Muestra el destello '300' al acertar con precisión perfecta." : "Shows floating '300' judgement popups on perfect hits.";
                outDetail2 = esp ? "Muchos jugadores competitivos lo ocultan para ver solo 100, 50 y Miss." : "Many competitive players hide 300s to only see 100, 50, and Miss.";
                break;
            case 21:
                outTitle = LOC(STR_SET_MULTI_DL);
                outSub   = LOC(STR_SET_MULTI_DL_DESC);
                outVal   = ctx.settings.getMultiDlOptionName();
                outDetail1 = esp ? "Elige si deseas descargar el fondo del mapa o solo el audio y notas en salas." : "Choose whether to download beatmap backgrounds in multiplayer rooms.";
                outDetail2 = esp ? "'Solo Nivel' acelera la transferencia entre consolas 3DS." : "'Map Only' speeds up transfers between 3DS consoles.";
                break;
            case 13:
                outTitle = LOC(STR_SET_ACTIVE_SKIN);
                std::snprintf(buf, sizeof(buf), "%s (%d/%d)", truncateString(activeSkin.name, 16).c_str(), ctx.settings.skinIndex + 1, std::max(1, (int)ctx.skinMgr.skins.size()));
                outSub   = esp ? "Aspecto de círculos, cursor, colores y sonidos" : "Hitcircle textures, cursor, colors & sounds";
                outVal   = truncateString(activeSkin.name, 14);
                outDetail1 = esp ? "Cambia entre los temas incluidos y tus propios skins en /3ds/osu-3ds/skins/." : "Switch between built-in themes and custom skins in /3ds/osu-3ds/skins/.";
                outDetail2 = buf;
                break;
            case 17:
                outTitle = LOC(STR_SET_CURSOR_SIZE);
                std::snprintf(buf, sizeof(buf), "%.1fx", ctx.settings.cursorScale);
                outSub   = esp ? "Tamaño del puntero durante la partida" : "Gameplay cursor size multiplier";
                outVal   = buf;
                outDetail1 = esp ? "Aumenta el tamaño del cursor hasta 2.0x para verlo mejor en la pantalla 3DS." : "Scale cursor up to 2.0x for better visibility on the 3DS screen.";
                outDetail2 = esp ? "Observa la vista previa en vivo en el panel izquierdo de arriba." : "Check the live preview in the top-left panel.";
                break;
            case 18:
                outTitle = LOC(STR_SET_BG_DIM);
                std::snprintf(buf, sizeof(buf), "%d%%", ctx.settings.bgDim);
                outSub   = esp ? "Oscurece la ilustración de fondo al jugar" : "Darkens beatmap background during gameplay";
                outVal   = buf;
                outDetail1 = esp ? "Un valor alto (75%-95%) hace que los círculos y el AR destaquen con claridad." : "Higher dim (75%-95%) makes hit circles stand out clearly.";
                outDetail2 = esp ? "El panel izquierdo superior muestra el oscurecimiento exacto en vivo." : "The top-left panel previews the exact background dim in real time.";
                break;
            case 28:
                outTitle = esp ? "Marco del Área de Juego" : "Playfield Safe Border";
                outSub   = esp ? "Muestra el borde guía del área 512x384" : "Draws the 512x384 playfield boundary box";
                outVal   = ctx.settings.showPlayfieldBorder ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Dibuja un marco sutil alrededor del área donde pueden aparecer círculos." : "Draws a subtle border around the area where circles can spawn.";
                outDetail2 = esp ? "Muy útil para ubicarte cuando juegas con el fondo al 95%-100% de oscuridad." : "Helpful for spatial awareness at high background dim.";
                break;
            case 19:
                outTitle = LOC(STR_SET_VIDEOS);
                outSub   = LOC(STR_SET_VIDEOS_DESC);
                outVal   = ctx.settings.enableVideo ? LOC(STR_ON) : LOC(STR_OFF);
                outDetail1 = esp ? "Reproduce videos .3dsv/.mjpg sincronizados en el fondo de la canción." : "Plays synchronized .3dsv/.mjpg background videos during songs.";
                outDetail2 = esp ? "Desactívalo si prefieres máxima fluidez o fondos estáticos." : "Disable if you prefer static backgrounds.";
                break;
            case 16:
                outTitle = LOC(STR_SET_TAIKO_BG);
                outSub   = LOC(STR_SET_TAIKO_BG_DESC);
                outVal   = ctx.settings.getTaikoBgModeName();
                outDetail1 = esp ? "Elige entre el escenario Festivo Arcade con bailarines, Imagen o Video." : "Choose between Festive Arcade stage, Beatmap Image, or Video.";
                outDetail2 = esp ? "El modo Festivo reacciona al Kiai Time y a tu racha de combo." : "Festive mode reacts dynamically to Kiai Time and your combo.";
                break;
            default:
                outTitle = "Ajuste";
                outSub   = "";
                outVal   = "";
                outDetail1 = "";
                outDetail2 = "";
                break;
        }
    };

    // =========================================================================
    // PANTALLA SUPERIOR (400x240) — CABECERA LAZER + 2 PANELES EN VIVO + PIE
    // =========================================================================
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        float dBg = depthShift(-3.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(22, 14, 40), Ui::rgba(10, 7, 20), ctx.clrPrimary, 0.01f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, 255);
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 215) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(12, 8, 22, dimAlpha));
        }

        // 1. Cabecera superior inspirada en el diseño osu!lazer (0..28)
        Ui::gradV(0.0f, 0.0f, 400.0f, 28.0f, 0.30f, Ui::rgba(18, 12, 32, 245), Ui::rgba(14, 10, 26, 215));
        C2D_DrawRectSolid(0.0f, 27.0f, 0.31f, 400.0f, 1.5f, Ui::rgba(255, 60, 145, 220));

        // Insignia rosa izquierda: "AJUSTES · [SECCIÓN]"
        Ui::slantRectGrad(6.0f, 3.0f, 106.0f, 22.0f, -5.0f, 0.32f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawTextWithShadow(ctx.textBuf, 14.0f, 4.0f, 0.29f, ctx.clrWhite, esp ? "AJUSTES ·" : "SETTINGS ·", 0.34f);
        drawTextWithShadow(ctx.textBuf, 14.0f, 13.5f, 0.34f, ctx.clrWhite, tabShortNames[activeTab], 0.34f);

        // Subtítulo central y estado de hardware a la derecha
        drawTextWithShadow(ctx.textBuf, 118.0f, 8.0f, 0.36f, ctx.clrWhite, tabSubtitles[activeTab], 0.34f);
        drawTextWithShadow(ctx.textBuf, 308.0f, 8.5f, 0.31f, Ui::rgba(45, 235, 160), "60 FPS", 0.34f);
        drawTextWithShadow(ctx.textBuf, 350.0f, 8.5f, 0.31f, Ui::rgba(80, 200, 255), ctx.isNew3DS ? "N3DS" : "PICA200", 0.34f);

        float dPanel = depthShift(1.6f);
        float panL_X = 10.0f + dPanel;
        float panR_X = 202.0f + dPanel;
        float panY   = 33.0f;
        float panW   = 188.0f;
        float panH   = 175.0f;

        // 2. Paneles Laterales de Contexto y Comprobación en Vivo
        if (activeTab == 1) {
            // --- PESTAÑA TECLAS: Idéntica al diseño de referencia ---
            Ui::panel(panL_X, panY, panW, panH, 0.14f, Ui::rgba(16, 11, 28, 232), Ui::rgba(255, 65, 150));
            drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + 7.0f, 0.33f, Ui::rgba(255, 75, 160),
                esp ? "COMPROBACIÓN DE PULSACIONES" : "LIVE INPUT CHECK", 0.18f);
            drawRightAlignedText(ctx.textBuf, panL_X + panW - 8.0f, panY + 7.5f, 0.30f, Ui::rgba(45, 235, 160),
                esp ? "EN VIVO" : "LIVE", 0.18f);

            float boxX = panL_X + 10.0f;
            float boxY = panY + 24.0f;
            float boxW = panW - 20.0f;
            float boxH = 122.0f;
            Ui::panel(boxX, boxY, boxW, boxH, 0.16f, Ui::rgba(8, 6, 15, 245), Ui::rgba(50, 38, 78));

            bool k1Down = false, k2Down = false;
            if (ctx.settingsControlsMode == MODE_STD) {
                k1Down = (ctx.kHeld & ctx.settings.keyOsuK1) != 0;
                k2Down = (ctx.kHeld & ctx.settings.keyOsuK2) != 0;
                float cx = boxX + boxW * 0.5f;
                float cy = boxY + 52.0f;
                bool activeHit = k1Down || k2Down || (s_liveBurstTimer > 0);
                float rad = activeHit ? (29.0f + (s_liveBurstTimer * 0.35f)) : 27.0f;
                u32 cClr = (k2Down || s_liveLastHitKey == 2) ? Ui::rgba(65, 175, 255) : Ui::rgba(255, 55, 145);

                Ui::glow(cx, cy, 0.18f, rad + 12.0f, Ui::withAlpha(cClr, activeHit ? 150 : 85), 3);
                C2D_DrawCircleSolid(cx, cy, 0.20f, rad, Ui::withAlpha(cClr, 220));
                C2D_DrawCircleSolid(cx, cy, 0.21f, rad - 3.0f, Ui::rgba(115, 20, 68));
                drawRing(cx, cy, rad, 2.2f, Ui::rgba(255, 130, 195), 28, 0.22f);
                if (s_liveBurstTimer > 0) {
                    float br = 28.0f + (16 - s_liveBurstTimer) * 1.6f;
                    u8 ba = static_cast<u8>((s_liveBurstTimer * 220) / 16);
                    drawRing(cx, cy, br, 2.0f, Ui::rgba(255, 230, 110, ba), 24, 0.23f);
                }
                drawCenteredTextWithShadow(ctx.textBuf, cx, cy - 8.0f, 0.58f, ctx.clrWhite,
                    (k2Down || s_liveLastHitKey == 2) ? "2" : "1", 0.24f);
            } else if (ctx.settingsControlsMode == MODE_TAIKO) {
                bool dL = (ctx.kHeld & ctx.settings.keyTaikoDonLeft) != 0;
                bool dR = (ctx.kHeld & ctx.settings.keyTaikoDonRight) != 0;
                bool kL = (ctx.kHeld & ctx.settings.keyTaikoKatsuLeft) != 0;
                bool kR = (ctx.kHeld & ctx.settings.keyTaikoKatsuRight) != 0;
                k1Down = dL || dR;
                k2Down = kL || kR;
                float cx = boxX + boxW * 0.5f;
                float cy = boxY + 52.0f;
                C2D_DrawCircleSolid(cx, cy, 0.19f, 30.0f, (kL || kR) ? Ui::rgba(60, 185, 255) : Ui::rgba(30, 80, 145));
                C2D_DrawCircleSolid(cx, cy, 0.21f, 20.0f, (dL || dR) ? Ui::rgba(255, 75, 85) : Ui::rgba(150, 35, 45));
                drawRing(cx, cy, 30.0f, 2.0f, ctx.clrWhite, 28, 0.22f);
                drawCenteredTextWithShadow(ctx.textBuf, cx, cy - 6.0f, 0.42f, ctx.clrWhite,
                    (dL || dR) ? "DON!" : ((kL || kR) ? "KATSU!" : "TAIKO"), 0.24f);
            } else if (ctx.settingsControlsMode == MODE_CATCH) {
                bool mL = (ctx.kHeld & ctx.settings.keyCatchLeft) != 0;
                bool mR = (ctx.kHeld & ctx.settings.keyCatchRight) != 0;
                bool dS = (ctx.kHeld & ctx.settings.keyCatchDash) != 0;
                k1Down = mL || mR;
                k2Down = dS;
                float cx = boxX + boxW * 0.5f + (mL ? -28.0f : (mR ? 28.0f : std::sin(ctx.animTick * 0.06f) * 24.0f));
                float cy = boxY + 68.0f;
                C2D_DrawCircleSolid(boxX + boxW * 0.5f, boxY + 34.0f, 0.20f, 11.0f, Ui::rgba(255, 85, 95));
                Ui::pill(cx - 24.0f, cy, 48.0f, 8.0f, 0.22f, dS ? Ui::rgba(255, 195, 50) : Ui::rgba(65, 230, 145));
            } else {
                bool c1 = (ctx.kHeld & ctx.settings.keyManiaCol1) != 0;
                bool c2 = (ctx.kHeld & ctx.settings.keyManiaCol2) != 0;
                bool c3 = (ctx.kHeld & ctx.settings.keyManiaCol3) != 0;
                bool c4 = (ctx.kHeld & ctx.settings.keyManiaCol4) != 0;
                k1Down = c1 || c2;
                k2Down = c3 || c4;
                bool cols[4] = { c1, c2, c3, c4 };
                u32 clrs[4]  = { Ui::rgba(60, 180, 255), Ui::rgba(255, 75, 160), Ui::rgba(255, 75, 160), Ui::rgba(60, 180, 255) };
                for (int c = 0; c < 4; ++c) {
                    float lx = boxX + 22.0f + c * 32.0f;
                    Ui::panel(lx, boxY + 14.0f, 28.0f, 72.0f, 0.19f, cols[c] ? clrs[c] : Ui::rgba(22, 18, 36, 240), clrs[c]);
                }
            }

            drawCenteredText(ctx.textBuf, boxX + boxW * 0.5f, boxY + boxH - 18.0f, 0.30f, Ui::rgba(175, 170, 205),
                esp ? "Pulsa tus teclas para probar registro" : "Press bound keys to test input", 0.24f);

            char k1TapStr[32], k2TapStr[32];
            std::snprintf(k1TapStr, sizeof(k1TapStr), "K1 Taps: %d", s_liveTapCountK1);
            std::snprintf(k2TapStr, sizeof(k2TapStr), "K2 Taps: %d", s_liveTapCountK2);
            drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + panH - 19.0f, 0.32f, k1Down ? ctx.clrWhite : Ui::rgba(190, 185, 215), k1TapStr, 0.20f);
            drawRightAlignedText(ctx.textBuf, panL_X + panW - 10.0f, panY + panH - 19.0f, 0.32f, Ui::rgba(65, 205, 255), k2TapStr, 0.20f);

            // Panel Derecho en TECLAS: Mapeo Hardware Nintendo 3DS
            Ui::panel(panR_X, panY, panW, panH, 0.14f, Ui::rgba(14, 14, 28, 232), Ui::rgba(45, 215, 185));
            drawTextWithShadow(ctx.textBuf, panR_X + 10.0f, panY + 7.0f, 0.32f, Ui::rgba(55, 225, 200),
                esp ? "MAPEO HARDWARE NINTENDO 3DS" : "NINTENDO 3DS HID MAPPING", 0.18f);

            const char* modeLabels[4] = { "osu!standard", "osu!taiko", "osu!catch", "osu!mania 4K" };
            int selRow = std::max(0, std::min(3, ctx.settingsOption - 4));
            uint32_t selMask = ctx.settings.keyOsuK1;
            if (ctx.settingsControlsMode == MODE_STD) {
                uint32_t m[4] = { ctx.settings.keyOsuK1, ctx.settings.keyOsuK2, ctx.settings.keyOsuSmoke, ctx.settings.keyQuickRetry };
                selMask = m[selRow];
            } else if (ctx.settingsControlsMode == MODE_TAIKO) {
                uint32_t m[4] = { ctx.settings.keyTaikoDonLeft, ctx.settings.keyTaikoDonRight, ctx.settings.keyTaikoKatsuLeft, ctx.settings.keyTaikoKatsuRight };
                selMask = m[selRow];
            } else if (ctx.settingsControlsMode == MODE_CATCH) {
                uint32_t m[4] = { ctx.settings.keyCatchLeft, ctx.settings.keyCatchRight, ctx.settings.keyCatchDash, ctx.settings.keyQuickRetry };
                selMask = m[selRow];
            } else {
                uint32_t m[4] = { ctx.settings.keyManiaCol1, ctx.settings.keyManiaCol2, ctx.settings.keyManiaCol3, ctx.settings.keyManiaCol4 };
                selMask = m[selRow];
            }
            std::string curKeyStr = truncateString(GameSettings::formatKeyMask(selMask), 16);

            auto drawHwRow = [&](float ryOff, const char* lbl, const char* val, u32 valClr) {
                drawText(ctx.textBuf, panR_X + 10.0f, panY + ryOff, 0.31f, Ui::rgba(175, 170, 205), lbl, 0.18f);
                drawRightAlignedText(ctx.textBuf, panR_X + panW - 10.0f, panY + ryOff, 0.32f, valClr, val, 0.18f);
                C2D_DrawLine(panR_X + 10.0f, panY + ryOff + 15.0f, Ui::rgba(45, 42, 70, 140),
                             panR_X + panW - 10.0f, panY + ryOff + 15.0f, Ui::rgba(45, 42, 70, 140), 1.0f, 0.17f);
            };

            drawHwRow(28.0f,  esp ? "Frecuencia Polling" : "Polling Rate",   "60 Hz (16.6ms)",           Ui::rgba(45, 235, 160));
            drawHwRow(50.0f,  esp ? "Modo Seleccionado"  : "Selected Mode",  modeLabels[ctx.settingsControlsMode], Ui::rgba(65, 205, 255));
            drawHwRow(72.0f,  esp ? "Botones Activos"    : "Assigned Keys",  curKeyStr.c_str(),          Ui::rgba(255, 95, 175));
            drawHwRow(94.0f,  "Anti-Ghosting",                               esp ? "Habilitado" : "Enabled", Ui::rgba(255, 210, 65));
            drawHwRow(116.0f, esp ? "D-Pad simultáneo"   : "Simultaneous D-Pad", esp ? "Soportado" : "Supported", ctx.clrWhite);

            Ui::panel(panR_X + 10.0f, panY + 141.0f, panW - 20.0f, 24.0f, 0.18f, Ui::rgba(10, 14, 24, 245), Ui::rgba(45, 215, 165));
            drawText(ctx.textBuf, panR_X + 18.0f, panY + 147.0f, 0.31f, Ui::rgba(185, 190, 215), "hidScanInput()", 0.20f);
            drawRightAlignedText(ctx.textBuf, panR_X + panW - 16.0f, panY + 147.0f, 0.31f, Ui::rgba(45, 235, 160),
                esp ? "OK Cero Retardo" : "OK Zero Latency", 0.20f);
        } else {
            // --- PESTAÑAS 0 (PANTALLA), 2 (AUDIO), 3 (JUEGO), 4 (VISUAL) ---
            Ui::panel(panL_X, panY, panW, panH, 0.14f, Ui::rgba(16, 12, 30, 230), ctx.clrPrimary);
            Ui::panel(panR_X, panY, panW, panH, 0.14f, Ui::rgba(14, 13, 28, 230), Ui::rgba(65, 195, 255));

            // Panel Izquierdo: Previsualización en Vivo según la sección
            if (activeTab == 0) {
                drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + 7.0f, 0.33f, Ui::rgba(255, 85, 170),
                    esp ? "ESQUEMA DE PANTALLAS 3DS" : "3DS SCREEN LAYOUT", 0.18f);
                drawRightAlignedText(ctx.textBuf, panL_X + panW - 8.0f, panY + 7.5f, 0.29f, Ui::rgba(45, 235, 160),
                    esp ? "EN VIVO" : "LIVE", 0.18f);

                // Mini Pantalla Superior 3DS
                float topMiniX = panL_X + 30.0f, topMiniY = panY + 25.0f, topMiniW = 128.0f, topMiniH = 56.0f;
                bool notesOnTop = !ctx.settings.playOnBottomScreen;
                Ui::panel(topMiniX, topMiniY, topMiniW, topMiniH, 0.17f,
                    notesOnTop ? Ui::rgba(38, 20, 56, 245) : Ui::rgba(18, 16, 30, 235),
                    notesOnTop ? Ui::rgba(255, 65, 155) : Ui::rgba(75, 70, 105));
                drawCenteredTextWithShadow(ctx.textBuf, topMiniX + topMiniW * 0.5f, topMiniY + 6.0f, 0.29f,
                    notesOnTop ? Ui::rgba(255, 120, 190) : Ui::rgba(170, 165, 200),
                    notesOnTop ? (esp ? "SUPERIOR: NOTAS AQUÍ" : "TOP: NOTES HERE") : (esp ? "SUPERIOR: ESTADÍSTICAS" : "TOP: HUD & STATS"), 0.20f);
                if (notesOnTop) {
                    C2D_DrawCircleSolid(topMiniX + 46.0f, topMiniY + 34.0f, 0.20f, 10.0f, Ui::rgba(255, 65, 150));
                    drawRing(topMiniX + 46.0f, topMiniY + 34.0f, 10.0f, 1.5f, ctx.clrWhite, 16, 0.21f);
                    float curMiniX = topMiniX + 12.0f + (ctx.cursorOsuX / 512.0f) * (topMiniW - 24.0f);
                    float curMiniY = topMiniY + 18.0f + (ctx.cursorOsuY / 384.0f) * (topMiniH - 24.0f);
                    C2D_DrawCircleSolid(curMiniX, curMiniY, 0.22f, 4.0f, ctx.clrCursor);
                } else {
                    drawCenteredText(ctx.textBuf, topMiniX + topMiniW * 0.5f, topMiniY + 28.0f, 0.30f, Ui::rgba(140, 215, 255),
                        esp ? "Fondo + Precisión + Combo" : "BG + Accuracy + Combo", 0.20f);
                }

                // Mini Pantalla Inferior 3DS (con representación proporcional del área Tablet)
                float botMiniX = panL_X + 42.0f, botMiniY = panY + 87.0f, botMiniW = 104.0f, botMiniH = 60.0f;
                Ui::panel(botMiniX, botMiniY, botMiniW, botMiniH, 0.17f,
                    !notesOnTop ? Ui::rgba(24, 48, 42, 245) : Ui::rgba(20, 16, 36, 245),
                    !notesOnTop ? Ui::rgba(45, 235, 160) : Ui::rgba(165, 95, 255));

                if (!notesOnTop) {
                    drawCenteredTextWithShadow(ctx.textBuf, botMiniX + botMiniW * 0.5f, botMiniY + 6.0f, 0.29f, Ui::rgba(45, 235, 160),
                        esp ? "INFERIOR: TOCA NOTAS 1:1" : "BOTTOM: DIRECT TOUCH 1:1", 0.20f);
                    C2D_DrawCircleSolid(botMiniX + 52.0f, botMiniY + 36.0f, 0.20f, 11.0f, Ui::rgba(255, 65, 150));
                    drawRing(botMiniX + 52.0f, botMiniY + 36.0f, 11.0f, 1.5f, ctx.clrWhite, 16, 0.21f);
                } else {
                    float mRx = botMiniX + (rx / 320.0f) * botMiniW;
                    float mRy = botMiniY + (ry / 240.0f) * botMiniH;
                    float mRw = (rw / 320.0f) * botMiniW;
                    float mRh = (rh / 240.0f) * botMiniH;
                    C2D_DrawRectSolid(mRx, mRy, 0.19f, mRw, mRh, Ui::rgba(255, 65, 150, 90));
                    drawRectOutline(mRx, mRy, mRw, mRh, 1.5f, Ui::rgba(255, 95, 175), 0.20f);
                    drawCenteredTextWithShadow(ctx.textBuf, botMiniX + botMiniW * 0.5f, botMiniY + 6.0f, 0.28f, Ui::rgba(220, 190, 255),
                        esp ? "ÁREA TABLET ACTIVA" : "ACTIVE TABLET AREA", 0.21f);
                }

                const char* modeBadgeStr = ctx.settings.playOnBottomScreen
                    ? (esp ? "TÁCTIL DIRECTO ABAJO" : "DIRECT TOUCH BOTTOM")
                    : (ctx.settings.tapWithStylus ? (esp ? "TABLET + TAP ARRIBA" : "TABLET + TAP TOP") : (esp ? "HÍBRIDO PC ARRIBA" : "PC HYBRID TOP"));
                drawCenteredTextWithShadow(ctx.textBuf, panL_X + panW * 0.5f, panY + 154.0f, 0.32f, Ui::rgba(65, 220, 255), modeBadgeStr, 0.20f);
            } else if (activeTab == 2) {
                drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + 7.0f, 0.33f, Ui::rgba(255, 85, 170),
                    esp ? "MONITOR DE AUDIO Y COMPÁS" : "AUDIO & TIMING MONITOR", 0.18f);
                float boxX = panL_X + 10.0f, boxY = panY + 24.0f, boxW = panW - 20.0f, boxH = 120.0f;
                Ui::panel(boxX, boxY, boxW, boxH, 0.16f, Ui::rgba(8, 6, 16, 245), Ui::rgba(55, 42, 85));

                float pulse = ctx.getBpmPulse();
                float cx = boxX + boxW * 0.5f, cy = boxY + 44.0f;
                float r = 18.0f + pulse * 6.0f;
                Ui::glow(cx, cy, 0.18f, r + 8.0f, Ui::rgba(255, 65, 150, 110), 3);
                C2D_DrawCircleSolid(cx, cy, 0.20f, r, Ui::rgba(255, 65, 150));
                drawRing(cx, cy, r, 2.0f, ctx.clrWhite, 24, 0.21f);
                drawCenteredTextWithShadow(ctx.textBuf, cx, cy - 5.0f, 0.34f, ctx.clrWhite, "osu!", 0.22f);

                // Escala visual de Offset (-150 ms .. 0 .. +150 ms)
                float barX = boxX + 14.0f, barY = boxY + 86.0f, barW = boxW - 28.0f;
                C2D_DrawRectSolid(barX, barY, 0.19f, barW, 6.0f, Ui::rgba(35, 30, 55));
                C2D_DrawRectSolid(barX + barW * 0.5f - 1.0f, barY - 3.0f, 0.20f, 2.0f, 12.0f, ctx.clrWhite);
                float needleNorm = (ctx.settings.audioOffsetMs + 150.0f) / 300.0f;
                float needleX = barX + std::max(0.0f, std::min(1.0f, needleNorm)) * barW;
                C2D_DrawCircleSolid(needleX, barY + 3.0f, 0.22f, 5.5f, Ui::rgba(45, 235, 160));
                char offStr[48];
                std::snprintf(offStr, sizeof(offStr), "Offset: %+d ms", ctx.settings.audioOffsetMs);
                drawCenteredText(ctx.textBuf, cx, boxY + 100.0f, 0.32f, Ui::rgba(45, 235, 160), offStr, 0.22f);

                char volSummary[64];
                std::snprintf(volSummary, sizeof(volSummary), "%s: %d%%   ·   SFX: %d%%",
                    esp ? "Música" : "Music", ctx.settings.musicVolume, ctx.settings.sfxVolume);
                drawCenteredText(ctx.textBuf, panL_X + panW * 0.5f, panY + 152.0f, 0.32f, Ui::rgba(195, 190, 225), volSummary, 0.20f);
            } else if (activeTab == 3) {
                drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + 7.0f, 0.33f, Ui::rgba(255, 85, 170),
                    esp ? "SIMULACIÓN DE JUGABILIDAD" : "GAMEPLAY SIMULATION", 0.18f);
                float boxX = panL_X + 10.0f, boxY = panY + 24.0f, boxW = panW - 20.0f, boxH = 120.0f;
                Ui::panel(boxX, boxY, boxW, boxH, 0.16f, Ui::rgba(8, 6, 16, 245), Ui::rgba(55, 42, 85));

                float animP = (ctx.animTick % 90) / 90.0f;
                float sEnd = ctx.settings.snakingSliders ? (boxX + 36.0f + animP * 92.0f) : (boxX + 128.0f);
                Ui::pill(boxX + 24.0f, boxY + 42.0f, std::max(24.0f, sEnd - (boxX + 24.0f)), 24.0f, 0.19f, Ui::rgba(255, 65, 150, 160));
                C2D_DrawCircleSolid(boxX + 36.0f, boxY + 54.0f, 0.21f, 13.0f, Ui::rgba(255, 65, 150));
                drawRing(boxX + 36.0f, boxY + 54.0f, 13.0f, 1.8f, ctx.clrWhite, 20, 0.22f);
                drawCenteredText(ctx.textBuf, boxX + 36.0f, boxY + 48.0f, 0.36f, ctx.clrWhite, "1", 0.23f);

                if (ctx.settings.showHit300) {
                    drawCenteredTextWithShadow(ctx.textBuf, boxX + 122.0f, boxY + 20.0f, 0.40f, Ui::rgba(80, 225, 255), "300", 0.23f);
                }
                drawCenteredText(ctx.textBuf, boxX + boxW * 0.5f, boxY + 94.0f, 0.33f, Ui::rgba(45, 235, 160),
                    ctx.settings.getGameModeName(), 0.22f);
                drawCenteredText(ctx.textBuf, panL_X + panW * 0.5f, panY + 152.0f, 0.32f, Ui::rgba(195, 190, 225),
                    ctx.settings.getLanguageName(), 0.20f);
            } else {
                drawTextWithShadow(ctx.textBuf, panL_X + 10.0f, panY + 7.0f, 0.33f, Ui::rgba(255, 85, 170),
                    esp ? "PREVISUALIZACIÓN VISUAL" : "LIVE VISUAL PREVIEW", 0.18f);
                float boxX = panL_X + 10.0f, boxY = panY + 24.0f, boxW = panW - 20.0f, boxH = 120.0f;
                Ui::panel(boxX, boxY, boxW, boxH, 0.16f, Ui::rgba(8, 6, 16, 245), Ui::rgba(55, 42, 85));
                if (ctx.bgTex.loaded) {
                    ctx.bgTex.drawRect(boxX + 2.0f, boxY + 2.0f, boxW - 4.0f, boxH - 4.0f, 0.17f, 255);
                }
                u8 previewDim = static_cast<u8>((ctx.settings.bgDim * 235) / 100);
                C2D_DrawRectSolid(boxX + 2.0f, boxY + 2.0f, 0.18f, boxW - 4.0f, boxH - 4.0f, Ui::rgba(10, 8, 18, previewDim));
                if (ctx.settings.showPlayfieldBorder) {
                    drawRectOutline(boxX + 10.0f, boxY + 10.0f, boxW - 20.0f, boxH - 20.0f, 1.5f, Ui::rgba(255, 255, 255, 190), 0.19f);
                }
                // Colores del skin activo y cursor a escala real
                for (size_t c = 0; c < std::min((size_t)4, activeSkin.comboColors.size()); ++c) {
                    u32 cc = C2D_Color32(activeSkin.comboColors[c].r, activeSkin.comboColors[c].g, activeSkin.comboColors[c].b, 255);
                    C2D_DrawCircleSolid(boxX + 34.0f + c * 26.0f, boxY + 46.0f, 0.20f, 9.0f, cc);
                    drawRing(boxX + 34.0f + c * 26.0f, boxY + 46.0f, 9.0f, 1.4f, ctx.clrWhite, 16, 0.21f);
                }
                float cRad = 7.0f * ctx.settings.cursorScale;
                Ui::glow(boxX + 128.0f, boxY + 80.0f, 0.21f, cRad * 1.8f, Ui::withAlpha(ctx.clrCursor, 130), 2);
                C2D_DrawCircleSolid(boxX + 128.0f, boxY + 80.0f, 0.22f, cRad, ctx.clrCursor);
                C2D_DrawCircleSolid(boxX + 128.0f, boxY + 80.0f, 0.23f, 2.2f, ctx.clrWhite);

                char visStr[64];
                std::snprintf(visStr, sizeof(visStr), "Cursor: %.1fx   ·   Dim: %d%%", ctx.settings.cursorScale, ctx.settings.bgDim);
                drawCenteredText(ctx.textBuf, panL_X + panW * 0.5f, panY + 152.0f, 0.32f, Ui::rgba(195, 190, 225), visStr, 0.20f);
            }

            // Panel Derecho: Inspector claro del apartado seleccionado + progreso en la sección
            std::string mTitle, mSub, mVal, mDet1, mDet2;
            getOptionMeta(ctx.settingsOption, mTitle, mSub, mVal, mDet1, mDet2);
            int curRow = getRowInTab(activeTab, ctx.settingsOption) + 1;
            int totRow = tabItemCounts[activeTab];
            char idxBadge[32];
            std::snprintf(idxBadge, sizeof(idxBadge), "%d / %d", curRow, totRow);

            drawTextWithShadow(ctx.textBuf, panR_X + 10.0f, panY + 7.0f, 0.32f, Ui::rgba(65, 205, 255),
                esp ? "DETALLE DEL APARTADO" : "SETTING INSPECTOR", 0.18f);
            drawRightAlignedText(ctx.textBuf, panR_X + panW - 10.0f, panY + 7.5f, 0.31f, Ui::rgba(255, 110, 185), idxBadge, 0.18f);

            Ui::panel(panR_X + 8.0f, panY + 24.0f, panW - 16.0f, 52.0f, 0.16f, Ui::rgba(24, 18, 44, 240), ctx.clrPrimary);
            drawTextWithShadow(ctx.textBuf, panR_X + 15.0f, panY + 29.0f, 0.37f, ctx.clrWhite, truncateString(mTitle, 24).c_str(), 0.20f);
            Ui::pill(panR_X + 14.0f, panY + 49.0f, panW - 28.0f, 20.0f, 0.19f, Ui::rgba(45, 32, 78, 245));
            drawCenteredTextWithShadow(ctx.textBuf, panR_X + panW * 0.5f, panY + 59.0f, 0.35f, Ui::rgba(65, 235, 195), mVal.c_str(), 0.21f);

            // Explicación clara dividida en líneas legibles
            auto drawWrapped = [&](float startY, const std::string& text, u32 clr) {
                const size_t maxChars = 31;
                if (text.size() <= maxChars) {
                    drawText(ctx.textBuf, panR_X + 10.0f, startY, 0.30f, clr, text.c_str(), 0.20f);
                } else {
                    size_t split = text.rfind(' ', maxChars);
                    if (split == std::string::npos) split = maxChars;
                    std::string l1 = text.substr(0, split);
                    std::string l2 = text.substr(split + (text[split] == ' ' ? 1 : 0));
                    drawText(ctx.textBuf, panR_X + 10.0f, startY, 0.30f, clr, l1.c_str(), 0.20f);
                    drawText(ctx.textBuf, panR_X + 10.0f, startY + 13.0f, 0.30f, clr, truncateString(l2, 33).c_str(), 0.20f);
                }
            };
            drawWrapped(panY + 84.0f, mDet1, Ui::rgba(225, 220, 245));
            drawWrapped(panY + 116.0f, mDet2, Ui::rgba(155, 205, 245));

            Ui::panel(panR_X + 8.0f, panY + 145.0f, panW - 16.0f, 22.0f, 0.17f, Ui::rgba(10, 12, 22, 240), Ui::rgba(55, 48, 85));
            drawCenteredText(ctx.textBuf, panR_X + panW * 0.5f, panY + 156.0f, 0.29f, Ui::rgba(180, 175, 210),
                esp ? "Desliza la lista táctil o usa Arriba/Abajo" : "Scroll touch list or use D-Pad Up/Down", 0.20f);
        }

        // Modal de Calibración en Pantalla Superior si está activo
        if (ctx.calibrationMode) {
            float dCal = depthShift(4.0f);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.88f, 400.0f, 240.0f, Ui::rgba(8, 6, 16, 205));
            Ui::panel(30.0f + dCal, 46.0f, 340.0f, 138.0f, 0.90f, Ui::rgba(22, 16, 40, 250), Ui::rgba(45, 235, 160));
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dCal, 62.0f, 0.48f, Ui::rgba(45, 235, 160), LOC(STR_CALIB_TITLE), 0.95f);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 84.0f, 0.36f, ctx.clrWhite, LOC(STR_CALIB_DESC), 0.95f);

            u64 elapsed = osGetTime() - ctx.calibStartTick;
            float beatPhase = (elapsed % 500) / 500.0f;
            float pulseR = 16.0f * (1.0f - beatPhase * 0.50f);
            Ui::ring(200.0f + dCal, 116.0f, 0.96f, pulseR, 2.5f, ctx.clrPrimary, 24);

            char calStr[96];
            std::snprintf(calStr, sizeof(calStr), LOC(STR_CALIB_STATUS_FMT),
                static_cast<int>(ctx.calibSamples.size()), ctx.settings.audioOffsetMs);
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dCal, 144.0f, 0.42f, Ui::rgba(65, 215, 255), calStr, 0.96f);
            drawCenteredText(ctx.textBuf, 200.0f + dCal, 164.0f, 0.33f, Ui::rgba(200, 195, 225), LOC(STR_CALIB_APPLY_HINT), 0.96f);
        }

        // 3. Pie de Pantalla Superior con insignias circulares estilo consola (212..240)
        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.30f, Ui::rgba(14, 10, 26, 210), Ui::rgba(10, 7, 20, 250));
        C2D_DrawCircleSolid(20.0f, 226.0f, 0.33f, 7.5f, Ui::rgba(235, 45, 65));
        drawCenteredTextWithShadow(ctx.textBuf, 20.0f, 226.0f, 0.31f, ctx.clrWhite, "B", 0.35f);
        drawTextWithShadow(ctx.textBuf, 32.0f, 220.0f, 0.32f, Ui::rgba(225, 220, 245), esp ? "Volver" : "Back", 0.34f);

        C2D_DrawCircleSolid(88.0f, 226.0f, 0.33f, 7.5f, Ui::rgba(35, 195, 115));
        drawCenteredTextWithShadow(ctx.textBuf, 88.0f, 226.0f, 0.31f, ctx.clrWhite, "X", 0.35f);
        const char* xHint = (activeTab == 1) ? (esp ? "Restablecer" : "Default") : ((activeTab == 0) ? (esp ? "Editor Área" : "Area Editor") : (esp ? "Calibrar" : "Calibrate"));
        drawTextWithShadow(ctx.textBuf, 100.0f, 220.0f, 0.32f, Ui::rgba(225, 220, 245), xHint, 0.34f);

        C2D_DrawCircleSolid(186.0f, 226.0f, 0.33f, 7.5f, Ui::rgba(235, 175, 40));
        drawCenteredTextWithShadow(ctx.textBuf, 186.0f, 226.0f, 0.31f, ctx.clrBgDark, "Y", 0.35f);
        drawTextWithShadow(ctx.textBuf, 198.0f, 220.0f, 0.32f, Ui::rgba(225, 220, 245), esp ? "Centrar Área" : "Reset Area", 0.34f);

        drawRightAlignedText(ctx.textBuf, 392.0f, 220.0f, 0.31f, Ui::rgba(135, 130, 165), "400x240 (5:3)", 0.34f);
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // =========================================================================
    // PANTALLA INFERIOR (320x240) — MENÚ DESPLAZABLE POR SECCIÓN + EDITOR VISUAL
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(20, 14, 36), Ui::rgba(10, 7, 18), ctx.clrPrimary, 0.01f);

    if (s_tabletEditorOpen) {
        // --- LIENZO INTERACTIVO DE PANTALLA COMPLETA PARA AJUSTAR EL ÁREA DE TABLET ---
        C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, 320.0f, 240.0f, Ui::rgba(12, 9, 22, 245));
        for (int gx = 40; gx < 320; gx += 40) {
            C2D_DrawLine(gx, 0.0f, Ui::rgba(45, 38, 70, 120), gx, 240.0f, Ui::rgba(45, 38, 70, 120), 1.0f, 0.06f);
        }
        for (int gy = 40; gy < 240; gy += 40) {
            C2D_DrawLine(0.0f, gy, Ui::rgba(45, 38, 70, 120), 320.0f, gy, Ui::rgba(45, 38, 70, 120), 1.0f, 0.06f);
        }

        Ui::panel(rx, ry, rw, rh, 0.12f, Ui::rgba(255, 55, 145, 75), Ui::rgba(255, 75, 165));
        drawRectOutline(rx, ry, rw, rh, 2.0f, Ui::rgba(255, 85, 170), 0.15f);
        C2D_DrawLine(rx + rw * 0.5f, ry, Ui::rgba(255, 255, 255, 110), rx + rw * 0.5f, ry + rh, Ui::rgba(255, 255, 255, 110), 1.0f, 0.14f);
        C2D_DrawLine(rx, ry + rh * 0.5f, Ui::rgba(255, 255, 255, 110), rx + rw, ry + rh * 0.5f, Ui::rgba(255, 255, 255, 110), 1.0f, 0.14f);
        drawRing(ctx.settings.tabletCenterX, ctx.settings.tabletCenterY, 7.0f, 2.0f, ctx.clrCursor, 20, 0.20f);

        char edInfo[96];
        std::snprintf(edInfo, sizeof(edInfo), "%s: %d%% (%dx%d px) · Centro: %.0f, %.0f",
            esp ? "Área" : "Area", static_cast<int>(std::round(ctx.settings.tabletScale * 100.0f)),
            (int)rw, (int)rh, ctx.settings.tabletCenterX, ctx.settings.tabletCenterY);
        Ui::pill(16.0f, 8.0f, 288.0f, 20.0f, 0.30f, Ui::rgba(18, 14, 34, 235));
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 18.0f, 0.32f, Ui::rgba(65, 220, 255), edInfo, 0.32f);

        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 36.0f, 0.31f, ctx.clrWhite,
            esp ? "Toca/arrastra para mover · L/R cambia tamaño · Y centra" : "Drag to move · L/R changes size · Y centers", 0.32f);

        Ui::button(90.0f, 205.0f, 140.0f, 26.0f, 0.30f, Ui::rgba(255, 42, 133), Ui::rgba(255, 160, 210), true);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 218.0f, 0.35f, ctx.clrWhite,
            esp ? "Listo / Guardar (B)" : "Done / Save (B)", 0.35f);
    } else {
        // --- CONTENIDO DESPLAZABLE DE LA SECCIÓN ACTIVA (z = 0.12..0.28) ---
        float vTop, vBot, cardH, cardStep, maxSc;
        getViewportMetrics(activeTab, vTop, vBot, cardH, cardStep, maxSc);
        float curScroll = s_scrollY[activeTab];
        int itemCount = tabItemCounts[activeTab];

        for (int i = 0; i < itemCount; ++i) {
            float cy = vTop + 2.0f + i * cardStep - curScroll;
            if (cy + cardH < vTop - 4.0f || cy > vBot + 4.0f) continue;

            int opt = tabItems[activeTab][i];
            bool sel = (ctx.settingsOption == opt);
            float cx = 8.0f;
            float cw = 296.0f;

            u32 cardBg = sel ? Ui::rgba(36, 24, 58, 245) : Ui::rgba(18, 15, 30, 235);
            u32 borderClr = sel ? Ui::rgba(255, 55, 145) : Ui::rgba(48, 42, 74);
            Ui::panel(cx, cy, cw, cardH, 0.14f, cardBg, sel ? Ui::rgba(255, 55, 145) : 0);
            drawRectOutline(cx, cy, cw, cardH, sel ? 1.5f : 1.0f, borderClr, 0.16f);

            if (activeTab == 1 && opt >= 4 && opt <= 7) {
                // Tarjetas de asignación de teclas estilo imagen de referencia
                int row = opt - 4;
                const char* actTitle = "";
                const char* actHint  = "";
                uint32_t mask = 0;
                if (ctx.settingsControlsMode == MODE_STD) {
                    const char* t[4] = {
                        "Click 1 / Tap 1 (K1)",
                        "Click 2 / Tap 2 (K2)",
                        esp ? "Rastro de Humo (Smoke)" : "Smoke Trail",
                        esp ? "Reintento Rápido" : "Quick Retry"
                    };
                    const char* h[4] = {
                        esp ? "Golpe principal" : "Primary tap",
                        esp ? "Golpe secundario" : "Secondary tap",
                        esp ? "Dibujar en pantalla" : "Draw cursor trail",
                        esp ? "Mantener para reiniciar" : "Hold to restart"
                    };
                    uint32_t m[4] = { ctx.settings.keyOsuK1, ctx.settings.keyOsuK2, ctx.settings.keyOsuSmoke, ctx.settings.keyQuickRetry };
                    actTitle = t[row]; actHint = h[row]; mask = m[row];
                } else if (ctx.settingsControlsMode == MODE_TAIKO) {
                    const char* t[4] = {
                        esp ? "Don Izquierdo (Rojo)" : "Left Don (Red)",
                        esp ? "Don Derecho (Rojo)" : "Right Don (Red)",
                        esp ? "Katsu Izquierdo (Azul)" : "Left Katsu (Blue)",
                        esp ? "Katsu Derecho (Azul)" : "Right Katsu (Blue)"
                    };
                    const char* h[4] = { "Centro izq.", "Centro der.", "Borde izq.", "Borde der." };
                    uint32_t m[4] = { ctx.settings.keyTaikoDonLeft, ctx.settings.keyTaikoDonRight, ctx.settings.keyTaikoKatsuLeft, ctx.settings.keyTaikoKatsuRight };
                    actTitle = t[row]; actHint = h[row]; mask = m[row];
                } else if (ctx.settingsControlsMode == MODE_CATCH) {
                    const char* t[4] = {
                        esp ? "Mover a la Izquierda" : "Move Left",
                        esp ? "Mover a la Derecha" : "Move Right",
                        esp ? "Acelerar / Dash" : "Dash (Boost)",
                        esp ? "Reintento Rápido" : "Quick Retry"
                    };
                    const char* h[4] = {
                        esp ? "Plato <-" : "Plate <-",
                        esp ? "Plato ->" : "Plate ->",
                        esp ? "Velocidad x2" : "Speed x2",
                        esp ? "Reiniciar" : "Retry"
                    };
                    uint32_t m[4] = { ctx.settings.keyCatchLeft, ctx.settings.keyCatchRight, ctx.settings.keyCatchDash, ctx.settings.keyQuickRetry };
                    actTitle = t[row]; actHint = h[row]; mask = m[row];
                } else {
                    const char* t[4] = {
                        esp ? "Columna 1 (Extrema Izq.)" : "Column 1 (Far Left)",
                        esp ? "Columna 2 (Centro Izq.)"  : "Column 2 (Mid Left)",
                        esp ? "Columna 3 (Centro Der.)"  : "Column 3 (Mid Right)",
                        esp ? "Columna 4 (Extrema Der.)" : "Column 4 (Far Right)"
                    };
                    const char* h[4] = {
                        esp ? "Carril 1" : "Lane 1",
                        esp ? "Carril 2" : "Lane 2",
                        esp ? "Carril 3" : "Lane 3",
                        esp ? "Carril 4" : "Lane 4"
                    };
                    uint32_t m[4] = { ctx.settings.keyManiaCol1, ctx.settings.keyManiaCol2, ctx.settings.keyManiaCol3, ctx.settings.keyManiaCol4 };
                    actTitle = t[row]; actHint = h[row]; mask = m[row];
                }

                drawTextWithShadow(ctx.textBuf, cx + 10.0f, cy + 6.0f, 0.36f, ctx.clrWhite, actTitle, 0.18f);
                drawText(ctx.textBuf, cx + 10.0f, cy + 21.0f, 0.29f, Ui::rgba(165, 160, 195), actHint, 0.18f);

                std::string kFmt = truncateString(GameSettings::formatKeyMask(mask), 14);
                Ui::pill(cx + 172.0f, cy + 8.0f, 114.0f, 22.0f, 0.19f, Ui::rgba(85, 22, 58, 245));
                drawRectOutline(cx + 172.0f, cy + 8.0f, 114.0f, 22.0f, 1.0f, Ui::rgba(255, 65, 150), 0.20f);
                drawCenteredTextWithShadow(ctx.textBuf, cx + 229.0f, cy + 19.0f, 0.32f, Ui::rgba(255, 110, 185), kFmt.c_str(), 0.21f);
            } else {
                std::string mTitle, mSub, mVal, mDet1, mDet2;
                getOptionMeta(opt, mTitle, mSub, mVal, mDet1, mDet2);

                drawTextWithShadow(ctx.textBuf, cx + 10.0f, cy + 5.0f, 0.36f, sel ? ctx.clrWhite : Ui::rgba(235, 230, 250),
                    truncateString(mTitle, 24).c_str(), 0.18f);
                drawText(ctx.textBuf, cx + 10.0f, cy + 21.0f, 0.28f, Ui::rgba(165, 160, 195),
                    truncateString(mSub, 28).c_str(), 0.18f);

                // Controles a la derecha según el tipo de apartado
                if (opt == 1 || opt == 2 || opt == 3 || opt == 8 || opt == 9 || opt == 10 || opt == 17 || opt == 18) {
                    float ratio = 0.5f;
                    if (opt == 1) ratio = (ctx.settings.tabletScale - 0.30f) / 0.70f;
                    else if (opt == 2) ratio = ctx.settings.tabletCenterX / 320.0f;
                    else if (opt == 3) ratio = ctx.settings.tabletCenterY / 240.0f;
                    else if (opt == 8) ratio = ctx.settings.musicVolume / 100.0f;
                    else if (opt == 9) ratio = ctx.settings.sfxVolume / 100.0f;
                    else if (opt == 10) ratio = (ctx.settings.audioOffsetMs + 150.0f) / 300.0f;
                    else if (opt == 17) ratio = (ctx.settings.cursorScale - 0.5f) / 1.5f;
                    else if (opt == 18) ratio = (ctx.settings.bgDim - 20.0f) / 80.0f;

                    drawRightAlignedText(ctx.textBuf, cx + cw - 10.0f, cy + 5.0f, 0.31f, Ui::rgba(65, 220, 255), mVal.c_str(), 0.19f);
                    Ui::sliderBar(cx + 174.0f, cy + 21.0f, 110.0f, 9.0f, 0.19f, ratio, sel ? Ui::rgba(255, 65, 150) : Ui::rgba(65, 195, 255), sel);
                } else if (opt == 11 || opt == 27 || opt == 14 || opt == 15 || opt == 20 || opt == 28 || opt == 19) {
                    bool isOn = false;
                    if (opt == 11) isOn = ctx.settings.beatmapHitsounds;
                    else if (opt == 27) isOn = ctx.settings.songPreview;
                    else if (opt == 14) isOn = ctx.settings.snakingSliders;
                    else if (opt == 15) isOn = ctx.settings.comboBursts;
                    else if (opt == 20) isOn = ctx.settings.showHit300;
                    else if (opt == 28) isOn = ctx.settings.showPlayfieldBorder;
                    else if (opt == 19) isOn = ctx.settings.enableVideo;
                    Ui::toggleSwitch(cx + cw - 58.0f, cy + 10.0f, 48.0f, 18.0f, 0.19f, isOn, Ui::rgba(45, 225, 150));
                } else {
                    u32 pillBg = sel ? Ui::rgba(255, 42, 133, 240) : Ui::rgba(45, 34, 76, 240);
                    if (opt == 0) pillBg = ctx.settings.playOnBottomScreen ? Ui::rgba(28, 145, 105, 240) : Ui::rgba(145, 40, 135, 240);
                    else if (opt == 24 || opt == 26) pillBg = Ui::rgba(35, 125, 215, 240);
                    Ui::pill(cx + 174.0f, cy + 8.0f, 112.0f, 22.0f, 0.19f, pillBg);
                    drawCenteredTextWithShadow(ctx.textBuf, cx + 230.0f, cy + 19.0f, 0.31f, ctx.clrWhite,
                        truncateString(mVal, 15).c_str(), 0.21f);
                }
            }
        }

        // Barra de desplazamiento vertical derecha (x = 309, y = vTop+2..vBot-2)
        float sbX = 309.0f;
        float sbY = vTop + 2.0f;
        float sbH = (vBot - vTop) - 4.0f;
        C2D_DrawRectSolid(sbX + 1.0f, sbY, 0.25f, 3.0f, sbH, Ui::rgba(26, 22, 44, 220));
        if (maxSc > 0.0f) {
            float thumbH = std::max(20.0f, sbH * ((vBot - vTop) / ((vBot - vTop) + maxSc)));
            float thumbY = sbY + (curScroll / maxSc) * (sbH - thumbH);
            C2D_DrawRectSolid(sbX, thumbY, 0.27f, 5.0f, thumbH, Ui::rgba(255, 65, 150, 240));
        } else {
            C2D_DrawRectSolid(sbX + 1.0f, sbY, 0.27f, 3.0f, sbH, Ui::rgba(95, 75, 145, 180));
        }

        // --- CAPA SUPERIOR FIJA EN PANTALLA INFERIOR (Cubre bordes del scroll con z = 0.42..0.55) ---
        float headerH = (activeTab == 1) ? 54.0f : 31.0f;
        C2D_DrawRectSolid(0.0f, 0.0f, 0.40f, 320.0f, headerH, Ui::rgba(12, 9, 22, 252));
        C2D_DrawLine(0.0f, headerH, Ui::rgba(255, 55, 145, 160), 320.0f, headerH, Ui::rgba(255, 55, 145, 160), 1.0f, 0.41f);

        // 5 Pestañas superiores estilo osu!lazer (PANTALLA | TECLAS | AUDIO | JUEGO | VISUAL)
        for (int t = 0; t < 5; ++t) {
            float tx = 6.0f + t * 62.0f;
            float tw = 59.0f;
            float th = 22.0f;
            bool isTab = (activeTab == t);
            u32 tabBg = isTab ? Ui::rgba(85, 18, 56, 250) : Ui::rgba(20, 17, 34, 240);
            Ui::panel(tx, 4.0f, tw, th, 0.44f, tabBg, 0);
            if (isTab) {
                drawRectOutline(tx, 4.0f, tw, th, 1.5f, Ui::rgba(255, 65, 150), 0.46f);
            }
            drawCenteredTextWithShadow(ctx.textBuf, tx + tw * 0.5f, 15.0f, 0.30f,
                isTab ? ctx.clrWhite : Ui::rgba(165, 160, 195), tabShortNames[t], 0.48f);
        }

        // Sub-selector de modo en la pestaña TECLAS: [Standard] [Taiko] [Catch] [Mania]
        if (activeTab == 1) {
            const char* modePills[4] = { "Standard", "Taiko", "Catch", "Mania" };
            for (int m = 0; m < 4; ++m) {
                float mx = 8.0f + m * 76.0f;
                float mw = 72.0f;
                float mh = 19.0f;
                bool isCurM = (ctx.settingsControlsMode == m);
                u32 mBg = isCurM ? Ui::rgba(255, 38, 130, 250) : Ui::rgba(22, 19, 36, 240);
                Ui::pill(mx, 31.0f, mw, mh, 0.45f, mBg);
                drawCenteredTextWithShadow(ctx.textBuf, mx + mw * 0.5f, 40.5f, 0.31f,
                    isCurM ? ctx.clrWhite : Ui::rgba(175, 170, 205), modePills[m], 0.48f);
            }
        }

        // --- PIE FIJO DE PANTALLA INFERIOR (y = 199..240, z = 0.42..0.55) ---
        C2D_DrawRectSolid(0.0f, 199.0f, 0.40f, 320.0f, 41.0f, Ui::rgba(12, 9, 22, 252));
        C2D_DrawLine(0.0f, 199.0f, Ui::rgba(60, 50, 95, 180), 320.0f, 199.0f, Ui::rgba(60, 50, 95, 180), 1.0f, 0.41f);

        // Botón Izquierdo: [<- Volver al Menú]
        Ui::button(8.0f, 205.0f, 146.0f, 28.0f, 0.44f, Ui::rgba(24, 22, 38, 250), Ui::rgba(75, 70, 110), false);
        drawCenteredTextWithShadow(ctx.textBuf, 81.0f, 219.0f, 0.34f, Ui::rgba(220, 215, 240),
            esp ? "<- Volver al Menú" : "<- Back to Menu", 0.48f);

        // Botón Derecho: [Guardar Cambios] en rosa osu!lazer
        Ui::button(166.0f, 205.0f, 146.0f, 28.0f, 0.44f, Ui::rgba(255, 38, 130, 250), Ui::rgba(255, 145, 200), true);
        drawCenteredTextWithShadow(ctx.textBuf, 239.0f, 219.0f, 0.35f, ctx.clrWhite,
            esp ? "Guardar Cambios" : "Save Changes", 0.48f);
    }

    // Modal Táctil de Asignación de Teclas cuando se está capturando un botón
    if (ctx.capturingKeyIndex != -1) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.85f, 320.0f, 240.0f, Ui::rgba(8, 6, 16, 235));
        Ui::panel(10.0f, 24.0f, 300.0f, 192.0f, 0.88f, Ui::rgba(20, 15, 36, 252), Ui::rgba(255, 65, 150));

        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 38.0f, 0.42f, Ui::rgba(255, 85, 170),
            esp ? "PRESIONA UN BOTÓN O TÓCALO ABAJO" : "PRESS ANY BUTTON OR TAP BELOW", 0.92f);
        drawCenteredText(ctx.textBuf, 160.0f, 58.0f, 0.32f, Ui::rgba(195, 190, 225),
            esp ? "Pulsa B físico para cancelar o toca el botón en pantalla" : "Press physical B to cancel or tap a button chip below", 0.92f);

        struct TouchKeyChip { float x, y, w, h; const char* lbl; u32 clr; };
        const TouchKeyChip chips[] = {
            { 18.0f,  86.0f, 42.0f, 22.0f, "L",      Ui::rgba(55, 48, 85) },
            { 64.0f,  86.0f, 42.0f, 22.0f, "ZL",     Ui::rgba(45, 40, 72) },
            { 214.0f, 86.0f, 42.0f, 22.0f, "ZR",     Ui::rgba(45, 40, 72) },
            { 260.0f, 86.0f, 42.0f, 22.0f, "R",      Ui::rgba(55, 48, 85) },
            { 18.0f,  116.0f, 42.0f, 22.0f, "D-Arr", Ui::rgba(48, 42, 76) },
            { 64.0f,  116.0f, 42.0f, 22.0f, "D-Aba", Ui::rgba(48, 42, 76) },
            { 18.0f,  144.0f, 42.0f, 22.0f, "D-Izq", Ui::rgba(48, 42, 76) },
            { 64.0f,  144.0f, 42.0f, 22.0f, "D-Der", Ui::rgba(48, 42, 76) },
            { 214.0f, 116.0f, 42.0f, 22.0f, "X",     Ui::rgba(45, 115, 210) },
            { 260.0f, 116.0f, 42.0f, 22.0f, "Y",     Ui::rgba(40, 175, 95) },
            { 214.0f, 144.0f, 42.0f, 22.0f, "A",     Ui::rgba(220, 55, 75) },
            { 260.0f, 144.0f, 42.0f, 22.0f, "B",     Ui::rgba(215, 165, 35) },
            { 114.0f, 144.0f, 44.0f, 22.0f, "SEL",   Ui::rgba(60, 52, 90) },
            { 162.0f, 144.0f, 44.0f, 22.0f, "START", Ui::rgba(60, 52, 90) }
        };
        for (const auto& ch : chips) {
            Ui::pill(ch.x, ch.y, ch.w, ch.h, 0.91f, ch.clr);
            drawCenteredTextWithShadow(ctx.textBuf, ch.x + ch.w * 0.5f, ch.y + ch.h * 0.5f, 0.32f, ctx.clrWhite, ch.lbl, 0.93f);
        }

        Ui::button(90.0f, 180.0f, 140.0f, 24.0f, 0.91f, Ui::rgba(45, 32, 68), Ui::rgba(255, 75, 150), false);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 192.0f, 0.33f, ctx.clrWhite,
            esp ? "Cancelar (B)" : "Cancel (B)", 0.94f);
    }

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.95f, 10.0f, Ui::withAlpha(ctx.clr300, 130), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.96f, 4.0f, ctx.clr300);
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
