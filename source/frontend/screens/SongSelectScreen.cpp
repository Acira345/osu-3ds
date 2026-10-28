#include "SongSelectScreen.hpp"
#include "ModsSelectScreen.hpp"

namespace {

// Dibuja una estrella vectorial de 5 puntas (★)
void drawStarVector(float cx, float cy, float r, float z, u32 color) {
    for (int i = 0; i < 5; ++i) {
        float a0 = -1.5707963f + i * 1.2566371f;
        float a1 = a0 + 0.6283185f;
        float a2 = a0 - 0.6283185f;
        float xTip = cx + std::cos(a0) * r;
        float yTip = cy + std::sin(a0) * r;
        float xR   = cx + std::cos(a1) * (r * 0.42f);
        float yR   = cy + std::sin(a1) * (r * 0.42f);
        float xL   = cx + std::cos(a2) * (r * 0.42f);
        float yL   = cy + std::sin(a2) * (r * 0.42f);
        C2D_DrawTriangle(cx, cy, color, xL, yL, color, xTip, yTip, color, z);
        C2D_DrawTriangle(cx, cy, color, xTip, yTip, color, xR, yR, color, z);
    }
}

// Dibuja un icono vectorial representativo de cada modo de juego (0: Standard, 1: Taiko, 2: Catch, 3: Mania)
void drawModeIcon(float cx, float cy, int mode, float z, float scale = 1.0f) {
    switch (mode) {
        case 1: { // osu!taiko: Tambor Don / Katsu
            // Aro exterior azul (Katsu)
            C2D_DrawCircleSolid(cx, cy, z, 6.2f * scale, Ui::rgba(0, 160, 245));
            // Núcleo central rojo (Don)
            C2D_DrawCircleSolid(cx, cy, z + 0.005f, 4.0f * scale, Ui::rgba(240, 60, 50));
            // Punto central blanco
            C2D_DrawCircleSolid(cx, cy, z + 0.010f, 1.4f * scale, Ui::rgba(255, 255, 255));
            break;
        }
        case 2: { // osu!catch: Fruta (Manzana con hojita)
            // Cuerpo de la manzana (rojo vivo)
            C2D_DrawCircleSolid(cx, cy + 0.8f * scale, z, 4.8f * scale, Ui::rgba(235, 65, 75));
            // Brillo de la fruta
            C2D_DrawCircleSolid(cx - 1.4f * scale, cy - 0.2f * scale, z + 0.005f, 1.2f * scale, Ui::rgba(255, 175, 185));
            // Hoja verde
            u32 leafClr = Ui::rgba(46, 213, 115);
            C2D_DrawTriangle(cx, cy - 3.0f * scale, leafClr,
                             cx + 3.2f * scale, cy - 5.5f * scale, leafClr,
                             cx + 3.2f * scale, cy - 2.5f * scale, leafClr, z + 0.010f);
            break;
        }
        case 3: { // osu!mania: 4 columnas / teclas de piano (cyan / blanco / blanco / cyan)
            float kw = 2.4f * scale;
            float kh = 9.6f * scale;
            float kx = cx - 5.1f * scale;
            float ky = cy - 4.8f * scale;
            // Marco exterior oscuro
            C2D_DrawRectSolid(kx - 1.0f, ky - 1.0f, z, (kw * 4.0f + 3.0f * 0.6f) + 2.0f, kh + 2.0f, Ui::rgba(18, 12, 30));
            u32 cKey = Ui::rgba(56, 210, 255);
            u32 wKey = Ui::rgba(240, 240, 250);
            C2D_DrawRectSolid(kx + 0.0f * (kw + 0.6f), ky, z + 0.005f, kw, kh, cKey);
            C2D_DrawRectSolid(kx + 1.0f * (kw + 0.6f), ky, z + 0.005f, kw, kh, wKey);
            C2D_DrawRectSolid(kx + 2.0f * (kw + 0.6f), ky, z + 0.005f, kw, kh, wKey);
            C2D_DrawRectSolid(kx + 3.0f * (kw + 0.6f), ky, z + 0.005f, kw, kh, cKey);
            break;
        }
        default: { // Mode 0 (osu!standard): Círculo clásico de osu!
            // Aro exterior rosa/magenta
            drawRing(cx, cy, 5.8f * scale, 1.4f * scale, Ui::rgba(255, 102, 171), 16, z);
            // Núcleo central blanco
            C2D_DrawCircleSolid(cx, cy, z + 0.005f, 2.6f * scale, Ui::rgba(255, 255, 255));
            break;
        }
    }
}

// Emblema compacto de Grado (SS, S, A, B, C, D)
void drawGradeBadge(C2D_TextBuf textBuf, float x, float y, float size, float z,
                    const std::string& grade, u32 gradeClr) {
    Ui::glow(x + size * 0.5f, y + size * 0.5f, z - 0.01f, size * 0.65f, Ui::withAlpha(gradeClr, 75), 2);
    Ui::panel(x, y, size, size, z, Ui::rgba(20, 14, 32, 210), gradeClr);
    drawRectOutline(x, y, size, size, 1.2f, gradeClr, z + 0.01f);
    const char* gStr = grade.empty() ? "-" : grade.c_str();
    drawCenteredTextWithShadow(textBuf, x + size * 0.5f, y + size * 0.5f - 1.5f,
                               0.68f, gradeClr, gStr, z + 0.02f, Ui::rgba(0, 0, 0, 220));
}

// Icono vectorial de Trofeo para el botón de Récords Locales
void drawTrophyIcon(float cx, float cy, float z, u32 color) {
    C2D_DrawTriangle(cx - 5.5f, cy - 6.0f, color,
                     cx + 5.5f, cy - 6.0f, color,
                     cx, cy + 0.5f, color, z);
    C2D_DrawRectSolid(cx - 5.5f, cy - 6.0f, z, 11.0f, 3.5f, color);
    drawRing(cx - 5.5f, cy - 3.5f, 2.2f, 1.0f, color, 8, z);
    drawRing(cx + 5.5f, cy - 3.5f, 2.2f, 1.0f, color, 8, z);
    C2D_DrawRectSolid(cx - 1.2f, cy + 0.5f, z, 2.4f, 3.5f, color);
    Ui::pill(cx - 5.5f, cy + 4.0f, 11.0f, 2.6f, z, color);
    C2D_DrawCircleSolid(cx - 1.0f, cy - 3.5f, z + 0.005f, 1.1f, Ui::rgba(255, 255, 255, 230));
}

// Icono vectorial de Bote de Basura para el botón de Borrar
void drawTrashIcon(float cx, float cy, float z, u32 color) {
    C2D_DrawLine(cx - 2.5f, cy - 6.5f, color, cx + 2.5f, cy - 6.5f, color, 1.2f, z);
    Ui::pill(cx - 6.5f, cy - 5.0f, 13.0f, 2.4f, z, color);
    C2D_DrawRectSolid(cx - 4.5f, cy - 2.2f, z, 9.0f, 8.0f, color);
    u32 ribClr = Ui::rgba(20, 10, 24);
    C2D_DrawLine(cx - 2.2f, cy - 1.0f, ribClr, cx - 2.2f, cy + 4.5f, ribClr, 1.0f, z + 0.005f);
    C2D_DrawLine(cx, cy - 1.0f, ribClr, cx, cy + 4.5f, ribClr, 1.0f, z + 0.005f);
    C2D_DrawLine(cx + 2.2f, cy - 1.0f, ribClr, cx + 2.2f, cy + 4.5f, ribClr, 1.0f, z + 0.005f);
    Ui::pill(cx - 4.5f, cy + 4.5f, 9.0f, 2.0f, z, color);
}

// Icono vectorial de Stylus / Modo Táctil
void drawTouchIcon(float cx, float cy, float z, u32 color) {
    drawRectOutline(cx - 6.0f, cy - 6.0f, 12.0f, 13.0f, 1.0f, color, z);
    drawRing(cx, cy - 0.5f, 2.5f, 1.0f, Ui::brighten(color, 1.35f), 10, z + 0.005f);
    C2D_DrawCircleSolid(cx, cy - 0.5f, z + 0.008f, 1.2f, Ui::rgba(255, 255, 255));
    C2D_DrawLine(cx - 2.0f, cy + 5.0f, color, cx + 2.0f, cy + 5.0f, color, 1.0f, z + 0.005f);
}

// Icono vectorial de Lupa para el botón Buscar
void drawSearchIcon(float cx, float cy, float z, u32 color) {
    drawRing(cx - 1.5f, cy - 1.5f, 4.4f, 1.4f, color, 14, z);
    C2D_DrawCircleSolid(cx - 2.5f, cy - 2.5f, z + 0.005f, 1.0f, Ui::rgba(255, 255, 255, 200));
    C2D_DrawLine(cx + 1.8f, cy + 1.8f, color, cx + 5.8f, cy + 5.8f, color, 2.2f, z);
}

// Icono vectorial de Ajustes/Ecualizador para el botón Mods
void drawModsIcon(float cx, float cy, float z, u32 color) {
    u32 trackClr = Ui::withAlpha(color, 140);
    C2D_DrawLine(cx - 4.5f, cy - 5.5f, trackClr, cx - 4.5f, cy + 5.5f, trackClr, 1.2f, z);
    C2D_DrawCircleSolid(cx - 4.5f, cy - 1.8f, z + 0.005f, 2.0f, color);

    C2D_DrawLine(cx, cy - 5.5f, trackClr, cx, cy + 5.5f, trackClr, 1.2f, z);
    C2D_DrawCircleSolid(cx, cy + 2.2f, z + 0.005f, 2.0f, color);

    C2D_DrawLine(cx + 4.5f, cy - 5.5f, trackClr, cx + 4.5f, cy + 5.5f, trackClr, 1.2f, z);
    C2D_DrawCircleSolid(cx + 4.5f, cy - 0.5f, z + 0.005f, 2.0f, color);
}

// Estructura de elementos del carrusel unificado estilo osu!lazer
struct CarouselItem {
    enum Type { SET, DIFF } type;
    int setIdx;
    int diffIdx;
    float y;
    float h;
};

// Estado persistente del carrusel con inercia y arrastre táctil suave
float s_scrollY = 0.0f;
float s_targetScrollY = 0.0f;
float s_scrollVelocity = 0.0f;
bool  s_isDragging = false;
float s_touchStartX = 0.0f;
float s_touchStartY = 0.0f;
float s_touchLastY = 0.0f;
float s_touchDragAccum = 0.0f;
int   s_lastSelectedSet = -1;
int   s_lastSelectedDiff = -1;
bool  s_lastCardExpanded = false;
bool  s_cardExpanded = false;
bool  s_focusOnDiff = false;

void resetSongSelectScroll() {
    s_scrollY = 0.0f;
    s_targetScrollY = 0.0f;
    s_scrollVelocity = 0.0f;
    s_isDragging = false;
    s_lastSelectedSet = -1;
    s_lastSelectedDiff = -1;
    s_lastCardExpanded = false;
    s_cardExpanded = false;
    s_focusOnDiff = false;
}

} // namespace

void runSongSelectScreen(AppContext& ctx) {
    ctx.audio.update();

    auto autoSelectDiffForMode = [&](int targetMode) {
        if (!ctx.songMgr.sets.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            if (ctx.selectedDiff >= 0 && ctx.selectedDiff < static_cast<int>(curSet.difficulties.size())) {
                if (curSet.difficulties[ctx.selectedDiff].mode != targetMode) {
                    for (size_t d = 0; d < curSet.difficulties.size(); ++d) {
                        if (curSet.difficulties[d].mode == targetMode) {
                            ctx.selectedDiff = static_cast<int>(d);
                            break;
                        }
                    }
                }
            }
        }
    };

    // Construcción dinámica de la lista de tarjetas (Sets y Dificultades expandidas si la tarjeta está abierta)
    std::vector<CarouselItem> items;
    float totalContentHeight = 0.0f;
    float viewHeight = 170.0f;
    float maxScroll = 0.0f;

    auto rebuildCarouselItems = [&]() {
        items.clear();
        int numSets = static_cast<int>(ctx.songMgr.sets.size());
        float curListY = 6.0f;
        for (int s = 0; s < numSets; ++s) {
            bool isSelSet = (s == ctx.selectedSet);
            CarouselItem setItm;
            setItm.type = CarouselItem::SET;
            setItm.setIdx = s;
            setItm.diffIdx = 0;
            setItm.y = curListY;
            setItm.h = isSelSet ? 40.0f : 38.0f;
            items.push_back(setItm);
            curListY += setItm.h + 4.0f;

            if (isSelSet && s_cardExpanded) {
                const auto& curSet = ctx.songMgr.sets[s];
                int nDiffs = static_cast<int>(curSet.difficulties.size());
                for (int d = 0; d < nDiffs; ++d) {
                    CarouselItem diffItm;
                    diffItm.type = CarouselItem::DIFF;
                    diffItm.setIdx = s;
                    diffItm.diffIdx = d;
                    diffItm.y = curListY;
                    diffItm.h = 26.0f;
                    items.push_back(diffItm);
                    curListY += diffItm.h + 3.0f;
                }
                curListY += 2.0f;
            }
        }
        totalContentHeight = curListY + 6.0f;
        maxScroll = std::max(0.0f, totalContentHeight - viewHeight);
    };

    rebuildCarouselItems();

    // Debounce de preview para fluidez a 60 FPS continuos
    bool carouselSettled = (std::fabs(ctx.carouselPos - static_cast<float>(ctx.selectedSet)) < 0.05f) &&
                           (!s_isDragging) && (std::fabs(s_scrollVelocity) < 0.5f);
    if (!carouselSettled) {
        if (ctx.previewDelay < (ctx.isNew3DS ? 10 : 16)) {
            ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
        }
    } else if (ctx.previewDelay > 0) {
        if (--ctx.previewDelay == 0) {
            ctx.updateSongPreview();
        }
    }

    // Física de scroll suave e inercia táctil
    if (!s_isDragging) {
        if (std::fabs(s_scrollVelocity) > 0.05f) {
            s_scrollY += s_scrollVelocity;
            s_scrollVelocity *= 0.88f;
            if (std::fabs(s_scrollVelocity) < 0.1f) s_scrollVelocity = 0.0f;
            s_targetScrollY = s_scrollY;
        }
        s_scrollY = Ui::approach(s_scrollY, s_targetScrollY, 0.20f);
        if (s_scrollY < 0.0f) {
            s_targetScrollY = 0.0f;
            s_scrollY = Ui::approach(s_scrollY, 0.0f, 0.25f);
        } else if (s_scrollY > maxScroll) {
            s_targetScrollY = maxScroll;
            s_scrollY = Ui::approach(s_scrollY, maxScroll, 0.25f);
        }
    }

    static bool s_lastFocusOnDiff = false;
    // Centrado suave en el medio de la pantalla cuando cambia la selección o se abre/cierra la tarjeta
    if (s_lastSelectedSet != ctx.selectedSet || s_lastSelectedDiff != ctx.selectedDiff ||
        s_lastCardExpanded != s_cardExpanded || s_lastFocusOnDiff != s_focusOnDiff) {
        s_lastSelectedSet = ctx.selectedSet;
        s_lastSelectedDiff = ctx.selectedDiff;
        s_lastCardExpanded = s_cardExpanded;
        s_lastFocusOnDiff = s_focusOnDiff;
        if (!s_isDragging && !items.empty()) {
            for (const auto& itm : items) {
                bool isTarget = false;
                if (s_cardExpanded && s_focusOnDiff) {
                    isTarget = (itm.type == CarouselItem::DIFF && itm.setIdx == ctx.selectedSet && itm.diffIdx == ctx.selectedDiff);
                } else {
                    isTarget = (itm.type == CarouselItem::SET && itm.setIdx == ctx.selectedSet);
                }
                if (isTarget) {
                    float itemCenterInList = itm.y + itm.h * 0.5f;
                    float desiredScroll = itemCenterInList - 85.0f; // 85px = centro cómodo del visor de 170px
                    s_targetScrollY = std::max(0.0f, std::min(maxScroll, desiredScroll));
                    break;
                }
            }
        }
    }

    // Subida a GPU del fondo en segundo plano
    if (ctx.bgLoader.poll(ctx.bgTex)) ctx.bgFade = 0.0f;
    ctx.bgFade = Ui::approach(ctx.bgFade, 255.0f, 0.12f);
    if (ctx.settings.songPreview && ctx.audio.isSongFinished() && !ctx.audio.isPreviewLoading()) {
        ctx.updateSongPreview();
    }

    // Botón SELECT: Alternar entre navegación de canciones y navegación de repeticiones (Replays)
    if ((ctx.kDown & KEY_SELECT) && !ctx.deleteConfirmOpen && !ctx.searchKeyboardOpen && !ctx.modMenuOpen) {
        if (!ctx.rankingExpanded) {
            ctx.rankingExpanded = true;
            ctx.songInfoHidden = false;
            ctx.selectingReplayMode = true;
            ctx.selectedRecordIdx = 0;
            ctx.audio.playHitSound(2);
        } else {
            ctx.selectingReplayMode = !ctx.selectingReplayMode;
            if (ctx.selectingReplayMode) ctx.selectedRecordIdx = 0;
            ctx.audio.playHitSound(ctx.selectingReplayMode ? 2 : 0);
        }
    }

    // =========================================================================
    // MODALES: BORRAR, MODS, TECLADO DE BÚSQUEDA
    // =========================================================================
    if (ctx.deleteConfirmOpen) {
        if (ctx.kDown & KEY_B) {
            ctx.deleteConfirmOpen = false;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_A) {
            if (!ctx.songMgr.sets.empty() && ctx.selectedSet >= 0 && ctx.selectedSet < static_cast<int>(ctx.songMgr.sets.size())) {
                ctx.audio.stopMusic();
                ctx.songMgr.deleteSet(ctx.selectedSet, &ctx.scoreMgr);
                if (ctx.selectedSet >= static_cast<int>(ctx.songMgr.sets.size())) {
                    ctx.selectedSet = std::max(0, static_cast<int>(ctx.songMgr.sets.size()) - 1);
                }
                ctx.selectedDiff = 0;
                ctx.carouselPos = static_cast<float>(ctx.selectedSet);
                resetSongSelectScroll();
                ctx.audio.playHitSound(2);
                ctx.updateSongPreview();
            }
            ctx.deleteConfirmOpen = false;
        }
        if (ctx.touchJustDown) {
            if (ctx.touch.px >= 28 && ctx.touch.px <= 154 && ctx.touch.py >= 146 && ctx.touch.py <= 184) {
                ctx.deleteConfirmOpen = false;
                ctx.audio.playHitSound(0);
            } else if (ctx.touch.px >= 166 && ctx.touch.px <= 292 && ctx.touch.py >= 146 && ctx.touch.py <= 184) {
                if (!ctx.songMgr.sets.empty() && ctx.selectedSet >= 0 && ctx.selectedSet < static_cast<int>(ctx.songMgr.sets.size())) {
                    ctx.audio.stopMusic();
                    ctx.songMgr.deleteSet(ctx.selectedSet, &ctx.scoreMgr);
                    if (ctx.selectedSet >= static_cast<int>(ctx.songMgr.sets.size())) {
                        ctx.selectedSet = std::max(0, static_cast<int>(ctx.songMgr.sets.size()) - 1);
                    }
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = static_cast<float>(ctx.selectedSet);
                    resetSongSelectScroll();
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                }
                ctx.deleteConfirmOpen = false;
            }
        }
    } else if (ctx.modMenuOpen) {
        handleModsInput(ctx, false);
    } else if (ctx.searchKeyboardOpen) {
        if (ctx.searchKeyFlashTimer > 0) ctx.searchKeyFlashTimer--;

        if (ctx.kDown & (KEY_B | KEY_START)) {
            ctx.searchKeyboardOpen = false;
            ctx.audio.playHitSound(0);
        } else if (ctx.kDown & KEY_X) {
            ctx.songMgr.searchQuery.clear();
            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
            ctx.selectedSet = 0;
            ctx.selectedDiff = 0;
            ctx.carouselPos = 0.0f;
            resetSongSelectScroll();
            ctx.audio.playHitSound(0);
            ctx.updateSongPreview();
        } else if (ctx.kDown & KEY_Y) {
            if (!ctx.songMgr.searchQuery.empty()) {
                ctx.songMgr.searchQuery.pop_back();
                ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                ctx.selectedSet = 0;
                ctx.selectedDiff = 0;
                ctx.carouselPos = 0.0f;
                resetSongSelectScroll();
                ctx.audio.playHitSound(0);
                ctx.updateSongPreview();
            }
        } else if (ctx.kDown & (KEY_ZL | KEY_ZR | KEY_A)) {
            ctx.searchKeyboardOpen = false;
            ctx.audio.playHitSound(2);
        }

        if (!ctx.songMgr.sets.empty()) {
            int numSets = static_cast<int>(ctx.songMgr.sets.size());
            int setDelta = 0;
            if (ctx.kDown & (KEY_DUP | KEY_L))   setDelta = -1;
            if (ctx.kDown & (KEY_DDOWN | KEY_R)) setDelta = +1;
            if (setDelta != 0) {
                ctx.selectedSet = (ctx.selectedSet + setDelta + numSets) % numSets;
                ctx.selectedDiff = 0;
                ctx.carouselPos = static_cast<float>(ctx.selectedSet);
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
            }
        }

        if (ctx.touchJustDown) {
            if (ctx.touch.px >= 232.0f && ctx.touch.px <= 258.0f && ctx.touch.py >= 3.0f && ctx.touch.py <= 31.0f) {
                ctx.songMgr.searchQuery.clear();
                ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                ctx.selectedSet = 0;
                ctx.selectedDiff = 0;
                ctx.carouselPos = 0.0f;
                resetSongSelectScroll();
                ctx.audio.playHitSound(0);
                ctx.updateSongPreview();
            } else if (ctx.touch.px >= 262.0f && ctx.touch.px <= 316.0f && ctx.touch.py >= 3.0f && ctx.touch.py <= 31.0f) {
                ctx.searchKeyboardOpen = false;
                ctx.audio.playHitSound(2);
            } else if (ctx.touch.py >= 34.0f && ctx.touch.py <= 52.0f) {
                if (ctx.touch.px >= 4.0f && ctx.touch.px <= 44.0f) {
                    ctx.songMgr.searchQuery.clear();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = 0.0f;
                    resetSongSelectScroll();
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 47.0f && ctx.touch.px <= 79.0f)  ctx.toggleSearchTag("mode:std");
                else if (ctx.touch.px >= 82.0f && ctx.touch.px <= 122.0f)   ctx.toggleSearchTag("mode:taiko");
                else if (ctx.touch.px >= 125.0f && ctx.touch.px <= 165.0f)  ctx.toggleSearchTag("mode:catch");
                else if (ctx.touch.px >= 168.0f && ctx.touch.px <= 208.0f)  ctx.toggleSearchTag("mode:mania");
                else if (ctx.touch.px >= 211.0f && ctx.touch.px <= 243.0f)  ctx.toggleSearchTag("★4");
                else if (ctx.touch.px >= 246.0f && ctx.touch.px <= 278.0f)  ctx.toggleSearchTag("★5");
            } else if (ctx.touch.py >= 54.0f && ctx.touch.py <= 86.0f) {
                const char* numChars = "1234567890";
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 26.5f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 24.5f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += numChars[i];
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.carouselPos = 0.0f;
                            resetSongSelectScroll();
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
                if (ctx.touch.px >= 271.0f && ctx.touch.px <= 316.0f && !ctx.songMgr.searchQuery.empty()) {
                    ctx.songMgr.searchQuery.pop_back();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = 0.0f;
                    resetSongSelectScroll();
                    ctx.audio.playHitSound(0);
                    ctx.updateSongPreview();
                }
            } else if (ctx.touch.py >= 89.0f && ctx.touch.py <= 121.0f) {
                const char* r1Chars = "qwertyuiop";
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 31.0f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 29.0f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r1Chars[i];
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.carouselPos = 0.0f;
                            resetSongSelectScroll();
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            } else if (ctx.touch.py >= 124.0f && ctx.touch.py <= 156.0f) {
                const char* r2Chars = "asdfghjkl-";
                for (int i = 0; i < 10; ++i) {
                    float kx = 8.0f + i * 30.2f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 28.2f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r2Chars[i];
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.carouselPos = 0.0f;
                            resetSongSelectScroll();
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            } else if (ctx.touch.py >= 159.0f && ctx.touch.py <= 191.0f) {
                const char* r3Chars = "zxcvbnm./";
                for (int i = 0; i < 9; ++i) {
                    float kx = 16.0f + i * 32.0f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 29.8f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r3Chars[i];
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.carouselPos = 0.0f;
                            resetSongSelectScroll();
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            } else if (ctx.touch.py >= 194.0f && ctx.touch.py <= 238.0f) {
                if (ctx.touch.px >= 4.0f && ctx.touch.px <= 60.0f) {
                    ctx.songMgr.searchQuery.clear();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = 0.0f;
                    resetSongSelectScroll();
                    ctx.audio.playHitSound(0);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 63.0f && ctx.touch.px <= 205.0f) {
                    if (ctx.songMgr.searchQuery.size() < 32 && !ctx.songMgr.searchQuery.empty() && ctx.songMgr.searchQuery.back() != ' ') {
                        ctx.songMgr.searchQuery += ' ';
                        ctx.audio.playHitSound(0);
                    }
                } else if (ctx.touch.px >= 208.0f && ctx.touch.px <= 260.0f && !ctx.songMgr.searchQuery.empty()) {
                    ctx.songMgr.searchQuery.pop_back();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = 0.0f;
                    resetSongSelectScroll();
                    ctx.audio.playHitSound(0);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 263.0f && ctx.touch.px <= 316.0f) {
                    ctx.searchKeyboardOpen = false;
                    ctx.audio.playHitSound(2);
                }
            }
        }
    } else {
        // =========================================================================
        // NAVEGACIÓN Y CONTROLES FÍSICOS
        // =========================================================================
        if (ctx.kDown & KEY_B) {
            ctx.audio.playHitSound(0);
            if (ctx.rankingExpanded) {
                ctx.rankingExpanded = false;
                return;
            }
            if (ctx.multiMgr.inMultiplayer) {
                if (ctx.multiMgr.isHost()) {
                    ctx.multiMgr.hostChangingSong = false;
                    ctx.multiMgr.broadcastLobbyState();
                }
                ctx.appState = STATE_MULTI_LOBBY;
            } else {
                ctx.audio.setLoop(false);
                ctx.appState = STATE_MAIN_MENU;
            }
            return;
        }

        if (ctx.kDown & KEY_START) {
            ctx.settings.gameMode = (ctx.settings.gameMode + 1) % 4;
            ctx.settings.save();
            ctx.audio.playHitSound(2);
            if (!ctx.songMgr.sets.empty()) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                if (ctx.selectedDiff >= 0 && ctx.selectedDiff < static_cast<int>(curSet.difficulties.size())) {
                    if (curSet.difficulties[ctx.selectedDiff].mode != ctx.settings.gameMode) {
                        for (size_t d = 0; d < curSet.difficulties.size(); ++d) {
                            if (curSet.difficulties[d].mode == ctx.settings.gameMode) {
                                ctx.selectedDiff = static_cast<int>(d);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (ctx.kDown & (KEY_ZL | KEY_ZR)) {
            ctx.openSearchKeyboard();
        }

        // Botón X: Alternar vista estilizada de Récords Locales (Top 5 de la dificultad actual)
        if (ctx.kDown & KEY_X) {
            ctx.rankingExpanded = !ctx.rankingExpanded;
            if (ctx.rankingExpanded) ctx.songInfoHidden = false;
            else ctx.selectingReplayMode = false;
            ctx.audio.playHitSound(2);
        }

        // Botón Y: Ocultar / Mostrar los paneles de datos y dificultad en la pantalla superior
        // para apreciar el fondo del beatmap limpio
        if (ctx.kDown & KEY_Y) {
            ctx.songInfoHidden = !ctx.songInfoHidden;
            if (ctx.songInfoHidden) {
                ctx.rankingExpanded = false;
                ctx.selectingReplayMode = false;
            }
            ctx.audio.playHitSound(0);
        }

        if (ctx.selectingReplayMode) {
            // Cancelar modo de navegación de repeticiones con Botón B y regresar al carrusel
            if (ctx.kDown & KEY_B) {
                ctx.selectingReplayMode = false;
                ctx.audio.playHitSound(0);
            }

            int curModeIdx = 0;
            std::string curOsuPath = "";
            if (!ctx.songMgr.sets.empty() && ctx.selectedSet < static_cast<int>(ctx.songMgr.sets.size())) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                if (!curSet.difficulties.empty()) {
                    int dIdx = std::max(0, std::min(ctx.selectedDiff, static_cast<int>(curSet.difficulties.size()) - 1));
                    const auto& curDiff = curSet.difficulties[dIdx];
                    curOsuPath = curDiff.osuPath;
                    curModeIdx = (ctx.settings.gameMode == MODE_TAIKO || curDiff.mode == 1) ? 1
                               : (ctx.settings.gameMode == MODE_CATCH || curDiff.mode == 2) ? 2
                               : (ctx.settings.gameMode == MODE_MANIA || curDiff.mode == 3) ? 3 : 0;
                }
            }

            auto topScores = ctx.scoreMgr.getTopRecords(curOsuPath, 4, curModeIdx);
            int numRecs = static_cast<int>(topScores.size());

            if (numRecs > 0) {
                int recDelta = 0;
                if (ctx.kDown & (KEY_DUP | KEY_L))   recDelta = -1;
                if (ctx.kDown & (KEY_DDOWN | KEY_R)) recDelta = +1;
                if (recDelta == 0 && ctx.analogNavCooldown == 0) {
                    if (ctx.cstick.dy > 45 || ctx.cpad.dy > 65) {
                        recDelta = -1;
                        ctx.analogNavCooldown = 8;
                    } else if (ctx.cstick.dy < -45 || ctx.cpad.dy < -65) {
                        recDelta = +1;
                        ctx.analogNavCooldown = 8;
                    }
                }
                if (recDelta != 0) {
                    ctx.selectedRecordIdx = std::max(0, std::min(ctx.selectedRecordIdx + recDelta, numRecs - 1));
                    ctx.audio.playHitSound(0);
                }

                // Pulsar A y entrar a la repetición seleccionada
                if (ctx.kDown & KEY_A) {
                    int targetRank = ctx.selectedRecordIdx + 1;
                    if (ReplayManager::hasTopReplay(curOsuPath, curModeIdx, targetRank)) {
                        std::string rPath = ReplayManager::getTopReplayPath(curOsuPath, curModeIdx, targetRank);
                        if (ctx.playReplayFromFile(rPath)) {
                            ctx.audio.playHitSound(2);
                            return;
                        }
                    } else {
                        ctx.audio.playHitSound(4);
                    }
                }
            }
        } else if (!ctx.songMgr.sets.empty()) {
            int numSets = static_cast<int>(ctx.songMgr.sets.size());
            int navDir = 0;
            if (ctx.kDown & KEY_DUP)   navDir = -1;
            if (ctx.kDown & KEY_DDOWN) navDir = +1;

            if (navDir == 0 && ctx.analogNavCooldown == 0) {
                if (ctx.cstick.dy > 45 || ctx.cpad.dy > 65) {
                    navDir = -1;
                    ctx.analogNavCooldown = (std::abs(ctx.cstick.dy) > 90 || std::abs(ctx.cpad.dy) > 110) ? 5 : 8;
                } else if (ctx.cstick.dy < -45 || ctx.cpad.dy < -65) {
                    navDir = +1;
                    ctx.analogNavCooldown = (std::abs(ctx.cstick.dy) > 90 || std::abs(ctx.cpad.dy) > 110) ? 5 : 8;
                }
            }

            // Gatillos L / R: salto directo de canción (siguiente / anterior)
            if (ctx.kDown & KEY_L) {
                ctx.selectedSet = (ctx.selectedSet - 1 + numSets) % numSets;
                ctx.selectedDiff = 0;
                s_cardExpanded = false;
                s_focusOnDiff = false;
                autoSelectDiffForMode(ctx.settings.gameMode);
                ctx.songSelectCardAnim = 0.0f;
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                rebuildCarouselItems();
            } else if (ctx.kDown & KEY_R) {
                ctx.selectedSet = (ctx.selectedSet + 1) % numSets;
                ctx.selectedDiff = 0;
                s_cardExpanded = false;
                s_focusOnDiff = false;
                autoSelectDiffForMode(ctx.settings.gameMode);
                ctx.songSelectCardAnim = 0.0f;
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                rebuildCarouselItems();
            }

            // Navegación vertical: si está abierta baja entre dificultades, y al acabar pasa a la siguiente canción
            if (navDir != 0) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                int nDiffs = static_cast<int>(curSet.difficulties.size());

                if (navDir > 0) { // BAJAR
                    if (s_cardExpanded && nDiffs > 0) {
                        if (!s_focusOnDiff) {
                            s_focusOnDiff = true;
                            ctx.selectedDiff = 0;
                            ctx.audio.playHitSound(0);
                        } else if (ctx.selectedDiff < nDiffs - 1) {
                            ctx.selectedDiff++;
                            ctx.audio.playHitSound(0);
                        } else {
                            // Al acabar de bajar las dificultades, pasa a la siguiente canción cerrada
                            ctx.selectedSet = (ctx.selectedSet + 1) % numSets;
                            ctx.selectedDiff = 0;
                            s_cardExpanded = false;
                            s_focusOnDiff = false;
                            autoSelectDiffForMode(ctx.settings.gameMode);
                            ctx.songSelectCardAnim = 0.0f;
                            ctx.audio.playHitSound(0);
                            ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                        }
                    } else {
                        // Tarjeta cerrada: pasa a la siguiente canción
                        ctx.selectedSet = (ctx.selectedSet + 1) % numSets;
                        ctx.selectedDiff = 0;
                        s_cardExpanded = false;
                        s_focusOnDiff = false;
                        autoSelectDiffForMode(ctx.settings.gameMode);
                        ctx.songSelectCardAnim = 0.0f;
                        ctx.audio.playHitSound(0);
                        ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                    }
                } else { // SUBIR
                    if (s_cardExpanded && nDiffs > 0 && s_focusOnDiff) {
                        if (ctx.selectedDiff > 0) {
                            ctx.selectedDiff--;
                            ctx.audio.playHitSound(0);
                        } else {
                            // En la primera dificultad: subir regresa el foco a la cabecera de la tarjeta
                            s_focusOnDiff = false;
                            ctx.audio.playHitSound(0);
                        }
                    } else {
                        // En la cabecera o tarjeta cerrada: pasa a la canción anterior cerrada
                        ctx.selectedSet = (ctx.selectedSet - 1 + numSets) % numSets;
                        ctx.selectedDiff = 0;
                        s_cardExpanded = false;
                        s_focusOnDiff = false;
                        autoSelectDiffForMode(ctx.settings.gameMode);
                        ctx.songSelectCardAnim = 0.0f;
                        ctx.audio.playHitSound(0);
                        ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                    }
                }
                rebuildCarouselItems();
            }

            // Flechas Izquierda / Derecha: cambio rápido de dificultad
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int numDiffs = static_cast<int>(curSet.difficulties.size());
            if (numDiffs > 0) {
                int diffDelta = 0;
                if (ctx.kDown & KEY_DLEFT)  diffDelta = -1;
                if (ctx.kDown & KEY_DRIGHT) diffDelta = +1;
                if (diffDelta != 0) {
                    s_cardExpanded = true;
                    s_focusOnDiff = true;
                    ctx.selectedDiff = (ctx.selectedDiff + diffDelta + numDiffs) % numDiffs;
                    ctx.songSelectCardAnim = 0.25f;
                    ctx.audio.playHitSound(0);
                    ctx.previewDelay = ctx.isNew3DS ? 6 : 10;
                    rebuildCarouselItems();
                }
            }

            // Botón A: Abrir si está cerrada, Cerrar si está en cabecera, Jugar si está en dificultad
            if (ctx.kDown & KEY_A) {
                if (!s_cardExpanded) {
                    // Pulsar A en tarjeta cerrada la abre
                    s_cardExpanded = true;
                    s_focusOnDiff = false;
                    ctx.audio.playHitSound(2);
                    rebuildCarouselItems();
                } else if (!s_focusOnDiff) {
                    // Si está en la cabecera abierta, pulsar A la cierra
                    s_cardExpanded = false;
                    ctx.audio.playHitSound(0);
                    rebuildCarouselItems();
                } else {
                    // ¡En dificultad seleccionada: ahora sí juega!
                    if (ctx.multiMgr.inMultiplayer) {
                        const auto& s = ctx.songMgr.sets[ctx.selectedSet];
                        if (!s.difficulties.empty()) {
                            int dIdx = std::max(0, std::min(ctx.selectedDiff, static_cast<int>(s.difficulties.size()) - 1));
                            const auto& d = s.difficulties[dIdx];
                            QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, ctx.multiMgr.localSlot);
                            if (ctx.multiMgr.isHost()) {
                                ctx.multiMgr.hostChangingSong = false;
                                if (ctx.multiPickerAction == PICKER_ACTION_QUEUE) ctx.multiMgr.enqueueBeatmap(qm);
                                else ctx.multiMgr.setActiveBeatmap(qm);
                            } else {
                                ctx.multiMgr.requestQueueMap(qm);
                            }
                            ctx.audio.playHitSound(2);
                            ctx.appState = STATE_MULTI_LOBBY;
                            return;
                        }
                    } else {
                        ctx.startSelectedBeatmap();
                        return;
                    }
                }
            }
        }

        // =========================================================================
        // ENTRADA TÁCTIL EN PANTALLA INFERIOR (CARRUSEL TÁCTIL SUAVE + BOTONES)
        // =========================================================================
        if (ctx.touchJustDown) {
            // 1. Barra superior: [STD] [TAIKO] [CATCH] [MANIA] | [RECORDS] [BORRAR]
            // 1. Barra superior: [STD] [TAIKO] [CATCH] [MANIA] | Trofeo [RECORDS] | Papelera [BORRAR]
            if (ctx.touch.py >= 2 && ctx.touch.py <= 28) {
                if (ctx.touch.px >= 4 && ctx.touch.px <= 48) {
                    ctx.settings.gameMode = MODE_STD;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                    autoSelectDiffForMode(MODE_STD);
                } else if (ctx.touch.px >= 50 && ctx.touch.px <= 94) {
                    ctx.settings.gameMode = MODE_TAIKO;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                    autoSelectDiffForMode(MODE_TAIKO);
                } else if (ctx.touch.px >= 96 && ctx.touch.px <= 140) {
                    ctx.settings.gameMode = MODE_CATCH;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                    autoSelectDiffForMode(MODE_CATCH);
                } else if (ctx.touch.px >= 142 && ctx.touch.px <= 188) {
                    ctx.settings.gameMode = MODE_MANIA;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                    autoSelectDiffForMode(MODE_MANIA);
                } else if (ctx.touch.px >= 194 && ctx.touch.px <= 254) {
                    ctx.rankingExpanded = !ctx.rankingExpanded;
                    if (ctx.rankingExpanded) ctx.songInfoHidden = false;
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 256 && ctx.touch.px <= 316) {
                    if (!ctx.multiMgr.inMultiplayer && !ctx.songMgr.sets.empty()) {
                        ctx.deleteConfirmOpen = true;
                        ctx.audio.playHitSound(0);
                    }
                }
            }
            // 2. Barra inferior de botones (y: 198..238)
            else if (ctx.touch.py >= 198 && ctx.touch.py <= 238) {
                if (ctx.touch.px >= 4 && ctx.touch.px <= 65) { // TACTIL / TABLET
                    ctx.settings.playOnBottomScreen = !ctx.settings.playOnBottomScreen;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 66 && ctx.touch.px <= 127) { // BUSCAR
                    ctx.openSearchKeyboard();
                } else if (ctx.touch.px >= 128 && ctx.touch.px <= 189) { // MODS
                    ctx.modMenuOpen = true;
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 190 && ctx.touch.px <= 254) { // ORDEN
                    ctx.songMgr.sortMode = (ctx.songMgr.sortMode + 1) % 4;
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    s_cardExpanded = false;
                    s_focusOnDiff = false;
                    ctx.carouselPos = 0.0f;
                    resetSongSelectScroll();
                    rebuildCarouselItems();
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 256 && ctx.touch.px <= 318) { // JUGAR / Botón circular osu!
                    ctx.startSelectedBeatmap();
                    return;
                }
            }
            // 3. Área del carrusel: Iniciar arrastre fluido con lápiz táctil
            else if (ctx.touch.py >= 28 && ctx.touch.py < 198 && !ctx.songMgr.sets.empty()) {
                s_isDragging = true;
                s_touchStartX = ctx.touch.px;
                s_touchStartY = ctx.touch.py;
                s_touchLastY = ctx.touch.py;
                s_touchDragAccum = 0.0f;
                s_scrollVelocity = 0.0f;
            }
        }

        // Manejo de arrastre continuo e inercia táctil (deslizar con el lápiz táctil)
        if (s_isDragging) {
            if (ctx.isTouching) {
                float dy = ctx.touch.py - s_touchLastY;
                s_touchDragAccum += std::fabs(dy);
                s_scrollY -= dy;
                s_scrollVelocity = -dy * 0.85f;
                s_touchLastY = ctx.touch.py;
                s_targetScrollY = s_scrollY;
            }
            if (ctx.touchJustUp || !ctx.isTouching) {
                s_isDragging = false;
                if (s_touchDragAccum < 7.0f) {
                    // Pulsación limpia (TAP): seleccionar canción o dificultad pulsada
                    float listY = (s_touchStartY - 28.0f) + s_scrollY;
                    for (const auto& itm : items) {
                        if (listY >= itm.y && listY <= itm.y + itm.h) {
                            if (itm.type == CarouselItem::SET) {
                                bool tappedExpandBtn = (s_touchStartX >= 240.0f);
                                if (itm.setIdx == ctx.selectedSet) {
                                    // Tocar la canción seleccionada o el botón de expandir abre o cierra las dificultades
                                    s_cardExpanded = !s_cardExpanded;
                                    s_focusOnDiff = false;
                                    ctx.audio.playHitSound(0);
                                    rebuildCarouselItems();
                                } else {
                                    ctx.selectedSet = itm.setIdx;
                                    ctx.selectedDiff = 0;
                                    s_cardExpanded = tappedExpandBtn;
                                    s_focusOnDiff = false;
                                    autoSelectDiffForMode(ctx.settings.gameMode);
                                    ctx.songSelectCardAnim = 0.0f;
                                    ctx.audio.playHitSound(0);
                                    ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                                    rebuildCarouselItems();
                                }
                            } else if (itm.type == CarouselItem::DIFF) {
                                if (itm.diffIdx == ctx.selectedDiff && s_focusOnDiff) {
                                    // Tocar de nuevo la dificultad activa: ¡ahora sí juega!
                                    ctx.startSelectedBeatmap();
                                    return;
                                } else {
                                    ctx.selectedDiff = itm.diffIdx;
                                    s_focusOnDiff = true;
                                    ctx.songSelectCardAnim = 0.25f;
                                    ctx.audio.playHitSound(0);
                                    ctx.previewDelay = ctx.isNew3DS ? 6 : 10;
                                }
                            }
                            break;
                        }
                    }
                } else {
                    // Impulso inercial limitado suave
                    s_scrollVelocity = std::max(-18.0f, std::min(18.0f, s_scrollVelocity));
                }
            }
        }
    }

    ctx.carouselPos = Ui::approach(ctx.carouselPos, static_cast<float>(ctx.selectedSet), 0.22f);
    ctx.songSelectCardAnim = Ui::approach(ctx.songSelectCardAnim, 1.0f, 0.20f);
    float cardSlide = (1.0f - ctx.songSelectCardAnim) * 16.0f;

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    u32 pinkAccent = Ui::rgba(255, 102, 171); // #FF66AB

    // =========================================================================
    // PANTALLA SUPERIOR (400x240, 3D Estereoscópico, Fondo Visible y Paneles Translúcidos)
    // =========================================================================
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        C2D_TargetClear(curTop, Ui::rgba(12, 8, 26));
        C2D_SceneBegin(curTop);

        if (ctx.modMenuOpen) {
            std::string title = "";
            std::string version = "";
            float bCS = 4.0f, bAR = 8.0f, bOD = 7.0f, bHP = 6.0f;
            if (!ctx.songMgr.sets.empty() && !ctx.songMgr.sets[ctx.selectedSet].difficulties.empty()) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                int nDiffs = static_cast<int>(curSet.difficulties.size());
                int curD = std::max(0, std::min(ctx.selectedDiff, nDiffs - 1));
                const auto& diff = curSet.difficulties[curD];
                title = curSet.title;
                version = diff.version;
                bCS = diff.cs;
                bAR = diff.ar;
                bOD = diff.od;
                bHP = diff.hp;
            }
            renderModsTop(ctx, eyeSign, bCS, bAR, bOD, bHP, title, version);
            Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
            continue;
        }

        // 1. Fondo del Beatmap nítido y brillante en cada canción
        float dBg = depthShift(-3.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick),
                     Ui::rgba(22, 14, 38), Ui::rgba(10, 7, 22), pinkAccent, 0.01f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, static_cast<u8>(ctx.bgFade));
            // Velo oscuro muy ligero para que el arte del beatmap destaque siempre
            u8 dimA = ctx.songInfoHidden ? 25 : 70;
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(10, 7, 18, dimA));
        }

        // 2. Cabecera superior translúcida
        float dHead = depthShift(1.0f);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.25f, 400.0f, 26.0f, Ui::rgba(14, 10, 24, 175));
        C2D_DrawRectSolid(0.0f, 25.0f, 0.26f, 400.0f, 1.0f, Ui::withAlpha(pinkAccent, 110));

        Ui::pill(10.0f + dHead, 4.0f, 38.0f, 18.0f, 0.28f, pinkAccent);
        drawCenteredText(ctx.textBuf, 29.0f + dHead, 12.5f, 0.38f, ctx.clrWhite, "osu!", 0.32f);

        drawTextWithShadow(ctx.textBuf, 54.0f + dHead, 5.0f, 0.44f, ctx.clrWhite, LOC(STR_SONG_SELECT_TITLE), 0.32f);

        int curDiffMode = 0;
        if (!ctx.songMgr.sets.empty() && !ctx.songMgr.sets[ctx.selectedSet].difficulties.empty()) {
            int curD = std::max(0, std::min(ctx.selectedDiff, static_cast<int>(ctx.songMgr.sets[ctx.selectedSet].difficulties.size()) - 1));
            curDiffMode = ctx.songMgr.sets[ctx.selectedSet].difficulties[curD].mode;
        }
        int effectiveMode = (curDiffMode != 0) ? curDiffMode : ctx.settings.gameMode;

        const char* modeBadgeStr = (effectiveMode == MODE_TAIKO) ? "TAIKO"
                                 : (effectiveMode == MODE_CATCH) ? "CATCH"
                                 : (effectiveMode == MODE_MANIA) ? "MANIA"
                                 : "STD";
        u32 modeBadgeBg = (effectiveMode == MODE_TAIKO) ? Ui::rgba(80, 25, 20, 210)
                        : (effectiveMode == MODE_CATCH) ? Ui::rgba(15, 60, 30, 210)
                        : (effectiveMode == MODE_MANIA) ? Ui::rgba(50, 20, 75, 210)
                        : Ui::rgba(65, 25, 55, 200);
        u32 modeBadgeClr = (effectiveMode == MODE_TAIKO) ? Ui::rgba(255, 95, 75)
                         : (effectiveMode == MODE_CATCH) ? Ui::rgba(60, 235, 125)
                         : (effectiveMode == MODE_MANIA) ? Ui::rgba(195, 115, 255)
                         : pinkAccent;

        Ui::pill(216.0f + dHead, 4.0f, 46.0f, 18.0f, 0.28f, modeBadgeBg);
        drawCenteredTextWithShadow(ctx.textBuf, 239.0f + dHead, 12.5f, 0.34f, modeBadgeClr, modeBadgeStr, 0.32f);

        // Contador de canciones grande y legible a la derecha
        char cntStr[24];
        std::snprintf(cntStr, sizeof(cntStr), "%d / %d",
                      ctx.songMgr.sets.empty() ? 0 : (ctx.selectedSet + 1),
                      static_cast<int>(ctx.songMgr.sets.size()));
        Ui::pill(334.0f + dHead, 4.0f, 56.0f, 18.0f, 0.28f, Ui::rgba(24, 18, 38, 190));
        drawCenteredTextWithShadow(ctx.textBuf, 362.0f + dHead, 12.5f, 0.36f, ctx.clrWhite, cntStr, 0.32f);

        // 3. Contenido central: Si NO está oculto con Y, mostrar los 2 recuadros translúcidos
        if (!ctx.songMgr.sets.empty() && !ctx.songMgr.sets[ctx.selectedSet].difficulties.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int nDiffs = static_cast<int>(curSet.difficulties.size());
            int curD = std::max(0, std::min(ctx.selectedDiff, nDiffs - 1));
            const auto& diff = curSet.difficulties[curD];

            if (ctx.songInfoHidden) {
                // Vista limpia: solo una pastilla flotante sutil abajo a la izquierda para admirar el fondo
                float dClean = depthShift(2.0f);
                Ui::panel(12.0f + dClean, 162.0f, 376.0f, 44.0f, 0.25f, Ui::rgba(14, 10, 24, 115), pinkAccent);
                drawTextWithShadow(ctx.textBuf, 22.0f + dClean, 166.0f, 0.50f, ctx.clrWhite,
                                   truncateString(curSet.title, 28).c_str(), 0.30f);
                std::string cleanSub = truncateString(curSet.artist, 24) + "  •  [" + truncateString(diff.version, 16) + "]";
                drawTextWithShadow(ctx.textBuf, 22.0f + dClean, 185.0f, 0.38f, Ui::rgba(220, 215, 240),
                                   cleanSub.c_str(), 0.30f);
            } else {
                // --- RECUADRO IZQUIERDO TRANSLÚCIDO: DATOS DE LA CANCIÓN O RÉCORDS LOCALES ESTILIZADOS ---
                float dCardL = depthShift(2.2f);
                float cardLX = 10.0f + dCardL - cardSlide;
                float cardLY = 33.0f;
                float cardLW = 232.0f;
                float cardLH = 175.0f;

                int curModeIdx = (ctx.settings.gameMode == MODE_TAIKO || diff.mode == 1) ? 1
                               : (ctx.settings.gameMode == MODE_CATCH || diff.mode == 2) ? 2
                               : (ctx.settings.gameMode == MODE_MANIA || diff.mode == 3) ? 3 : 0;

                static std::string s_cachedOsuPath = "";
                static int s_cachedModeIdx = -1;
                static size_t s_cachedRecordCount = 0;
                static std::vector<ScoreRecord> s_cachedTopScores;
                if (s_cachedOsuPath != diff.osuPath || s_cachedModeIdx != curModeIdx || s_cachedRecordCount != ctx.scoreMgr.records.size()) {
                    s_cachedOsuPath = diff.osuPath;
                    s_cachedModeIdx = curModeIdx;
                    s_cachedRecordCount = ctx.scoreMgr.records.size();
                    s_cachedTopScores = ctx.scoreMgr.getTopRecords(diff.osuPath, 4, curModeIdx);
                }
                const auto& topScores = s_cachedTopScores;

                if (ctx.rankingExpanded) {
                    // VISTA DE RÉCORDS LOCALES ESTILO OSU!LAZER (Translúcida sobre el fondo del beatmap)
                    u32 goldAccent = Ui::rgba(255, 195, 40);
                    Ui::panel(cardLX, cardLY, cardLW, cardLH, 0.25f, Ui::rgba(16, 11, 28, 145), goldAccent);

                    // Cabecera inclinada de Récords Locales
                    const char* hdrTxt = ctx.selectingReplayMode ? LOC(STR_SONG_CHOOSE_REPLAY) : LOC(STR_SONG_LOCAL_SCORES_HDR);
                    u32 hdrTxtClr = ctx.selectingReplayMode ? Ui::rgba(0, 229, 255) : Ui::rgba(18, 12, 28);
                    u32 hdrBgClr  = ctx.selectingReplayMode ? Ui::rgba(22, 18, 38) : goldAccent;
                    Ui::slantRect(cardLX + 8.0f, cardLY + 6.0f, 120.0f, 18.0f, -4.0f, 0.28f, hdrBgClr);
                    drawCenteredText(ctx.textBuf, cardLX + 68.0f, cardLY + 15.0f, 0.33f, hdrTxtClr, hdrTxt, 0.31f);

                    drawTextWithShadow(ctx.textBuf, cardLX + 134.0f, cardLY + 7.5f, 0.34f, Ui::rgba(220, 215, 240),
                                       truncateString(diff.version, 11).c_str(), 0.30f);

                    if (topScores.empty()) {
                        Ui::slantRect(cardLX + 14.0f, cardLY + 56.0f, cardLW - 28.0f, 64.0f, -5.0f, 0.27f, Ui::rgba(24, 18, 42, 165));
                        drawCenteredTextWithShadow(ctx.textBuf, cardLX + cardLW * 0.5f, cardLY + 78.0f, 0.38f,
                                                   ctx.clrWhite, LOC(STR_SONG_NO_RECORDS_YET), 0.30f);
                        drawCenteredTextWithShadow(ctx.textBuf, cardLX + cardLW * 0.5f, cardLY + 98.0f, 0.33f,
                                                   Ui::rgba(56, 210, 255), LOC(STR_SONG_EMPTY_DIFF_PROMPT), 0.30f);
                    } else {
                        for (size_t s = 0; s < topScores.size() && s < 4; ++s) {
                            const auto& r = topScores[s];
                            float rY = cardLY + 30.0f + s * 35.0f;
                            float rX = cardLX + 8.0f;
                            float rW = cardLW - 16.0f;
                            float rH = 31.0f;

                            u32 rkClr = (s == 0) ? Ui::rgba(255, 210, 55)
                                      : (s == 1) ? Ui::rgba(56, 210, 255)
                                      : (s == 2) ? Ui::rgba(235, 145, 75)
                                      : pinkAccent;
                            u32 gClr = getScoreGradeColor(r.bestGrade, ctx.clrCursor);

                            bool isSelectedRow = ctx.selectingReplayMode && (static_cast<int>(s) == ctx.selectedRecordIdx);
                            if (isSelectedRow) {
                                Ui::glow(rX + rW * 0.5f, rY + rH * 0.5f, 0.26f, 16.0f, Ui::withAlpha(Ui::rgba(0, 229, 255), 110), 2);
                                drawRectOutline(rX - 1.0f, rY - 1.0f, rW + 2.0f, rH + 2.0f, 1.8f, Ui::rgba(0, 229, 255), 0.32f);
                            }

                            // Tarjeta inclinada estilo osu!lazer para cada récord
                            Ui::slantRect(rX, rY, rW, rH, -4.0f, 0.27f,
                                          isSelectedRow ? Ui::rgba(32, 45, 72, 225) : ((s == 0) ? Ui::rgba(38, 26, 58, 195) : Ui::rgba(20, 15, 36, 175)));
                            Ui::slantRect(rX, rY, 3.5f, rH, -4.0f, 0.28f, isSelectedRow ? Ui::rgba(0, 229, 255) : rkClr);

                            // Posición (#1..#4)
                            char rkTxt[8];
                            std::snprintf(rkTxt, sizeof(rkTxt), "#%u", static_cast<unsigned int>(s + 1));
                            drawTextWithShadow(ctx.textBuf, rX + 7.0f, rY + 7.0f, 0.36f, rkClr, rkTxt, 0.30f);

                            // Insignia de Grado (SS / S / A / B / C / D)
                            Ui::slantRect(rX + 28.0f, rY + 4.0f, 24.0f, 23.0f, -3.0f, 0.28f, Ui::rgba(12, 8, 22, 220));
                            drawCenteredTextWithShadow(ctx.textBuf, rX + 40.0f, rY + 15.0f, 0.40f, gClr, r.bestGrade.c_str(), 0.31f);

                            // Puntuación principal (arriba)
                            drawTextWithShadow(ctx.textBuf, rX + 58.0f, rY + 2.0f, 0.40f, ctx.clrWhite,
                                               formatScoreThousands(r.bestScore).c_str(), 0.30f);

                            // Mods o PP a la derecha superior (o indicador de Replay si está disponible)
                            bool hasRep = (s < 3) && ReplayManager::hasTopReplay(diff.osuPath, curModeIdx, static_cast<int>(s + 1));
                            if (hasRep) {
                                Ui::slantRect(rX + rW - 54.0f, rY + 3.0f, 50.0f, 15.0f, -3.0f, 0.28f, Ui::rgba(0, 229, 255));
                                drawCenteredText(ctx.textBuf, rX + rW - 29.0f, rY + 10.5f, 0.27f, Ui::rgba(14, 10, 24), "▶ REPLAY", 0.30f);
                            } else {
                                char ppTxt[24];
                                std::snprintf(ppTxt, sizeof(ppTxt), "%.0fpp", r.bestPP);
                                drawTextWithShadow(ctx.textBuf, rX + rW - 44.0f, rY + 3.0f, 0.33f, pinkAccent, ppTxt, 0.30f);
                            }

                            // Precisión, Combo y Mods (abajo)
                            char subLine[64];
                            if (r.bestMods != MOD_NONE) {
                                std::snprintf(subLine, sizeof(subLine), "%.1f%%  •  %dx  •  %s",
                                              r.bestAcc, r.bestCombo, formatModsString(r.bestMods).c_str());
                            } else {
                                std::snprintf(subLine, sizeof(subLine), "%.1f%%   •   %dx combo",
                                              r.bestAcc, r.bestCombo);
                            }
                            drawTextWithShadow(ctx.textBuf, rX + 58.0f, rY + 16.5f, 0.31f, Ui::rgba(46, 213, 115), subLine, 0.30f);
                        }
                    }
                } else {
                    Ui::panel(cardLX, cardLY, cardLW, cardLH, 0.25f, Ui::rgba(16, 11, 28, 125), pinkAccent);

                    // Título grande y legible
                    drawTextWithShadow(ctx.textBuf, cardLX + 10.0f, cardLY + 7.0f, 0.50f, ctx.clrWhite,
                                       truncateString(curSet.title, 21).c_str(), 0.30f);

                    // Artista
                    drawTextWithShadow(ctx.textBuf, cardLX + 10.0f, cardLY + 27.0f, 0.38f, Ui::rgba(210, 205, 235),
                                       truncateString(curSet.artist, 26).c_str(), 0.30f);

                    // Línea limpia: BPM y Duración
                    int liveBpm = static_cast<int>(std::round(diff.getBPM(getModsSpeedMultiplier(ctx.settings.activeMods))));
                    int songDurSec = (diff.lastObjectTime > diff.firstObjectTime)
                                   ? (diff.lastObjectTime - diff.firstObjectTime) / 1000 : 90;
                    char statsBuf[64];
                    std::snprintf(statsBuf, sizeof(statsBuf), "%d BPM   •   %d:%02d",
                                  liveBpm, songDurSec / 60, songDurSec % 60);
                    drawTextWithShadow(ctx.textBuf, cardLX + 10.0f, cardLY + 46.0f, 0.37f, Ui::rgba(56, 210, 255), statsBuf, 0.30f);

                    // Cuadrícula CS / AR / OD / HP con letra grande (0.37f)
                    float gaugeY = cardLY + 69.0f;
                    float dispCS = diff.cs, dispAR = diff.ar, dispOD = diff.od, dispHP = diff.hp;
                    if (ctx.settings.activeMods & MOD_EZ) { dispCS *= 0.5f; dispAR *= 0.5f; dispOD *= 0.5f; dispHP *= 0.5f; }
                    if (ctx.settings.activeMods & MOD_HR) {
                        dispCS = std::min(10.0f, dispCS * 1.3f);
                        dispAR = std::min(10.0f, dispAR * 1.4f);
                        dispOD = std::min(10.0f, dispOD * 1.4f);
                        dispHP = std::min(10.0f, dispHP * 1.4f);
                    }
                    Ui::metricGauge(cardLX + 10.0f,  gaugeY, "CS", dispCS, 10.0f, ctx.textBuf, 0.28f);
                    Ui::metricGauge(cardLX + 64.0f,  gaugeY, "AR", dispAR, 10.0f, ctx.textBuf, 0.28f);
                    Ui::metricGauge(cardLX + 118.0f, gaugeY, "OD", dispOD, 10.0f, ctx.textBuf, 0.28f);
                    Ui::metricGauge(cardLX + 172.0f, gaugeY, "HP", dispHP, 10.0f, ctx.textBuf, 0.28f);

                    // Sección inferior del recuadro izquierdo: Mejor Récord Personal + Gráfica sutil
                    C2D_DrawRectSolid(cardLX + 10.0f, cardLY + 96.0f, 0.28f, cardLW - 20.0f, 1.0f, Ui::rgba(255, 255, 255, 35));

                    const ScoreRecord* rec = topScores.empty() ? nullptr : &topScores[0];

                    if (rec) {
                        u32 gClr = getScoreGradeColor(rec->bestGrade, ctx.clrCursor);
                        drawGradeBadge(ctx.textBuf, cardLX + 10.0f, cardLY + 104.0f, 36.0f, 0.28f, rec->bestGrade, gClr);

                        drawTextWithShadow(ctx.textBuf, cardLX + 54.0f, cardLY + 103.0f, 0.46f,
                                           ctx.clrWhite, formatScoreThousands(rec->bestScore).c_str(), 0.30f);

                        char recSub[64];
                        std::snprintf(recSub, sizeof(recSub), "%.1f%%   •   %dx   •   %.0f PP",
                                      rec->bestAcc, rec->bestCombo, rec->bestPP);
                        drawTextWithShadow(ctx.textBuf, cardLX + 54.0f, cardLY + 123.0f, 0.35f,
                                           Ui::rgba(46, 213, 115), recSub, 0.30f);
                    } else {
                        drawTextWithShadow(ctx.textBuf, cardLX + 10.0f, cardLY + 104.0f, 0.38f,
                                           Ui::rgba(220, 215, 240), LOC(STR_SONG_NO_LOCAL_RECORD), 0.30f);
                    }

                    // Curva de densidad limpia en la base del recuadro izquierdo
                    float playhead = -1.0f;
                    if (ctx.audio.getIsPlaying() && diff.lastObjectTime > diff.firstObjectTime) {
                        int curMs = ctx.audio.getTimeMs();
                        if (curMs >= diff.firstObjectTime && curMs <= diff.lastObjectTime) {
                            playhead = static_cast<float>(curMs - diff.firstObjectTime) / static_cast<float>(diff.lastObjectTime - diff.firstObjectTime);
                        }
                    }
                    Ui::strainGraph(cardLX + 10.0f, cardLY + 143.0f, cardLW - 20.0f, 24.0f, 0.28f,
                                    diff.strainGraph, pinkAccent, playhead, 0.75f);
                }

                // --- RECUADRO DERECHO TRANSLÚCIDO: DIFICULTADES (< >) ---
                float dCardR = depthShift(2.2f);
                float cardRX = 250.0f + dCardR + cardSlide;
                float cardRY = 33.0f;
                float cardRW = 140.0f;
                float cardRH = 175.0f;

                Ui::panel(cardRX, cardRY, cardRW, cardRH, 0.25f, Ui::rgba(16, 11, 28, 125), Ui::rgba(56, 210, 255));

                char diffTitle[32];
                std::snprintf(diffTitle, sizeof(diffTitle), LOC(STR_SONG_DIFFS_COUNT_FMT), nDiffs);
                drawTextWithShadow(ctx.textBuf, cardRX + 8.0f, cardRY + 6.0f, 0.36f, Ui::rgba(56, 210, 255), diffTitle, 0.30f);

                // Lista vertical de hasta 4 dificultades con letra grande y clara
                int startD = std::max(0, std::min(curD - 1, nDiffs - 4));
                int endD = std::min(nDiffs, startD + 4);

                for (int d = startD; d < endD; ++d) {
                    int slot = d - startD;
                    float rowX = cardRX + 6.0f;
                    float rowY = cardRY + 26.0f + slot * 35.0f;
                    float rowW = cardRW - 12.0f;
                    float rowH = 31.0f;
                    bool isCur = (d == curD) && s_focusOnDiff;

                    const auto& dEnt = curSet.difficulties[d];
                    float dStars = getModAdjustedStars(dEnt.starRating, ctx.settings.activeMods);
                    u32 starClr = getStarRatingColor(dStars);

                    Ui::panel(rowX, rowY, rowW, rowH, 0.28f,
                              isCur ? Ui::rgba(38, 24, 62, 195) : Ui::rgba(18, 14, 30, 135),
                              isCur ? starClr : Ui::rgba(60, 50, 85, 140));
                    if (isCur) {
                        drawRectOutline(rowX, rowY, rowW, rowH, 1.2f, starClr, 0.29f);
                    }

                    // Nombre de la dificultad con letra grande (0.37f)
                    drawTextWithShadow(ctx.textBuf, rowX + 7.0f, rowY + 3.0f, 0.37f,
                                       isCur ? ctx.clrWhite : Ui::rgba(210, 205, 230),
                                       truncateString(dEnt.version, 13).c_str(), 0.31f);

                    // Estrella vectorial + valor numérico claro
                    drawStarVector(rowX + 12.0f, rowY + 23.0f, 3.8f, 0.31f, starClr);
                    char sBuf[16];
                    std::snprintf(sBuf, sizeof(sBuf), "%.2f", dStars);
                    drawTextWithShadow(ctx.textBuf, rowX + 19.0f, rowY + 15.5f, 0.35f, starClr, sBuf, 0.31f);

                    // Icono del modo en que fue diseñada la dificultad
                    drawModeIcon(rowX + rowW - 14.0f, rowY + 16.0f, dEnt.mode, 0.31f, 0.90f);
                }
            }
        }

        // 4. Barra de atajos inferior limpia (sin datos innecesarios como resolución/píxeles)
        float dFoot = depthShift(1.0f);
        float footY = 216.0f;
        C2D_DrawRectSolid(0.0f, footY, 0.25f, 400.0f, 24.0f, Ui::rgba(12, 8, 20, 185));
        C2D_DrawRectSolid(0.0f, footY, 0.26f, 400.0f, 1.0f, Ui::rgba(55, 42, 80, 160));

        if (ctx.selectingReplayMode) {
            drawKeyBadge(ctx.textBuf, 10.0f + dFoot,  footY + 4.0f, "A", LOC(STR_SONG_WATCH_REPLAY), Ui::rgba(0, 229, 255));
            drawKeyBadge(ctx.textBuf, 98.0f + dFoot,  footY + 4.0f, LOC(STR_SONG_SELECT_REPLAY_KEY), LOC(STR_SONG_SELECT_REPLAY_DESC), Ui::rgba(255, 195, 40));
            drawKeyBadge(ctx.textBuf, 226.0f + dFoot, footY + 4.0f, "SELECT", LOC(STR_SONG_BACK_TO_SONGS), Ui::rgba(255, 102, 171));
            drawKeyBadge(ctx.textBuf, 346.0f + dFoot, footY + 4.0f, "B", LOC(STR_MENU_EXIT), Ui::rgba(148, 163, 184));
        } else if (ctx.rankingExpanded) {
            drawKeyBadge(ctx.textBuf, 10.0f + dFoot,  footY + 4.0f, "A", LOC(STR_MENU_PLAY), Ui::rgba(56, 210, 255));
            drawKeyBadge(ctx.textBuf, 86.0f + dFoot,  footY + 4.0f, "< >", LOC(STR_DIFFICULTY), Ui::rgba(255, 195, 40));
            drawKeyBadge(ctx.textBuf, 184.0f + dFoot, footY + 4.0f, "SELECT", LOC(STR_SONG_SELECT_REPLAY_DESC), Ui::rgba(0, 229, 255));
            drawKeyBadge(ctx.textBuf, 292.0f + dFoot, footY + 4.0f, "X", LOC(STR_SONG_VIEW_DATA), Ui::rgba(46, 213, 115));
        } else {
            drawKeyBadge(ctx.textBuf, 10.0f + dFoot,  footY + 4.0f, "A", LOC(STR_MENU_PLAY), Ui::rgba(56, 210, 255));
            drawKeyBadge(ctx.textBuf, 86.0f + dFoot,  footY + 4.0f, "< >", LOC(STR_DIFFICULTY), Ui::rgba(255, 195, 40));
            drawKeyBadge(ctx.textBuf, 184.0f + dFoot, footY + 4.0f, "Y", ctx.songInfoHidden ? LOC(STR_SONG_SHOW_INFO) : LOC(STR_SONG_HIDE_INFO), pinkAccent);
            drawKeyBadge(ctx.textBuf, 292.0f + dFoot, footY + 4.0f, "X", LOC(STR_SONG_RECORDS_LABEL), Ui::rgba(46, 213, 115));
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // =========================================================================
    // PANTALLA INFERIOR (320x240, Táctil: Modos + Records + Borrar, 4 Canciones Amplias, 5 Botones)
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, Ui::rgba(10, 7, 20));
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                 Ui::rgba(18, 12, 32), Ui::rgba(10, 7, 20), pinkAccent, 0.01f);
    if (ctx.bgTex.loaded) {
        ctx.bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.02f, static_cast<u8>(ctx.bgFade * 0.28f));
        C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 320.0f, 240.0f, Ui::rgba(12, 8, 22, 160));
    }

    if (ctx.searchKeyboardOpen) {
        // Renderizado del teclado de búsqueda
        Ui::panel(2.0f, 2.0f, 316.0f, 236.0f, 0.50f, Ui::rgba(14, 10, 22, 245), pinkAccent);
        Ui::panel(4.0f, 3.0f, 226.0f, 28.0f, 0.55f, Ui::rgba(24, 18, 36, 255), pinkAccent);
        drawRing(14.0f, 17.0f, 5.0f, 1.4f, ctx.clrCursor, 14, 0.58f);
        C2D_DrawLine(18.0f, 21.0f, ctx.clrCursor, 22.0f, 25.0f, ctx.clrCursor, 2.0f, 0.58f);

        bool showCursor = (ctx.animTick / 20) % 2 == 0;
        std::string dispQuery = truncateString(ctx.songMgr.searchQuery, 22);
        if (showCursor) dispQuery += "|";
        drawTextWithShadow(ctx.textBuf, 27.0f, 7.0f, 0.40f, ctx.clrWhite, dispQuery.c_str(), 0.62f);

        Ui::panel(232.0f, 3.0f, 26.0f, 28.0f, 0.55f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 80, 100));
        drawCenteredTextWithShadow(ctx.textBuf, 245.0f, 17.0f, 0.38f, Ui::rgba(255, 140, 160), "X", 0.62f);

        Ui::panel(262.0f, 3.0f, 54.0f, 28.0f, 0.55f, Ui::rgba(22, 68, 42, 255), ctx.clr100);
        drawCenteredTextWithShadow(ctx.textBuf, 289.0f, 17.0f, 0.36f, ctx.clrWhite, LOC(STR_DONE), 0.62f);

        const char* numLabels[] = {"1","2","3","4","5","6","7","8","9","0"};
        for (int i = 0; i < 10; ++i) {
            float kx = 4.0f + i * 26.5f;
            Ui::panel(kx, 54.0f, 24.5f, 32.0f, 0.60f, Ui::rgba(32, 26, 48, 255), Ui::rgba(75, 65, 105, 255));
            drawCenteredTextWithShadow(ctx.textBuf, kx + 12.25f, 70.0f, 0.44f, ctx.clrWhite, numLabels[i], 0.68f);
        }
        Ui::panel(271.0f, 54.0f, 45.0f, 32.0f, 0.60f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 100, 130));
        drawCenteredTextWithShadow(ctx.textBuf, 293.5f, 70.0f, 0.32f, ctx.clrWhite, LOC(STR_DELETE), 0.68f);

        const char* r1Labels[] = {"Q","W","E","R","T","Y","U","I","O","P"};
        for (int i = 0; i < 10; ++i) {
            float kx = 4.0f + i * 31.0f;
            Ui::panel(kx, 89.0f, 29.0f, 32.0f, 0.60f, Ui::rgba(32, 26, 48, 255), Ui::rgba(75, 65, 105, 255));
            drawCenteredTextWithShadow(ctx.textBuf, kx + 14.5f, 105.0f, 0.44f, ctx.clrWhite, r1Labels[i], 0.68f);
        }

        const char* r2Labels[] = {"A","S","D","F","G","H","J","K","L","-"};
        for (int i = 0; i < 10; ++i) {
            float kx = 8.0f + i * 30.2f;
            Ui::panel(kx, 124.0f, 28.2f, 32.0f, 0.60f, Ui::rgba(32, 26, 48, 255), Ui::rgba(75, 65, 105, 255));
            drawCenteredTextWithShadow(ctx.textBuf, kx + 14.1f, 140.0f, 0.44f, ctx.clrWhite, r2Labels[i], 0.68f);
        }

        const char* r3Labels[] = {"Z","X","C","V","B","N","M",".","/"};
        for (int i = 0; i < 9; ++i) {
            float kx = 16.0f + i * 32.0f;
            Ui::panel(kx, 159.0f, 29.8f, 32.0f, 0.60f, Ui::rgba(32, 26, 48, 255), Ui::rgba(75, 65, 105, 255));
            drawCenteredTextWithShadow(ctx.textBuf, kx + 14.9f, 175.0f, 0.44f, ctx.clrWhite, r3Labels[i], 0.68f);
        }

        Ui::panel(4.0f, 194.0f, 56.0f, 42.0f, 0.60f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 90, 110));
        drawCenteredTextWithShadow(ctx.textBuf, 32.0f, 215.0f, 0.34f, ctx.clrWhite, LOC(STR_CLEAR), 0.68f);
        Ui::panel(63.0f, 194.0f, 142.0f, 42.0f, 0.60f, Ui::rgba(36, 30, 56, 255), Ui::rgba(90, 80, 125));
        drawCenteredTextWithShadow(ctx.textBuf, 134.0f, 215.0f, 0.36f, ctx.clrWhite, LOC(STR_SPACE), 0.68f);
        Ui::panel(208.0f, 194.0f, 52.0f, 42.0f, 0.60f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 120, 140));
        drawCenteredTextWithShadow(ctx.textBuf, 234.0f, 215.0f, 0.34f, ctx.clrWhite, LOC(STR_DELETE), 0.68f);
        Ui::panel(263.0f, 194.0f, 53.0f, 42.0f, 0.60f, Ui::rgba(22, 68, 42, 255), ctx.clr100);
        drawCenteredTextWithShadow(ctx.textBuf, 289.5f, 215.0f, 0.38f, ctx.clrWhite, "OK", 0.68f);
    } else if (ctx.modMenuOpen) {
        renderModsBottom(ctx, false);
    } else {
        // =====================================================================
        // VISTA PRINCIPAL: CARRUSEL FLUIDO + BARRAS PROTECTORAS SUPERIOR/INFERIOR
        // =====================================================================

        // 1. Carrusel continuo fluido estilo osu!lazer
        if (items.empty()) {
            drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 100.0f, 0.45f, ctx.clrWhite, LOC(STR_SONG_NO_BEATMAPS), 0.22f);
            drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 120.0f, 0.35f, Ui::rgba(180, 175, 210), LOC(STR_SONG_EMPTY_HINT), 0.22f);
        } else {
            for (const auto& itm : items) {
                float screenY = 28.0f + (itm.y - s_scrollY);
                if (screenY + itm.h < 28.0f || screenY > 198.0f) continue; // Culling exacto en los márgenes de las barras

                if (itm.type == CarouselItem::SET) {
                    int s = itm.setIdx;
                    const auto& cSet = ctx.songMgr.sets[s];
                    bool isSel = (s == ctx.selectedSet);
                    float cardX = 8.0f;
                    float cardW = 302.0f;
                    float cardH = itm.h;

                    float setStars = cSet.difficulties.empty() ? 1.0f : getModAdjustedStars(cSet.difficulties[0].starRating, ctx.settings.activeMods);
                    u32 starClr = getStarRatingColor(setStars);

                    // Sombra suave proyectada
                    C2D_DrawRectSolid(cardX + 2.0f, screenY + 2.0f, 0.17f, cardW, cardH, Ui::rgba(0, 0, 0, 105));

                    // Fondo degradado horizontal tipo tarjeta osu!lazer
                    u32 bgLeft  = isSel ? Ui::lerpColor(Ui::rgba(38, 28, 58, 240), starClr, 0.28f) : Ui::rgba(22, 16, 36, 205);
                    u32 bgRight = isSel ? Ui::rgba(26, 20, 44, 248) : Ui::rgba(14, 10, 24, 215);
                    Ui::gradH(cardX, screenY, cardW, cardH, 0.18f, bgLeft, bgRight);

                    // Borde lateral redondeado/cápsula característico de osu!lazer
                    Ui::pill(cardX, screenY, 8.0f, cardH, 0.19f, isSel ? Ui::brighten(starClr, 1.25f) : starClr);
                    C2D_DrawRectSolid(cardX + 4.0f, screenY, 0.19f, isSel ? 3.5f : 2.5f, cardH, isSel ? Ui::brighten(starClr, 1.25f) : starClr);

                    // Línea sutil de brillo en el borde superior
                    C2D_DrawRectSolid(cardX + 6.0f, screenY, 0.20f, cardW - 6.0f, 1.0f, Ui::rgba(255, 255, 255, isSel ? 65 : 28));

                    if (isSel) {
                        drawRectOutline(cardX, screenY, cardW, cardH, 1.3f, Ui::brighten(starClr, 1.25f), 0.21f);
                        Ui::glow(cardX + 8.0f, screenY + cardH * 0.5f, 0.17f, 22.0f, Ui::withAlpha(starClr, 85), 2);
                    } else {
                        drawRectOutline(cardX, screenY, cardW, cardH, 0.8f, Ui::rgba(60, 50, 80, 140), 0.19f);
                    }

                    // Título en negrita y Artista debajo
                    drawTextWithShadow(ctx.textBuf, cardX + 13.0f, screenY + 3.0f, isSel ? 0.45f : 0.40f,
                                       ctx.clrWhite, truncateString(cSet.title, 23).c_str(), 0.22f);
                    drawTextWithShadow(ctx.textBuf, cardX + 13.0f, screenY + (isSel ? 18.0f : 16.5f), 0.32f,
                                       Ui::rgba(200, 195, 225), truncateString(cSet.artist, 27).c_str(), 0.22f);

                    // Insignia verde RANKED y espectro de dificultades colorido (puntos estilo osu!lazer)
                    float pillX = cardX + 13.0f;
                    float pillY = screenY + (isSel ? 28.5f : 26.5f);
                    float pW = 34.0f, pH = 9.5f;
                    Ui::pill(pillX, pillY, pW, pH, 0.22f, Ui::rgba(18, 55, 30, 240));
                    drawRectOutline(pillX, pillY, pW, pH, 0.8f, Ui::rgba(46, 213, 115), 0.23f);
                    drawCenteredText(ctx.textBuf, pillX + pW * 0.5f, pillY + 5.0f, 0.22f, Ui::rgba(60, 245, 135), "RANKED", 0.24f);

                    float dotX = pillX + pW + 5.0f;
                    float dotY = pillY + pH * 0.5f;
                    int maxDots = std::min(10, static_cast<int>(cSet.difficulties.size()));
                    for (int di = 0; di < maxDots; ++di) {
                        float ds = getModAdjustedStars(cSet.difficulties[di].starRating, ctx.settings.activeMods);
                        u32 dc = getStarRatingColor(ds);
                        C2D_DrawCircleSolid(dotX + di * 5.5f, dotY, 0.23f, 1.8f, dc);
                    }

                    // Indicador en la derecha: contador de dificultades o botón de expansión/cierre
                    if (isSel) {
                        float pillW = 58.0f;
                        float pillH = 18.0f;
                        float pillX = cardX + cardW - pillW - 6.0f;
                        float pillY = screenY + (cardH - pillH) * 0.5f;
                        Ui::pill(pillX, pillY, pillW, pillH, 0.22f, Ui::rgba(14, 10, 24, 210));
                        drawRectOutline(pillX, pillY, pillW, pillH, 1.0f, starClr, 0.23f);
                        const char* expText = s_cardExpanded ? "▲ CERRAR" : "▼ ABRIR";
                        drawCenteredTextWithShadow(ctx.textBuf, pillX + pillW * 0.5f, pillY + 9.0f, 0.30f, ctx.clrWhite, expText, 0.24f);
                    } else {
                        int nDiffs = static_cast<int>(cSet.difficulties.size());
                        float bdgW = 54.0f;
                        float bdgH = 17.0f;
                        float bdgX = cardX + cardW - bdgW - 8.0f;
                        float bdgY = screenY + (cardH - bdgH) * 0.5f;
                        Ui::pill(bdgX, bdgY, bdgW, bdgH, 0.20f, Ui::rgba(16, 12, 26, 210));
                        drawRectOutline(bdgX, bdgY, bdgW, bdgH, 0.8f, Ui::rgba(85, 75, 115, 160), 0.21f);
                        char cntBuf[16];
                        std::snprintf(cntBuf, sizeof(cntBuf), "%d DIF%s", nDiffs, nDiffs == 1 ? "" : "S");
                        drawCenteredTextWithShadow(ctx.textBuf, bdgX + bdgW * 0.5f, bdgY + 8.5f, 0.32f,
                                                   Ui::rgba(215, 210, 235), cntBuf, 0.23f);
                    }
                } else if (itm.type == CarouselItem::DIFF) {
                    int s = itm.setIdx;
                    int d = itm.diffIdx;
                    const auto& cSet = ctx.songMgr.sets[s];
                    const auto& dEnt = cSet.difficulties[d];
                    bool isDiffSel = (d == ctx.selectedDiff) && s_focusOnDiff;
                    float diffX = 22.0f;
                    float diffW = 288.0f;
                    float diffH = itm.h;
                    float dStars = getModAdjustedStars(dEnt.starRating, ctx.settings.activeMods);
                    u32 starClr = getStarRatingColor(dStars);

                    // Fondo de la tarjeta de dificultad tintado según el color de la dificultad
                    u32 bgClr = isDiffSel ? Ui::lerpColor(Ui::rgba(36, 26, 52, 240), starClr, 0.35f)
                                          : Ui::lerpColor(Ui::rgba(20, 15, 32, 215), starClr, 0.16f);
                    C2D_DrawRectSolid(diffX + 2.0f, screenY + 2.0f, 0.17f, diffW, diffH, Ui::rgba(0, 0, 0, 80));
                    C2D_DrawRectSolid(diffX, screenY, 0.18f, diffW, diffH, bgClr);

                    if (isDiffSel) {
                        drawRectOutline(diffX, screenY, diffW, diffH, 1.3f, starClr, 0.21f);
                        C2D_DrawRectSolid(diffX, screenY, 0.21f, 3.5f, diffH, starClr);
                    } else {
                        drawRectOutline(diffX, screenY, diffW, diffH, 0.8f, Ui::rgba(50, 42, 70, 120), 0.20f);
                        C2D_DrawRectSolid(diffX, screenY, 0.20f, 2.0f, diffH, Ui::withAlpha(starClr, 160));
                    }

                    // Icono circular con aureola en la izquierda (estilo osu!lazer)
                    float icX = diffX + 11.0f;
                    float icY = screenY + diffH * 0.5f;
                    if (isDiffSel) {
                        Ui::glow(icX, icY, 0.20f, 10.0f, Ui::withAlpha(starClr, 120), 2);
                        C2D_DrawCircleSolid(icX, icY, 0.22f, 6.5f, starClr);
                        C2D_DrawCircleSolid(icX, icY, 0.23f, 4.5f, Ui::rgba(22, 16, 32));
                        C2D_DrawCircleSolid(icX, icY, 0.24f, 2.2f, ctx.clrWhite);
                    } else {
                        C2D_DrawCircleSolid(icX, icY, 0.22f, 5.0f, Ui::withAlpha(starClr, 180));
                        C2D_DrawCircleSolid(icX, icY, 0.23f, 3.2f, Ui::rgba(18, 14, 28));
                        C2D_DrawCircleSolid(icX, icY, 0.24f, 1.5f, starClr);
                    }

                    // Línea superior: Nombre de la dificultad + "mapped by [mapper]"
                    std::string vStr = truncateString(dEnt.version, 15);
                    drawTextWithShadow(ctx.textBuf, diffX + 22.0f, screenY + 2.5f, 0.35f,
                                       isDiffSel ? ctx.clrWhite : Ui::rgba(235, 230, 245), vStr.c_str(), 0.23f);
                    if (!dEnt.creator.empty()) {
                        std::string mStr = "mapped by " + truncateString(dEnt.creator, 12);
                        drawTextWithShadow(ctx.textBuf, diffX + 130.0f, screenY + 2.5f, 0.28f,
                                           Ui::rgba(165, 160, 190), mStr.c_str(), 0.23f);
                    }

                    // Línea inferior: Estrella vectorial + valor numérico + estrellas decorativas
                    drawStarVector(diffX + 26.0f, screenY + 18.0f, 3.0f, 0.23f, starClr);
                    char sNum[16];
                    std::snprintf(sNum, sizeof(sNum), "%.2f", dStars);
                    drawTextWithShadow(ctx.textBuf, diffX + 32.0f, screenY + 12.5f, 0.32f, starClr, sNum, 0.23f);

                    int sCount = std::min(5, static_cast<int>(std::floor(dStars)));
                    for (int st = 0; st < sCount; ++st) {
                        drawStarVector(diffX + 66.0f + st * 6.5f, screenY + 18.0f, 2.2f, 0.23f, starClr);
                    }

                    if (dEnt.mode != 0) {
                        drawModeIcon(diffX + diffW - 14.0f, screenY + 13.0f, dEnt.mode, 0.23f, 0.80f);
                    }
                }
            }

            // Barra de desplazamiento suave (Scrollbar)
            if (totalContentHeight > viewHeight) {
                float trackH = 166.0f;
                float thumbH = std::max(16.0f, trackH * (viewHeight / totalContentHeight));
                float scrollRatio = std::max(0.0f, std::min(1.0f, s_scrollY / maxScroll));
                float thumbY = 30.0f + (trackH - thumbH) * scrollRatio;
                C2D_DrawRectSolid(316.0f, 30.0f, 0.25f, 3.0f, trackH, Ui::rgba(255, 255, 255, 15));
                Ui::pill(316.0f, thumbY, 3.0f, thumbH, 0.26f, s_isDragging ? pinkAccent : Ui::rgba(180, 175, 215, 140));
            }
        }

        // 2. Barra superior: Iconos de Modos [STD] [TAIKO] [CATCH] [MANIA] | Trofeo [RECORDS] | Papelera [BORRAR]
        // Capa sólida superior (z=0.35f, opacidad 255) para que nada del carrusel se asome
        C2D_DrawRectSolid(0.0f, 0.0f, 0.35f, 320.0f, 28.0f, Ui::rgba(14, 10, 24, 255));
        C2D_DrawLine(0.0f, 27.5f, Ui::rgba(65, 55, 90, 160), 320.0f, 27.5f, Ui::rgba(65, 55, 90, 160), 1.0f, 0.36f);

        int curM = ctx.settings.gameMode;
        int curDiffMode = 0;
        if (!ctx.songMgr.sets.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            if (!curSet.difficulties.empty()) {
                int d = std::max(0, std::min(ctx.selectedDiff, static_cast<int>(curSet.difficulties.size()) - 1));
                curDiffMode = curSet.difficulties[d].mode;
            }
        }
        int effectiveMode = (curDiffMode != 0) ? curDiffMode : curM;

        auto drawModeChip = [&](float x, float w, int chipMode) {
            bool isCurrentSetting = (curM == chipMode);
            bool isEffective = (effectiveMode == chipMode);
            bool isBlocked = (chipMode != effectiveMode && isCurrentSetting);

            u32 bg, border;

            if (isBlocked) {
                // Modo configurado pero NO disponible para esta canción -> marcado en GRIS
                bg     = Ui::rgba(38, 35, 46, 220);
                border = Ui::rgba(75, 72, 85);
            } else if (isEffective) {
                // Modo activo que realmente se jugará
                if (chipMode == MODE_TAIKO) {
                    bg     = Ui::rgba(235, 75, 55);
                    border = ctx.clrWhite;
                } else if (chipMode == MODE_CATCH) {
                    bg     = Ui::rgba(46, 213, 115);
                    border = ctx.clrWhite;
                } else if (chipMode == MODE_MANIA) {
                    bg     = Ui::rgba(175, 90, 245);
                    border = ctx.clrWhite;
                } else {
                    bg     = pinkAccent;
                    border = ctx.clrWhite;
                }
            } else {
                // Modo inactivo normal
                bg     = Ui::rgba(24, 18, 36, 195);
                border = Ui::rgba(65, 55, 90);
            }

            Ui::panel(x, 4.0f, w, 22.0f, 0.37f, bg, border);
            drawModeIcon(x + w * 0.5f, 15.0f, chipMode, 0.38f, 1.25f);

            // Si está bloqueado (gris), dibujamos una sutil línea de tachado
            if (isBlocked) {
                C2D_DrawLine(x + 7.0f, 15.0f, Ui::rgba(170, 70, 70, 210),
                             x + w - 7.0f, 15.0f, Ui::rgba(170, 70, 70, 210), 1.2f, 0.39f);
            }
        };

        drawModeChip(6.0f,   42.0f, MODE_STD);
        drawModeChip(52.0f,  42.0f, MODE_TAIKO);
        drawModeChip(98.0f,  42.0f, MODE_CATCH);
        drawModeChip(144.0f, 42.0f, MODE_MANIA);

        // Botón con ICONO DE TROFEO (Récords Locales)
        u32 recBtnBg  = ctx.rankingExpanded ? Ui::rgba(255, 195, 40, 235) : Ui::rgba(32, 24, 52, 215);
        u32 recBtnBrd = ctx.rankingExpanded ? ctx.clrWhite : Ui::rgba(255, 195, 40);
        u32 recBtnTxt = ctx.rankingExpanded ? Ui::rgba(18, 12, 28) : Ui::rgba(255, 215, 75);
        Ui::panel(198.0f, 4.0f, 54.0f, 22.0f, 0.37f, recBtnBg, recBtnBrd);
        drawTrophyIcon(198.0f + 27.0f, 15.0f, 0.38f, recBtnTxt);

        // Botón con ICONO DE BOTE DE BASURA (Borrar Canción)
        Ui::panel(258.0f, 4.0f, 56.0f, 22.0f, 0.37f, Ui::rgba(110, 22, 38, 215), Ui::rgba(255, 80, 105));
        drawTrashIcon(258.0f + 28.0f, 15.0f, 0.38f, Ui::rgba(255, 220, 225));

        // 3. Barra inferior de 5 botones (con el icónico botón circular independiente de osu!)
        // Fondo protector para ocultar tarjetas al deslizarse hacia abajo
        C2D_DrawRectSolid(0.0f, 198.0f, 0.35f, 320.0f, 42.0f, Ui::rgba(14, 10, 24, 255));
        C2D_DrawLine(0.0f, 198.0f, Ui::rgba(65, 55, 90, 160), 320.0f, 198.0f, Ui::rgba(65, 55, 90, 160), 1.0f, 0.36f);

        float btnY = 200.0f;
        float btnH = 35.0f;
        float slant = -4.0f;

        // Botón 1: TOUCH / TABLET con icono de stylus/tablet
        bool isTablet = !ctx.settings.playOnBottomScreen;
        Ui::slantButton(6.0f, btnY, 58.0f, btnH, slant, Ui::rgba(66, 144, 251),
                        nullptr, nullptr, ctx.textBuf, false, 0.37f);
        drawTouchIcon(6.0f + 29.0f, btnY + 11.5f, 0.38f, Ui::rgba(255, 255, 255));
        drawCenteredTextWithShadow(ctx.textBuf, 6.0f + 29.0f, btnY + 23.5f, 0.32f,
                                   Ui::rgba(130, 200, 255), isTablet ? LOC(STR_CARD_TABLET) : LOC(STR_CARD_TOUCH), 0.39f);

        // Botón 2: BUSCAR con icono de lupa
        Ui::slantButton(68.0f, btnY, 58.0f, btnH, slant, Ui::rgba(0, 216, 240),
                        nullptr, nullptr, ctx.textBuf, !ctx.songMgr.searchQuery.empty(), 0.37f);
        drawSearchIcon(68.0f + 29.0f, btnY + 11.5f, 0.38f, Ui::rgba(255, 255, 255));
        drawCenteredTextWithShadow(ctx.textBuf, 68.0f + 29.0f, btnY + 23.5f, 0.32f,
                                   Ui::rgba(120, 245, 255), LOC(STR_SEARCH), 0.39f);

        // Botón 3: MODS con icono de ajustes / ecualizador
        Ui::slantButton(130.0f, btnY, 58.0f, btnH, slant, Ui::rgba(147, 85, 232),
                        nullptr, nullptr, ctx.textBuf, ctx.settings.activeMods != MOD_NONE, 0.37f);
        drawModsIcon(130.0f + 29.0f, btnY + 11.5f, 0.38f, Ui::rgba(255, 255, 255));
        drawCenteredTextWithShadow(ctx.textBuf, 130.0f + 29.0f, btnY + 23.5f, 0.32f,
                                   Ui::rgba(215, 175, 255), "MODS", 0.39f);

        // Botón 4: ORDEN (mantiene texto legible según el tipo de ordenación)
        const char* shortOrder = (ctx.songMgr.sortMode == SORT_BY_ARTIST) ? LOC(STR_CARD_ARTIST)
                               : (ctx.songMgr.sortMode == SORT_BY_STARS)  ? LOC(STR_CARD_STARS)
                               : (ctx.songMgr.sortMode == SORT_BY_RECORD) ? LOC(STR_CARD_RECORD)
                               : LOC(STR_CARD_TITLE);
        Ui::slantButton(192.0f, btnY, 60.0f, btnH, slant, Ui::rgba(66, 100, 220),
                        LOC(STR_CARD_ORDER), shortOrder, ctx.textBuf, false, 0.37f);

        // Botón 5: Botón circular libre de osu! (sin tarjeta rectangular de fondo)
        float osuX = 286.0f, osuY = 217.5f, osuR = 17.5f;
        Ui::glow(osuX, osuY, 0.38f, 22.0f, Ui::withAlpha(pinkAccent, 130), 3);
        C2D_DrawCircleSolid(osuX, osuY, 0.39f, osuR + 2.5f, ctx.clrWhite);
        C2D_DrawCircleSolid(osuX, osuY, 0.40f, osuR, pinkAccent);
        drawCenteredTextWithShadow(ctx.textBuf, osuX, osuY - 1.5f, 0.44f, ctx.clrWhite, "osu!", 0.42f, Ui::rgba(160, 30, 85));
    }

    // Modal de confirmación de eliminación
    if (ctx.deleteConfirmOpen) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.85f, 320.0f, 240.0f, Ui::rgba(8, 6, 14, 235));
        Ui::panel(16.0f, 38.0f, 288.0f, 156.0f, 0.88f, Ui::rgba(26, 18, 34, 252), Ui::rgba(255, 64, 96));
        Ui::pill(24.0f, 46.0f, 96.0f, 18.0f, 0.90f, Ui::rgba(180, 30, 55, 230));
        drawCenteredText(ctx.textBuf, 72.0f, 55.0f, 0.36f, ctx.clrWhite, LOC(STR_DELETE), 0.95f);

        std::string delTitle = (!ctx.songMgr.sets.empty() && ctx.selectedSet >= 0 && ctx.selectedSet < static_cast<int>(ctx.songMgr.sets.size()))
            ? ctx.songMgr.sets[ctx.selectedSet].title : "";
        drawCenteredText(ctx.textBuf, 160.0f, 82.0f, 0.44f, ctx.clrWhite, truncateString(delTitle, 28).c_str(), 0.95f);
        drawCenteredText(ctx.textBuf, 160.0f, 108.0f, 0.35f, Ui::rgba(255, 160, 175), LOC(STR_DELETE_CONFIRM_TITLE), 0.95f);
        drawCenteredText(ctx.textBuf, 160.0f, 126.0f, 0.32f, ctx.clrMuted, LOC(STR_DELETE_CONFIRM_DESC), 0.95f);

        Ui::button(28.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(36, 32, 50, 250), Ui::rgba(85, 75, 110), false);
        Ui::pill(34.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(65, 58, 85));
        drawCenteredText(ctx.textBuf, 45.0f, 165.0f, 0.32f, ctx.clrWhite, "B", 0.95f);
        drawTextWithShadow(ctx.textBuf, 62.0f, 157.0f, 0.38f, ctx.clrWhite, LOC(STR_BACK), 0.95f);

        Ui::button(166.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(165, 28, 48, 250), Ui::rgba(255, 80, 110), true);
        Ui::pill(172.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(255, 80, 110));
        drawCenteredText(ctx.textBuf, 183.0f, 165.0f, 0.32f, ctx.clrWhite, "A", 0.95f);
        drawTextWithShadow(ctx.textBuf, 200.0f, 157.0f, 0.38f, ctx.clrWhite, LOC(STR_DELETE), 0.95f);
    }

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
