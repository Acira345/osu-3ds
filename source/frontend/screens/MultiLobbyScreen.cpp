#include "MultiLobbyScreen.hpp"

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

    // Verificación y estado del beatmap para jugadores locales (con caché para evitar lecturas de SD cada frame)
    static std::string s_lastCheckedSongKey = "";
    static bool s_cachedHasLocalMap = true;

    // Si se acaba de descargar un mapa por UDS o TCP, indexarlo de inmediato en SongManager
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

    // Inicio de partida sincronizado: transición al estado de sincronización y carga
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
    // Roles fijos: KEY_SELECT no alterna roles para garantizar estabilidad absoluta de conexión
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
                                // Tocar botón [A] CAMBIAR MAPA
                                ctx.multiMgr.setActiveBeatmap(qm);
                                ctx.audio.playHitSound(2);
                                ctx.multiQueuePickerOpen = false;
                            } else if (ctx.touch.px >= 164.0f && ctx.touch.px <= 306.0f) {
                                // Tocar botón [X] + A LA COLA
                                ctx.multiMgr.enqueueBeatmap(qm);
                                ctx.audio.playHitSound(2);
                                ctx.multiQueuePickerOpen = false;
                            }
                        } else {
                            // Cliente: sugerir a la cola
                            ctx.multiMgr.requestQueueMap(qm);
                            ctx.audio.playHitSound(2);
                            ctx.multiQueuePickerOpen = false;
                        }
                    }
                }
            }
        } else {
            // Roles estrictamente fijos: no se permite alternar modo Host/Cliente tocando la pantalla

            // Pestañas de Criterio de Victoria (WinCondition)
            if (ctx.touch.py >= 46.0f && ctx.touch.py <= 72.0f) {
                if (ctx.touch.px >= 14.0f && ctx.touch.px <= 104.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_SCORE);
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 112.0f && ctx.touch.px <= 202.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_ACCURACY);
                    ctx.audio.playHitSound(0);
                } else if (ctx.touch.px >= 210.0f && ctx.touch.px <= 304.0f) {
                    if (ctx.multiMgr.isHost()) ctx.multiMgr.setWinConditionExplicit(WIN_COMBO);
                    ctx.audio.playHitSound(0);
                }
            }

            if (ctx.multiMgr.isHost()) {
                // Tarjeta 1: Modificadores (x: 10..104, y: 82..150)
                if (ctx.touch.px >= 10.0f && ctx.touch.px <= 104.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 150.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.appState = STATE_MODS_SELECT;
                    return;
                }
                // Tarjeta 2: Cambiar Canción Actual (x: 113..207, y: 82..150)
                if (ctx.touch.px >= 113.0f && ctx.touch.px <= 207.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 150.0f) {
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
                // Tarjeta 3: + Añadir a la Cola (x: 216..310, y: 82..150)
                if (ctx.touch.px >= 216.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 150.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.multiPickerAction = PICKER_ACTION_QUEUE;
                    ctx.appState = STATE_SONG_SELECT;
                    return;
                }
            } else {
                // Cliente: 2 tarjetas
                // Tarjeta 1: Modificadores (x: 12..154, y: 82..150)
                if (ctx.touch.px >= 12.0f && ctx.touch.px <= 154.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 150.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.appState = STATE_MODS_SELECT;
                    return;
                }
                // Tarjeta 2: + Añadir Canción (Cola) (x: 166..308, y: 82..150)
                if (ctx.touch.px >= 166.0f && ctx.touch.px <= 308.0f && ctx.touch.py >= 82.0f && ctx.touch.py <= 150.0f) {
                    ctx.audio.playHitSound(0);
                    ctx.multiPickerAction = PICKER_ACTION_QUEUE;
                    ctx.appState = STATE_SONG_SELECT;
                    return;
                }
            }

            // Botón Inferior Grande: LISTO / INICIAR PARTIDA / DESCARGAR MAPA
            if (ctx.touch.px >= 12.0f && ctx.touch.px <= 308.0f && ctx.touch.py >= 176.0f && ctx.touch.py <= 230.0f) {
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

    // RENDERIZADO DEL LOBBY (PANTALLA SUPERIOR E INFERIOR)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        // float dBg = depthShift(-3.0f);
        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

        // 1. Barra Superior del Lobby
        float dBar = depthShift(1.8f);
        Ui::pill(10.0f + dBar, 9.0f, 62.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
        drawCenteredText(ctx.textBuf, 41.0f + dBar, 17.0f, 0.36f, ctx.clrWhite, "osu!3DS", 0.30f);

        const char* lobbyTitle = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
            ? LOC(STR_MULTI_ROOMS_UDS)
            : LOC(STR_MULTI_ROOMS_LAN);
        drawText(ctx.textBuf, 78.0f + dBar, 11.0f, 0.44f, ctx.clrWhite, lobbyTitle, 0.30f);

        char slotBadge[16];
        std::snprintf(slotBadge, sizeof(slotBadge), "[%d Slots]", ctx.multiMgr.getActivePlayerCount());
        Ui::pill(240.0f + dBar, 9.0f, 52.0f, 18.0f, 0.25f, Ui::rgba(20, 160, 210, 220));
        drawCenteredText(ctx.textBuf, 266.0f + dBar, 17.0f, 0.32f, ctx.clrWhite, slotBadge, 0.30f);

        // Regla de sala en la esquina superior derecha
        const char* winCritName = (ctx.multiMgr.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                                ((ctx.multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
        char ruleLabel[64];
        std::snprintf(ruleLabel, sizeof(ruleLabel), "%s: %s", LOC(STR_MULTI_RULE), winCritName);
        Ui::pill(296.0f + dBar, 9.0f, 96.0f, 18.0f, 0.25f, Ui::rgba(45, 38, 20, 220));
        drawRectOutline(296.0f + dBar, 9.0f, 96.0f, 18.0f, 1.2f, Ui::rgba(255, 195, 45), 0.26f);
        drawCenteredText(ctx.textBuf, 344.0f + dBar, 17.0f, 0.30f, Ui::rgba(255, 205, 50), ruleLabel, 0.30f);

        // 2. Columna Izquierda: 4 Tarjetas de Jugadores (Slots 0..3)
        float dCards = depthShift(2.5f);
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            float cy = 34.0f + i * 44.0f;
            float cx = 10.0f + dCards;
            float cw = 196.0f;
            float ch = 40.0f;

            const auto& p = ctx.multiMgr.players[i];
            if (p.active) {
                u32 borderClr = p.isHost ? Ui::rgba(255, 195, 45) :
                                (p.isLocal ? Ui::rgba(40, 215, 235) :
                                ((i == 2) ? Ui::rgba(180, 80, 240) : Ui::rgba(60, 200, 120)));

                Ui::panel(cx, cy, cw, ch, 0.20f, Ui::rgba(22, 19, 36, 240), borderClr);

                // Nombre de jugador
                drawText(ctx.textBuf, cx + 8.0f, cy + 5.0f, 0.44f, ctx.clrWhite, p.name, 0.25f);

                // Píldora de rol: HOST o TÚ
                if (p.isHost) {
                    Ui::pill(cx + 96.0f, cy + 6.0f, 36.0f, 13.0f, 0.26f, Ui::rgba(255, 130, 20));
                    drawCenteredText(ctx.textBuf, cx + 114.0f, cy + 12.0f, 0.28f, ctx.clrWhite, "HOST", 0.30f);
                } else if (p.isLocal) {
                    Ui::pill(cx + 96.0f, cy + 6.0f, 26.0f, 13.0f, 0.26f, Ui::rgba(40, 215, 235));
                    drawCenteredText(ctx.textBuf, cx + 109.0f, cy + 12.0f, 0.28f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_YOU), 0.30f);
                }

                // Subtítulo: Mods equipados y Ping
                char subStr[48];
                std::string modStr = (p.activeMods != MOD_NONE) ? formatModsString(p.activeMods) : "None";
                std::snprintf(subStr, sizeof(subStr), "+%s . %ums", modStr.c_str(), p.pingMs);
                drawText(ctx.textBuf, cx + 8.0f, cy + 22.0f, 0.32f, ctx.clrMuted, subStr, 0.25f);

                // Píldora de estado en la derecha de la tarjeta
                if (p.status == STATUS_READY) {
                    Ui::pill(cx + cw - 52.0f, cy + 12.0f, 44.0f, 17.0f, 0.26f, Ui::rgba(30, 165, 80));
                    drawCenteredText(ctx.textBuf, cx + cw - 30.0f, cy + 20.0f, 0.32f, ctx.clrWhite, LOC(STR_READY), 0.30f);
                } else if (p.status == STATUS_DOWNLOADING) {
                    char dlPill[16];
                    std::snprintf(dlPill, sizeof(dlPill), "%d%%", p.downloadProgress);
                    Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(220, 120, 20));
                    drawCenteredText(ctx.textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, ctx.clrWhite, dlPill, 0.30f);
                } else if (p.status == STATUS_MISSING_MAP) {
                    Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(215, 50, 60));
                    drawCenteredText(ctx.textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, ctx.clrWhite, "NO MAP", 0.30f);
                } else {
                    Ui::pill(cx + cw - 62.0f, cy + 12.0f, 54.0f, 17.0f, 0.26f, Ui::rgba(45, 40, 60));
                    drawCenteredText(ctx.textBuf, cx + cw - 35.0f, cy + 20.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_WAITING), 0.30f);
                }
            } else {
                Ui::panel(cx, cy, cw, ch, 0.18f, Ui::rgba(16, 14, 26, 170));
                drawRectOutline(cx, cy, cw, ch, 1.0f, Ui::rgba(40, 36, 56), 0.20f);
                drawCenteredText(ctx.textBuf, cx + cw * 0.5f, cy + 19.0f, 0.34f, ctx.clrMuted, LOC(STR_MULTI_SLOT_OPEN), 0.25f);
            }
        }

        // 3. Columna Derecha: Mapa Actual y Fila de Canciones (Map Queue)
        float dRight = depthShift(2.0f);
        float rx = 214.0f + dRight;
        float rw = 176.0f;

        // Tarjeta 1: SIGUIENTE MAPA
        Ui::panel(rx, 34.0f, rw, 80.0f, 0.20f, Ui::rgba(20, 18, 34, 240), ctx.multiMgr.hostChangingSong ? Ui::rgba(255, 185, 45) : Ui::rgba(40, 215, 235));
        if (ctx.multiMgr.hostChangingSong) {
            Ui::glow(rx + rw * 0.5f, 42.0f, 0.22f, 70.0f, Ui::rgba(255, 185, 45, 90), 2);
            drawText(ctx.textBuf, rx + 8.0f, 38.0f, 0.32f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_CHANGING_MAP), 0.25f);
            drawCenteredTextWithShadow(ctx.textBuf, rx + rw * 0.5f, 70.0f, 0.38f, Ui::rgba(255, 215, 60), LOC(STR_MULTI_HOST_CHOOSING), 0.25f);
        } else {
            drawText(ctx.textBuf, rx + 8.0f, 38.0f, 0.32f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_NEXT_MAP), 0.25f);

            char lenStr[16];
            std::snprintf(lenStr, sizeof(lenStr), "%d:%02d", ctx.multiMgr.activeBeatmap.lengthSec / 60, ctx.multiMgr.activeBeatmap.lengthSec % 60);
            drawText(ctx.textBuf, rx + rw - 36.0f, 38.0f, 0.32f, ctx.clrMuted, lenStr, 0.25f);

            std::string dispTitle = ctx.multiMgr.activeBeatmap.title;
            drawText(ctx.textBuf, rx + 8.0f, 52.0f, 0.44f, ctx.clrWhite, truncateString(dispTitle, 16).c_str(), 0.25f);

            std::string dispArtist = ctx.multiMgr.activeBeatmap.artist;
            drawText(ctx.textBuf, rx + 8.0f, 68.0f, 0.34f, ctx.clrMuted, truncateString(dispArtist, 20).c_str(), 0.25f);

            char starsStr[24];
            std::snprintf(starsStr, sizeof(starsStr), "* %.2f", ctx.multiMgr.activeBeatmap.starRating);
            drawText(ctx.textBuf, rx + 8.0f, 86.0f, 0.36f, Ui::rgba(255, 205, 50), starsStr, 0.25f);

            char bpmStr[16];
            std::snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM", ctx.multiMgr.activeBeatmap.bpm);
            drawText(ctx.textBuf, rx + rw - 54.0f, 86.0f, 0.34f, ctx.clrMuted, bpmStr, 0.25f);
        }

        // Tarjeta 2: EN COLA (Map Queue)
        Ui::panel(rx, 120.0f, rw, 90.0f, 0.20f, Ui::rgba(18, 16, 30, 240), Ui::rgba(85, 75, 110));
        char qHeader[24];
        std::snprintf(qHeader, sizeof(qHeader), "%s (%d)", LOC(STR_MULTI_ENQUEUE), static_cast<int>(ctx.multiMgr.mapQueue.size()));
        drawText(ctx.textBuf, rx + 8.0f, 124.0f, 0.32f, Ui::rgba(160, 150, 190), qHeader, 0.25f);

        if (ctx.multiMgr.mapQueue.empty()) {
            drawCenteredTextWithShadow(ctx.textBuf, rx + rw * 0.5f, 150.0f, 0.36f, Ui::rgba(220, 215, 240), LOC(STR_MULTI_QUEUE_EMPTY), 0.25f);
            Ui::pill(rx + 14.0f, 168.0f, rw - 28.0f, 22.0f, 0.24f, Ui::rgba(20, 50, 75, 240));
            drawRectOutline(rx + 14.0f, 168.0f, rw - 28.0f, 22.0f, 1.2f, Ui::rgba(40, 215, 235), 0.26f);
            Ui::pill(rx + 20.0f, 172.0f, 14.0f, 14.0f, 0.27f, Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, rx + 27.0f, 179.0f, 0.28f, Ui::rgba(0, 0, 0, 245), "X", 0.30f);
            drawTextWithShadow(ctx.textBuf, rx + 38.0f, 172.0f, 0.32f, Ui::rgba(180, 240, 255), LOC(STR_MULTI_TAP_TO_ADD), 0.30f);
        } else {
            for (size_t q = 0; q < ctx.multiMgr.mapQueue.size() && q < 3; ++q) {
                float qy = 142.0f + q * 22.0f;
                Ui::panel(rx + 6.0f, qy, rw - 12.0f, 18.0f, 0.22f, Ui::rgba(26, 22, 42, 220));
                char qItemStr[48];
                std::snprintf(qItemStr, sizeof(qItemStr), "#%u %s", static_cast<unsigned int>(q + 2), ctx.multiMgr.mapQueue[q].title.c_str());
                drawTextWithShadow(ctx.textBuf, rx + 10.0f, qy + 2.0f, 0.32f, ctx.clrWhite, truncateString(qItemStr, 22).c_str(), 0.26f);
            }
        }

        // Barra inferior de atajos de control o Toast
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::glow(200.0f + dToast, 224.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
            Ui::pill(30.0f + dToast, 212.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
            drawRectOutline(30.0f + dToast, 212.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        } else {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 10.0f + dBar, byY, "A", ctx.multiMgr.isHost() ? LOC(STR_MULTI_START_MATCH) : LOC(STR_READY), Ui::rgba(255, 75, 80));
            drawKeyBadge(ctx.textBuf, 88.0f + dBar, byY, "Y", "Mods", Ui::rgba(50, 215, 115));
            drawKeyBadge(ctx.textBuf, 158.0f + dBar, byY, "X", LOC(STR_MULTI_ENQUEUE), Ui::rgba(50, 165, 255));
            drawKeyBadge(ctx.textBuf, 226.0f + dBar, byY, "B", LOC(STR_BACK), Ui::rgba(255, 195, 45));

            Ui::pill(296.0f + dBar, byY - 1.0f, 32.0f, 15.0f, 0.30f, Ui::rgba(24, 60, 85));
            drawCenteredText(ctx.textBuf, 312.0f + dBar, byY + 6.5f, 0.28f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_ROLE), 0.32f);
            drawTextWithShadow(ctx.textBuf, 334.0f + dBar, byY, 0.34f, ctx.clrWhite, ctx.multiMgr.isHost() ? "Host" : LOC(STR_MULTI_CLIENT), 0.32f);
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // PANTALLA INFERIOR (PANEL TÁCTIL DEL LOBBY)
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.02f);

    if (ctx.multiQueuePickerOpen) {
        Ui::panel(10.0f, 8.0f, 300.0f, 224.0f, 0.15f, Ui::rgba(16, 14, 28, 250), ctx.clrPrimary);
        const char* pickerTitle = ctx.multiMgr.isHost()
            ? ((ctx.multiPickerAction == PICKER_ACTION_QUEUE) ? LOC(STR_MULTI_ADD_QUEUE) : LOC(STR_MULTI_CHANGE_MAP))
            : LOC(STR_MULTI_SUGGEST_MAP);
        drawCenteredText(ctx.textBuf, 160.0f, 18.0f, 0.40f, ctx.clrWhite, pickerTitle, 0.20f);

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
            drawCenteredTextWithShadow(ctx.textBuf, 84.0f, 212.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_CHANGE_MAP), 0.25f);

            Ui::button(166.0f, 198.0f, 140.0f, 28.0f, 0.20f, Ui::rgba(40, 180, 220), Ui::rgba(160, 230, 255), false);
            drawCenteredTextWithShadow(ctx.textBuf, 236.0f, 212.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
        } else {
            Ui::button(20.0f, 198.0f, 280.0f, 28.0f, 0.20f, Ui::rgba(180, 80, 240), Ui::rgba(220, 160, 255), false);
            drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 212.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_SUGGEST_MAP), 0.25f);
        }
    } else {
        drawTextWithShadow(ctx.textBuf, 12.0f, 10.0f, 0.42f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_TOUCH_PANEL), 0.20f);
        if (ctx.multiMgr.isHost()) {
            Ui::pill(248.0f, 6.0f, 60.0f, 17.0f, 0.25f, Ui::rgba(255, 140, 20));
            drawCenteredText(ctx.textBuf, 278.0f, 14.5f, 0.30f, ctx.clrWhite, "HOST", 0.30f);
        } else {
            Ui::pill(242.0f, 6.0f, 66.0f, 17.0f, 0.25f, Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, 275.0f, 14.5f, 0.30f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_CLIENT), 0.30f);
        }

        drawTextWithShadow(ctx.textBuf, 12.0f, 28.0f, 0.33f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_WIN_CRITERIA), 0.20f);
        const char* critText = (ctx.multiMgr.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                               ((ctx.multiMgr.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
        drawTextWithShadow(ctx.textBuf, 214.0f, 28.0f, 0.35f, Ui::rgba(40, 215, 235), critText, 0.20f);

        // 3 Pestañas interactivas de regla de victoria
        const char* tabNames[3] = { LOC(STR_MULTI_WIN_SCORE), LOC(STR_MULTI_WIN_ACC), LOC(STR_MULTI_WIN_COMBO) };
        for (int t = 0; t < 3; ++t) {
            float tx = 14.0f + t * 98.0f;
            float ty = 46.0f;
            float tw = 92.0f;
            float th = 26.0f;
            bool activeTab = (static_cast<int>(ctx.multiMgr.winCondition) == t);

            u32 tabBg = activeTab ? Ui::rgba(40, 215, 235) : Ui::rgba(26, 22, 40, 240);
            u32 tabTxt = activeTab ? Ui::rgba(10, 8, 18, 255) : Ui::rgba(200, 195, 225);
            Ui::panel(tx, ty, tw, th, 0.20f, tabBg, activeTab ? Ui::rgba(255, 255, 255) : 0);
            if (activeTab) {
                Ui::glow(tx + tw * 0.5f, ty + th * 0.5f, 0.18f, tw * 0.45f, Ui::rgba(40, 215, 235, 80), 2);
            }
            drawCenteredTextWithShadow(ctx.textBuf, tx + tw * 0.5f, ty + th * 0.5f, 0.34f, tabTxt, tabNames[t], 0.25f);
        }

        if (ctx.multiMgr.isHost()) {
            // Tarjeta 1: Modificadores (x: 10..104, y: 82..148)
            Ui::panel(10.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
            drawTextWithShadow(ctx.textBuf, 14.0f, 88.0f, 0.34f, ctx.clrWhite, "MODS", 0.25f);
            std::string myMods = (ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods != MOD_NONE)
                ? ("+" + formatModsString(ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods)) : "None";
            Ui::pill(14.0f, 108.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, 57.0f, 116.0f, 0.28f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 10).c_str(), 0.30f);
            drawCenteredTextWithShadow(ctx.textBuf, 57.0f, 134.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_MODS_MENU), 0.25f);

            // Tarjeta 2: Cambiar Canción Actual (x: 113..207, y: 82..148)
            bool clientDownloading = ctx.multiMgr.isAnyClientDownloading();
            u32 c2Border = clientDownloading ? Ui::rgba(140, 50, 60) : Ui::rgba(255, 140, 20);
            Ui::panel(113.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), c2Border);
            drawTextWithShadow(ctx.textBuf, 117.0f, 88.0f, 0.34f, clientDownloading ? ctx.clrMiss : ctx.clrWhite, LOC(STR_MULTI_CHANGE_MAP), 0.25f);
            Ui::pill(117.0f, 108.0f, 86.0f, 16.0f, 0.26f, c2Border);
            drawCenteredText(ctx.textBuf, 160.0f, 116.0f, 0.28f, ctx.clrWhite, clientDownloading ? LOC(STR_MULTI_BLOCKED) : LOC(STR_MULTI_ACTIVE_MAP), 0.30f);
            drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 134.0f, 0.30f, clientDownloading ? ctx.clrMiss : Ui::rgba(215, 210, 235), clientDownloading ? LOC(STR_MULTI_DL_ACTIVE) : LOC(STR_MULTI_TAP_CHANGE), 0.25f);

            // Tarjeta 3: + Añadir a la Cola (x: 216..310, y: 82..148)
            Ui::panel(216.0f, 82.0f, 94.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(180, 80, 240));
            drawTextWithShadow(ctx.textBuf, 220.0f, 88.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
            Ui::pill(220.0f, 108.0f, 86.0f, 16.0f, 0.26f, Ui::rgba(180, 80, 240));
            drawCenteredText(ctx.textBuf, 263.0f, 116.0f, 0.28f, ctx.clrWhite, LOC(STR_MULTI_ENQUEUE), 0.30f);
            drawCenteredTextWithShadow(ctx.textBuf, 263.0f, 134.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_TAP_QUEUE), 0.25f);
        } else {
            // Tarjeta 1: Modificadores (Cliente)
            Ui::panel(12.0f, 82.0f, 142.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(40, 215, 235));
            drawText(ctx.textBuf, 20.0f, 88.0f, 0.36f, ctx.clrWhite, "MODS", 0.25f);
            std::string myMods = (ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods != MOD_NONE)
                ? ("+" + formatModsString(ctx.multiMgr.players[ctx.multiMgr.localSlot].activeMods)) : "None";
            Ui::pill(92.0f, 86.0f, 56.0f, 15.0f, 0.26f, Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, 120.0f, 93.0f, 0.26f, Ui::rgba(0, 0, 0, 240), truncateString(myMods, 7).c_str(), 0.30f);
            drawTextWithShadow(ctx.textBuf, 20.0f, 114.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_TAP_OPEN), 0.25f);
            drawTextWithShadow(ctx.textBuf, 20.0f, 128.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_MODS_MENU), 0.25f);

            // Tarjeta 2: + Añadir Canción (Cola Cliente)
            Ui::panel(166.0f, 82.0f, 142.0f, 66.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(180, 80, 240));
            drawTextWithShadow(ctx.textBuf, 174.0f, 88.0f, 0.36f, ctx.clrWhite, LOC(STR_MULTI_ADD_QUEUE), 0.25f);
            Ui::pill(268.0f, 86.0f, 34.0f, 15.0f, 0.26f, Ui::rgba(180, 80, 240));
            drawCenteredText(ctx.textBuf, 285.0f, 93.0f, 0.26f, ctx.clrWhite, LOC(STR_MULTI_ENQUEUE), 0.30f);
            drawTextWithShadow(ctx.textBuf, 174.0f, 114.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_SUGGEST_MAP), 0.25f);
            drawTextWithShadow(ctx.textBuf, 174.0f, 128.0f, 0.30f, Ui::rgba(215, 210, 235), LOC(STR_MULTI_FOR_ROOM), 0.25f);
        }

        char statusSummary[64];
        std::snprintf(statusSummary, sizeof(statusSummary), "%s: %d/%d", LOC(STR_READY), ctx.multiMgr.getReadyPlayerCount(), ctx.multiMgr.getActivePlayerCount());
        drawTextWithShadow(ctx.textBuf, 14.0f, 158.0f, 0.34f, ctx.clrWhite, statusSummary, 0.25f);

        const char* subStat = (ctx.multiMgr.hostChangingSong) ? LOC(STR_MULTI_CHANGING_MAP) :
                              (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) ? LOC(STR_MULTI_DOWNLOAD_MAP) :
                              (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_DOWNLOADING) ? LOC(STR_MULTI_DL_ACTIVE) :
                              (ctx.multiMgr.areAllPlayersReady()) ? LOC(STR_MULTI_ALL_READY) :
                              (ctx.multiMgr.isHost() ? LOC(STR_MULTI_WAITING_PLAYERS) : LOC(STR_MULTI_WAITING_HOST));
        drawTextWithShadow(ctx.textBuf, 168.0f, 158.0f, 0.30f, Ui::rgba(40, 215, 235), subStat, 0.25f);

        float btnX = 12.0f;
        float btnY = 176.0f;
        float btnW = 296.0f;
        float btnH = 50.0f;

        if (!hasLocalMap || ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_MISSING_MAP) {
            Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.16f, btnW * 0.52f, Ui::rgba(20, 160, 230, 90), 3);
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(20, 135, 215, 255), Ui::rgba(160, 230, 255));
            drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, ctx.clrWhite, LOC(STR_MULTI_DL_MAP_TAP), 0.30f);
        } else if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_DOWNLOADING) {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(220, 110, 20, 255), Ui::rgba(255, 180, 60));
            float prog = ctx.multiMgr.players[ctx.multiMgr.localSlot].downloadProgress / 100.0f;
            if (prog > 0.02f) {
                C2D_DrawRectSolid(btnX + 4.0f, btnY + 4.0f, 0.22f, (btnW - 8.0f) * std::min(1.0f, prog), btnH - 8.0f, Ui::rgba(255, 155, 30, 200));
            }
            char dlTxt[48];
            std::snprintf(dlTxt, sizeof(dlTxt), "%s (%d%%)...", LOC(STR_MULTI_DL_ACTIVE), ctx.multiMgr.players[ctx.multiMgr.localSlot].downloadProgress);
            drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, ctx.clrWhite, dlTxt, 0.30f);
        } else if (ctx.multiMgr.hostChangingSong && !ctx.multiMgr.isHost()) {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(45, 38, 55, 240), Ui::rgba(180, 130, 40));
            drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.42f, Ui::rgba(255, 205, 50), LOC(STR_MULTI_CHANGING_MAP), 0.30f);
        } else if (ctx.multiMgr.isHost()) {
            if (ctx.multiMgr.areAllPlayersReady()) {
                Ui::glow(btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.18f, btnW * 0.55f, Ui::rgba(30, 215, 90, 80), 4);
                Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(30, 195, 80, 255), Ui::rgba(180, 255, 200));
                drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.52f, ctx.clrWhite, LOC(STR_MULTI_START_MATCH), 0.30f);
            } else {
                Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(45, 40, 60, 240), Ui::rgba(85, 75, 110));
                drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.44f, ctx.clrMuted, LOC(STR_MULTI_WAITING_PLAYERS), 0.30f);
            }
        } else if (ctx.multiMgr.players[ctx.multiMgr.localSlot].status == STATUS_READY) {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(28, 75, 50, 240), Ui::rgba(40, 215, 120));
            drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, Ui::rgba(180, 255, 210), LOC(STR_MULTI_UNREADY), 0.30f);
        } else {
            Ui::panel(btnX, btnY, btnW, btnH, 0.20f, Ui::rgba(30, 160, 75, 240), Ui::rgba(100, 255, 160));
            drawCenteredText(ctx.textBuf, btnX + btnW * 0.5f, btnY + btnH * 0.5f, 0.46f, ctx.clrWhite, LOC(STR_MULTI_READY_TAP), 0.30f);
        }
    }

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
