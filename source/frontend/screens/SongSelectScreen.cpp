#include "SongSelectScreen.hpp"

void runSongSelectScreen(AppContext& ctx) {
    ctx.audio.update();
    float beatPulse = ctx.getBpmPulse();

    // Debounce inteligente: solo cargar fondo y audio cuando el carrusel se ha detenido visualmente
    // y el usuario ha dejado de desplazarse entre canciones
    bool carouselSettled = (std::fabs(ctx.carouselPos - static_cast<float>(ctx.selectedSet)) < 0.05f);
    if (!carouselSettled) {
        // Mientras el carrusel se está moviendo con suavidad, aplazamos la carga
        // para que el hilo de renderizado fluya a 60 FPS continuos sin competir por I/O
        if (ctx.previewDelay < (ctx.isNew3DS ? 10 : 16)) {
            ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
        }
    } else if (ctx.previewDelay > 0) {
        if (--ctx.previewDelay == 0) {
            ctx.updateSongPreview();
        }
    }

    // Subir a GPU el fondo decodificado en segundo plano (si ya esta listo)
    if (ctx.bgLoader.poll(ctx.bgTex)) ctx.bgFade = 0.0f;
    ctx.bgFade = Ui::approach(ctx.bgFade, 255.0f, 0.12f);
    if (ctx.settings.songPreview && ctx.audio.isSongFinished() && !ctx.audio.isPreviewLoading()) {
        ctx.updateSongPreview();
    }

    // Animación suave de elevación e iluminación de las tarjetas inferiores estilo osu!lazer
    ctx.footerCardHover = -1;
    if (ctx.isTouching && !ctx.modMenuOpen && !ctx.deleteConfirmOpen && !ctx.rankingExpanded && !ctx.searchKeyboardOpen && ctx.touch.py >= 136 && ctx.touch.py <= 236) {
        for (int k = 0; k < 5; ++k) {
            float bx = 8.0f + k * 62.0f;
            if (ctx.touch.px >= bx && ctx.touch.px <= bx + 56.0f) {
                ctx.footerCardHover = k;
                break;
            }
        }
    }
    for (int k = 0; k < 5; ++k) {
        float targetLift = 0.0f;
        if (ctx.footerCardHover == k) targetLift = 1.0f;
        else if (k == 2 && ctx.modMenuOpen) targetLift = 0.85f;
        else if (k == 1 && !ctx.songMgr.searchQuery.empty()) targetLift = 0.25f + 0.15f * std::sin(ctx.animTick * 0.12f);
        ctx.footerCardLift[k] = Ui::approach(ctx.footerCardLift[k], targetLift, 0.25f);
    }

    // Abrir/Cerrar menú de Mods con SELECT o botón táctil en pantalla inferior
    if (ctx.kDown & KEY_SELECT && !ctx.deleteConfirmOpen && !ctx.searchKeyboardOpen) {
        ctx.modMenuOpen = !ctx.modMenuOpen;
        ctx.audio.playHitSound(0);
    }
    if (ctx.touchJustDown && !ctx.modMenuOpen && !ctx.deleteConfirmOpen && !ctx.searchKeyboardOpen) {
        // Tocar botón pequeño [BORRAR] en el header (x: 246..312, y: 6..28) (Deshabilitado en multijugador)
        if (ctx.touch.py >= 6 && ctx.touch.py <= 28 && ctx.touch.px >= 246 && ctx.touch.px <= 312) {
            if (!ctx.multiMgr.inMultiplayer && !ctx.songMgr.sets.empty()) {
                ctx.deleteConfirmOpen = true;
                ctx.audio.playHitSound(0);
            }
        }
        // Tocar la dificultad / estrellas para alternar a la siguiente dificultad del mapa
        else if (ctx.touch.py >= 32 && ctx.touch.py <= 54 && ctx.touch.px >= 8 && ctx.touch.px <= 148) {
            if (!ctx.songMgr.sets.empty()) {
                const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
                if (curSet.difficulties.size() > 1) {
                    ctx.selectedDiff = (ctx.selectedDiff + 1) % curSet.difficulties.size();
                    ctx.audio.playHitSound(0);
                    ctx.previewDelay = ctx.isNew3DS ? 6 : 10;
                    ctx.updateSongPreview();
                }
            }
        }
        // Tocar pestañas [ RESUMEN ] / [ RANKING LOCAL ] o el botón de acción
        else if (ctx.touch.py >= 50 && ctx.touch.py <= 72) {
            if (ctx.touch.px >= 8 && ctx.touch.px <= 118) {
                ctx.songSelectTab = 0;
                ctx.rankingExpanded = false;
                ctx.audio.playHitSound(0);
            } else if (ctx.touch.px >= 122 && ctx.touch.px <= 244) {
                ctx.songSelectTab = 1;
                ctx.audio.playHitSound(0);
            } else if (ctx.touch.px >= 246 && ctx.touch.px <= 312) {
                if (ctx.songSelectTab == 1) {
                    ctx.rankingExpanded = !ctx.rankingExpanded;
                    ctx.audio.playHitSound(2);
                } else {
                    ctx.songSelectTab = 1;
                    ctx.audio.playHitSound(0);
                }
            }
        }
        // Si está en RESUMEN y toca la tarjeta de Personal Best, abrir Ranking Local
        else if (ctx.songSelectTab == 0 && ctx.touch.py >= 72 && ctx.touch.py <= 112 && ctx.touch.px >= 8 && ctx.touch.px <= 312) {
            ctx.songSelectTab = 1;
            ctx.audio.playHitSound(0);
        }
        // Si está en RANKING LOCAL (modo compacto) y toca la lista, expandirla
        else if (ctx.songSelectTab == 1 && !ctx.rankingExpanded && ctx.touch.py >= 72 && ctx.touch.py <= 142 && ctx.touch.px >= 8 && ctx.touch.px <= 312) {
            ctx.rankingExpanded = true;
            ctx.audio.playHitSound(0);
        }
        // Si está en modo expandido y toca la barra inferior de cerrar
        else if (ctx.rankingExpanded && ctx.touch.py >= 220 && ctx.touch.py <= 240) {
            ctx.rankingExpanded = false;
            ctx.audio.playHitSound(0);
        }

        // Solo procesar las 5 tarjetas inferiores si NO está el ranking expandido
        if (!ctx.rankingExpanded) {
            for (int k = 0; k < 5; ++k) {
                float bx = 8.0f + k * 62.0f;
                if (ctx.touch.px >= bx && ctx.touch.px <= bx + 56.0f && ctx.touch.py >= 136 && ctx.touch.py <= 236) {
                    ctx.footerCardLift[k] = 1.0f;
                    if (k == 0) { // Alternar Modo Táctil / Tablet en la misma tarjeta
                        if (!ctx.multiMgr.inMultiplayer) {
                            ctx.settings.playOnBottomScreen = !ctx.settings.playOnBottomScreen;
                            ctx.settings.save();
                            ctx.audio.playHitSound(2);
                        }
                    } else if (k == 1) { // Buscar
                        ctx.openSearchKeyboard();
                        ctx.touchJustDown = false;
                        break;
                    } else if (k == 2) { // Mods
                        ctx.modMenuOpen = true;
                        ctx.audio.playHitSound(0);
                    } else if (k == 3) { // Orden
                        ctx.songMgr.sortMode = (ctx.songMgr.sortMode + 1) % 4;
                        ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                        ctx.selectedSet = 0;
                        ctx.selectedDiff = 0;
                        ctx.rankingScroll = 0;
                        ctx.carouselPos = 0.0f;
                        ctx.audio.playHitSound(2);
                        ctx.updateSongPreview();
                    } else if (k == 4) { // Modo de Juego (STD / TAIKO / CATCH / MANIA)
                        if (!ctx.multiMgr.inMultiplayer) {
                            if (ctx.settings.gameMode == MODE_STD) ctx.settings.gameMode = MODE_TAIKO;
                            else if (ctx.settings.gameMode == MODE_TAIKO) ctx.settings.gameMode = MODE_CATCH;
                            else if (ctx.settings.gameMode == MODE_CATCH) ctx.settings.gameMode = MODE_MANIA;
                            else ctx.settings.gameMode = MODE_STD;
                            ctx.settings.save();
                            ctx.audio.playHitSound(2);
                        }
                    }
                    break;
                }
            }
        }
    }

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
            if (ctx.touch.px >= 28 && ctx.touch.px <= 154 && ctx.touch.py >= 146 && ctx.touch.py <= 184) { // CANCELAR
                ctx.deleteConfirmOpen = false;
                ctx.audio.playHitSound(0);
            } else if (ctx.touch.px >= 166 && ctx.touch.px <= 292 && ctx.touch.py >= 146 && ctx.touch.py <= 184) { // BORRAR
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

        // Soporte táctil para las 15 tarjetas de Mods en la cuadrícula 5x3
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
            // Botón Limpiar (esquina superior derecha) o volver
            if (ctx.touch.px >= 220 && ctx.touch.px <= 312 && ctx.touch.py >= 6 && ctx.touch.py <= 36) {
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
        }
    } else if (ctx.searchKeyboardOpen) {
        if (ctx.searchKeyFlashTimer > 0) ctx.searchKeyFlashTimer--;

        // Botones físicos cuando el teclado está abierto
        if (ctx.kDown & (KEY_B | KEY_START)) {
            ctx.searchKeyboardOpen = false;
            ctx.audio.playHitSound(0);
        }
        else if (ctx.kDown & KEY_X) {
            ctx.songMgr.searchQuery.clear();
            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
            ctx.selectedSet = 0;
            ctx.selectedDiff = 0;
            ctx.rankingScroll = 0;
            ctx.carouselPos = 0.0f;
            ctx.audio.playHitSound(0);
            ctx.updateSongPreview();
        }
        else if (ctx.kDown & KEY_Y) {
            if (!ctx.songMgr.searchQuery.empty()) {
                ctx.songMgr.searchQuery.pop_back();
                ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                ctx.selectedSet = 0;
                ctx.selectedDiff = 0;
                ctx.rankingScroll = 0;
                ctx.carouselPos = 0.0f;
                ctx.audio.playHitSound(0);
                ctx.updateSongPreview();
            }
        }
        else if (ctx.kDown & (KEY_ZL | KEY_ZR)) {
            ctx.searchKeyboardOpen = false;
            ctx.audio.playHitSound(0);
        }
        else if (ctx.kDown & KEY_A) {
            ctx.searchKeyboardOpen = false;
            ctx.audio.playHitSound(2);
        }

        // Navegar por la lista filtrada de canciones mientras el teclado está abierto
        if (!ctx.songMgr.sets.empty()) {
            int numSets = static_cast<int>(ctx.songMgr.sets.size());
            int setDelta = 0;
            if (ctx.kDown & (KEY_DUP | KEY_L))   setDelta = -1;
            if (ctx.kDown & (KEY_DDOWN | KEY_R)) setDelta = +1;
            if (setDelta == 0 && ctx.analogNavCooldown == 0) {
                if (ctx.cstick.dy > 45 || ctx.cpad.dy > 65) {
                    setDelta = -1;
                    ctx.analogNavCooldown = 11;
                } else if (ctx.cstick.dy < -45 || ctx.cpad.dy < -65) {
                    setDelta = +1;
                    ctx.analogNavCooldown = 11;
                }
            }
            if (setDelta != 0) {
                ctx.selectedSet = (ctx.selectedSet + setDelta + numSets) % numSets;
                ctx.selectedDiff = 0;
                ctx.rankingScroll = 0;
                ctx.carouselPos = static_cast<float>(ctx.selectedSet);
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
            }
        }

        // Mantener pulsado el botón de borrar para repetición rápida
        bool touchingDelete = ctx.isTouching && (
            (ctx.touch.px >= 271.0f && ctx.touch.px <= 316.0f && ctx.touch.py >= 54.0f && ctx.touch.py <= 86.0f) ||
            (ctx.touch.px >= 208.0f && ctx.touch.px <= 260.0f && ctx.touch.py >= 194.0f && ctx.touch.py <= 236.0f)
        );
        if (touchingDelete) {
            ctx.searchBackspaceHoldTimer++;
            if (ctx.searchBackspaceHoldTimer > 20 && (ctx.searchBackspaceHoldTimer % 4 == 0)) {
                if (!ctx.songMgr.searchQuery.empty()) {
                    ctx.songMgr.searchQuery.pop_back();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.rankingScroll = 0;
                    ctx.carouselPos = 0.0f;
                    ctx.audio.playHitSound(0);
                    ctx.updateSongPreview();
                }
            }
        } else {
            ctx.searchBackspaceHoldTimer = 0;
        }

        // Entrada táctil por pulsación única
        if (ctx.touchJustDown) {
            // 1. Cabecera: Limpiar [✕] o Listo [LISTO]
            if (ctx.touch.px >= 232.0f && ctx.touch.px <= 258.0f && ctx.touch.py >= 3.0f && ctx.touch.py <= 31.0f) {
                ctx.songMgr.searchQuery.clear();
                ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                ctx.selectedSet = 0;
                ctx.selectedDiff = 0;
                ctx.rankingScroll = 0;
                ctx.carouselPos = 0.0f;
                ctx.audio.playHitSound(0);
                ctx.updateSongPreview();
            } else if (ctx.touch.px >= 262.0f && ctx.touch.px <= 316.0f && ctx.touch.py >= 3.0f && ctx.touch.py <= 31.0f) {
                ctx.searchKeyboardOpen = false;
                ctx.audio.playHitSound(2);
            }
            // 2. Fila de Filtros Rápidos (Chips) en y: 34..52
            else if (ctx.touch.py >= 34.0f && ctx.touch.py <= 52.0f) {
                if (ctx.touch.px >= 4.0f && ctx.touch.px <= 44.0f) { // TODOS
                    ctx.songMgr.searchQuery.clear();
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.rankingScroll = 0;
                    ctx.carouselPos = 0.0f;
                    ctx.audio.playHitSound(2);
                    ctx.updateSongPreview();
                } else if (ctx.touch.px >= 47.0f && ctx.touch.px <= 79.0f) { // STD
                    ctx.toggleSearchTag("mode:std");
                } else if (ctx.touch.px >= 82.0f && ctx.touch.px <= 122.0f) { // TAIKO
                    ctx.toggleSearchTag("mode:taiko");
                } else if (ctx.touch.px >= 125.0f && ctx.touch.px <= 165.0f) { // CATCH
                    ctx.toggleSearchTag("mode:catch");
                } else if (ctx.touch.px >= 168.0f && ctx.touch.px <= 208.0f) { // MANIA
                    ctx.toggleSearchTag("mode:mania");
                } else if (ctx.touch.px >= 211.0f && ctx.touch.px <= 243.0f) { // ★4+
                    ctx.toggleSearchTag("★4");
                } else if (ctx.touch.px >= 246.0f && ctx.touch.px <= 278.0f) { // ★5+
                    ctx.toggleSearchTag("★5");
                }
            }
            // 3. Fila 0: Números 1..0 y Borrar superior (y: 54..86)
            else if (ctx.touch.py >= 54.0f && ctx.touch.py <= 86.0f) {
                bool hit = false;
                const char* numChars = "1234567890";
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 26.5f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 24.5f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += numChars[i];
                            ctx.searchLastKeyIndex = i;
                            ctx.searchKeyFlashTimer = 6;
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.rankingScroll = 0;
                            ctx.carouselPos = 0.0f;
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        hit = true;
                        break;
                    }
                }
                if (!hit && ctx.touch.px >= 271.0f && ctx.touch.px <= 316.0f) {
                    if (!ctx.songMgr.searchQuery.empty()) {
                        ctx.songMgr.searchQuery.pop_back();
                        ctx.searchLastKeyIndex = 10;
                        ctx.searchKeyFlashTimer = 6;
                        ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                        ctx.selectedSet = 0;
                        ctx.selectedDiff = 0;
                        ctx.rankingScroll = 0;
                        ctx.carouselPos = 0.0f;
                        ctx.audio.playHitSound(0);
                        ctx.updateSongPreview();
                    }
                }
            }
            // 4. Fila 1: Q W E R T Y U I O P (y: 89..121)
            else if (ctx.touch.py >= 89.0f && ctx.touch.py <= 121.0f) {
                const char* r1Chars = "qwertyuiop";
                for (int i = 0; i < 10; ++i) {
                    float kx = 4.0f + i * 31.0f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 29.0f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r1Chars[i];
                            ctx.searchLastKeyIndex = 11 + i;
                            ctx.searchKeyFlashTimer = 6;
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.rankingScroll = 0;
                            ctx.carouselPos = 0.0f;
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            }
            // 5. Fila 2: A S D F G H J K L - (y: 124..156)
            else if (ctx.touch.py >= 124.0f && ctx.touch.py <= 156.0f) {
                const char* r2Chars = "asdfghjkl-";
                for (int i = 0; i < 10; ++i) {
                    float kx = 8.0f + i * 30.2f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 28.2f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r2Chars[i];
                            ctx.searchLastKeyIndex = 21 + i;
                            ctx.searchKeyFlashTimer = 6;
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.rankingScroll = 0;
                            ctx.carouselPos = 0.0f;
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            }
            // 6. Fila 3: Z X C V B N M . / (y: 159..191)
            else if (ctx.touch.py >= 159.0f && ctx.touch.py <= 191.0f) {
                const char* r3Chars = "zxcvbnm./";
                for (int i = 0; i < 9; ++i) {
                    float kx = 16.0f + i * 32.0f;
                    if (ctx.touch.px >= kx && ctx.touch.px <= kx + 29.8f) {
                        if (ctx.songMgr.searchQuery.size() < 32) {
                            ctx.songMgr.searchQuery += r3Chars[i];
                            ctx.searchLastKeyIndex = 31 + i;
                            ctx.searchKeyFlashTimer = 6;
                            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                            ctx.selectedSet = 0;
                            ctx.selectedDiff = 0;
                            ctx.rankingScroll = 0;
                            ctx.carouselPos = 0.0f;
                            ctx.audio.playHitSound(0);
                            ctx.updateSongPreview();
                        }
                        break;
                    }
                }
            }
            // 7. Fila 4: Acciones inferiores (y: 194..238)
            else if (ctx.touch.py >= 194.0f && ctx.touch.py <= 238.0f) {
                // Limpiar (x: 4..60)
                if (ctx.touch.px >= 4.0f && ctx.touch.px <= 60.0f) {
                    ctx.songMgr.searchQuery.clear();
                    ctx.searchLastKeyIndex = 40;
                    ctx.searchKeyFlashTimer = 6;
                    ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                    ctx.selectedSet = 0;
                    ctx.selectedDiff = 0;
                    ctx.rankingScroll = 0;
                    ctx.carouselPos = 0.0f;
                    ctx.audio.playHitSound(0);
                    ctx.updateSongPreview();
                }
                // Espacio (x: 63..205)
                else if (ctx.touch.px >= 63.0f && ctx.touch.px <= 205.0f) {
                    if (ctx.songMgr.searchQuery.size() < 32 && !ctx.songMgr.searchQuery.empty() && ctx.songMgr.searchQuery.back() != ' ') {
                        ctx.songMgr.searchQuery += ' ';
                        ctx.searchLastKeyIndex = 41;
                        ctx.searchKeyFlashTimer = 6;
                        ctx.audio.playHitSound(0);
                    }
                }
                // Borrar (x: 208..260)
                else if (ctx.touch.px >= 208.0f && ctx.touch.px <= 260.0f) {
                    if (!ctx.songMgr.searchQuery.empty()) {
                        ctx.songMgr.searchQuery.pop_back();
                        ctx.searchLastKeyIndex = 42;
                        ctx.searchKeyFlashTimer = 6;
                        ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
                        ctx.selectedSet = 0;
                        ctx.selectedDiff = 0;
                        ctx.rankingScroll = 0;
                        ctx.carouselPos = 0.0f;
                        ctx.audio.playHitSound(0);
                        ctx.updateSongPreview();
                    }
                }
                // OK / Listo (x: 263..316)
                else if (ctx.touch.px >= 263.0f && ctx.touch.px <= 316.0f) {
                    ctx.searchLastKeyIndex = 43;
                    ctx.searchKeyFlashTimer = 6;
                    ctx.searchKeyboardOpen = false;
                    ctx.audio.playHitSound(2);
                }
            }
        }
    } else {
        if (ctx.rankingExpanded) {
            if (ctx.kDown & KEY_B) {
                ctx.rankingExpanded = false;
                ctx.audio.playHitSound(0);
                return;
            }
            if (ctx.kDown & KEY_DUP) {
                if (ctx.rankingScroll > 0) {
                    ctx.rankingScroll--;
                    ctx.audio.playHitSound(0);
                }
            }
            if (ctx.kDown & KEY_DDOWN) {
                ctx.rankingScroll++;
                ctx.audio.playHitSound(0);
            }
        } else {
            if (ctx.kDown & KEY_B) {
                if (ctx.songSelectTab != 0) {
                    ctx.songSelectTab = 0;
                    ctx.audio.playHitSound(0);
                    return;
                }
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
        }

        if (ctx.kDown & KEY_START) {
            ctx.songMgr.sortMode = (ctx.songMgr.sortMode + 1) % 4;
            ctx.songMgr.applySortAndFilter(&ctx.scoreMgr);
            ctx.selectedSet = 0;
            ctx.selectedDiff = 0;
            ctx.rankingScroll = 0;
            ctx.carouselPos = 0.0f;
            ctx.audio.playHitSound(2);
            ctx.updateSongPreview();
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

            // Desplazamiento vertical de canciones con C-Stick (New 3DS) o Circle Pad (Arriba / Abajo)
            if (setDelta == 0 && ctx.analogNavCooldown == 0 && !ctx.rankingExpanded) {
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
                ctx.rankingScroll = 0;
                ctx.audio.playHitSound(0);
                ctx.previewDelay = ctx.isNew3DS ? 10 : 16;
            }

            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            int numDiffs = static_cast<int>(curSet.difficulties.size());
            if (numDiffs > 0) {
                int diffDelta = 0;
                if (ctx.kDown & KEY_DLEFT)  diffDelta = -1;
                if (ctx.kDown & KEY_DRIGHT) diffDelta = +1;
                if (diffDelta == 0 && ctx.analogNavCooldown == 0 && !ctx.rankingExpanded) {
                    if (ctx.cstick.dx < -45 || ctx.cpad.dx < -65)      { diffDelta = -1; ctx.analogNavCooldown = 10; }
                    else if (ctx.cstick.dx > 45 || ctx.cpad.dx > 65)   { diffDelta = +1; ctx.analogNavCooldown = 10; }
                }
                if (diffDelta != 0) {
                    ctx.selectedDiff = (ctx.selectedDiff + diffDelta + numDiffs) % numDiffs;
                    ctx.rankingScroll = 0;
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
                                if (ctx.multiPickerAction == PICKER_ACTION_QUEUE) {
                                    ctx.multiMgr.enqueueBeatmap(qm);
                                } else {
                                    ctx.multiMgr.setActiveBeatmap(qm);
                                }
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
    }

    ctx.carouselPos = Ui::approach(ctx.carouselPos, static_cast<float>(ctx.selectedSet), 0.22f);

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // --- PANTALLA SUPERIOR: FONDO DEL BEATMAP, LISTA DE CANCIONES Y ESTRELLAS (★) EN 3D ESTEREOSCÓPICO ---
    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);

        float dBg = depthShift(-3.5f);
        if (ctx.bgTex.loaded) {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), ctx.clrPrimary, 0.01f);
            ctx.bgTex.drawRect(dBg, 0.0f, 400.0f, 240.0f, 0.02f, static_cast<u8>(ctx.bgFade));
            u8 dimAlpha = static_cast<u8>((ctx.settings.bgDim * 200) / 100);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 400.0f, 240.0f, Ui::rgba(14, 12, 22, dimAlpha));
        } else {
            Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(30, 18, 54), Ui::rgba(10, 8, 22), ctx.clrPrimary, 0.02f);
        }
        // Vineta izquierda para dar legibilidad al panel de informacion
        Ui::gradH(0.0f, 0.0f, 240.0f, 240.0f, 0.05f, Ui::rgba(8, 6, 16, 220), Ui::rgba(8, 6, 16, 0));

        // Cabecera superior estilo osu!lazer con branding y badges
        Ui::gradV(0.0f, 0.0f, 400.0f, 30.0f, 0.3f, Ui::rgba(14, 11, 24, 250), Ui::rgba(14, 11, 24, 180));
        C2D_DrawRectSolid(0.0f, 29.0f, 0.31f, 400.0f, 1.5f, Ui::rgba(255, 102, 171, 190));

        Ui::pill(10.0f, 6.0f, 42.0f, 18.0f, 0.32f, Ui::rgba(255, 102, 171, 230));
        drawCenteredText(ctx.textBuf, 31.0f, 15.0f, 0.37f, ctx.clrWhite, "osu!", 0.9f);
        if (ctx.multiMgr.inMultiplayer) {
            const char* sTitle = (ctx.multiPickerAction == PICKER_ACTION_QUEUE)
                ? LOC(STR_SONG_MULTI_QUEUE)
                : LOC(STR_SONG_MULTI_PICK);
            drawText(ctx.textBuf, 58.0f, 7.5f, 0.44f, ctx.clrCursor, sTitle, 0.9f);
        } else {
            drawText(ctx.textBuf, 58.0f, 7.5f, 0.44f, ctx.clrWhite, LOC(STR_SONG_SELECT_TITLE), 0.9f);
        }

        if (ctx.searchKeyboardOpen) {
            std::string fBadge = ctx.songMgr.searchQuery.empty() ? std::string(LOC(STR_SEARCH)) + "..." : truncateString(ctx.songMgr.searchQuery, 12);
            Ui::pill(210.0f, 6.0f, 130.0f, 18.0f, 0.32f, Ui::rgba(255, 102, 171, 230));
            drawCenteredTextWithShadow(ctx.textBuf, 275.0f, 15.0f, 0.34f, ctx.clrWhite, fBadge.c_str(), 0.9f);
        } else if (!ctx.songMgr.searchQuery.empty()) {
            std::string fBadge = std::string(LOC(STR_SONG_FILTER_BADGE)) + ": " + truncateString(ctx.songMgr.searchQuery, 10);
            Ui::pill(236.0f, 6.0f, 104.0f, 18.0f, 0.32f, Ui::rgba(0, 180, 220, 230));
            drawCenteredTextWithShadow(ctx.textBuf, 288.0f, 15.0f, 0.34f, ctx.clrWhite, fBadge.c_str(), 0.9f);
        } else {
            char sortBadge[64];
            std::snprintf(sortBadge, sizeof(sortBadge), "%s: %s", LOC(STR_MODE), SongManager::getSortModeName(ctx.songMgr.sortMode));
            Ui::pill(236.0f, 6.0f, 104.0f, 18.0f, 0.32f, Ui::rgba(45, 38, 72, 230));
            drawCenteredTextWithShadow(ctx.textBuf, 288.0f, 15.0f, 0.34f, ctx.clrCursor, sortBadge, 0.9f);
        }

        char headerInfo[96];
        std::snprintf(headerInfo, sizeof(headerInfo), "%d / %d",
            ctx.songMgr.sets.empty() ? 0 : (ctx.selectedSet + 1),
            static_cast<int>(ctx.songMgr.sets.size()));
        Ui::pill(346.0f, 6.0f, 46.0f, 18.0f, 0.32f, Ui::rgba(36, 30, 54, 230));
        drawCenteredText(ctx.textBuf, 369.0f, 15.0f, 0.36f, ctx.clrWhite, headerInfo, 0.9f);

        if (ctx.songMgr.sets.empty()) {
            float dEmpty = depthShift(2.5f);
            Ui::panel(40.0f + dEmpty, 70.0f, 320.0f, 100.0f, 0.3f, Ui::rgba(30, 27, 44, 235), ctx.clrMiss);
            if (!ctx.songMgr.searchQuery.empty()) {
                drawText(ctx.textBuf, 56.0f + dEmpty, 82.0f, 0.48f, ctx.clrCursor, LOC(STR_SONG_NO_SEARCH_RESULTS));
                drawText(ctx.textBuf, 56.0f + dEmpty, 108.0f, 0.46f, ctx.clrWhite, truncateString("\"" + ctx.songMgr.searchQuery + "\"", 34).c_str());
                drawText(ctx.textBuf, 56.0f + dEmpty, 134.0f, 0.40f, ctx.clr300, LOC(STR_SONG_TAP_SEARCH_CLEAR));
            } else {
                drawText(ctx.textBuf, 56.0f + dEmpty, 82.0f, 0.5f, ctx.clrMiss, LOC(STR_SONG_NO_BEATMAPS));
                drawText(ctx.textBuf, 56.0f + dEmpty, 110.0f, 0.43f, ctx.clrWhite, LOC(STR_SONG_PUT_FILES));
                drawText(ctx.textBuf, 56.0f + dEmpty, 130.0f, 0.45f, ctx.clr300, "sdmc:/3ds/osu-3ds/songs/");
            }
        } else {
            const int nSets = static_cast<int>(ctx.songMgr.sets.size());

            // ---- Carrusel de canciones (derecha), estilo tarjetas de sala multijugador ----
            for (int off = -4; off <= 4; ++off) {
                int idx = ctx.selectedSet + off;
                if (idx < 0 || idx >= nSets) continue;
                float rel = idx - ctx.carouselPos;
                float dist = std::fabs(rel);
                if (dist > 4.2f) continue;
                const auto& cs = ctx.songMgr.sets[idx];
                if (cs.difficulties.empty()) continue;

                bool sel = (idx == ctx.selectedSet);
                float focus = std::max(0.0f, 1.0f - dist);
                float dCard = depthShift(0.8f + focus * 3.2f - dist * 0.45f);
                float cardH = 38.0f + focus * 12.0f;
                float cy = 122.0f + rel * 46.0f;
                float cx = 206.0f - focus * 36.0f - (sel ? beatPulse * 4.0f : 0.0f) + std::max(0.0f, dist - 1.0f) * 6.0f + dCard;
                float cardY = cy - cardH * 0.5f;
                float cardW = 400.0f - (cx - dCard) + 10.0f;
                if (cardY > 240.0f || cardY + cardH < 30.0f) continue;

                float maxStars = cs.difficulties.back().starRating;
                u32 starClr = getStarRatingColor(getModAdjustedStars(maxStars, ctx.settings.activeMods));
                u8 fade = static_cast<u8>(255.0f * std::max(0.35f, 1.0f - dist * 0.18f));

                u32 bgClr = sel ? Ui::rgba(36, 26, 54, 250) : Ui::rgba(22, 19, 36, fade);
                u32 borderClr = sel ? starClr : Ui::rgba(50, 44, 72, fade);

                // Sombra suave proyectada estilo tarjeta multijugador
                C2D_DrawRectSolid(cx + 2.0f, cardY + 3.0f, 0.08f, cardW, cardH, Ui::rgba(0, 0, 0, 85));

                // Tarjeta rectangular con degradado, sombra y borde superior
                Ui::panel(cx, cardY, cardW, cardH, 0.10f, bgClr, borderClr);

                if (sel) {
                    // Brillo suave y pastilla vertical de acento iluminada como en salas multi
                    Ui::glow(cx + 10.0f, cardY + cardH * 0.5f, 0.09f, 26.0f, Ui::withAlpha(starClr, 100), 3);
                    Ui::pill(cx + 4.0f, cardY + 4.0f, 4.0f, cardH - 8.0f, 0.14f, starClr);
                } else {
                    // Barra vertical de acento sutil
                    C2D_DrawRectSolid(cx, cardY, 0.12f, 3.5f, cardH, Ui::withAlpha(starClr, fade));
                }

                u32 titleClr = sel ? ctx.clrWhite : Ui::rgba(215, 210, 235, fade);
                drawTextWithShadow(ctx.textBuf, cx + 14.0f, cardY + 4.0f, sel ? 0.46f : 0.40f, titleClr, truncateString(cs.title, sel ? 24 : 22).c_str(), 0.15f);
                drawText(ctx.textBuf, cx + 14.0f, cardY + (sel ? 21.0f : 18.0f), 0.32f, Ui::rgba(185, 180, 210, fade), truncateString(cs.artist, 26).c_str(), 0.15f);

                if (sel) {
                    int dIdx = std::min(ctx.selectedDiff, (int)cs.difficulties.size() - 1);
                    std::string diffName = cs.difficulties[dIdx].version;
                    std::string creatorName = cs.difficulties[dIdx].creator;
                    drawText(ctx.textBuf, cx + 14.0f, cardY + 33.0f, 0.28f, Ui::rgba(255, 224, 120, 255),
                             truncateString("[" + diffName + "] " + std::string(LOC(STR_BY)) + " " + creatorName, 28).c_str(), 0.15f);
                }

                // Pastilla de estrellas a la derecha estilo tarjeta de sala multijugador
                char sTag[16];
                std::snprintf(sTag, sizeof(sTag), "*%.2f", getModAdjustedStars(maxStars, ctx.settings.activeMods));
                float pw = 42.0f;
                Ui::pill(400.0f - pw - 8.0f + dCard, cardY + cardH * 0.5f - 7.5f, pw, 15.0f, 0.16f, Ui::withAlpha(starClr, fade));
                drawCenteredText(ctx.textBuf, 400.0f - pw * 0.5f - 8.0f + dCard, cardY + cardH * 0.5f, 0.28f, Ui::rgba(0, 0, 0, 245), sTag, 0.18f);
            }

            // ---- Panel de informacion (izquierda) flotando hacia adelante en 3D ----
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            const int totalDiffs = static_cast<int>(curSet.difficulties.size());
            const int dIdx = std::max(0, std::min(ctx.selectedDiff, totalDiffs - 1));
            float dInfo = depthShift(2.2f);

            Ui::panel(8.0f + dInfo, 36.0f, 166.0f, 74.0f, 0.3f, Ui::rgba(24, 21, 40, 225), ctx.clrPrimary);
            drawText(ctx.textBuf, 16.0f + dInfo, 40.0f, 0.50f, ctx.clrWhite, truncateString(curSet.title, 22).c_str(), 0.6f);
            drawText(ctx.textBuf, 16.0f + dInfo, 58.0f, 0.40f, ctx.clrCursor, truncateString(curSet.artist, 26).c_str(), 0.6f);
            if (totalDiffs > 0) {
                const auto& dd = curSet.difficulties[dIdx];
                drawText(ctx.textBuf, 16.0f + dInfo, 73.0f, 0.34f, ctx.clrMuted, truncateString(std::string(LOC(STR_SONG_MAP_BY)) + " " + dd.creator, 28).c_str(), 0.6f);
                char cnt[64];
                float liveBpm = dd.getBPM(getModsSpeedMultiplier(ctx.settings.activeMods));
                std::snprintf(cnt, sizeof(cnt), "%.0f BPM | %dc %ds%s",
                    liveBpm, dd.circleCount, dd.sliderCount,
                    curSet.hasVideo ? " | [VIDEO]" : "");
                drawText(ctx.textBuf, 16.0f + dInfo, 88.0f, 0.34f, curSet.hasVideo ? ctx.clr100 : Ui::rgba(150, 215, 255), cnt, 0.6f);
            }

            // ---- Burbujas de dificultad (estilo lazer) con latido BPM ----
            if (totalDiffs > 0) {
                Ui::panel(8.0f + dInfo, 118.0f, 166.0f, 86.0f, 0.3f, Ui::rgba(24, 21, 40, 225));
                const int maxB = 8;
                int bStart = std::max(0, std::min(dIdx - maxB / 2, totalDiffs - maxB));
                int bEnd = std::min(totalDiffs, bStart + maxB);
                for (int i = bStart; i < bEnd; ++i) {
                    float bx = 22.0f + (i - bStart) * 19.0f + dInfo;
                    float by = 136.0f;
                    bool isSel = (i == dIdx);
                    float st = getModAdjustedStars(curSet.difficulties[i].starRating, ctx.settings.activeMods);
                    u32 sc = getStarRatingColor(st);
                    if (isSel) {
                        float bR = 9.2f + beatPulse * 1.4f;
                        float dBub = depthShift(1.0f);
                        Ui::glow(bx + dBub, by, 0.35f, 14.5f + beatPulse * 3.0f, Ui::withAlpha(sc, static_cast<u8>(130 + beatPulse * 50)), 4);
                        C2D_DrawCircleSolid(bx + dBub, by, 0.4f, bR, sc);
                        Ui::ring(bx + dBub, by, 0.45f, bR + 2.0f, 1.5f, ctx.clrWhite, 18);
                    } else {
                        C2D_DrawCircleSolid(bx, by, 0.4f, 6.0f, Ui::withAlpha(sc, 190));
                    }
                }
                const auto& sd = curSet.difficulties[dIdx];
                float selStars = getModAdjustedStars(sd.starRating, ctx.settings.activeMods);
                u32 selClr = getStarRatingColor(selStars);
                drawText(ctx.textBuf, 14.0f + dInfo, 154.0f, 0.42f, ctx.clrWhite, truncateString(sd.version, 24).c_str(), 0.6f);
                char sl[48];
                std::snprintf(sl, sizeof(sl), "*%.2f", selStars);
                Ui::pill(14.0f + dInfo, 172.0f, 46.0f, 15.0f, 0.5f, selClr);
                drawCenteredText(ctx.textBuf, 37.0f + dInfo, 179.5f, 0.34f, Ui::rgba(20, 16, 30), sl, 0.7f);
                int curSelMode = (ctx.settings.gameMode == MODE_TAIKO || sd.mode == 1) ? 1 : (ctx.settings.gameMode == MODE_CATCH || sd.mode == 2) ? 2 : (ctx.settings.gameMode == MODE_MANIA || sd.mode == 3) ? 3 : 0;
                const auto* rec = ctx.scoreMgr.getRecord(sd.osuPath, curSelMode);
                if (rec) {
                    std::snprintf(sl, sizeof(sl), "%s  %.1f%%", rec->bestGrade.c_str(), rec->bestAcc);
                    drawText(ctx.textBuf, 68.0f + dInfo, 172.0f, 0.38f, ctx.clr100, sl, 0.6f);
                    std::snprintf(sl, sizeof(sl), "%.0f pp", rec->bestPP);
                    drawText(ctx.textBuf, 68.0f + dInfo, 186.0f, 0.34f, ctx.clr300, sl, 0.6f);
                } else {
                    drawText(ctx.textBuf, 68.0f + dInfo, 174.0f, 0.34f, ctx.clrMuted, LOC(STR_SONG_NO_RECORD), 0.6f);
                }
            }
        }

        // Barra inferior de atajos estilo multijugador
        Ui::gradV(0.0f, 212.0f, 400.0f, 28.0f, 0.3f, Ui::rgba(14, 11, 26, 140), Ui::rgba(14, 11, 26, 250));
        if (ctx.searchKeyboardOpen) {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 10.0f, byY, "B", LOC(STR_DONE), Ui::rgba(50, 165, 255));
            drawKeyBadge(ctx.textBuf, 76.0f, byY, "X", LOC(STR_CLEAR), Ui::rgba(255, 180, 40));
            drawKeyBadge(ctx.textBuf, 152.0f, byY, "Y", LOC(STR_DELETE), Ui::rgba(255, 75, 80));
            drawTextWithShadow(ctx.textBuf, 224.0f, byY, 0.33f, ctx.clrCursor, LOC(STR_SEARCH_KEYBOARD_HINT), 0.35f);
        } else if (ctx.multiMgr.inMultiplayer) {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 10.0f, byY, "A", LOC(STR_MULTI_JOIN), Ui::rgba(50, 165, 255));
            drawKeyBadge(ctx.textBuf, 78.0f, byY, "B", LOC(STR_BACK), Ui::rgba(255, 75, 80));
            drawKeyBadge(ctx.textBuf, 146.0f, byY, "SELECT", "Mods", Ui::rgba(255, 102, 171));
            drawKeyBadge(ctx.textBuf, 230.0f, byY, "< >", LOC(STR_DIFFICULTY), Ui::rgba(50, 215, 115));
        } else {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 8.0f, byY, "A", LOC(STR_MENU_PLAY), Ui::rgba(50, 165, 255));
            drawKeyBadge(ctx.textBuf, 76.0f, byY, "START", LOC(STR_MODE), Ui::rgba(165, 120, 255));
            drawKeyBadge(ctx.textBuf, 156.0f, byY, "SELECT", "Mods", Ui::rgba(255, 102, 171));
            drawKeyBadge(ctx.textBuf, 238.0f, byY, "Y", "Wi-Fi", Ui::rgba(255, 180, 40));
            drawKeyBadge(ctx.textBuf, 308.0f, byY, "X", LOC(STR_MENU_SETTINGS), Ui::rgba(50, 215, 115));
        }
        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // --- PANTALLA INFERIOR: DETALLES, ESTRELLAS, PP Y RECORD O SELECTOR DE MODS ---
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(26, 20, 44), Ui::rgba(12, 10, 22), ctx.clrPrimary, 0.02f);
    if (ctx.bgTex.loaded) {
        ctx.bgTex.drawRect(0.0f, 0.0f, 320.0f, 240.0f, 0.03f, 65);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.04f, 320.0f, 240.0f, Ui::rgba(14, 11, 24, 175));
    }
    if (ctx.searchKeyboardOpen) {
        // Fondo oscuro estilizado para el teclado
        Ui::panel(2.0f, 2.0f, 316.0f, 236.0f, 0.50f, Ui::rgba(14, 10, 22, 255), ctx.clrPrimary);

        // =========================================================
        // 1. BARRA DE BÚSQUEDA Y ENTRADA DE TEXTO (y: 3..31)
        // =========================================================
        Ui::panel(4.0f, 3.0f, 226.0f, 28.0f, 0.55f, Ui::rgba(24, 18, 36, 255), ctx.clrPrimary);
        // Icono de lupa estilizada
        drawRing(14.0f, 17.0f, 5.0f, 1.4f, ctx.clrCursor, 14, 0.58f);
        C2D_DrawLine(18.0f, 21.0f, ctx.clrCursor, 22.0f, 25.0f, ctx.clrCursor, 2.0f, 0.58f);

        if (!ctx.songMgr.searchQuery.empty()) {
            bool showCursor = (ctx.animTick / 20) % 2 == 0;
            std::string dispQuery = truncateString(ctx.songMgr.searchQuery, 22);
            if (showCursor) dispQuery += "|";
            drawTextWithShadow(ctx.textBuf, 27.0f, 7.0f, 0.40f, ctx.clrWhite, dispQuery.c_str(), 0.62f);
        } else {
            bool showCursor = (ctx.animTick / 20) % 2 == 0;
            drawTextWithShadow(ctx.textBuf, 27.0f, 7.0f, 0.35f, Ui::rgba(160, 150, 185),
                showCursor ? (std::string(LOC(STR_SEARCH_PLACEHOLDER)) + "|").c_str() : LOC(STR_SEARCH_PLACEHOLDER), 0.62f);
        }

        // Botón Limpiar [X]
        Ui::panel(232.0f, 3.0f, 26.0f, 28.0f, 0.55f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 80, 100));
        drawCenteredTextWithShadow(ctx.textBuf, 245.0f, 17.0f, 0.38f, Ui::rgba(255, 140, 160), "X", 0.62f);

        // Botón Listo [LISTO]
        Ui::panel(262.0f, 3.0f, 54.0f, 28.0f, 0.55f, Ui::rgba(22, 68, 42, 255), ctx.clr100);
        drawCenteredTextWithShadow(ctx.textBuf, 289.0f, 17.0f, 0.36f, ctx.clrWhite, LOC(STR_DONE), 0.62f);

        // =========================================================
        // 2. FILA DE FILTROS RÁPIDOS / CHIPS (y: 34..52)
        // =========================================================
        bool hasStd = (ctx.songMgr.searchQuery.find("mode:std") != std::string::npos);
        bool hasTaiko = (ctx.songMgr.searchQuery.find("mode:taiko") != std::string::npos);
        bool hasCatch = (ctx.songMgr.searchQuery.find("mode:catch") != std::string::npos);
        bool hasMania = (ctx.songMgr.searchQuery.find("mode:mania") != std::string::npos);
        bool hasStar4 = (ctx.songMgr.searchQuery.find("★4") != std::string::npos || ctx.songMgr.searchQuery.find("stars>4") != std::string::npos);
        bool hasStar5 = (ctx.songMgr.searchQuery.find("★5") != std::string::npos || ctx.songMgr.searchQuery.find("stars>5") != std::string::npos);

        // Chip TODOS
        bool noFilter = ctx.songMgr.searchQuery.empty();
        Ui::panel(4.0f, 34.0f, 40.0f, 17.0f, 0.55f,
            noFilter ? Ui::rgba(75, 45, 110, 240) : Ui::rgba(24, 20, 38, 230),
            noFilter ? ctx.clrCursor : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 24.0f, 42.5f, 0.28f, noFilter ? ctx.clrWhite : Ui::rgba(195, 185, 220), LOC(STR_SONG_ALL), 0.62f);

        // Chip STD
        Ui::panel(47.0f, 34.0f, 32.0f, 17.0f, 0.55f,
            hasStd ? Ui::rgba(75, 25, 60, 240) : Ui::rgba(24, 20, 38, 230),
            hasStd ? Ui::rgba(255, 102, 171) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 63.0f, 42.5f, 0.28f, hasStd ? Ui::rgba(255, 150, 205) : Ui::rgba(195, 185, 220), "STD", 0.62f);

        // Chip TAIKO
        Ui::panel(82.0f, 34.0f, 40.0f, 17.0f, 0.55f,
            hasTaiko ? Ui::rgba(75, 25, 32, 240) : Ui::rgba(24, 20, 38, 230),
            hasTaiko ? Ui::rgba(255, 75, 80) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 102.0f, 42.5f, 0.28f, hasTaiko ? Ui::rgba(255, 140, 150) : Ui::rgba(195, 185, 220), "TAIKO", 0.62f);

        // Chip CATCH
        Ui::panel(125.0f, 34.0f, 40.0f, 17.0f, 0.55f,
            hasCatch ? Ui::rgba(20, 70, 40, 240) : Ui::rgba(24, 20, 38, 230),
            hasCatch ? Ui::rgba(45, 215, 115) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 145.0f, 42.5f, 0.28f, hasCatch ? Ui::rgba(140, 255, 180) : Ui::rgba(195, 185, 220), "CATCH", 0.62f);

        // Chip MANIA
        Ui::panel(168.0f, 34.0f, 40.0f, 17.0f, 0.55f,
            hasMania ? Ui::rgba(20, 60, 85, 240) : Ui::rgba(24, 20, 38, 230),
            hasMania ? Ui::rgba(70, 195, 255) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 188.0f, 42.5f, 0.28f, hasMania ? Ui::rgba(155, 235, 255) : Ui::rgba(195, 185, 220), "MANIA", 0.62f);

        // Chip *4+
        Ui::panel(211.0f, 34.0f, 32.0f, 17.0f, 0.55f,
            hasStar4 ? Ui::rgba(75, 62, 20, 240) : Ui::rgba(24, 20, 38, 230),
            hasStar4 ? Ui::rgba(255, 215, 0) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 227.0f, 42.5f, 0.28f, hasStar4 ? Ui::rgba(255, 235, 95) : Ui::rgba(195, 185, 220), "*4+", 0.62f);

        // Chip *5+
        Ui::panel(246.0f, 34.0f, 32.0f, 17.0f, 0.55f,
            hasStar5 ? Ui::rgba(75, 45, 20, 240) : Ui::rgba(24, 20, 38, 230),
            hasStar5 ? Ui::rgba(255, 140, 50) : Ui::rgba(60, 52, 80));
        drawCenteredTextWithShadow(ctx.textBuf, 262.0f, 42.5f, 0.28f, hasStar5 ? Ui::rgba(255, 180, 105) : Ui::rgba(195, 185, 220), "*5+", 0.62f);

        // Contador de resultados
        char cntBadge[16];
        std::snprintf(cntBadge, sizeof(cntBadge), "%d", static_cast<int>(ctx.songMgr.sets.size()));
        Ui::panel(281.0f, 34.0f, 35.0f, 17.0f, 0.55f, Ui::rgba(30, 24, 48, 240), ctx.clrCursor);
        drawCenteredTextWithShadow(ctx.textBuf, 298.5f, 42.5f, 0.28f, ctx.clrCursor, cntBadge, 0.62f);

        // =========================================================
        // 3. TECLADO QWERTY TÁCTIL (y: 54..238)
        // =========================================================
        auto drawSearchKey = [&](int keyId, float kx, float ky, float kw, float kh, const char* label, float fontScale, u32 customFill = 0, u32 customBorder = 0) {
            bool isFlashed = (ctx.searchKeyFlashTimer > 0 && ctx.searchLastKeyIndex == keyId);
            u32 bg = isFlashed ? Ui::rgba(255, 102, 171, 255)
                   : (customFill != 0 ? customFill : Ui::rgba(32, 26, 48, 255));
            u32 bdr = isFlashed ? ctx.clrWhite
                    : (customBorder != 0 ? customBorder : Ui::rgba(75, 65, 105, 255));
            u32 textClr = isFlashed ? ctx.clrWhite : Ui::rgba(250, 248, 255);

            Ui::panel(kx, ky, kw, kh, 0.60f, bg, bdr);
            C2D_DrawRectSolid(kx + 1.0f, ky + 1.0f, 0.62f, kw - 2.0f, 1.2f, isFlashed ? ctx.clrWhite : Ui::rgba(255, 255, 255, 50));
            drawCenteredTextWithShadow(ctx.textBuf, kx + kw * 0.5f, ky + kh * 0.5f, fontScale, textClr, label, 0.68f);
        };

        // Fila 0: 1 2 3 4 5 6 7 8 9 0 + Borrar (y: 54..86)
        const char* numLabels[] = {"1","2","3","4","5","6","7","8","9","0"};
        for (int i = 0; i < 10; ++i) {
            float kx = 4.0f + i * 26.5f;
            drawSearchKey(i, kx, 54.0f, 24.5f, 32.0f, numLabels[i], 0.44f);
        }
        drawSearchKey(10, 271.0f, 54.0f, 45.0f, 32.0f, LOC(STR_DELETE), 0.32f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 100, 130));

        // Fila 1: Q W E R T Y U I O P (y: 89..121)
        const char* r1Labels[] = {"Q","W","E","R","T","Y","U","I","O","P"};
        for (int i = 0; i < 10; ++i) {
            float kx = 4.0f + i * 31.0f;
            drawSearchKey(11 + i, kx, 89.0f, 29.0f, 32.0f, r1Labels[i], 0.44f);
        }

        // Fila 2: A S D F G H J K L - (y: 124..156)
        const char* r2Labels[] = {"A","S","D","F","G","H","J","K","L","-"};
        for (int i = 0; i < 10; ++i) {
            float kx = 8.0f + i * 30.2f;
            drawSearchKey(21 + i, kx, 124.0f, 28.2f, 32.0f, r2Labels[i], 0.44f);
        }

        // Fila 3: Z X C V B N M . / (y: 159..191)
        const char* r3Labels[] = {"Z","X","C","V","B","N","M",".","/"};
        for (int i = 0; i < 9; ++i) {
            float kx = 16.0f + i * 32.0f;
            drawSearchKey(31 + i, kx, 159.0f, 29.8f, 32.0f, r3Labels[i], 0.44f);
        }

        // Fila 4: Acciones inferiores (y: 194..236)
        drawSearchKey(40, 4.0f, 194.0f, 56.0f, 42.0f, LOC(STR_CLEAR), 0.30f, Ui::rgba(52, 22, 38, 255), Ui::rgba(255, 90, 110));
        drawSearchKey(41, 63.0f, 194.0f, 142.0f, 42.0f, LOC(STR_SPACE), 0.35f, Ui::rgba(36, 30, 56, 255), Ui::rgba(90, 80, 125));
        drawSearchKey(42, 208.0f, 194.0f, 52.0f, 42.0f, LOC(STR_DELETE), 0.30f, Ui::rgba(56, 24, 42, 255), Ui::rgba(255, 120, 140));
        drawSearchKey(43, 263.0f, 194.0f, 53.0f, 42.0f, "OK", 0.36f, Ui::rgba(22, 68, 42, 255), ctx.clr100);
    } else if (ctx.modMenuOpen) {
        Ui::panel(6.0f, 6.0f, 308.0f, 228.0f, 0.10f, Ui::rgba(22, 18, 34, 250), ctx.clrPrimary);

        // Cabecera del menú de Mods
        Ui::pill(12.0f, 10.0f, 44.0f, 15.0f, 0.14f, Ui::rgba(255, 102, 171, 220));
        drawCenteredText(ctx.textBuf, 34.0f, 17.5f, 0.35f, ctx.clrWhite, "MODS", 0.16f);
        drawText(ctx.textBuf, 60.0f, 10.0f, 0.42f, ctx.clrWhite, LOC(STR_MODS_INFO), 0.16f);

        // Multiplicador total de score
        float mult = getModsScoreMultiplier(ctx.settings.activeMods);
        char multStr[32];
        if (ctx.settings.activeMods & (MOD_AUTO | MOD_RX | MOD_AP)) {
            std::snprintf(multStr, sizeof(multStr), "Unranked");
        } else {
            std::snprintf(multStr, sizeof(multStr), "Score: %.2fx", mult);
        }
        u32 multClr = (mult > 1.0f) ? ctx.clr100 : ((mult == 1.0f) ? ctx.clr300 : ctx.clrMiss);
        Ui::pill(222.0f, 9.0f, 88.0f, 16.0f, 0.14f, Ui::rgba(30, 24, 48, 220));
        drawCenteredText(ctx.textBuf, 266.0f, 17.0f, 0.34f, multClr, multStr, 0.16f);

        drawText(ctx.textBuf, 12.0f, 28.0f, 0.28f, ctx.clrMuted, LOC(STR_MODS_CONTROLS_HINT), 0.16f);

        // Cuadrícula 5x3 de los 15 Modificadores
        for (int m = 0; m < 15; ++m) {
            int col = m % 5;
            int row = m / 5;
            float mx = 8.0f + col * 62.0f;
            float my = 43.0f + row * 43.0f;
            bool isDisallowedInMulti = ctx.multiMgr.inMultiplayer && (modTable[m].bit == MOD_AUTO || modTable[m].bit == MOD_RX || modTable[m].bit == MOD_AP);
            bool active = !isDisallowedInMulti && ((ctx.settings.activeMods & modTable[m].bit) != 0);
            bool cursorOn = (ctx.modCursor == m);
            u32 mClr = isDisallowedInMulti ? Ui::rgba(80, 70, 95) : modTable[m].color;

            u32 cardBg = isDisallowedInMulti
                ? Ui::rgba(18, 14, 24, 210)
                : (active
                    ? Ui::lerpColor(Ui::rgba(26, 22, 40, 245), mClr, 0.38f)
                    : (cursorOn ? Ui::rgba(42, 36, 62, 240) : Ui::rgba(28, 24, 42, 230)));

            Ui::panel(mx, my, 56.0f, 39.0f, 0.20f, cardBg, cursorOn ? ctx.clrCursor : (active ? mClr : Ui::rgba(55, 48, 80)));
            if (active) {
                C2D_DrawRectSolid(mx + 2.0f, my, 0.22f, 52.0f, 2.5f, mClr);
            }
            drawRectOutline(mx, my, 56.0f, 39.0f, cursorOn ? 1.8f : 1.0f, cursorOn ? ctx.clrCursor : (active ? mClr : Ui::rgba(55, 48, 80)), 0.24f);
            drawCenteredText(ctx.textBuf, mx + 28.0f, my + 13.0f, 0.48f, isDisallowedInMulti ? ctx.clrMiss : (active ? ctx.clrWhite : Ui::brighten(mClr, 1.25f)), modTable[m].code, 0.25f);
            drawCenteredText(ctx.textBuf, mx + 28.0f, my + 28.0f, 0.29f, isDisallowedInMulti ? ctx.clrMiss : (active ? ctx.clrWhite : ctx.clrMuted), isDisallowedInMulti ? LOC(STR_NOT_AVAIL) : Localization::getIndexed(Localization::STR_MOD_NAME_0, m), 0.25f);
        }

        // Panel inferior con descripción enriquecida del mod seleccionado
        int selM = std::max(0, std::min(14, ctx.modCursor));
        const auto& curDef = modTable[selM];
        bool curActive = (ctx.settings.activeMods & curDef.bit) != 0;

        Ui::panel(8.0f, 174.0f, 304.0f, 56.0f, 0.20f, Ui::rgba(18, 14, 28, 248), curDef.color);
        C2D_DrawRectSolid(8.0f, 174.0f, 0.23f, 304.0f, 2.0f, curDef.color);

        // Badge y nombre del mod seleccionado
        Ui::pill(14.0f, 179.0f, 28.0f, 14.0f, 0.24f, curDef.color);
        drawCenteredText(ctx.textBuf, 28.0f, 186.0f, 0.33f, ctx.clrWhite, curDef.code, 0.26f);
        drawText(ctx.textBuf, 46.0f, 179.0f, 0.40f, ctx.clrWhite, curDef.fullName, 0.26f);

        // Insignia de estado ACTIVO / INACTIVO
        bool isSelDisallowed = ctx.multiMgr.inMultiplayer && (curDef.bit == MOD_AUTO || curDef.bit == MOD_RX || curDef.bit == MOD_AP);
        Ui::pill(242.0f, 179.0f, 62.0f, 14.0f, 0.24f, isSelDisallowed ? Ui::rgba(60, 20, 25, 235) : (curActive ? Ui::rgba(25, 75, 45, 235) : Ui::rgba(35, 30, 50, 235)));
        drawCenteredText(ctx.textBuf, 273.0f, 186.0f, 0.29f, isSelDisallowed ? ctx.clrMiss : (curActive ? ctx.clr100 : ctx.clrMuted), isSelDisallowed ? LOC(STR_NOT_AVAIL) : (curActive ? LOC(STR_ON) : LOC(STR_OFF)), 0.26f);

        // Texto descriptivo del efecto del mod
        drawText(ctx.textBuf, 14.0f, 196.0f, 0.33f, Ui::rgba(220, 215, 240), Localization::getIndexed(Localization::STR_MOD_DESC_0, selM), 0.26f);
        if (isSelDisallowed) {
            drawText(ctx.textBuf, 14.0f, 213.0f, 0.28f, ctx.clrMiss, LOC(STR_MODS_NOT_ONLINE), 0.26f);
        } else {
            drawText(ctx.textBuf, 14.0f, 213.0f, 0.28f, ctx.clr300, LOC(STR_MODS_CONTROLS_HINT), 0.26f);
        }
    } else {
        if (!ctx.songMgr.sets.empty() && !ctx.songMgr.sets[ctx.selectedSet].difficulties.empty()) {
            const auto& curSet = ctx.songMgr.sets[ctx.selectedSet];
            const auto& d = curSet.difficulties[ctx.selectedDiff];

            float dispCS = d.cs, dispAR = d.ar, dispOD = d.od, dispHP = d.hp;
            if (ctx.settings.activeMods & MOD_EZ) {
                dispCS *= 0.5f; dispAR *= 0.5f; dispOD *= 0.5f; dispHP *= 0.5f;
            }
            if (ctx.settings.activeMods & MOD_HR) {
                dispCS = std::min(10.0f, dispCS * 1.3f);
                dispAR = std::min(10.0f, dispAR * 1.4f);
                dispOD = std::min(10.0f, dispOD * 1.4f);
                dispHP = std::min(10.0f, dispHP * 1.4f);
            }

            float modStars = getModAdjustedStars(d.starRating, ctx.settings.activeMods);
            float maxSSPP  = calculatePP(d.starRating, d.ar, d.od, d.maxPossibleCombo, d.maxPossibleCombo,
                                         d.totalObjects(), 0, 0, 0, ctx.settings.activeMods & ~MOD_AUTO);
            int curDiffMode = (ctx.settings.gameMode == MODE_TAIKO || d.mode == 1) ? 1 : (ctx.settings.gameMode == MODE_CATCH || d.mode == 2) ? 2 : (ctx.settings.gameMode == MODE_MANIA || d.mode == 3) ? 3 : 0;
            const auto* rec = ctx.scoreMgr.getRecord(d.osuPath, curDiffMode);

            // =========================================================================
            // 1. HEADER CARD ESTILO OSU!LAZER: TITULO, ARTISTA Y METRICAS (y: 6..54)
            // =========================================================================
            Ui::panel(8.0f, 6.0f, 304.0f, 48.0f, 0.15f, Ui::rgba(20, 16, 32, 245), Ui::rgba(50, 42, 75));

            // Badge [RANKED] estilo osu!lazer + Título
            Ui::pill(14.0f, 9.0f, 48.0f, 13.0f, 0.22f, Ui::rgba(25, 95, 45, 230));
            drawCenteredText(ctx.textBuf, 38.0f, 15.5f, 0.29f, Ui::rgba(110, 235, 110), "RANKED");
            drawText(ctx.textBuf, 66.0f, 8.5f, 0.42f, ctx.clrWhite, truncateString(curSet.title, 22).c_str(), 0.30f);

            // Botón pequeño [ BORRAR ] en el header (x: 250, y: 8.5, w: 56, h: 14)
            Ui::pill(250.0f, 8.5f, 56.0f, 14.0f, 0.22f, Ui::rgba(140, 25, 38, 230));
            drawRectOutline(250.0f, 8.5f, 56.0f, 14.0f, 1.0f, Ui::rgba(255, 75, 95), 0.24f);
            drawCenteredText(ctx.textBuf, 278.0f, 15.5f, 0.28f, Ui::rgba(255, 220, 225), LOC(STR_DELETE), 0.32f);

            // Artista y Creador
            std::string artistCreator = curSet.artist + "  //  " + d.creator;
            drawText(ctx.textBuf, 14.0f, 23.5f, 0.32f, Ui::rgba(185, 180, 210), truncateString(artistCreator, 36).c_str(), 0.30f);

            // Rating de estrellas estilo lazer + Dificultad + Specs inline
            u32 starClr = getStarRatingColor(modStars);
            char starBadge[24];
            std::snprintf(starBadge, sizeof(starBadge), "* %.2f", modStars);
            Ui::pill(14.0f, 37.0f, 46.0f, 14.0f, 0.22f, Ui::withAlpha(starClr, 60));
            drawCenteredText(ctx.textBuf, 37.0f, 44.0f, 0.32f, starClr, starBadge);

            drawText(ctx.textBuf, 64.0f, 37.5f, 0.36f, starClr, truncateString(d.version, 13).c_str(), 0.30f);

            char specsStr[64];
            std::snprintf(specsStr, sizeof(specsStr), "CS %.1f  AR %.1f  OD %.1f  HP %.1f", dispCS, dispAR, dispOD, dispHP);
            Ui::pill(152.0f, 36.0f, 154.0f, 16.0f, 0.22f, Ui::rgba(28, 22, 44, 235));
            drawCenteredTextWithShadow(ctx.textBuf, 229.0f, 44.0f, 0.32f, Ui::rgba(235, 230, 255), specsStr, 0.30f);

            // =========================================================================
            // 2. PESTAÑAS: RESUMEN / RANKING LOCAL (y: 56..70)
            // =========================================================================
            static std::vector<ScoreRecord> cachedMapRecords;
            static std::string cachedMapOsuPath = "";
            static int cachedMapMode = -999;
            if (d.osuPath != cachedMapOsuPath || curDiffMode != cachedMapMode) {
                cachedMapOsuPath = d.osuPath;
                cachedMapMode = curDiffMode;
                cachedMapRecords = ctx.scoreMgr.getTopRecords(d.osuPath, 10, curDiffMode);
            }
            const std::vector<ScoreRecord>& mapRecords = cachedMapRecords;
            int numRecs = static_cast<int>(mapRecords.size());
            if (ctx.rankingScroll >= numRecs && numRecs > 0) ctx.rankingScroll = numRecs - 1;
            if (ctx.rankingScroll < 0) ctx.rankingScroll = 0;

            // Tab 0: [ 📊 RESUMEN ] (x: 8..118)
            bool tab0Active = (ctx.songSelectTab == 0);
            u32 tab0Bg = tab0Active ? Ui::rgba(55, 30, 75, 245) : Ui::rgba(22, 18, 32, 220);
            u32 tab0Border = tab0Active ? ctx.clrPrimary : Ui::rgba(45, 38, 62);
            Ui::panel(8.0f, 56.0f, 110.0f, 15.0f, 0.22f, tab0Bg, tab0Border);
            drawCenteredTextWithShadow(ctx.textBuf, 63.0f, 62.5f, 0.30f, tab0Active ? ctx.clrWhite : Ui::rgba(210, 205, 230), LOC(STR_SUMMARY));

            // Tab 1: [ 🏆 RANKING LOCAL (N) ] (x: 122..244)
            bool tab1Active = (ctx.songSelectTab == 1);
            u32 tab1Bg = tab1Active ? Ui::rgba(65, 35, 80, 245) : Ui::rgba(22, 18, 32, 220);
            u32 tab1Border = tab1Active ? ctx.clrCursor : Ui::rgba(45, 38, 62);
            Ui::panel(122.0f, 56.0f, 122.0f, 15.0f, 0.22f, tab1Bg, tab1Border);
            char rnkTabStr[32];
            std::snprintf(rnkTabStr, sizeof(rnkTabStr), "%s (%d)", LOC(STR_LOCAL_RANKING), numRecs);
            drawCenteredTextWithShadow(ctx.textBuf, 183.0f, 62.5f, 0.30f, tab1Active ? ctx.clrCursor : Ui::rgba(210, 205, 230), rnkTabStr);

            // Botón de acción lateral (x: 248..312)
            if (ctx.songSelectTab == 1) {
                u32 btnBg = ctx.rankingExpanded ? Ui::rgba(180, 40, 60, 230) : Ui::rgba(45, 75, 135, 230);
                Ui::pill(248.0f, 56.0f, 64.0f, 15.0f, 0.23f, btnBg);
                drawCenteredTextWithShadow(ctx.textBuf, 280.0f, 62.5f, 0.28f, ctx.clrWhite, ctx.rankingExpanded ? LOC(STR_COLLAPSE) : LOC(STR_EXPAND));
            } else {
                Ui::pill(248.0f, 56.0f, 64.0f, 15.0f, 0.22f, Ui::rgba(45, 32, 60, 220));
                drawCenteredTextWithShadow(ctx.textBuf, 280.0f, 62.5f, 0.28f, Ui::rgba(255, 175, 215), LOC(STR_SONG_VIEW_TOP));
            }

            if (ctx.songSelectTab == 0) {
                // =====================================================================
                // 2A. PERSONAL BEST CARD ESTILO OSU!LAZER (y: 74..112)
                // =====================================================================
                Ui::gradH(8.0f, 74.0f, 304.0f, 38.0f, 0.20f, Ui::rgba(46, 22, 60, 235), Ui::rgba(24, 18, 38, 245));
                drawRectOutline(8.0f, 74.0f, 304.0f, 38.0f, 1.0f, Ui::rgba(255, 102, 171, 95), 0.25f);

                // Chip "MEJOR RECORD"
                Ui::pill(14.0f, 77.0f, 68.0f, 12.0f, 0.25f, Ui::rgba(75, 32, 95, 235));
                drawCenteredTextWithShadow(ctx.textBuf, 48.0f, 83.0f, 0.28f, Ui::rgba(255, 140, 205), LOC(STR_SONG_BEST_RECORD));

                if (rec) {
                    // Puntuación numérica con formato
                    drawTextWithShadow(ctx.textBuf, 86.0f, 75.0f, 0.44f, ctx.clrWhite, formatScoreThousands(rec->bestScore).c_str(), 0.30f);

                    // Precisión y combo máximo
                    char accCombo[64];
                    std::snprintf(accCombo, sizeof(accCombo), "ACC: %.1f%%  COMBO: %dx", rec->bestAcc, rec->bestCombo);
                    drawTextWithShadow(ctx.textBuf, 14.0f, 95.0f, 0.30f, Ui::rgba(110, 235, 180), accCombo, 0.30f);

                    // Pill de PP ganado
                    char ppStr[32];
                    std::snprintf(ppStr, sizeof(ppStr), "%.1f pp", rec->bestPP);
                    Ui::pill(152.0f, 93.0f, 48.0f, 14.0f, 0.25f, Ui::rgba(45, 38, 80, 220));
                    drawCenteredTextWithShadow(ctx.textBuf, 176.0f, 99.5f, 0.29f, Ui::rgba(215, 205, 255), ppStr);

                    // Pill de Mods jugados (si los hubo)
                    if (rec->bestMods != MOD_NONE) {
                        std::string mStr = "+" + formatModsString(rec->bestMods);
                        Ui::pill(204.0f, 93.0f, 40.0f, 14.0f, 0.25f, Ui::rgba(85, 40, 105, 220));
                        drawCenteredTextWithShadow(ctx.textBuf, 224.0f, 99.5f, 0.28f, ctx.clrCursor, mStr.c_str());
                    }

                    // Insignia de Rango estilo Lazer (SS, S, A, etc.)
                    u32 gClr = getScoreGradeColor(rec->bestGrade, ctx.clrCursor);
                    Ui::panel(248.0f, 76.0f, 58.0f, 33.0f, 0.25f, Ui::rgba(35, 18, 48, 235), gClr);
                    drawCenteredTextWithShadow(ctx.textBuf, 277.0f, 89.0f, 0.46f, gClr, rec->bestGrade.c_str(), 0.32f);
                    drawCenteredTextWithShadow(ctx.textBuf, 277.0f, 102.0f, 0.22f, Ui::rgba(255, 180, 225), "GRADE");
                } else {
                    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 85.0f, 0.38f, ctx.clrWhite, LOC(STR_SONG_NO_RECORDS_YET));
                    char ssMaxStr[64];
                    std::snprintf(ssMaxStr, sizeof(ssMaxStr), "Max SS: %.0f pp  •  %d %s  •  %s", maxSSPP, d.totalObjects(), LOC(STR_OBJECTS), LOC(STR_SONG_PLAY_NOW));
                    drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 99.0f, 0.30f, Ui::rgba(255, 225, 95), ssMaxStr);
                }

                // =====================================================================
                // 2B. CURVA DE ESFUERZO / STRAIN GRAPH ESTILO OSU!LAZER (y: 114..142)
                // =====================================================================
                Ui::panel(8.0f, 114.0f, 304.0f, 28.0f, 0.18f, Ui::rgba(20, 16, 32, 240), starClr);
                drawTextWithShadow(ctx.textBuf, 14.0f, 115.5f, 0.29f, ctx.clrWhite, LOC(STR_SONG_DIFF_DENSITY), 0.28f);
                char objCntStr[32];
                std::snprintf(objCntStr, sizeof(objCntStr), "%d %s", d.totalObjects(), LOC(STR_OBJECTS));
                drawTextWithShadow(ctx.textBuf, 232.0f, 115.5f, 0.29f, Ui::rgba(225, 220, 245), objCntStr, 0.28f);

                float playhead = -1.0f;
                if (ctx.audio.getIsPlaying() && d.lastObjectTime > d.firstObjectTime) {
                    int curMs = ctx.audio.getTimeMs();
                    if (curMs >= d.firstObjectTime && curMs <= d.lastObjectTime) {
                        playhead = static_cast<float>(curMs - d.firstObjectTime) / static_cast<float>(d.lastObjectTime - d.firstObjectTime);
                    }
                }
                Ui::strainGraph(12.0f, 122.5f, 296.0f, 18.0f, 0.22f, d.strainGraph, starClr, playhead);
            } else {
                // =====================================================================
                // 2C. RANKING LOCAL: TABLA DE MEJORES PUNTUACIONES
                // =====================================================================
                if (numRecs == 0) {
                    float emptyH = ctx.rankingExpanded ? 150.0f : 68.0f;
                    Ui::panel(8.0f, 74.0f, 304.0f, emptyH, 0.20f, Ui::rgba(24, 18, 38, 240), Ui::rgba(65, 40, 85));
                    drawCenteredText(ctx.textBuf, 160.0f, 74.0f + emptyH * 0.40f, 0.38f, ctx.clrWhite, LOC(STR_SONG_NO_RECORDS_YET));
                    drawCenteredText(ctx.textBuf, 160.0f, 74.0f + emptyH * 0.65f, 0.28f, ctx.clrCursor, LOC(STR_SONG_PLAY_TO_RECORD));
                } else if (!ctx.rankingExpanded) {
                    // Modo compacto: 3 filas en y: 74..142
                    int displayCount = std::min(numRecs, 3);
                    for (int i = 0; i < displayCount; ++i) {
                        const auto& r = mapRecords[i];
                        float ry = 74.0f + i * 23.0f;
                        u32 gClr = getScoreGradeColor(r.bestGrade, ctx.clrCursor);

                        u32 rankClr = (i == 0) ? Ui::rgba(255, 215, 0)
                                    : ((i == 1) ? Ui::rgba(210, 220, 235)
                                    : ((i == 2) ? Ui::rgba(220, 145, 75)
                                                : Ui::rgba(135, 125, 155)));

                        Ui::gradH(8.0f, ry, 304.0f, 21.0f, 0.20f, Ui::rgba(28, 20, 42, 245), Ui::rgba(18, 14, 28, 245));
                        drawRectOutline(8.0f, ry, 304.0f, 21.0f, 1.0f, (i == 0) ? Ui::withAlpha(rankClr, 140) : Ui::rgba(45, 38, 65, 180), 0.22f);
                        C2D_DrawRectSolid(8.0f, ry, 0.24f, 2.5f, 21.0f, rankClr);

                        // 1. Rango
                        char rnkStr[8];
                        std::snprintf(rnkStr, sizeof(rnkStr), "#%d", i + 1);
                        Ui::pill(12.0f, ry + 3.0f, 22.0f, 15.0f, 0.23f, (i == 0) ? rankClr : Ui::rgba(35, 28, 52, 220));
                        drawCenteredText(ctx.textBuf, 23.0f, ry + 10.5f, 0.28f, (i == 0) ? Ui::rgba(20, 16, 28) : rankClr, rnkStr);

                        // 2. Grado
                        Ui::pill(36.0f, ry + 3.0f, 22.0f, 15.0f, 0.23f, Ui::rgba(24, 18, 36, 230));
                        drawRectOutline(36.0f, ry + 3.0f, 22.0f, 15.0f, 1.0f, gClr, 0.24f);
                        drawCenteredText(ctx.textBuf, 47.0f, ry + 10.5f, 0.30f, gClr, r.bestGrade.c_str());

                        // 3. Score
                        drawText(ctx.textBuf, 62.0f, ry + 3.5f, 0.38f, ctx.clrWhite, formatScoreThousands(r.bestScore).c_str(), 0.26f);

                        // 4. Acc & Combo
                        char acBuf[32];
                        std::snprintf(acBuf, sizeof(acBuf), "%.1f%% %dx", r.bestAcc, r.bestCombo);
                        drawText(ctx.textBuf, 140.0f, ry + 4.5f, 0.27f, Ui::rgba(130, 240, 190), acBuf, 0.26f);

                        // 5. Mods
                        if (r.bestMods != MOD_NONE) {
                            std::string mStr = "+" + formatModsString(r.bestMods);
                            Ui::pill(210.0f, ry + 3.0f, 48.0f, 15.0f, 0.23f, Ui::rgba(75, 30, 95, 220));
                            drawCenteredText(ctx.textBuf, 234.0f, ry + 10.5f, 0.27f, ctx.clrCursor, mStr.c_str());
                        } else {
                            Ui::pill(210.0f, ry + 3.0f, 48.0f, 15.0f, 0.23f, Ui::rgba(34, 28, 48, 200));
                            drawCenteredText(ctx.textBuf, 234.0f, ry + 10.5f, 0.26f, ctx.clrMuted, "None");
                        }

                        // 6. PP
                        Ui::pill(262.0f, ry + 3.0f, 46.0f, 15.0f, 0.23f, Ui::rgba(42, 34, 75, 220));
                        char ppBuf[16];
                        std::snprintf(ppBuf, sizeof(ppBuf), "%.0f pp", r.bestPP);
                        drawCenteredText(ctx.textBuf, 285.0f, ry + 10.5f, 0.28f, Ui::rgba(205, 190, 255), ppBuf);
                    }
                } else {
                    // Modo expandido: hasta 6 filas en y: 74..224
                    int maxVisible = 6;
                    int startIdx = std::max(0, std::min(ctx.rankingScroll, std::max(0, numRecs - maxVisible)));
                    int endIdx = std::min(numRecs, startIdx + maxVisible);

                    for (int idx = startIdx; idx < endIdx; ++idx) {
                        const auto& r = mapRecords[idx];
                        float ry = 74.0f + (idx - startIdx) * 25.0f;
                        u32 gClr = getScoreGradeColor(r.bestGrade, ctx.clrCursor);

                        u32 rankClr = (idx == 0) ? Ui::rgba(255, 215, 0)
                                    : ((idx == 1) ? Ui::rgba(210, 220, 235)
                                    : ((idx == 2) ? Ui::rgba(220, 145, 75)
                                                  : Ui::rgba(135, 125, 155)));

                        Ui::gradH(8.0f, ry, 304.0f, 23.0f, 0.20f, Ui::rgba(28, 20, 42, 245), Ui::rgba(18, 14, 28, 245));
                        drawRectOutline(8.0f, ry, 304.0f, 23.0f, 1.0f, (idx == 0) ? Ui::withAlpha(rankClr, 150) : Ui::rgba(45, 38, 65, 180), 0.22f);
                        C2D_DrawRectSolid(8.0f, ry, 0.24f, 3.0f, 23.0f, rankClr);

                        // 1. Rango
                        char rnkStr[8];
                        std::snprintf(rnkStr, sizeof(rnkStr), "#%d", idx + 1);
                        Ui::pill(12.0f, ry + 3.5f, 24.0f, 16.0f, 0.23f, (idx == 0) ? rankClr : Ui::rgba(35, 28, 52, 220));
                        drawCenteredText(ctx.textBuf, 24.0f, ry + 11.5f, 0.30f, (idx == 0) ? Ui::rgba(20, 16, 28) : rankClr, rnkStr);

                        // 2. Grado
                        Ui::pill(38.0f, ry + 3.5f, 24.0f, 16.0f, 0.23f, Ui::rgba(24, 18, 36, 230));
                        drawRectOutline(38.0f, ry + 3.5f, 24.0f, 16.0f, 1.0f, gClr, 0.24f);
                        drawCenteredText(ctx.textBuf, 50.0f, ry + 11.5f, 0.32f, gClr, r.bestGrade.c_str());

                        // 3. Score
                        drawText(ctx.textBuf, 66.0f, ry + 4.5f, 0.40f, ctx.clrWhite, formatScoreThousands(r.bestScore).c_str(), 0.26f);

                        // 4. Acc & Combo
                        char acBuf[32];
                        std::snprintf(acBuf, sizeof(acBuf), "%.1f%%  %dx", r.bestAcc, r.bestCombo);
                        drawText(ctx.textBuf, 144.0f, ry + 5.5f, 0.28f, Ui::rgba(130, 240, 190), acBuf, 0.26f);

                        // 5. Mods
                        if (r.bestMods != MOD_NONE) {
                            std::string mStr = "+" + formatModsString(r.bestMods);
                            Ui::pill(210.0f, ry + 3.5f, 48.0f, 16.0f, 0.23f, Ui::rgba(75, 30, 95, 220));
                            drawCenteredText(ctx.textBuf, 234.0f, ry + 11.5f, 0.28f, ctx.clrCursor, mStr.c_str());
                        } else {
                            Ui::pill(210.0f, ry + 3.5f, 48.0f, 16.0f, 0.23f, Ui::rgba(34, 28, 48, 200));
                            drawCenteredText(ctx.textBuf, 234.0f, ry + 11.5f, 0.27f, ctx.clrMuted, "None");
                        }

                        // 6. PP
                        Ui::pill(262.0f, ry + 3.5f, 46.0f, 16.0f, 0.23f, Ui::rgba(42, 34, 75, 220));
                        char ppBuf[16];
                        std::snprintf(ppBuf, sizeof(ppBuf), "%.0f pp", r.bestPP);
                        drawCenteredText(ctx.textBuf, 285.0f, ry + 11.5f, 0.29f, Ui::rgba(205, 190, 255), ppBuf);
                    }

                    // Barra de estado inferior en modo expandido
                    Ui::gradV(8.0f, 224.0f, 304.0f, 15.0f, 0.25f, Ui::rgba(18, 14, 28, 220), Ui::rgba(18, 14, 28, 250));
                    drawText(ctx.textBuf, 12.0f, 225.5f, 0.26f, ctx.clrMuted, LOC(STR_SONG_CLOSE_HINT), 0.30f);
                    if (startIdx > 0) {
                        drawText(ctx.textBuf, 276.0f, 225.5f, 0.28f, ctx.clrCursor, "[^]", 0.30f);
                    }
                    if (endIdx < numRecs) {
                        drawText(ctx.textBuf, 294.0f, 225.5f, 0.28f, ctx.clrCursor, "[v]", 0.30f);
                    }
                }
            }
        } else {
            Ui::panel(8.0f, 6.0f, 304.0f, 136.0f, 0.15f, Ui::rgba(20, 16, 32, 245), Ui::rgba(50, 42, 75));
            drawCenteredText(ctx.textBuf, 160.0f, 55.0f, 0.44f, ctx.clrCursor, LOC(STR_SONG_NO_FILTER_MATCH));
            drawCenteredText(ctx.textBuf, 160.0f, 80.0f, 0.36f, ctx.clrWhite, LOC(STR_SONG_TAP_SEARCH_CLEAR));
        }

        // =========================================================================
        // 4. BARRA HORIZONTAL DE TARJETAS ESTILO OSU!LAZER (y: 146..222)
        // =========================================================================
        if (!ctx.rankingExpanded) {
            const float cardW = 56.0f;
            const float cardH = 76.0f;
            const float baseCardY = 146.0f;

        for (int k = 0; k < 5; ++k) {
            float lift = ctx.footerCardLift[k];
            float bx = 8.0f + k * 62.0f;
            float by = baseCardY - (lift * 9.0f);
            float cx = bx + (cardW * 0.5f);
            float cy = by + 23.0f; // Centro del icono grande
            float cz = 0.20f;

            // Colores temáticos y acentos por tarjeta
            u32 accentClr = ctx.clrWhite;
            u32 bgBase    = Ui::rgba(22, 18, 34, 245);
            const char* cardTitle = "";
            char cardSub[32] = "";
            u32 subClr = ctx.clrMuted;

            if (k == 0) { // MODO TACTIL / TABLET (TOGGLE UNIFICADO)
                if (ctx.multiMgr.inMultiplayer) {
                    accentClr = ctx.clrCursor;
                    bgBase    = Ui::rgba(22, 45, 65, 245);
                    cardTitle = LOC(STR_CARD_ROOM);
                    std::snprintf(cardSub, sizeof(cardSub), "%s", ctx.multiMgr.isHost() ? LOC(STR_CARD_HOST) : LOC(STR_CARD_GUEST));
                    subClr = ctx.clrCursor;
                } else {
                    bool bTactil = ctx.settings.playOnBottomScreen;
                    accentClr = bTactil ? ctx.clr100 : ctx.clrPrimary;
                    bgBase    = bTactil ? Ui::rgba(24, 52, 40, 245) : Ui::rgba(36, 26, 64, 245);
                    cardTitle = bTactil ? LOC(STR_CARD_TOUCH) : LOC(STR_CARD_TABLET);
                    std::snprintf(cardSub, sizeof(cardSub), "%s", bTactil ? LOC(STR_CARD_DOWN) : LOC(STR_CARD_UP));
                    subClr = bTactil ? Ui::rgba(140, 245, 140) : Ui::rgba(195, 175, 255);
                }
            } else if (k == 1) { // BUSCAR
                bool hasFilter = !ctx.songMgr.searchQuery.empty();
                accentClr = hasFilter ? ctx.clrCursor : Ui::rgba(0, 216, 240);
                bgBase    = hasFilter ? Ui::rgba(22, 45, 65, 245) : Ui::rgba(18, 26, 42, 245);
                cardTitle = LOC(STR_SEARCH);
                if (hasFilter) {
                    std::snprintf(cardSub, sizeof(cardSub), "%s", truncateString(ctx.songMgr.searchQuery, 7).c_str());
                    subClr = ctx.clrCursor;
                } else {
                    std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_FILTER));
                    subClr = ctx.clrMuted;
                }
            } else if (k == 2) { // MODS
                bool hasMods = (ctx.settings.activeMods != MOD_NONE);
                accentClr = hasMods ? Ui::rgba(255, 102, 171) : Ui::rgba(240, 110, 185);
                bgBase    = (hasMods || ctx.modMenuOpen) ? Ui::rgba(54, 20, 50, 245) : Ui::rgba(26, 18, 36, 245);
                cardTitle = "MODS";
                if (hasMods) {
                    std::string mStr = "+" + formatModsString(ctx.settings.activeMods);
                    std::snprintf(cardSub, sizeof(cardSub), "%s", mStr.c_str());
                    subClr = ctx.clrCursor;
                } else {
                    std::snprintf(cardSub, sizeof(cardSub), "SELECT");
                    subClr = ctx.clrMuted;
                }
            } else if (k == 3) { // ORDEN
                accentClr = Ui::rgba(165, 120, 255);
                bgBase    = Ui::rgba(26, 22, 46, 245);
                cardTitle = LOC(STR_CARD_ORDER);
                if (ctx.songMgr.sortMode == SORT_BY_TITLE)       std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_TITLE));
                else if (ctx.songMgr.sortMode == SORT_BY_ARTIST) std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_ARTIST));
                else if (ctx.songMgr.sortMode == SORT_BY_STARS)  std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_STARS));
                else                                         std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_RECORD));
                subClr = Ui::rgba(205, 185, 255);
            } else if (k == 4) { // MODOS DE JUEGO (STD / TAIKO / CATCH / MANIA)
                if (ctx.multiMgr.inMultiplayer) {
                    accentClr = Ui::rgba(180, 80, 240);
                    bgBase    = Ui::rgba(40, 20, 52, 245);
                    cardTitle = "ONLINE";
                    std::snprintf(cardSub, sizeof(cardSub), "Lobby");
                    subClr    = Ui::rgba(215, 160, 255);
                } else {
                    cardTitle = LOC(STR_MODE);
                    int curM = ctx.settings.gameMode;
                    if (curM == MODE_TAIKO) {
                        accentClr = Ui::rgba(255, 75, 80);
                        bgBase    = Ui::rgba(44, 18, 24, 245);
                        std::snprintf(cardSub, sizeof(cardSub), "Taiko");
                        subClr    = Ui::rgba(255, 150, 160);
                    } else if (curM == MODE_CATCH) {
                        accentClr = Ui::rgba(45, 215, 115);
                        bgBase    = Ui::rgba(16, 42, 26, 245);
                        std::snprintf(cardSub, sizeof(cardSub), "Catch");
                        subClr    = Ui::rgba(130, 255, 180);
                    } else if (curM == MODE_MANIA) {
                        accentClr = Ui::rgba(70, 195, 255);
                        bgBase    = Ui::rgba(18, 36, 52, 245);
                        std::snprintf(cardSub, sizeof(cardSub), "Mania");
                        subClr    = Ui::rgba(160, 230, 255);
                    } else {
                        accentClr = Ui::rgba(255, 102, 171);
                        bgBase    = Ui::rgba(48, 18, 38, 245);
                        std::snprintf(cardSub, sizeof(cardSub), "%s", LOC(STR_CARD_STANDARD));
                        subClr    = Ui::rgba(255, 180, 215);
                    }
                }
            }

            // Sombra inferior cuando la tarjeta se eleva
            if (lift > 0.02f) {
                C2D_DrawRectSolid(bx + 2.0f, by + cardH, 0.17f, cardW - 4.0f, 2.0f + lift * 5.0f, Ui::rgba(0, 0, 0, static_cast<u8>(80 * lift)));
                Ui::glow(cx, by + (cardH * 0.5f), 0.18f, 32.0f, Ui::withAlpha(accentClr, static_cast<u8>(90 * lift)), 3);
            }

            // Fondo de la tarjeta con estilo button multijugador
            u32 finalBg = (lift > 0.05f) ? Ui::brighten(bgBase, 1.0f + lift * 0.35f) : bgBase;
            Ui::button(bx, by, cardW, cardH, cz, finalBg, accentClr, lift > 0.05f);

            // =====================================================================
            // DIBUJADO DE ICONOS VECTORIALES PROCEDURALES (GRANDES Y CENTRADOS)
            // =====================================================================
            float iz = cz + 0.03f;
            u32 iconClr = (lift > 0.05f) ? ctx.clrWhite : accentClr;

            if (k == 0) {
                // Icono 0: Modo Táctil / Tablet (Consola 3DS con pantalla activa y stylus)
                bool bTactil = ctx.settings.playOnBottomScreen;
                if (bTactil) {
                    // Pantalla Superior (contorno sutil)
                    drawRectOutline(cx - 13.0f, cy - 13.0f, 26.0f, 11.0f, 1.2f, Ui::rgba(110, 105, 135, 180), iz);
                    // Pantalla Inferior (activa, brillante, táctil)
                    Ui::panel(cx - 11.0f, cy + 1.0f, 22.0f, 11.0f, iz, Ui::rgba(25, 95, 55, 245), ctx.clr100);
                    // Stylus tocando la pantalla táctil
                    C2D_DrawLine(cx + 14.0f, cy - 4.0f, ctx.clrWhite, cx + 3.0f, cy + 6.0f, ctx.clr100, 2.2f, iz + 0.01f);
                    C2D_DrawCircleSolid(cx + 3.0f, cy + 6.0f, iz + 0.02f, 1.8f, ctx.clrWhite);
                } else {
                    // Pantalla Superior (activa, brillante, juego en 3D)
                    Ui::panel(cx - 13.0f, cy - 13.0f, 26.0f, 12.0f, iz, Ui::rgba(65, 45, 120, 245), ctx.clrPrimary);
                    // Pantalla Inferior (touchpad / tablet)
                    drawRectOutline(cx - 11.0f, cy + 2.0f, 22.0f, 10.0f, 1.2f, Ui::rgba(110, 105, 135, 180), iz);
                    // Stylus apuntando a la pantalla superior
                    C2D_DrawLine(cx - 4.0f, cy + 9.0f, ctx.clrWhite, cx + 5.0f, cy - 6.0f, ctx.clrPrimary, 2.2f, iz + 0.01f);
                    C2D_DrawCircleSolid(cx + 5.0f, cy - 6.0f, iz + 0.02f, 1.8f, ctx.clrWhite);
                }
            } else if (k == 1) {
                // Icono 1: Buscar (Lupa estilizada grande)
                float lcx = cx - 3.5f;
                float lcy = cy - 3.5f;
                float lRadius = 8.5f;
                // Relleno de cristal
                C2D_DrawCircleSolid(lcx, lcy, iz, lRadius, Ui::rgba(0, 180, 230, 45));
                // Borde metálico de la lente
                drawRing(lcx, lcy, lRadius, 2.2f, iconClr, 22, iz + 0.01f);
                // Brillo especular
                C2D_DrawCircleSolid(lcx - 3.5f, lcy - 3.5f, iz + 0.02f, 1.8f, ctx.clrWhite);
                // Mango diagonal de la lupa
                C2D_DrawLine(lcx + 6.0f, lcy + 6.0f, iconClr, lcx + 14.0f, lcy + 14.0f, iconClr, 3.2f, iz + 0.01f);
                C2D_DrawCircleSolid(lcx + 14.0f, lcy + 14.0f, iz + 0.02f, 2.0f, iconClr);
            } else if (k == 2) {
                // Icono 2: Mods (Flechas dinámicas de intercambio estilo osu! Lazer)
                // Flecha superior hacia la derecha
                float aY1 = cy - 5.5f;
                C2D_DrawLine(cx - 12.0f, aY1, iconClr, cx + 5.0f, aY1, iconClr, 2.4f, iz);
                C2D_DrawTriangle(cx + 5.0f, aY1 - 5.0f, iconClr, cx + 12.0f, aY1, iconClr, cx + 5.0f, aY1 + 5.0f, iconClr, iz + 0.01f);
                // Flecha inferior hacia la izquierda
                float aY2 = cy + 5.5f;
                C2D_DrawLine(cx + 12.0f, aY2, iconClr, cx - 5.0f, aY2, iconClr, 2.4f, iz);
                C2D_DrawTriangle(cx - 5.0f, aY2 - 5.0f, iconClr, cx - 12.0f, aY2, iconClr, cx - 5.0f, aY2 + 5.0f, iconClr, iz + 0.01f);
            } else if (k == 3) {
                // Icono 3: Orden (Barras ascendentes de estadísticas + Flecha)
                Ui::panel(cx - 12.0f, cy + 2.0f, 6.0f, 10.0f, iz, iconClr);
                Ui::panel(cx - 3.0f,  cy - 4.0f, 6.0f, 16.0f, iz, iconClr);
                Ui::panel(cx + 6.0f,  cy - 10.0f, 6.0f, 22.0f, iz, iconClr);
                // Triángulo apuntando hacia arriba
                C2D_DrawTriangle(cx + 5.0f, cy - 12.0f, ctx.clrWhite, cx + 13.0f, cy - 12.0f, ctx.clrWhite, cx + 9.0f, cy - 17.0f, ctx.clrWhite, iz + 0.01f);
            } else if (k == 4) {
                // Icono 4: Modos de juego con icono dinámico según el modo activo
                int curM = ctx.settings.gameMode;
                if (curM == MODE_TAIKO) {
                    // Icono TAIKO: Tambor Wadaiko estilizado con cara roja, aro azul y bachi
                    drawRing(cx, cy, 11.5f, 2.4f, Ui::rgba(55, 140, 255), 24, iz); // Aro Katsu (azul)
                    C2D_DrawCircleSolid(cx, cy, iz + 0.01f, 8.5f, Ui::rgba(255, 60, 65)); // Centro Don (rojo)
                    drawRing(cx, cy, 8.5f, 1.4f, ctx.clrWhite, 20, iz + 0.015f);
                    // Palillos de tambor (bachi) cruzados sutiles
                    C2D_DrawLine(cx - 7.0f, cy + 8.5f, ctx.clrWhite, cx + 2.5f, cy - 2.5f, ctx.clrWhite, 2.0f, iz + 0.02f);
                    C2D_DrawLine(cx + 7.0f, cy + 8.5f, ctx.clrWhite, cx - 2.5f, cy - 2.5f, ctx.clrWhite, 2.0f, iz + 0.02f);
                } else if (curM == MODE_CATCH) {
                    // Icono CATCH: Bandeja receptora y fruta jugosa con hoja
                    // Bandeja de Catcher
                    C2D_DrawLine(cx - 11.0f, cy + 9.0f, ctx.clrWhite, cx + 11.0f, cy + 9.0f, ctx.clrWhite, 2.4f, iz);
                    C2D_DrawLine(cx - 11.0f, cy + 9.0f, ctx.clrWhite, cx - 13.0f, cy + 5.5f, ctx.clrWhite, 2.0f, iz);
                    C2D_DrawLine(cx + 11.0f, cy + 9.0f, ctx.clrWhite, cx + 13.0f, cy + 5.5f, ctx.clrWhite, 2.0f, iz);
                    // Manzana estilizada
                    C2D_DrawCircleSolid(cx - 3.8f, cy - 0.5f, iz + 0.01f, 6.2f, Ui::rgba(255, 50, 70));
                    C2D_DrawCircleSolid(cx + 3.8f, cy - 0.5f, iz + 0.01f, 6.2f, Ui::rgba(255, 50, 70));
                    C2D_DrawCircleSolid(cx - 4.0f, cy - 3.0f, iz + 0.02f, 1.8f, Ui::rgba(255, 200, 200, 230)); // Brillo
                    // Tallo y hoja verde
                    C2D_DrawLine(cx, cy - 5.5f, Ui::rgba(140, 80, 45), cx + 2.0f, cy - 10.5f, Ui::rgba(140, 80, 45), 1.8f, iz + 0.02f);
                    C2D_DrawTriangle(cx + 2.0f, cy - 8.5f, Ui::rgba(45, 215, 80), cx + 7.5f, cy - 11.5f, Ui::rgba(45, 215, 80), cx + 6.0f, cy - 7.0f, Ui::rgba(45, 215, 80), iz + 0.03f);
                } else if (curM == MODE_MANIA) {
                    // Icono MANIA: Teclado de 4 columnas (4K) estilizado
                    float colW = 4.2f;
                    float colH = 20.0f;
                    float startX = cx - 10.5f;
                    float topY = cy - 10.0f;
                    for (int col = 0; col < 4; ++col) {
                        float kx = startX + col * 5.5f;
                        u32 kColor = (col == 0 || col == 3) ? ctx.clrWhite : Ui::rgba(60, 205, 255);
                        Ui::panel(kx, topY, colW, colH, iz, Ui::rgba(20, 26, 40, 240), kColor);
                        // Destello de pulsación inferior
                        C2D_DrawRectSolid(kx + 0.5f, topY + colH - 4.5f, iz + 0.01f, colW - 1.0f, 4.0f, kColor);
                    }
                } else {
                    // Icono STD: Círculo de golpe concéntrico con approach circle
                    drawRing(cx, cy, 12.0f, 1.6f, iconClr, 24, iz); // Approach circle
                    C2D_DrawCircleSolid(cx, cy, iz + 0.01f, 8.0f, Ui::rgba(255, 102, 171, 230)); // Hit circle
                    drawRing(cx, cy, 8.0f, 1.8f, ctx.clrWhite, 20, iz + 0.02f); // Borde blanco
                    C2D_DrawCircleSolid(cx, cy, iz + 0.03f, 2.5f, ctx.clrWhite); // Punto central "1"
                }
            }

            // Títulos limpios de la tarjeta (texto legible en ASCII sin emojis ni '?')
            drawCenteredTextWithShadow(ctx.textBuf, cx, by + 49.5f, 0.40f, ctx.clrWhite, cardTitle, cz + 0.05f);
            Ui::pill(bx + 4.0f, by + 59.5f, cardW - 8.0f, 13.5f, cz + 0.04f, Ui::rgba(14, 11, 22, 230));
            drawCenteredTextWithShadow(ctx.textBuf, cx, by + 66.0f, 0.31f, subClr, cardSub, cz + 0.06f);
        }
        }
    }

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

        // Boton Cancelar
        Ui::button(28.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(36, 32, 50, 250), Ui::rgba(85, 75, 110), false);
        Ui::pill(34.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(65, 58, 85));
        drawCenteredText(ctx.textBuf, 45.0f, 165.0f, 0.32f, ctx.clrWhite, "B", 0.95f);
        drawTextWithShadow(ctx.textBuf, 62.0f, 157.0f, 0.38f, ctx.clrWhite, LOC(STR_BACK), 0.95f);

        // Boton Borrar
        Ui::button(166.0f, 148.0f, 126.0f, 34.0f, 0.90f, Ui::rgba(165, 28, 48, 250), Ui::rgba(255, 80, 110), true);
        Ui::pill(172.0f, 155.0f, 22.0f, 20.0f, 0.92f, Ui::rgba(255, 80, 110));
        drawCenteredText(ctx.textBuf, 183.0f, 165.0f, 0.32f, ctx.clrWhite, "A", 0.95f);
        drawTextWithShadow(ctx.textBuf, 200.0f, 157.0f, 0.38f, ctx.clrWhite, LOC(STR_DELETE), 0.95f);
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
