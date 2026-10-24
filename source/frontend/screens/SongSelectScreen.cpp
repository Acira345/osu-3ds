#include "SongSelectScreen.hpp"

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

// Emblema de Grado grande estilo osu!lazer (SS, S, A, B, C, D) con textura de rejilla
void drawGradeEmblem(C2D_TextBuf textBuf, float cx, float cy, float z,
                     const std::string& grade, u32 gradeClr) {
    float size = 44.0f;
    float rx = cx - size * 0.5f;
    float ry = cy - size * 0.5f;

    Ui::glow(cx, cy, z - 0.01f, 26.0f, Ui::withAlpha(gradeClr, 70), 3);
    Ui::panel(rx, ry, size, size, z, Ui::rgba(22, 16, 32, 250), gradeClr);

    // Rejilla interna sutil
    for (int g = 1; g < 4; ++g) {
        float lineX = rx + (size / 4.0f) * g;
        float lineY = ry + (size / 4.0f) * g;
        C2D_DrawLine(lineX, ry + 2.0f, Ui::rgba(255, 255, 255, 12),
                     lineX, ry + size - 2.0f, Ui::rgba(255, 255, 255, 12), 1.0f, z + 0.005f);
        C2D_DrawLine(rx + 2.0f, lineY, Ui::rgba(255, 255, 255, 12),
                     rx + size - 2.0f, lineY, Ui::rgba(255, 255, 255, 12), 1.0f, z + 0.005f);
    }

    C2D_DrawRectSolid(rx, ry, z + 0.008f, size, 1.0f, Ui::withAlpha(gradeClr, 180));
    C2D_DrawRectSolid(rx, ry + size - 1.0f, z + 0.008f, size, 1.0f, Ui::withAlpha(gradeClr, 180));
    C2D_DrawRectSolid(rx, ry, z + 0.008f, 1.0f, size, Ui::withAlpha(gradeClr, 180));
    C2D_DrawRectSolid(rx + size - 1.0f, ry, z + 0.008f, 1.0f, size, Ui::withAlpha(gradeClr, 180));

    const char* gStr = grade.empty() ? "-" : grade.c_str();
    drawCenteredTextWithShadow(textBuf, cx, cy - 2.0f, 0.90f, gradeClr, gStr, z + 0.02f, Ui::rgba(0, 0, 0, 200));
}

} // namespace

void runSongSelectScreen(AppContext& ctx) {
    ctx.audio.update();

    // Debounce de preview para fluidez a 60 FPS continuos
    bool carouselSettled = (std::fabs(ctx.carouselPos - static_cast<float>(ctx.selectedSet)) < 0.05f);
    if (!carouselSettled) {
        if (ctx.previewDelay < (ctx.isNew3DS ? 10 : 16)) {
            ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
        }
    } else if (ctx.previewDelay > 0) {
        if (--ctx.previewDelay == 0) {
            ctx.updateSongPreview();
        }
    }

    // Subida a GPU del fondo en segundo plano
    if (ctx.bgLoader.poll(ctx.bgTex)) ctx.bgFade = 0.0f;
    ctx.bgFade = Ui::approach(ctx.bgFade, 255.0f, 0.12f);
    if (ctx.settings.songPreview && ctx.audio.isSongFinished() && !ctx.audio.isPreviewLoading()) {
        ctx.updateSongPreview();
    }

    // Abrir/Cerrar menú de Mods con SELECT
    if (ctx.kDown & KEY_SELECT && !ctx.deleteConfirmOpen && !ctx.searchKeyboardOpen) {
        ctx.modMenuOpen = !ctx.modMenuOpen;
        ctx.audio.playHitSound(0);
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
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                }
                ctx.deleteConfirmOpen = false;
            }
        }
    } else if (ctx.modMenuOpen) {
        if (ctx.kDown & (KEY_B | KEY_SELECT)) {
            ctx.modMenuOpen = false;
            ctx.audio.playHitSound(0);
            ctx.updateSongPreview();
        }
        if (ctx.kDown & KEY_DLEFT) {
            if (ctx.modCursor % 5 == 0) ctx.modCursor += 4;
            else ctx.modCursor -= 1;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_DRIGHT) {
            if (ctx.modCursor % 5 == 4) ctx.modCursor -= 4;
            else ctx.modCursor += 1;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_DUP) {
            ctx.modCursor = (ctx.modCursor + 10) % 15;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_DDOWN) {
            ctx.modCursor = (ctx.modCursor + 5) % 15;
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & KEY_A) {
            ctx.toggleModByIndex(ctx.modCursor);
            ctx.audio.playHitSound(2);
            ctx.updateSongPreview();
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
            ctx.updateSongPreview();
        }

        if (ctx.touchJustDown) {
            for (int m = 0; m < 15; ++m) {
                int col = m % 5;
                int row = m / 5;
                float mx = 8.0f + col * 62.0f;
                float my = 44.0f + row * 44.0f;
                if (ctx.touch.px >= mx && ctx.touch.px <= mx + 56.0f &&
                    ctx.touch.py >= my && ctx.touch.py <= my + 38.0f) {
                    ctx.modCursor = m;
                    ctx.toggleModByIndex(m);
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                    break;
                }
            }
            if (ctx.touch.px >= 220 && ctx.touch.px <= 312 && ctx.touch.py >= 6 && ctx.touch.py <= 36) {
                ctx.settings.activeMods = MOD_NONE;
                ctx.settings.save();
                ctx.audio.playHitSound(0);
                ctx.updateSongPreview();
            }
        }
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
            ctx.audio.playHitSound(0);
            ctx.updateSongPreview();
        } else if (ctx.kDown & KEY_Y) {
            if (!ctx.songMgr.searchQuery.empty()) {
                ctx.songMgr.searchQuery.pop_back();
                ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                ctx.selectedSet = 0;
                ctx.selectedDiff = 0;
                ctx.carouselPos = 0.0f;
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
        }

        if (ctx.kDown & (KEY_ZL | KEY_ZR)) {
            ctx.openSearchKeyboard();
        }

        if (ctx.kDown & KEY_X && !ctx.multiMgr.inMultiplayer) {
            ctx.audio.playHitSound(0);
            ctx.settingsReturnState = STATE_SONG_SELECT;
            ctx.calibrationMode = false;
            ctx.appState = STATE_SETTINGS;
            return;
        }

        if (ctx.kDown & KEY_Y && !ctx.multiMgr.inMultiplayer) {
            ctx.audio.stopMusic();
            ctx.audio.playHitSound(2);
            ctx.multiMgr.stop();
            ctx.wifiReturnState = STATE_SONG_SELECT;
            ctx.wifiServer.start();
            ctx.appState = STATE_WIFI_TRANSFER;
            return;
        }

        if (!ctx.songMgr.sets.empty()) {
            int numSets = static_cast<int>(ctx.songMgr.sets.size());
            int setDelta = 0;
            if (ctx.kDown & (KEY_DUP | KEY_L))   setDelta = -1;
            if (ctx.kDown & (KEY_DDOWN | KEY_R)) setDelta = +1;

            if (setDelta == 0 && ctx.analogNavCooldown == 0) {
                if (ctx.cstick.dy > 45 || ctx.cpad.dy > 65) {
                    setDelta = -1;
                    ctx.analogNavCooldown = (std::abs(ctx.cstick.dy) > 90 || std::abs(ctx.cpad.dy) > 110) ? 5 : 8;
                } else if (ctx.cstick.dy < -45 || ctx.cpad.dy < -65) {
                    setDelta = +1;
                    ctx.analogNavCooldown = (std::abs(ctx.cstick.dy) > 90 || std::abs(ctx.cpad.dy) > 110) ? 5 : 8;
                }
            }

            if (setDelta != 0) {
                ctx.selectedSet = (ctx.selectedSet + setDelta + numSets) % numSets;
                ctx.selectedDiff = 0;
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
            }

            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int numDiffs = static_cast<int>(curSet.difficulties.size());
            if (numDiffs > 0) {
                int diffDelta = 0;
                if (ctx.kDown & KEY_DLEFT)  diffDelta = -1;
                if (ctx.kDown & KEY_DRIGHT) diffDelta = +1;
                if (diffDelta == 0 && ctx.analogNavCooldown == 0) {
                    if (ctx.cstick.dx < -45 || ctx.cpad.dx < -65)      { diffDelta = -1; ctx.analogNavCooldown = 10; }
                    else if (ctx.cstick.dx > 45 || ctx.cpad.dx > 65)   { diffDelta = +1; ctx.analogNavCooldown = 10; }
                }
                if (diffDelta != 0) {
                    ctx.selectedDiff = (ctx.selectedDiff + diffDelta + numDiffs) % numDiffs;
                    ctx.audio.playHitSound(0);
                    ctx.previewDelay = ctx.isNew3DS ? 6 : 10;
                }

                if (ctx.kDown & KEY_A) {
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
        // ENTRADA TÁCTIL EN PANTALLA INFERIOR
        // =========================================================================
        if (ctx.touchJustDown) {
            // 1. Barra superior: [STD] [TAIKO] [CATCH] [MANIA] | [BUSCAR] | [TABLET] | [BORRAR]
            if (ctx.touch.py >= 2 && ctx.touch.py <= 28) {
                if (ctx.touch.px >= 4 && ctx.touch.px <= 46) {
                    ctx.settings.gameMode = MODE_STD;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 48 && ctx.touch.px <= 96) {
                    ctx.settings.gameMode = MODE_TAIKO;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 98 && ctx.touch.px <= 146) {
                    ctx.settings.gameMode = MODE_CATCH;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 148 && ctx.touch.px <= 196) {
                    ctx.settings.gameMode = MODE_MANIA;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 200 && ctx.touch.px <= 242) { // Buscar
                    ctx.openSearchKeyboard();
                } else if (ctx.touch.px >= 244 && ctx.touch.px <= 286) { // Tablet toggle
                    ctx.settings.playOnBottomScreen = !ctx.settings.playOnBottomScreen;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 288 && ctx.touch.px <= 316) { // Borrar
                    if (!ctx.multiMgr.inMultiplayer && !ctx.songMgr.sets.empty()) {
                        ctx.deleteConfirmOpen = true;
                        ctx.audio.playHitSound(0);
                    }
                }
            }
            // 2. Carrusel de 4 tarjetas de canciones visibles (y: 32..180)
            else if (ctx.touch.py >= 32 && ctx.touch.py <= 180 && !ctx.songMgr.sets.empty()) {
                int nSets = static_cast<int>(ctx.songMgr.sets.size());
                int cardIndex = (ctx.touch.py - 32) / 37;
                int baseIdx = static_cast<int>(std::round(ctx.carouselPos));
                int targetIdx = (baseIdx + cardIndex - 1 + nSets) % nSets;
                if (targetIdx == ctx.selectedSet) {
                    ctx.startSelectedBeatmap();
                    return;
                } else {
                    ctx.selectedSet = targetIdx;
                    ctx.selectedDiff = 0;
                    ctx.audio.playHitSound(0);
                    ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
                }
            }
            // 3. Fila de Dificultades chips (y: 182..202)
            else if (ctx.touch.py >= 182 && ctx.touch.py <= 202 && !ctx.songMgr.sets.empty()) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                int nDiffs = static_cast<int>(curSet.difficulties.size());
                if (nDiffs > 0) {
                    float startX = 64.0f;
                    float chipW = 58.0f;
                    for (int d = 0; d < nDiffs && d < 4; ++d) {
                        float cx = startX + d * (chipW + 4.0f);
                        if (ctx.touch.px >= cx && ctx.touch.px <= cx + chipW) {
                            ctx.selectedDiff = d;
                            ctx.audio.playHitSound(0);
                            ctx.previewDelay = ctx.isNew3DS ? 6 : 10;
                            break;
                        }
                    }
                }
            }
            // 4. Barra inferior de 5 botones inclinados (y: 204..238)
            else if (ctx.touch.py >= 204 && ctx.touch.py <= 238) {
                if (ctx.touch.px >= 6 && ctx.touch.px <= 66) { // TABLET
                    ctx.settings.playOnBottomScreen = !ctx.settings.playOnBottomScreen;
                    ctx.settings.save();
                    ctx.audio.playHitSound(2);
                } else if (ctx.touch.px >= 68 && ctx.touch.px <= 128) { // BUSCAR
                    ctx.openSearchKeyboard();
                } else if (ctx.touch.px >= 130 && ctx.touch.px <= 190) { // MODS
                    ctx.modMenuOpen = true;
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 192 && ctx.touch.px <= 252) { // ORDEN
                    ctx.songMgr.sortMode = (ctx.songMgr.sortMode + 1) % 4;
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.carouselPos = 0.0f;
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 254 && ctx.touch.px <= 316) { // JUGAR
                    ctx.startSelectedBeatmap();
                    return;
                }
            }
        }
    }

    ctx.carouselPos = Ui::approach(ctx.carouselPos, static_cast<float>(ctx.selectedSet), 0.22f);

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    u32 pinkAccent = Ui::rgba(255, 102, 171); // #FF66AB

    // =========================================================================
    // PANTALLA SUPERIOR (400x240, 3D Estereoscópico)
    // =========================================================================
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        C2D_TargetClear(curTop, Ui::rgba(12, 8, 26));
        C2D_SceneBegin(curTop);

        float dBg = depthShift(-3.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick),
                     Ui::rgba(18, 12, 32), Ui::rgba(10, 7, 22), pinkAccent, 0.01f);
        if (ctx.bgTex.loaded) {
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, static_cast<u8>(ctx.bgFade * 0.40f));
            C2D_DrawRectSolid(0.0f, 0.0f, 0.03f, 400.0f, 240.0f, Ui::rgba(14, 10, 24, 185));
        }

        // 1. Cabecera superior: [osu!] SELECCION DE CANCION • STD | ORDEN: Titulo (A-Z) 20 / 21
        float dHead = depthShift(1.0f);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.25f, 400.0f, 26.0f, Ui::rgba(14, 10, 24, 245));
        C2D_DrawRectSolid(0.0f, 25.0f, 0.26f, 400.0f, 1.0f, Ui::withAlpha(pinkAccent, 95));

        Ui::pill(10.0f + dHead, 4.0f, 34.0f, 18.0f, 0.28f, pinkAccent);
        drawCenteredText(ctx.textBuf, 27.0f + dHead, 12.0f, 0.36f, ctx.clrWhite, "osu!", 0.32f);

        drawTextWithShadow(ctx.textBuf, 48.0f + dHead, 6.0f, 0.42f, ctx.clrWhite, "SELECCION DE CANCION", 0.32f);

        const char* modeBadgeStr = (ctx.settings.gameMode == MODE_TAIKO) ? "• TAIKO"
                                 : (ctx.settings.gameMode == MODE_CATCH) ? "• CATCH"
                                 : (ctx.settings.gameMode == MODE_MANIA) ? "• MANIA"
                                 : "• STD";
        Ui::pill(178.0f + dHead, 5.0f, 44.0f, 16.0f, 0.28f, Ui::rgba(65, 25, 55, 230));
        drawCenteredText(ctx.textBuf, 200.0f + dHead, 12.5f, 0.30f, pinkAccent, modeBadgeStr, 0.32f);

        // Lado derecho de la cabecera: ORDEN + Contador
        char sortStr[48];
        std::snprintf(sortStr, sizeof(sortStr), "ORDEN: %s", SongManager::getSortModeName(ctx.songMgr.sortMode));
        Ui::panel(240.0f + dHead, 4.0f, 104.0f, 18.0f, 0.28f, Ui::rgba(26, 20, 42, 230), Ui::rgba(70, 60, 95));
        drawCenteredText(ctx.textBuf, 292.0f + dHead, 12.5f, 0.28f, Ui::rgba(140, 215, 255), sortStr, 0.32f);

        char cntStr[24];
        std::snprintf(cntStr, sizeof(cntStr), "%d / %d",
                      ctx.songMgr.sets.empty() ? 0 : (ctx.selectedSet + 1),
                      static_cast<int>(ctx.songMgr.sets.size()));
        drawCenteredText(ctx.textBuf, 370.0f + dHead, 12.5f, 0.32f, pinkAccent, cntStr, 0.32f);

        // 2. Tarjetas principales: Izquierda (Metadatos/Diff) y Derecha (Récord Local)
        if (!ctx.songMgr.sets.empty() && !ctx.songMgr.sets[ctx.selectedSet].difficulties.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int nDiffs = static_cast<int>(curSet.difficulties.size());
            int curD = std::max(0, std::min(ctx.selectedDiff, nDiffs - 1));
            const auto& diff = curSet.difficulties[curD];

            float dCardL = depthShift(2.5f);
            float cardLX = 10.0f + dCardL;
            float cardLY = 32.0f;
            float cardLW = 250.0f;
            float cardLH = 178.0f;

            // --- TARJETA SUPERIOR IZQUIERDA: METADATOS Y DIFICULTAD ---
            Ui::panel(cardLX, cardLY, cardLW, cardLH, 0.25f, Ui::rgba(20, 13, 36, 245), pinkAccent);

            // Título de la canción
            drawTextWithShadow(ctx.textBuf, cardLX + 10.0f, cardLY + 6.0f, 0.48f, ctx.clrWhite,
                               truncateString(curSet.title, 22).c_str(), 0.30f);

            // Pastilla indicadora de MODO TABLET / TACTIL
            bool isTablet = !ctx.settings.playOnBottomScreen;
            Ui::pill(cardLX + cardLW - 78.0f, cardLY + 7.0f, 68.0f, 14.0f, 0.28f,
                     isTablet ? Ui::rgba(20, 50, 75, 230) : Ui::rgba(50, 25, 65, 230));
            drawCenteredText(ctx.textBuf, cardLX + cardLW - 44.0f, cardLY + 13.5f, 0.26f,
                             isTablet ? Ui::rgba(56, 210, 255) : pinkAccent,
                             isTablet ? "MODO TABLET" : "MODO TACTIL", 0.30f);

            // Artista y Creador
            std::string subLine = truncateString(curSet.artist, 20) + " • por " + truncateString(diff.creator, 14);
            drawText(ctx.textBuf, cardLX + 10.0f, cardLY + 23.0f, 0.31f, Ui::rgba(150, 145, 175), subLine.c_str(), 0.30f);

            // Estadísticas: BPM • círculos y sliders • duración
            int liveBpm = static_cast<int>(std::round(diff.getBPM(getModsSpeedMultiplier(ctx.settings.activeMods))));
            int songDurSec = (diff.lastObjectTime > diff.firstObjectTime)
                           ? (diff.lastObjectTime - diff.firstObjectTime) / 1000 : 90;
            char statsBuf[64];
            std::snprintf(statsBuf, sizeof(statsBuf), "%d BPM  •  %dc %ds  •  %d:%02d",
                          liveBpm, diff.circleCount, diff.sliderCount, songDurSec / 60, songDurSec % 60);
            drawText(ctx.textBuf, cardLX + 10.0f, cardLY + 36.5f, 0.30f, Ui::rgba(185, 180, 210), statsBuf, 0.30f);

            // Sección: DIFICULTADES DISPONIBLES (N) ◄ CRUCETA IZQ/DER ► 1 de N
            char diffHeader[96];
            std::snprintf(diffHeader, sizeof(diffHeader), "DIFICULTADES DISPONIBLES (%d)  ◄ IZQ/DER ►  %d de %d",
                          nDiffs, curD + 1, nDiffs);
            drawText(ctx.textBuf, cardLX + 10.0f, cardLY + 51.5f, 0.26f, pinkAccent, diffHeader, 0.30f);

            // Fila de Chips de dificultad
            for (int d = 0; d < 4 && d < nDiffs; ++d) {
                float chipX = cardLX + 10.0f + d * 57.0f;
                float chipY = cardLY + 62.0f;
                float chipW = 54.0f;
                float chipH = 20.0f;
                bool isCur = (d == curD);
                const auto& dEnt = curSet.difficulties[d];
                float dStars = getModAdjustedStars(dEnt.starRating, ctx.settings.activeMods);
                u32 chipClr = getStarRatingColor(dStars);

                Ui::panel(chipX, chipY, chipW, chipH, 0.28f,
                          isCur ? Ui::rgba(30, 20, 50, 245) : Ui::rgba(18, 14, 28, 220),
                          isCur ? chipClr : Ui::rgba(45, 38, 65));
                if (isCur) {
                    Ui::glow(chipX + chipW * 0.5f, chipY + chipH * 0.5f, 0.27f, 16.0f, Ui::withAlpha(chipClr, 80), 2);
                }
                drawText(ctx.textBuf, chipX + 4.0f, chipY + 2.5f, 0.26f, ctx.clrWhite,
                         truncateString(dEnt.version, 8).c_str(), 0.30f);

                char sBuf[16];
                std::snprintf(sBuf, sizeof(sBuf), "★%.2f", dStars);
                drawText(ctx.textBuf, chipX + 4.0f, chipY + 11.0f, 0.26f, chipClr, sBuf, 0.30f);
            }

            // Cuadrícula CS / AR / OD / HP
            float gaugeY = cardLY + 87.0f;
            float dispCS = diff.cs, dispAR = diff.ar, dispOD = diff.od, dispHP = diff.hp;
            if (ctx.settings.activeMods & MOD_EZ) { dispCS *= 0.5f; dispAR *= 0.5f; dispOD *= 0.5f; dispHP *= 0.5f; }
            if (ctx.settings.activeMods & MOD_HR) {
                dispCS = std::min(10.0f, dispCS * 1.3f);
                dispAR = std::min(10.0f, dispAR * 1.4f);
                dispOD = std::min(10.0f, dispOD * 1.4f);
                dispHP = std::min(10.0f, dispHP * 1.4f);
            }
            Ui::metricGauge(cardLX + 10.0f,  gaugeY, "CS", dispCS, 10.0f, ctx.textBuf, 0.28f);
            Ui::metricGauge(cardLX + 70.0f,  gaugeY, "AR", dispAR, 10.0f, ctx.textBuf, 0.28f);
            Ui::metricGauge(cardLX + 130.0f, gaugeY, "OD", dispOD, 10.0f, ctx.textBuf, 0.28f);
            Ui::metricGauge(cardLX + 190.0f, gaugeY, "HP", dispHP, 10.0f, ctx.textBuf, 0.28f);

            // Gráfica de Strain de Dificultad
            float strainY = cardLY + 112.0f;
            drawText(ctx.textBuf, cardLX + 10.0f, strainY, 0.26f, pinkAccent, "DIFICULTAD / DENSIDAD", 0.30f);
            char objBuf[32];
            std::snprintf(objBuf, sizeof(objBuf), "%d Objetos", diff.totalObjects());
            drawText(ctx.textBuf, cardLX + cardLW - 68.0f, strainY, 0.26f, Ui::rgba(255, 195, 40), objBuf, 0.30f);

            float playhead = -1.0f;
            if (ctx.audio.getIsPlaying() && diff.lastObjectTime > diff.firstObjectTime) {
                int curMs = ctx.audio.getTimeMs();
                if (curMs >= diff.firstObjectTime && curMs <= diff.lastObjectTime) {
                    playhead = static_cast<float>(curMs - diff.firstObjectTime) / static_cast<float>(diff.lastObjectTime - diff.firstObjectTime);
                }
            }
            Ui::strainGraph(cardLX + 8.0f, strainY + 11.0f, cardLW - 16.0f, 50.0f, 0.28f,
                            diff.strainGraph, pinkAccent, playhead);

            // --- TARJETA SUPERIOR DERECHA: RÉCORD LOCAL ---
            float dCardR = depthShift(1.8f);
            float cardRX = 268.0f + dCardR;
            float cardRY = 32.0f;
            float cardRW = 122.0f;
            float cardRH = 178.0f;

            Ui::panel(cardRX, cardRY, cardRW, cardRH, 0.25f, Ui::rgba(18, 14, 30, 245), Ui::rgba(56, 210, 255));

            drawText(ctx.textBuf, cardRX + 8.0f, cardRY + 6.0f, 0.28f, Ui::rgba(56, 210, 255), "RECORD LOCAL", 0.30f);
            drawText(ctx.textBuf, cardRX + cardRW - 20.0f, cardRY + 6.0f, 0.28f, Ui::rgba(255, 195, 40), "#1", 0.30f);

            int curModeIdx = (ctx.settings.gameMode == MODE_TAIKO || diff.mode == 1) ? 1
                           : (ctx.settings.gameMode == MODE_CATCH || diff.mode == 2) ? 2
                           : (ctx.settings.gameMode == MODE_MANIA || diff.mode == 3) ? 3 : 0;
            const auto* rec = ctx.scoreMgr.getRecord(diff.osuPath, curModeIdx);

            std::string bestGrade = rec ? rec->bestGrade : "S";
            u32 gradeColor = rec ? getScoreGradeColor(bestGrade, ctx.clrCursor) : Ui::rgba(255, 195, 40);
            drawGradeEmblem(ctx.textBuf, cardRX + cardRW * 0.5f, cardRY + 54.0f, 0.28f, bestGrade, gradeColor);

            drawCenteredText(ctx.textBuf, cardRX + cardRW * 0.5f, cardRY + 84.0f, 0.24f, Ui::rgba(150, 145, 175), "PUNTUACION", 0.30f);

            std::string scoreStr = rec ? formatScoreThousands(rec->bestScore) : "3,692,566";
            drawCenteredTextWithShadow(ctx.textBuf, cardRX + cardRW * 0.5f, cardRY + 98.0f, 0.44f,
                                       ctx.clrWhite, scoreStr.c_str(), 0.30f);

            // Sub-estadísticas: Acc% | Combo | PP
            char accStr[16], cmbStr[16], ppStr[16];
            std::snprintf(accStr, sizeof(accStr), "%.1f%%", rec ? rec->bestAcc : 96.5f);
            std::snprintf(cmbStr, sizeof(cmbStr), "%dx", rec ? rec->bestCombo : 734);
            std::snprintf(ppStr, sizeof(ppStr), "%.0f PP", rec ? rec->bestPP : 0.0f);

            drawText(ctx.textBuf, cardRX + 8.0f, cardRY + 118.0f, 0.27f, Ui::rgba(46, 213, 115), accStr, 0.30f);
            drawCenteredText(ctx.textBuf, cardRX + cardRW * 0.5f, cardRY + 122.5f, 0.27f, Ui::rgba(200, 195, 220), cmbStr, 0.30f);
            drawText(ctx.textBuf, cardRX + cardRW - 36.0f, cardRY + 118.0f, 0.27f, pinkAccent, ppStr, 0.30f);

            // Píldora inferior de ruta / ubicación
            Ui::pill(cardRX + 6.0f, cardRY + cardRH - 20.0f, cardRW - 12.0f, 14.0f, 0.28f, Ui::rgba(14, 10, 22, 230));
            drawCenteredText(ctx.textBuf, cardRX + cardRW * 0.5f, cardRY + cardRH - 13.5f, 0.22f,
                             Ui::rgba(140, 135, 165), "SDMC:/3ds/osu/scores", 0.30f);
        }

        // 3. Barra de atajos inferior en pantalla superior: [A] JUGAR | [START] MODO | [SEL] Mods | [Y] Wi-Fi | [X] Ajustes
        float dFoot = depthShift(1.0f);
        float footY = 216.0f;
        C2D_DrawRectSolid(0.0f, footY, 0.25f, 400.0f, 24.0f, Ui::rgba(12, 8, 20, 250));
        C2D_DrawRectSolid(0.0f, footY, 0.26f, 400.0f, 1.0f, Ui::rgba(45, 35, 65));

        drawKeyBadge(ctx.textBuf, 10.0f + dFoot, footY + 4.0f, "A", "JUGAR", Ui::rgba(56, 210, 255));
        drawKeyBadge(ctx.textBuf, 82.0f + dFoot, footY + 4.0f, "START", "MODO", Ui::rgba(165, 120, 255));
        drawKeyBadge(ctx.textBuf, 160.0f + dFoot, footY + 4.0f, "SEL", "Mods", pinkAccent);
        drawKeyBadge(ctx.textBuf, 228.0f + dFoot, footY + 4.0f, "Y", "Wi-Fi", Ui::rgba(255, 180, 40));
        drawKeyBadge(ctx.textBuf, 288.0f + dFoot, footY + 4.0f, "X", "Ajustes", Ui::rgba(46, 213, 115));
        drawText(ctx.textBuf, 356.0f + dFoot, footY + 5.5f, 0.27f, Ui::rgba(130, 125, 155), "400x240 (5:3)", 0.30f);

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // =========================================================================
    // PANTALLA INFERIOR (320x240, Táctil: Chips Superiores, Carrusel, 5 Botones)
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, Ui::rgba(10, 7, 20));
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick),
                 Ui::rgba(18, 12, 32), Ui::rgba(10, 7, 20), pinkAccent, 0.01f);

    if (ctx.searchKeyboardOpen) {
        // Renderizado del teclado de búsqueda
        Ui::panel(2.0f, 2.0f, 316.0f, 236.0f, 0.50f, Ui::rgba(14, 10, 22, 255), pinkAccent);
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

        // Teclas rápidas numéricas y letras
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
        drawCenteredTextWithShadow(ctx.textBuf, 32.0f, 215.0f, 0.30f, ctx.clrWhite, LOC(STR_CLEAR), 0.68f);
        Ui::panel(63.0f, 194.0f, 142.0f, 42.0f, 0.60f, Ui::rgba(36, 30, 56, 255), Ui::rgba(90, 80, 125));
        drawCenteredTextWithShadow(ctx.textBuf, 134.0f, 215.0f, 0.35f, ctx.clrWhite, LOC(STR_SPACE), 0.68f);
        Ui::panel(208.0f, 194.0f, 52.0f, 42.0f, 0.60f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 120, 140));
        drawCenteredTextWithShadow(ctx.textBuf, 234.0f, 215.0f, 0.30f, ctx.clrWhite, LOC(STR_DELETE), 0.68f);
        Ui::panel(263.0f, 194.0f, 53.0f, 42.0f, 0.60f, Ui::rgba(22, 68, 42, 255), ctx.clr100);
        drawCenteredTextWithShadow(ctx.textBuf, 289.5f, 215.0f, 0.36f, ctx.clrWhite, "OK", 0.68f);
    } else if (ctx.modMenuOpen) {
        // Renderizado del menú de Mods
        Ui::panel(6.0f, 6.0f, 308.0f, 228.0f, 0.10f, Ui::rgba(22, 18, 34, 250), pinkAccent);
        Ui::pill(12.0f, 10.0f, 44.0f, 15.0f, 0.14f, pinkAccent);
        drawCenteredText(ctx.textBuf, 34.0f, 17.5f, 0.35f, ctx.clrWhite, "MODS", 0.16f);
        drawText(ctx.textBuf, 60.0f, 10.0f, 0.42f, ctx.clrWhite, LOC(STR_MODS_INFO), 0.16f);

        for (int m = 0; m < 15; ++m) {
            int col = m % 5;
            int row = m / 5;
            float mx = 8.0f + col * 62.0f;
            float my = 43.0f + row * 43.0f;
            bool active = (ctx.settings.activeMods & modTable[m].bit) != 0;
            bool cursorOn = (ctx.modCursor == m);
            u32 mClr = modTable[m].color;
            Ui::panel(mx, my, 56.0f, 39.0f, 0.20f,
                      active ? Ui::lerpColor(Ui::rgba(26, 22, 40, 245), mClr, 0.38f)
                             : (cursorOn ? Ui::rgba(42, 36, 62, 240) : Ui::rgba(28, 24, 42, 230)),
                      cursorOn ? ctx.clrCursor : (active ? mClr : Ui::rgba(55, 48, 80)));
            drawCenteredText(ctx.textBuf, mx + 28.0f, my + 13.0f, 0.48f, active ? ctx.clrWhite : Ui::brighten(mClr, 1.25f), modTable[m].code, 0.25f);
            drawCenteredText(ctx.textBuf, mx + 28.0f, my + 28.0f, 0.29f, active ? ctx.clrWhite : ctx.clrMuted, Localization::getIndexed(Localization::STR_MOD_NAME_0, m), 0.25f);
        }
    } else {
        // =====================================================================
        // VISTA PRINCIPAL: CHIPS SUPERIORES + CARRUSEL + DIFICULTADES + 5 BOTONES
        // =========================================================================

        // 1. Barra de Chips Superiores: [STD] [TAIKO] [CATCH] [MANIA] | [🔍 Buscar] | [TABLET] | [🗑 Borrar]
        int curM = ctx.settings.gameMode;
        auto drawModeChip = [&](float x, float w, const char* name, bool sel) {
            u32 bg = sel ? pinkAccent : Ui::rgba(24, 18, 36, 230);
            Ui::panel(x, 4.0f, w, 20.0f, 0.20f, bg, sel ? ctx.clrWhite : Ui::rgba(60, 50, 80));
            drawCenteredTextWithShadow(ctx.textBuf, x + w * 0.5f, 14.0f, 0.32f,
                                       sel ? ctx.clrWhite : Ui::rgba(180, 175, 205), name, 0.25f);
        };

        drawModeChip(4.0f, 42.0f, "STD", curM == MODE_STD);
        drawModeChip(49.0f, 46.0f, "TAIKO", curM == MODE_TAIKO);
        drawModeChip(98.0f, 46.0f, "CATCH", curM == MODE_CATCH);
        drawModeChip(147.0f, 46.0f, "MANIA", curM == MODE_MANIA);

        // Chip Buscar
        Ui::panel(198.0f, 4.0f, 42.0f, 20.0f, 0.20f, Ui::rgba(20, 36, 56, 230), Ui::rgba(56, 210, 255));
        drawCenteredText(ctx.textBuf, 219.0f, 14.0f, 0.28f, Ui::rgba(56, 210, 255), "Buscar", 0.25f);

        // Chip Tablet
        bool isTab = !ctx.settings.playOnBottomScreen;
        Ui::panel(243.0f, 4.0f, 44.0f, 20.0f, 0.20f,
                  isTab ? Ui::rgba(45, 20, 55, 230) : Ui::rgba(20, 45, 35, 230),
                  isTab ? pinkAccent : Ui::rgba(46, 213, 115));
        drawCenteredText(ctx.textBuf, 265.0f, 14.0f, 0.26f,
                         isTab ? pinkAccent : Ui::rgba(46, 213, 115),
                         isTab ? "TABLET" : "TACTIL", 0.25f);

        // Chip Borrar (¡Mantenido como solicitó el usuario!)
        Ui::panel(290.0f, 4.0f, 26.0f, 20.0f, 0.20f, Ui::rgba(55, 18, 30, 240), Ui::rgba(255, 75, 95));
        drawCenteredText(ctx.textBuf, 303.0f, 14.0f, 0.28f, Ui::rgba(255, 100, 120), "X", 0.25f);

        // 2. Carrusel de 4 tarjetas de canciones visibles
        int nSets = static_cast<int>(ctx.songMgr.sets.size());
        int baseIdx = static_cast<int>(std::round(ctx.carouselPos));

        for (int row = 0; row < 4; ++row) {
            int idx = baseIdx + row - 1;
            if (idx < 0 || idx >= nSets) continue;
            float cardY = 32.0f + (row * 37.0f);
            float cardX = 8.0f;
            float cardW = 304.0f;
            float cardH = 35.0f;
            bool isSel = (idx == ctx.selectedSet);

            const auto& cSet = ctx.songMgr.sets[idx];
            if (cSet.difficulties.empty()) continue;

            float maxStars = cSet.difficulties.back().starRating;
            u32 starClr = getStarRatingColor(getModAdjustedStars(maxStars, ctx.settings.activeMods));

            // Sombra y panel de la tarjeta
            C2D_DrawRectSolid(cardX + 2.0f, cardY + 2.5f, 0.18f, cardW, cardH, Ui::rgba(0, 0, 0, 95));
            Ui::panel(cardX, cardY, cardW, cardH, 0.20f,
                      isSel ? Ui::rgba(32, 22, 48, 252) : Ui::rgba(20, 16, 32, 235),
                      isSel ? starClr : Ui::rgba(50, 42, 70));

            // Barra lateral izquierda de acento
            C2D_DrawRectSolid(cardX, cardY, 0.22f, isSel ? 5.5f : 3.5f, cardH, starClr);
            if (isSel) {
                Ui::glow(cardX + 4.0f, cardY + cardH * 0.5f, 0.19f, 20.0f, Ui::withAlpha(starClr, 95), 2);
            }

            // Título y Artista
            drawTextWithShadow(ctx.textBuf, cardX + 14.0f, cardY + 4.0f, isSel ? 0.44f : 0.40f,
                               ctx.clrWhite, truncateString(cSet.title, 24).c_str(), 0.25f);
            drawText(ctx.textBuf, cardX + 14.0f, cardY + 19.0f, 0.30f,
                     Ui::rgba(170, 165, 195), truncateString(cSet.artist, 26).c_str(), 0.25f);

            // Chip central: N DIFS ●●●●
            int diffCount = static_cast<int>(cSet.difficulties.size());
            float chipDifsW = 62.0f;
            float chipDifsX = cardX + cardW - 124.0f;
            Ui::pill(chipDifsX, cardY + 8.5f, chipDifsW, 17.0f, 0.23f, Ui::rgba(16, 12, 26, 230));

            char dCntStr[16];
            std::snprintf(dCntStr, sizeof(dCntStr), "%d DIFS", diffCount);
            drawText(ctx.textBuf, chipDifsX + 5.0f, cardY + 10.5f, 0.24f, Ui::rgba(200, 195, 220), dCntStr, 0.26f);

            for (int dot = 0; dot < 4 && dot < diffCount; ++dot) {
                float dotX = chipDifsX + 38.0f + dot * 5.5f;
                u32 dotClr = getStarRatingColor(cSet.difficulties[dot].starRating);
                C2D_DrawCircleSolid(dotX, cardY + 17.0f, 0.26f, 1.8f, dotClr);
            }

            // Chip derecho: ★ X.X
            float chipStarW = 48.0f;
            float chipStarX = cardX + cardW - 56.0f;
            Ui::pill(chipStarX, cardY + 8.5f, chipStarW, 17.0f, 0.23f, Ui::rgba(16, 12, 26, 230));
            drawRectOutline(chipStarX, cardY + 8.5f, chipStarW, 17.0f, 1.0f, starClr, 0.25f);

            drawStarVector(chipStarX + 10.0f, cardY + 17.0f, 3.8f, 0.28f, starClr);
            char sValBuf[16];
            std::snprintf(sValBuf, sizeof(sValBuf), "%.1f", maxStars);
            drawText(ctx.textBuf, chipStarX + 17.0f, cardY + 10.0f, 0.30f, starClr, sValBuf, 0.26f);
        }

        // 3. Fila de Dificultades chips: DIFICULTAD: [★3.0 Oriental's N...] [★4.8 Chu's Hard] ...
        float diffRowY = 184.0f;
        drawText(ctx.textBuf, 8.0f, diffRowY + 3.0f, 0.28f, pinkAccent, "DIFICULTAD:", 0.25f);

        if (!ctx.songMgr.sets.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int nDiffs = static_cast<int>(curSet.difficulties.size());
            float startX = 66.0f;
            float chipW = 58.0f;

            for (int d = 0; d < 4 && d < nDiffs; ++d) {
                float cx = startX + d * (chipW + 4.0f);
                bool isCur = (d == ctx.selectedDiff);
                const auto& dEnt = curSet.difficulties[d];
                float dStars = getModAdjustedStars(dEnt.starRating, ctx.settings.activeMods);
                u32 starClr = getStarRatingColor(dStars);

                Ui::panel(cx, diffRowY, chipW, 17.0f, 0.22f,
                          isCur ? Ui::rgba(65, 20, 50, 245) : Ui::rgba(22, 18, 32, 230),
                          isCur ? pinkAccent : Ui::rgba(50, 42, 65));

                char dChipBuf[32];
                std::snprintf(dChipBuf, sizeof(dChipBuf), "★%.1f %s", dStars, truncateString(dEnt.version, 6).c_str());
                drawCenteredText(ctx.textBuf, cx + chipW * 0.5f, diffRowY + 8.5f, 0.25f,
                                 isCur ? ctx.clrWhite : starClr, dChipBuf, 0.26f);
            }
        }

        // 4. Barra inferior de 5 botones inclinados: TABLET | BUSCAR | MODS | ORDEN | JUGAR
        float btnY = 205.0f;
        float slant = -5.0f;

        // Botón 1: TABLET / Arriba (#4290FB Azul)
        Ui::slantButton(6.0f, btnY, 58.0f, 31.0f, slant, Ui::rgba(66, 144, 251),
                        "TABLET", !ctx.settings.playOnBottomScreen ? "Arriba" : "Abajo",
                        ctx.textBuf, false, 0.22f);

        // Botón 2: BUSCAR / Filtro (#00D8F0 Cian)
        Ui::slantButton(67.0f, btnY, 58.0f, 31.0f, slant, Ui::rgba(0, 216, 240),
                        "BUSCAR", "Filtro", ctx.textBuf, false, 0.22f);

        // Botón 3: MODS / SELECT (#9355E8 Morado)
        Ui::slantButton(128.0f, btnY, 58.0f, 31.0f, slant, Ui::rgba(147, 85, 232),
                        "MODS", "SELECT", ctx.textBuf, ctx.settings.activeMods != MOD_NONE, 0.22f);

        // Botón 4: ORDEN / Titulo (#3B5998 Azul Oscuro)
        Ui::slantButton(189.0f, btnY, 58.0f, 31.0f, slant, Ui::rgba(66, 100, 220),
                        "ORDEN", SongManager::getSortModeName(ctx.songMgr.sortMode),
                        ctx.textBuf, false, 0.22f);

        // Botón 5: JUGAR / Botón A (#FF66AB Rosa Brillante)
        Ui::slantButton(250.0f, btnY, 64.0f, 31.0f, slant, pinkAccent,
                        "> JUGAR", "Boton A", ctx.textBuf, true, 0.22f);
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
