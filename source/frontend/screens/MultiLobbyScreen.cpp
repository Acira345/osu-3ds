#include "MultiLobbyScreen.hpp"
#include "backend/i18n/Localization.hpp"

void runMultiLobbyScreen(AppContext& ctx) {
    ctx.audio.update();
    if (ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }
    ctx.multiMgr.update(false);

    if (!ctx.multiMgr.inMultiplayer) {
        ctx.multiMgr.startBrowser(ctx.sysUser);
        ctx.appState = STATE_MULTI_BROWSER;
        return;
    }

    // Verificación y estado del beatmap para jugadores locales
    static std::string s_lastCheckedSongKey = "";
    static bool s_cachedHasLocalMap = true;

    if (!ctx.multiMgr.lastDownloadedOsuPath.empty()) {
        ctx.songMgr.addSingleBeatmap(ctx.multiMgr.lastDownloadedOsuPath, false, &ctx.scoreMgr);
        ctx.multiMgr.lastDownloadedOsuPath.clear();
        s_lastCheckedSongKey = "";
    }

    std::string curSongKey = ctx.multiMgr.activeBeatmap.folderPath + "::" + ctx.multiMgr.activeBeatmap.osuFilename + "::" + ctx.multiMgr.activeBeatmap.version;
    if (curSongKey != s_lastCheckedSongKey || 
        ((ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_NOT_READY || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) && !s_cachedHasLocalMap)) {
        s_lastCheckedSongKey = curSongKey;
        if (!ctx.multiMgr.activeBeatmap.osuFilename.empty()) {
            std::string existingPath = ctx.findLocalMapFile(ctx.multiMgr.activeBeatmap.folderPath, ctx.multiMgr.activeBeatmap.osuFilename);
            s_cachedHasLocalMap = !existingPath.empty();
        } else {
            s_cachedHasLocalMap = true;
        }
    }
    bool hasLocalMap = s_cachedHasLocalMap;

    if (!ctx.multiMgr.activeBeatmap.osuFilename.empty()) {
        if (!hasLocalMap) {
            if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status != STATUS_DOWNLOADING &&
                ctx.multiMgr.players[ctx.multiMgr.localSlot].status != STATUS_MISSING_MAP) {
                ctx.multiMgr.players[ctx.multiMgr.localSlot].status = STATUS_MISSING_MAP;
                if (ctx.multiMgr.isHost()) {
                    ctx.multiMgr.broadcastLobbyState();
                } else {
                    ctx.multiMgr.sendPlayerUpdate();
                }
            }
        } else {
            if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                ctx.multiMgr.players[ctx.multiMgr.localSlot].status = ctx.multiMgr.isHost() ? STATUS_READY : STATUS_NOT_READY;
                if (ctx.multiMgr.isHost()) {
                    ctx.multiMgr.broadcastLobbyState();
                } else {
                    ctx.multiMgr.sendPlayerUpdate();
                }
            }
        }
    }

    // Inicio de partida sincronizado
    if (ctx.multiMgr.matchStarted) {
        ctx.multiSyncLoaded = false;
        ctx.multiMgr.matchGo = false;
        ctx.appState = STATE_MULTI_SYNC;
        return;
    }

    // Controles físicos
    if (ctx.kDown & KEY_B) {
        if (ctx.multiQueuePickerOpen) {
            ctx.multiQueuePickerOpen = false;
            ctx.audio.playHitSound(0);
        } else {
            ctx.audio.playHitSound(0);
            if (ctx.multiMgr.isHost()) {
                ctx.multiMgr.notifyRoomClosed();
            } else {
                ctx.multiMgr.notifyPlayerLeave();
            }
            ctx.multiMgr.stop();
            ctx.multiMgr.startBrowser(ctx.sysUser);
            ctx.appState = STATE_MULTI_BROWSER;
            return;
        }
    }
    if (ctx.kDown & KEY_A) {
        ctx.audio.playHitSound(2);
        if (ctx.multiQueuePickerOpen) {
            if (!ctx.songMgr.sets.empty()) {
                const auto& s = ctx.songMgr.sets[ctx.multiQueuePickerIndex % ctx.songMgr.sets.size()];
                if (!s.difficulties.empty()) {
                    int diffIdx = ctx.multiQueuePickerDiff % s.difficulties.size();
                    const auto& d = s.difficulties[diffIdx];
                    QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, ctx.multiMgr.localSlot);
                    if (ctx.multiMgr.isHost()) {
                        if (ctx.multiPickerAction == PICKER_ACTION_QUEUE) {
                            ctx.multiMgr.enqueueBeatmap(qm);
                        } else {
                            ctx.multiMgr.setActiveBeatmap(qm);
                        }
                    } else {
                        ctx.multiMgr.requestQueueMap(qm);
                    }
                }
            }
            ctx.multiQueuePickerOpen = false;
        } else {
            if (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status != STATUS_DOWNLOADING) {
                    ctx.multiMgr.startMapDownload(ctx.multiMgr.connectedHostIp);
                }
            } else if (ctx.multiMgr.isHost()) {
                if (ctx.multiMgr.areAllPlayersReady()) {
                    ctx.multiMgr.sendStartMatch();
                }
            } else {
                ctx.multiMgr.toggleLocalReady();
            }
        }
    }
    if (ctx.kDown & KEY_Y) {
        ctx.audio.playHitSound(0);
        ctx.appState = STATE_MODS_SELECT;
        return;
    }
    if (ctx.kDown & KEY_X) {
        ctx.audio.playHitSound(0);
        ctx.multiPickerAction = PICKER_ACTION_QUEUE;
        ctx.appState = STATE_SONG_SELECT;
        return;
    }

    if (ctx.multiQueuePickerOpen) {
        if (ctx.kDown & (KEY_DUP | KEY_CPAD_UP)) {
            if (ctx.multiQueuePickerIndex > 0) {
                ctx.multiQueuePickerIndex--;
                ctx.multiQueuePickerDiff = 0;
            }
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            if (ctx.multiQueuePickerIndex + 1 < static_cast<int>(ctx.songMgr.sets.size())) {
                ctx.multiQueuePickerIndex++;
                ctx.multiQueuePickerDiff = 0;
            }
            ctx.audio.playHitSound(0);
        }
        if (ctx.kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
            if (!ctx.songMgr.sets.empty()) {
                const auto& s = ctx.songMgr.sets[ctx.multiQueuePickerIndex % ctx.songMgr.sets.size()];
                if (ctx.multiQueuePickerDiff > 0) {
                    ctx.multiQueuePickerDiff--;
                    ctx.audio.playHitSound(0);
                } else if (!s.difficulties.empty()) {
                    ctx.multiQueuePickerDiff = static_cast<int>(s.difficulties.size()) - 1;
                    ctx.audio.playHitSound(0);
                }
            }
        }
        if (ctx.kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            if (!ctx.songMgr.sets.empty()) {
                const auto& s = ctx.songMgr.sets[ctx.multiQueuePickerIndex % ctx.songMgr.sets.size()];
                if (ctx.multiQueuePickerDiff + 1 < static_cast<int>(s.difficulties.size())) {
                    ctx.multiQueuePickerDiff++;
                    ctx.audio.playHitSound(0);
                } else {
                    ctx.multiQueuePickerDiff = 0;
                    ctx.audio.playHitSound(0);
                }
            }
        }
    }

    // Entradas táctiles
    if (ctx.touchJustDown) {
        if (ctx.multiQueuePickerOpen) {
            bool touchedSong = false;
            for (int k = 0; k < 4; ++k) {
                float ry = 38.0f + k * 40.0f;
                if (ctx.touch.py >= ry && ctx.touch.py <= ry + 36.0f) {
                    int targetIdx = ctx.multiQueuePickerIndex - (ctx.multiQueuePickerIndex % 4) + k;
                    if (targetIdx < static_cast<int>(ctx.songMgr.sets.size())) {
                        ctx.multiQueuePickerIndex = targetIdx;
                        const auto& s = ctx.songMgr.sets[targetIdx];
                        if (!s.difficulties.empty()) {
                            int diffIdx = (targetIdx == ctx.multiQueuePickerIndex) ? (ctx.multiQueuePickerDiff % s.difficulties.size()) : 0;
                            const auto& d = s.difficulties[diffIdx];
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
                        }
                    }
                    ctx.multiQueuePickerOpen = false;
                    touchedSong = true;
                    break;
                }
            }
            if (!touchedSong && ctx.touch.py >= 196.0f && ctx.touch.py <= 234.0f) {
                if (!ctx.songMgr.sets.empty()) {
                    const auto& s = ctx.songMgr.sets[ctx.multiQueuePickerIndex % ctx.songMgr.sets.size()];
                    if (!s.difficulties.empty()) {
                        int diffIdx = ctx.multiQueuePickerDiff % s.difficulties.size();
                        const auto& d = s.difficulties[diffIdx];
                        QueuedBeatmap qm(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, ctx.multiMgr.localSlot);

                        if (ctx.multiMgr.isHost()) {
                            ctx.multiMgr.hostChangingSong = false;
                            if (ctx.touch.px >= 14.0f && ctx.touch.px <= 154.0f) {
                                ctx.multiMgr.setActiveBeatmap(qm);
                                ctx.audio.playHitSound(2);
                                ctx.multiQueuePickerOpen = false;
                            } else if (ctx.touch.px >= 164.0f && ctx.touch.px <= 306.0f) {
                                ctx.multiMgr.enqueueBeatmap(qm);
                                ctx.audio.playHitSound(2);
                                ctx.multiQueuePickerOpen = false;
                            }
                        } else {
                            ctx.multiMgr.requestQueueMap(qm);
                            ctx.audio.playHitSound(2);
                            ctx.multiQueuePickerOpen = false;
                        }
                    }
                }
            }
        } else {
            // Pestañas de Criterio de Victoria (WinCondition)
            if (ctx.touch.py >= 42.0f && ctx.touch.py <= 74.0f) {
                if (ctx.touch.px >= 10.0f && ctx.touch.px <= 104.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_SCORE);
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 110.0f && ctx.touch.px <= 204.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_ACCURACY);
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 210.0f && ctx.touch.px <= 306.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_COMBO);
                    ctx.audio.playHitSound(0);
                }
            }

            if (ctx.multiMgr.isHost()) {
                // Tarjeta 1: Modificadores (x: 10..102, y: 82..146)
                if (ctx.touch.px >= 10.0f && ctx.touch.px <= 102.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 146.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.appState = STATE_MODS_SELECT;
                    return;
                }
                // Tarjeta 2: Cambiar Canción Actual (x: 110..204, y: 82..146)
                if (ctx.touch.px >= 110.0f && ctx.touch.px <= 204.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 146.0f) {
                    if (ctx.multiMgr.isAnyClientDownloading()) {
                        ctx.audio.playHitSound(4); // Bloqueado: jugador descargando
                        return;
                    }
                    ctx.audio.playHitSound(0);
                    ctx.multiPickerAction = PICKER_ACTION_ACTIVE;
                    ctx.multiMgr.hostChangingSong = true;
                    ctx.multiMgr.broadcastLobbyState();
                    ctx.appState = STATE_SONG_SELECT;
                    return;
                }
                // Tarjeta 3: + Añadir a la Cola (x: 212..306, y: 82..146)
                if (ctx.touch.px >= 212.0f && ctx.touch.px <= 306.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 146.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.multiPickerAction = PICKER_ACTION_QUEUE;
                    ctx.appState = STATE_SONG_SELECT;
                    return;
                }
            } else {
                // Cliente: 2 tarjetas
                // Tarjeta 1: Modificadores (x: 10..152, y: 82..146)
                if (ctx.touch.px >= 10.0f && ctx.touch.px <= 152.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 146.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.appState = STATE_MODS_SELECT;
                    return;
                }
                // Tarjeta 2: + Añadir Canción (Cola) (x: 164..306, y: 82..146)
                if (ctx.touch.px >= 164.0f && ctx.touch.px <= 306.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 146.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.multiPickerAction = PICKER_ACTION_QUEUE;
                    ctx.appState = STATE_SONG_SELECT;
                    return;
                }
            }

            // Botón Inferior Grande: LISTO / INICIAR PARTIDA / DESCARGAR MAPA
            if (ctx.touch.px >= 10.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= 176.0f && ctx.touch.py <= 232.0f) {
                ctx.audio.playHitSound(2);
                if (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) {
                    if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status != STATUS_DOWNLOADING) {
                        ctx.multiMgr.startMapDownload(ctx.multiMgr.connectedHostIp);
                    }
                } else if (ctx.multiMgr.isHost()) {
                    if (ctx.multiMgr.areAllPlayersReady()) {
                        ctx.multiMgr.sendStartMatch();
                    }
                } else {
                    ctx.multiMgr.toggleLocalReady();
                }
            }
        }
    }

    // =========================================================================
    // RENDERIZADO DEL LOBBY (PANTALLA SUPERIOR 400x240 & INFERIOR 320x240)
    // =========================================================================
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        // Fondo medianoche (400x240)
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(8, 6, 16), ctx.clrPrimary, 0.01f);

        // 1. Cabecera de sala (Y: 0, H: 26) idéntica al diseño solicitado
        float dBar = depthShift(1.8f);
        Ui::gradV(0.0f, 0.0f, 400.0f, 26.0f, 0.10f, Ui::rgba(27, 20, 45, 255), Ui::rgba(20, 15, 34, 255));
        C2D_DrawRectSolid(0.0f, 25.0f, 0.15f, 400.0f, 1.0f, Ui::rgba(0, 229, 255));

        // Insignia rosa osu!3DS
        Ui::slantRectGrad(8.0f + dBar, 3.5f, 78.0f, 19.0f, -4.0f, 0.25f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawCenteredTextWithShadow(ctx.textBuf, 47.0f + dBar, 13.0f, 0.34f, ctx.clrWhite, "osu!3DS", 0.30f);

        drawTextWithShadow(ctx.textBuf, 94.0f + dBar, 6.0f, 0.38f, ctx.clrWhite, LOC(STR_MULTI_LOBBY_TITLE), 0.30f);

        // Pastilla [4 Slots]
        char slotBadge[24];
        std::snprintf(slotBadge, sizeof(slotBadge), LOC(STR_MULTI_SLOTS_FMT), ctx.multiMgr.getActivePlayerCount());
        Ui::pill(240.0f + dBar, 4.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(20, 34, 58, 240));
        drawRectOutline(240.0f + dBar, 4.0f, 54.0f, 18.0f, 1.0f, Ui::rgba(0, 229, 255), 0.26f);
        drawCenteredText(ctx.textBuf, 267.0f + dBar, 13.0f, 0.30f, Ui::rgba(0, 229, 255), slotBadge, 0.30f);

        // Regla: Max Combo / Puntuación / Precisión en amarillo
        const char* winCritName = (ctx.multiMgr.winCondition == WIN_SCORE) ? LOC(STR_MULTI_WIN_SCORE) :
                                ((ctx.multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_MULTI_WIN_ACC) : LOC(STR_MULTI_WIN_COMBO));
        char ruleLabel[64];
        std::snprintf(ruleLabel, sizeof(ruleLabel), LOC(STR_MULTI_RULE_FMT), winCritName);
        Ui::pill(302.0f + dBar, 4.0f, 90.0f, 18.0f, 0.25f, Ui::rgba(45, 34, 20, 240));
        drawRectOutline(302.0f + dBar, 4.0f, 90.0f, 18.0f, 1.0f, Ui::rgba(255, 209, 102), 0.26f);
        drawCenteredText(ctx.textBuf, 347.0f + dBar, 13.0f, 0.30f, Ui::rgba(255, 209, 102), ruleLabel, 0.30f);

        // 2. Columna Izquierda: Dibujar los 4 slots de jugadores (X: 10, W: 245, H: 40)
        float dCards = depthShift(2.5f);
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            float cy = 32.0f + (i * 44.0f);
            float cx = 10.0f + dCards;
            float cw = 245.0f;
            float ch = 40.0f;

            const auto& p = ctx.multiMgr.players[i];
            if (p.active) {
                u32 slotBg = p.isLocal ? Ui::rgba(20, 34, 58, 255) : Ui::rgba(18, 22, 38, 255);
                u32 accent = p.isHost ? Ui::rgba(255, 209, 102, 255) :
                             (p.isLocal ? Ui::rgba(0, 229, 255, 255) :
                             ((p.status == STATUS_READY) ? Ui::rgba(74, 222, 128, 255) : Ui::rgba(148, 163, 184, 255)));

                Ui::panel(cx, cy, cw, ch, 0.20f, slotBg, accent);
                drawRectOutline(cx, cy, cw, ch, 1.2f, accent, 0.22f);

                // Nombre del jugador
                drawTextWithShadow(ctx.textBuf, cx + 10.0f, cy + 4.0f, 0.44f, ctx.clrWhite, p.name, 0.25f);

                // Insignia HOST o TÚ
                if (p.isHost) {
                    Ui::pill(cx + 98.0f, cy + 5.0f, 40.0f, 14.0f, 0.26f, Ui::rgba(255, 209, 102));
                    drawCenteredText(ctx.textBuf, cx + 118.0f, cy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_CARD_HOST), 0.30f);
                } else if (p.isLocal) {
                    Ui::pill(cx + 98.0f, cy + 5.0f, 30.0f, 14.0f, 0.26f, Ui::rgba(0, 229, 255));
                    drawCenteredText(ctx.textBuf, cx + 113.0f, cy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
                }

                // Subtítulo: Mods equipados y Ping
                char subStr[48];
                std::string modStr = (p.activeMods != MOD_NONE) ? formatModsString(p.activeMods) : LOC(STR_MULTI_NO_MODS);
                std::snprintf(subStr, sizeof(subStr), "+%s · %ums", modStr.c_str(), p.pingMs);
                drawText(ctx.textBuf, cx + 10.0f, cy + 22.0f, 0.29f, p.isLocal ? Ui::rgba(0, 229, 255) : Ui::rgba(148, 163, 184), subStr, 0.25f);

                // Estado Ready
                if (p.status == STATUS_READY) {
                    Ui::pill(cx + cw - 58.0f, cy + 10.0f, 50.0f, 20.0f, 0.26f, Ui::rgba(16, 50, 35, 240));
                    drawRectOutline(cx + cw - 58.0f, cy + 10.0f, 50.0f, 20.0f, 1.2f, Ui::rgba(74, 222, 128), 0.28f);
                    drawCenteredText(ctx.textBuf, cx + cw - 33.0f, cy + 20.0f, 0.32f, Ui::rgba(74, 222, 128), LOC(STR_MULTI_STATUS_READY), 0.30f);
                } else if (p.status == STATUS_DOWNLOADING) {
                    char dlPill[16];
                    std::snprintf(dlPill, sizeof(dlPill), "%d%%", p.downloadProgress);
                    Ui::pill(cx + cw - 64.0f, cy + 10.0f, 56.0f, 20.0f, 0.26f, Ui::rgba(220, 120, 20));
                    drawCenteredText(ctx.textBuf, cx + cw - 36.0f, cy + 20.0f, 0.30f, ctx.clrWhite, dlPill, 0.30f);
                } else if (p.status == STATUS_MISSING_MAP) {
                    Ui::pill(cx + cw - 64.0f, cy + 10.0f, 56.0f, 20.0f, 0.26f, Ui::rgba(215, 50, 60));
                    drawCenteredText(ctx.textBuf, cx + cw - 36.0f, cy + 20.0f, 0.30f, ctx.clrWhite, LOC(STR_MULTI_STATUS_NO_MAP), 0.30f);
                } else {
                    Ui::pill(cx + cw - 70.0f, cy + 10.0f, 62.0f, 20.0f, 0.26f, Ui::rgba(35, 30, 48, 240));
                    drawRectOutline(cx + cw - 70.0f, cy + 10.0f, 62.0f, 20.0f, 1.0f, Ui::rgba(148, 163, 184), 0.28f);
                    drawCenteredText(ctx.textBuf, cx + cw - 39.0f, cy + 20.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_STATUS_WAITING), 0.30f);
                }
            } else {
                Ui::panel(cx, cy, cw, ch, 0.18f, Ui::rgba(15, 12, 24, 180));
                drawRectOutline(cx, cy, cw, ch, 1.0f, Ui::rgba(35, 30, 50), 0.20f);
                drawCenteredText(ctx.textBuf, cx + cw * 0.5f, cy + 19.0f, 0.34f, Ui::rgba(90, 85, 115), LOC(STR_MULTI_EMPTY_SLOT), 0.25f);
            }
        }

        // 3. Columna Derecha: Tarjeta de Siguiente Mapa y Cola (X: 265, W: 125, H: 185)
        float dRight = depthShift(2.0f);
        float rx = 263.0f + dRight;
        float rw = 127.0f;

        // SIGUIENTE MAPA
        Ui::panel(rx, 32.0f, rw, 86.0f, 0.20f, Ui::rgba(13, 17, 31, 255), Ui::rgba(0, 229, 255));
        if (ctx.multiMgr.hostChangingSong) {
            Ui::glow(rx + rw * 0.5f, 42.0f, 0.22f, 50.0f, Ui::rgba(255, 209, 102, 90), 2);
            drawTextWithShadow(ctx.textBuf, rx + 6.0f, 36.0f, 0.30f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_CHANGING_MAP), 0.25f);
            drawCenteredTextWithShadow(ctx.textBuf, rx + rw * 0.5f, 68.0f, 0.33f, ctx.clrWhite, LOC(STR_MULTI_HOST_CHOOSING_SHORT), 0.25f);
        } else {
            drawTextWithShadow(ctx.textBuf, rx + 6.0f, 36.0f, 0.31f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_NEXT_MAP), 0.25f);

            char lenStr[16];
            std::snprintf(lenStr, sizeof(lenStr), "%d:%02d", ctx.multiMgr.activeBeatmap.lengthSec / 60, ctx.multiMgr.activeBeatmap.lengthSec % 60);
            drawText(ctx.textBuf, rx + rw - 30.0f, 36.0f, 0.30f, Ui::rgba(148, 163, 184), lenStr, 0.25f);

            std::string dispTitle = ctx.multiMgr.activeBeatmap.title;
            drawTextWithShadow(ctx.textBuf, rx + 6.0f, 50.0f, 0.40f, ctx.clrWhite, truncateString(dispTitle, 13).c_str(), 0.25f);

            std::string dispArtist = ctx.multiMgr.activeBeatmap.artist;
            drawText(ctx.textBuf, rx + 6.0f, 66.0f, 0.28f, Ui::rgba(148, 163, 184), truncateString(dispArtist, 16).c_str(), 0.25f);

            char starsStr[24];
            std::snprintf(starsStr, sizeof(starsStr), "* %.2f", ctx.multiMgr.activeBeatmap.starRating);
            drawTextWithShadow(ctx.textBuf, rx + 6.0f, 82.0f, 0.32f, Ui::rgba(255, 209, 102), starsStr, 0.25f);

            char bpmStr[16];
            std::snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM", ctx.multiMgr.activeBeatmap.bpm);
            drawText(ctx.textBuf, rx + rw - 48.0f, 82.0f, 0.30f, Ui::rgba(148, 163, 184), bpmStr, 0.25f);
        }

        // EN COLA
        Ui::panel(rx, 122.0f, rw, 85.0f, 0.20f, Ui::rgba(13, 17, 31, 255), Ui::rgba(45, 38, 65));
        char qHeader[24];
        std::snprintf(qHeader, sizeof(qHeader), LOC(STR_MULTI_QUEUE_COUNT_FMT), static_cast<int>(ctx.multiMgr.mapQueue.size()));
        drawText(ctx.textBuf, rx + 6.0f, 126.0f, 0.30f, Ui::rgba(148, 163, 184), qHeader, 0.25f);

        if (ctx.multiMgr.mapQueue.empty()) {
            drawCenteredText(ctx.textBuf, rx + rw * 0.5f, 150.0f, 0.32f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_QUEUE_EMPTY_LABEL), 0.25f);
            drawCenteredText(ctx.textBuf, rx + rw * 0.5f, 170.0f, 0.28f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_ADD_MAP_HINT), 0.25f);
        } else {
            for (size_t q = 0; q < ctx.multiMgr.mapQueue.size() && q < 2; ++q) {
                float qy = 142.0f + q * 28.0f;
                Ui::panel(rx + 4.0f, qy, rw - 8.0f, 24.0f, 0.22f, Ui::rgba(20, 25, 45, 220));
                drawRectOutline(rx + 4.0f, qy, rw - 8.0f, 24.0f, 1.0f, Ui::rgba(35, 30, 55), 0.24f);
                char qItemStr[48];
                std::snprintf(qItemStr, sizeof(qItemStr), "#%u %s", static_cast<unsigned int>(q + 2), ctx.multiMgr.mapQueue[q].title.c_str());
                drawTextWithShadow(ctx.textBuf, rx + 8.0f, qy + 4.0f, 0.30f, ctx.clrWhite, truncateString(qItemStr, 16).c_str(), 0.26f);
            }
        }

        // Barra inferior de atajos de control (Y: 216..240)
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.30f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        float byY = 220.0f;
        drawKeyBadge(ctx.textBuf, 10.0f + dBar, byY, "A", LOC(STR_MULTI_BADGE_TOGGLE_READY), Ui::rgba(235, 45, 65));
        drawKeyBadge(ctx.textBuf, 118.0f + dBar, byY, "Y", LOC(STR_MULTI_BADGE_CHOOSE_MODS), Ui::rgba(74, 222, 128));
        drawKeyBadge(ctx.textBuf, 210.0f + dBar, byY, "X", LOC(STR_MULTI_BADGE_QUEUE), Ui::rgba(0, 229, 255));
        drawKeyBadge(ctx.textBuf, 298.0f + dBar, byY, "B", LOC(STR_MULTI_BADGE_LEAVE), Ui::rgba(148, 163, 184));
        drawRightAlignedText(ctx.textBuf, 392.0f + dBar, byY, 0.28f, Ui::rgba(135, 130, 165), "400x240", 0.32f);

        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::panel(50.0f + dToast, 212.0f, 300.0f, 24.0f, 0.90f, Ui::rgba(180, 20, 40, 240), Ui::rgba(255, 100, 120));
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // =========================================================================
    // PANTALLA INFERIOR TÁCTIL (320x240) — IDÉNTICA A LA IMAGEN DE REFERENCIA
    // =========================================================================
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    // Fondo pantalla táctil (320x240)
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 240.0f, Ui::rgba(15, 11, 26));

    if (ctx.multiQueuePickerOpen) {
        Ui::panel(10.0f, 8.0f, 300.0f, 224.0f, 0.15f, Ui::rgba(16, 14, 28, 250), ctx.clrPrimary);
        const char* pickerTitle = ctx.multiMgr.isHost()
            ? ((ctx.multiPickerAction == PICKER_ACTION_QUEUE) ? LOC(STR_MULTI_PICKER_QUEUE_TITLE) : LOC(STR_MULTI_PICKER_CHANGE_TITLE))
            : LOC(STR_MULTI_PICKER_SUGGEST_TITLE);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 18.0f, 0.40f, ctx.clrWhite, pickerTitle, 0.20f);

        for (int k = 0; k < 4; ++k) {
            int idx = ctx.multiQueuePickerIndex - (ctx.multiQueuePickerIndex % 4) + k;
            float qy = 38.0f + k * 40.0f;
            if (idx < static_cast<int>(ctx.songMgr.sets.size())) {
                const auto& s = ctx.songMgr.sets[idx];
                bool isSel = (idx == ctx.multiQueuePickerIndex);
                u32 bgCard = isSel ? Ui::rgba(45, 38, 70, 255) : Ui::rgba(24, 20, 38, 230);
                Ui::panel(18.0f, qy, 284.0f, 36.0f, 0.20f, bgCard, isSel ? (ctx.multiMgr.isHost() ? Ui::rgba(255, 140, 20) : ctx.clrPrimary) : 0);
                drawText(ctx.textBuf, 24.0f, qy + 3.0f, 0.38f, ctx.clrWhite, truncateString(s.title, 26).c_str(), 0.25f);

                if (!s.difficulties.empty()) {
                    int diffIdx = isSel ? (ctx.multiQueuePickerDiff % s.difficulties.size()) : 0;
                    const auto& d = s.difficulties[diffIdx];
                    char diffInfo[64];
                    if (isSel && s.difficulties.size() > 1) {
                        std::snprintf(diffInfo, sizeof(diffInfo), "< %s > (* %.2f)", d.version.c_str(), d.starRating);
                    } else {
                        std::snprintf(diffInfo, sizeof(diffInfo), "[%s] * %.2f", d.version.c_str(), d.starRating);
                    }
                    drawText(ctx.textBuf, 24.0f, qy + 19.0f, 0.30f, isSel ? Ui::rgba(255, 205, 50) : ctx.clrMuted, truncateString(diffInfo, 30).c_str(), 0.25f);
                }
            }
        }

        if (ctx.multiMgr.isHost()) {
            Ui::button(14.0f, 198.0f, 140.0f, 28.0f, 0.20f, Ui::rgba(255, 140, 20), Ui::rgba(255, 200, 100), true);
            drawCenteredTextWithShadow(ctx.textBuf, 84.0f, 212.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_PICKER_CHANGE_BTN), 0.25f);

            Ui::button(166.0f, 198.0f, 140.0f, 28.0f, 0.20f, Ui::rgba(0, 229, 255), Ui::rgba(160, 230, 255), false);
            drawCenteredTextWithShadow(ctx.textBuf, 236.0f, 212.0f, 0.34f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_PICKER_QUEUE_BTN), 0.25f);
        } else {
            Ui::button(20.0f, 198.0f, 280.0f, 28.0f, 0.20f, Ui::rgba(180, 80, 240), Ui::rgba(220, 160, 255), false);
            drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 212.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_PICKER_SUGGEST_BTN), 0.25f);
        }
    } else {
        // Cabecera táctil
        drawTextWithShadow(ctx.textBuf, 12.0f, 6.0f, 0.38f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_TOUCH_HEADER_1), 0.20f);
        drawText(ctx.textBuf, 12.0f, 18.0f, 0.32f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_TOUCH_HEADER_2), 0.20f);
        drawRightAlignedText(ctx.textBuf, 308.0f, 6.0f, 0.28f, Ui::rgba(148, 163, 184), "320x240 (4:3)", 0.20f);

        // Criterio de Victoria del Podio
        drawText(ctx.textBuf, 12.0f, 32.0f, 0.31f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_WIN_CRITERIA), 0.20f);
        const char* critText = (ctx.multiMgr.winCondition == WIN_SCORE) ? LOC(STR_MULTI_WIN_SCORE) :
                               ((ctx.multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_MULTI_WIN_ACC) : LOC(STR_MULTI_WIN_COMBO));
        drawRightAlignedText(ctx.textBuf, 308.0f, 32.0f, 0.33f, Ui::rgba(0, 229, 255), critText, 0.20f);

        // 1. Selector de Regla de Victoria (X: 10, Y: 46, W: 300, H: 30)
        auto drawRuleBtn = [&](float x, const char* name, WinCondition cond) {
            bool active = (ctx.multiMgr.winCondition == cond);
            u32 bBg = active ? Ui::rgba(0, 229, 255) : Ui::rgba(20, 25, 45);
            u32 bTxt = active ? Ui::rgba(0, 0, 0, 255) : ctx.clrWhite;
            Ui::panel(x, 46.0f, 96.0f, 28.0f, 0.20f, bBg, active ? Ui::rgba(255, 255, 255) : 0);
            if (active) {
                Ui::glow(x + 48.0f, 60.0f, 0.18f, 40.0f, Ui::rgba(0, 229, 255, 80), 2);
            }
            drawCenteredTextWithShadow(ctx.textBuf, x + 48.0f, 60.0f, 0.33f, bTxt, name, 0.25f);
        };
        drawRuleBtn(10.0f,  LOC(STR_MULTI_WIN_SCORE), WIN_SCORE);
        drawRuleBtn(112.0f, LOC(STR_MULTI_WIN_ACC),   WIN_ACCURACY);
        drawRuleBtn(214.0f, LOC(STR_MULTI_WIN_COMBO), WIN_COMBO);

        // 2 y 3. Botones Táctiles de Modificadores y Añadir Canción (Y: 82, H: 64)
        if (ctx.multiMgr.isHost()) {
            // Host: 3 botones limpios (Mods, Cambiar Canción [con bloqueo si descargan], Añadir Cola)
            // Tarjeta 1: Modificadores
            Ui::panel(10.0f, 82.0f, 94.0f, 64.0f, 0.20f, Ui::rgba(22, 28, 48), Ui::rgba(0, 229, 255));
            drawTextWithShadow(ctx.textBuf, 14.0f, 86.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_MODIFIERS), 0.25f);
            std::string myMods = (ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods != MOD_NONE)
                ? ("+" + formatModsString(ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods)) : "None";
            Ui::pill(14.0f, 102.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(0, 229, 255));
            drawCenteredText(ctx.textBuf, 57.0f, 110.0f, 0.28f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 10).c_str(), 0.30f);
            drawText(ctx.textBuf, 14.0f, 124.0f, 0.26f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_MODS_MENU), 0.25f);

            // Tarjeta 2: Cambiar Canción (Bloqueada si hay jugadores descargando)
            bool isBlocked = ctx.multiMgr.isAnyClientDownloading();
            u32 c2Border = isBlocked ? Ui::rgba(140, 50, 60) : Ui::rgba(255, 209, 102);
            Ui::panel(112.0f, 82.0f, 96.0f, 64.0f, 0.20f, Ui::rgba(22, 28, 48), c2Border);
            drawTextWithShadow(ctx.textBuf, 116.0f, 86.0f, 0.32f, isBlocked ? ctx.clrMiss : ctx.clrWhite, LOC(STR_MULTI_CHANGE_MAP), 0.25f);
            Ui::pill(116.0f, 102.0f, 88.0f, 16.0f, 0.26f, c2Border);
            drawCenteredText(ctx.textBuf, 160.0f, 110.0f, 0.26f, ctx.clrWhite, isBlocked ? LOC(STR_MULTI_BLOCKED) : LOC(STR_MULTI_ACTIVE_PILL), 0.30f);
            drawText(ctx.textBuf, 116.0f, 124.0f, 0.25f, isBlocked ? ctx.clrMiss : Ui::rgba(148, 163, 184), isBlocked ? LOC(STR_MULTI_DL_ACTIVE) : LOC(STR_MULTI_PICK_NEW_MAP), 0.25f);

            // Tarjeta 3: Añadir a Cola
            Ui::panel(216.0f, 82.0f, 94.0f, 64.0f, 0.20f, Ui::rgba(22, 28, 48), Ui::rgba(180, 80, 240));
            drawTextWithShadow(ctx.textBuf, 220.0f, 86.0f, 0.32f, ctx.clrWhite, LOC(STR_MULTI_ADD_QUEUE_BTN), 0.25f);
            Ui::pill(220.0f, 102.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(180, 80, 240));
            drawCenteredText(ctx.textBuf, 263.0f, 110.0f, 0.26f, ctx.clrWhite, LOC(STR_MULTI_QUEUE_PILL), 0.30f);
            drawText(ctx.textBuf, 220.0f, 124.0f, 0.25f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_SUGGEST_MAP_HINT), 0.25f);
        } else {
            // Cliente: 2 botones anchos (Mods y Añadir Canción para sugerir a la cola)
            Ui::panel(10.0f, 82.0f, 145.0f, 64.0f, 0.20f, Ui::rgba(22, 28, 48), Ui::rgba(0, 229, 255));
            drawTextWithShadow(ctx.textBuf, 18.0f, 88.0f, 0.36f, ctx.clrWhite, LOC(STR_MULTI_MODIFIERS), 0.25f);
            std::string myMods = (ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods != MOD_NONE)
                ? ("+" + formatModsString(ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods)) : "None";
            Ui::pill(106.0f, 86.0f, 42.0f, 16.0f, 0.26f, Ui::rgba(0, 229, 255));
            drawCenteredText(ctx.textBuf, 127.0f, 94.0f, 0.26f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 6).c_str(), 0.30f);
            drawText(ctx.textBuf, 18.0f, 114.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_TAP_OPEN_MODS), 0.25f);
            drawText(ctx.textBuf, 18.0f, 128.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_OF_MODS), 0.25f);

            Ui::panel(165.0f, 82.0f, 145.0f, 64.0f, 0.20f, Ui::rgba(22, 28, 48), Ui::rgba(180, 80, 240));
            drawTextWithShadow(ctx.textBuf, 173.0f, 88.0f, 0.36f, ctx.clrWhite, LOC(STR_MULTI_ADD_SONG_CLIENT), 0.25f);
            Ui::pill(270.0f, 86.0f, 34.0f, 16.0f, 0.26f, Ui::rgba(180, 80, 240));
            drawCenteredText(ctx.textBuf, 287.0f, 94.0f, 0.26f, ctx.clrWhite, LOC(STR_MULTI_QUEUE_CLIENT), 0.30f);
            drawText(ctx.textBuf, 173.0f, 114.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_SUGGEST_BEATMAP_1), 0.25f);
            drawText(ctx.textBuf, 173.0f, 128.0f, 0.28f, Ui::rgba(148, 163, 184), LOC(STR_MULTI_SUGGEST_BEATMAP_2), 0.25f);
        }

        // Barra de estado de jugadores listos (Y: 154)
        char readyStat[64];
        std::snprintf(readyStat, sizeof(readyStat), LOC(STR_MULTI_PLAYERS_READY_FMT), ctx.multiMgr.getReadyPlayerCount(), ctx.multiMgr.getActivePlayerCount());
        drawTextWithShadow(ctx.textBuf, 12.0f, 154.0f, 0.34f, ctx.clrWhite, readyStat, 0.25f);

        const char* subStat = (ctx.multiMgr.hostChangingSong) ? LOC(STR_MULTI_HOST_CHANGING) :
                              (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) ? LOC(STR_MULTI_MISSING_MAP) :
                              (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_DOWNLOADING) ? LOC(STR_MULTI_DOWNLOADING) :
                              (ctx.multiMgr.areAllPlayersReady()) ? LOC(STR_MULTI_ALL_READY) :
                              (ctx.multiMgr.isHost() ? LOC(STR_MULTI_WAITING_PLAYERS) : LOC(STR_MULTI_WAITING_HOST));
        u32 subStatClr = (ctx.multiMgr.areAllPlayersReady()) ? Ui::rgba(74, 222, 128) : Ui::rgba(0, 229, 255);
        drawRightAlignedText(ctx.textBuf, 308.0f, 154.0f, 0.32f, subStatClr, subStat, 0.25f);

        // 4. Gran Botón de Ready / Start (X: 10, Y: 176, W: 300, H: 48)
        float btnX = 10.0f;
        float btnY = 176.0f;
        float btnW = 300.0f;
        float btnH = 48.0f;

        if (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) {
            Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.16f, btnW * 0.52f, Ui::rgba(0, 229, 255, 90), 3);
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(0, 180, 215), Ui::rgba(160, 230, 255));
            drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, ctx.clrWhite, LOC(STR_MULTI_BTN_DL_HOST), 0.30f);
        } else if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_DOWNLOADING) {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(220, 120, 20), Ui::rgba(255, 180, 60));
            float prog = ctx.multiMgr.players[ctx.multiMgr.localSlot].downloadProgress / 100.0f;
            if (prog > 0.02f) {
                C2D_DrawRectSolid(btnX + 4.0f, btnY + 4.0f, 0.22f, (btnW - 8.0f) * std::min(1.0f, prog), btnH - 8.0f, Ui::rgba(255, 160, 40, 200));
            }
            char dlTxt[48];
            std::snprintf(dlTxt, sizeof(dlTxt), LOC(STR_MULTI_BTN_DL_PROG_FMT), ctx.multiMgr.players[ctx.multiMgr.localSlot].downloadProgress);
            drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, ctx.clrWhite, dlTxt, 0.30f);
        } else if (ctx.multiMgr.isHost()) {
            if (ctx.multiMgr.areAllPlayersReady()) {
                Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.18f, btnW * 0.55f, Ui::rgba(74, 222, 128, 90), 4);
                Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(16, 185, 129), Ui::rgba(200, 255, 220));
                drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.50f, ctx.clrWhite, LOC(STR_MULTI_BTN_START_MATCH), 0.30f);
            } else {
                Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(255, 209, 102), Ui::rgba(255, 240, 160));
                drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_BTN_WAIT_READY), 0.30f);
            }
        } else if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_READY) {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(16, 185, 129), Ui::rgba(200, 255, 220));
            drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.48f, ctx.clrWhite, LOC(STR_MULTI_BTN_READY_CANCEL), 0.30f);
        } else {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(0, 229, 255), Ui::rgba(200, 255, 255));
            drawCenteredTextWithShadow(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.48f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_BTN_READY_CONFIRM), 0.30f);
        }
    }

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.95f, 10.0f, Ui::withAlpha(Ui::rgba(0, 229, 255), 130), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.96f, 4.0f, Ui::rgba(0, 229, 255));
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
