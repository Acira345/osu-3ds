#include "MultiBrowserScreen.hpp"

void runMultiBrowserScreen(AppContext& ctx) {
    ctx.audio.update();
    if (ctx.settings.songPreview && !ctx.audio.isPreviewLoading() && ctx.audio.isSongFinished()) {
        ctx.rotateMenuSong();
    }
    ctx.multiMgr.updateBrowser();

    // 1. Controles Físicos
    if (ctx.kDown & KEY_B) {
        ctx.audio.playHitSound(0);
        ctx.audio.setLoop(false);
        ctx.multiMgr.stop();
        ctx.appState = STATE_MAIN_MENU;
        return;
    }
    if (ctx.kDown & KEY_SELECT) {
        ctx.audio.playHitSound(1);
        ctx.multiMgr.toggleNetworkMode();
    }
    if (ctx.kDown & KEY_Y) {
        ctx.audio.playHitSound(1);
        ctx.multiMgr.refreshBrowser();
    }
    if (ctx.kDown & KEY_X) {
        ctx.audio.playHitSound(2);
        QueuedBeatmap initMap;
        if (!ctx.songMgr.sets.empty()) {
            const auto& s = ctx.songMgr.sets[ctx.selectedSet % ctx.songMgr.sets.size()];
            if (!s.difficulties.empty()) {
                const auto& d = s.difficulties[ctx.selectedDiff % s.difficulties.size()];
                initMap = QueuedBeatmap(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, 0);
            }
        }
        ctx.multiMgr.startHost(ctx.sysUser, initMap);
        ctx.appState = STATE_MULTI_LOBBY;
        return;
    }
    if (ctx.kDown & (KEY_DUP | KEY_CPAD_UP)) {
        if (ctx.multiMgr.selectedRoomIndex > 0) {
            ctx.multiMgr.selectedRoomIndex--;
            ctx.audio.playHitSound(0);
        }
    }
    if (ctx.kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        if (ctx.multiMgr.selectedRoomIndex + 1 < static_cast<int>(ctx.multiMgr.discoveredRooms.size())) {
            ctx.multiMgr.selectedRoomIndex++;
            ctx.audio.playHitSound(0);
        }
    }
    if (ctx.kDown & KEY_A) {
        if (!ctx.multiMgr.discoveredRooms.empty()) {
            if (ctx.multiMgr.joinSelectedRoom()) {
                ctx.audio.playHitSound(2);
                ctx.appState = STATE_MULTI_LOBBY;
                return;
            }
        }
    }

    // 2. Entradas Táctiles
    if (ctx.touchJustDown) {
        // Botón Modo de Red (UDS vs LAN) en la barra superior táctil
        if (ctx.touch.px >= 135.0f && ctx.touch.px <= 220.0f && ctx.touch.py >= 4.0f && ctx.touch.py <= 28.0f) {
            ctx.audio.playHitSound(1);
            ctx.multiMgr.toggleNetworkMode();
        }

        // Botón Refrescar en la esquina superior derecha
        if (ctx.touch.px >= 225.0f && ctx.touch.px <= 312.0f && ctx.touch.py >= 4.0f && ctx.touch.py <= 28.0f) {
            ctx.audio.playHitSound(1);
            ctx.multiMgr.refreshBrowser();
        }

        // Tocar tarjetas de salas en la lista
        int startIdx = 0;
        if (ctx.multiMgr.selectedRoomIndex >= 3) startIdx = ctx.multiMgr.selectedRoomIndex - 2;
        for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++k) {
            float cy = 34.0f + k * 48.0f;
            if (ctx.touch.px >= 10.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= cy && ctx.touch.py <= cy + 44.0f) {
                int idx = startIdx + k;
                if (ctx.multiMgr.selectedRoomIndex == idx) {
                    if (ctx.multiMgr.joinSelectedRoom()) {
                        ctx.audio.playHitSound(2);
                        ctx.appState = STATE_MULTI_LOBBY;
                        break;
                    }
                } else {
                    ctx.multiMgr.selectedRoomIndex = idx;
                    ctx.audio.playHitSound(1);
                }
            }
        }
        if (ctx.appState == STATE_MULTI_LOBBY) return;

        // Botón Volver [B] (inferior izquierdo)
        if (ctx.touch.px >= 10.0f && ctx.touch.px <= 74.0f && ctx.touch.py >= 186.0f && ctx.touch.py <= 234.0f) {
            ctx.audio.playHitSound(0);
            ctx.multiMgr.stop();
            ctx.appState = STATE_MAIN_MENU;
            return;
        }

        // Botón + CREAR SALA NUEVA [X] (inferior derecho)
        if (ctx.touch.px >= 82.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= 186.0f && ctx.touch.py <= 234.0f) {
            ctx.audio.playHitSound(2);
            QueuedBeatmap initMap;
            if (!ctx.songMgr.sets.empty()) {
                const auto& s = ctx.songMgr.sets[ctx.selectedSet % ctx.songMgr.sets.size()];
                if (!s.difficulties.empty()) {
                    const auto& d = s.difficulties[ctx.selectedDiff % s.difficulties.size()];
                    initMap = QueuedBeatmap(s.title, s.artist, d.version, d.creator, s.folderPath, d.osuPath, d.audioPath, d.starRating, d.getBPM(), 90, 0);
                }
            }
            ctx.multiMgr.startHost(ctx.sysUser, initMap);
            ctx.appState = STATE_MULTI_LOBBY;
            return;
        }
    }

    // 3. Renderizado Gráfico (Top & Bottom Screens)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    for (int eye = 0; eye < (ctx.stereoActive ? 2 : 1); ++eye) {
        C3D_RenderTarget* curTop = (eye == 0) ? ctx.topScreen : ctx.topScreenRight;
        float eyeSign = ctx.stereoActive ? ((eye == 0) ? 1.0f : -1.0f) : 0.0f;
        C2D_TargetClear(curTop, ctx.clrBgDark);
        C2D_SceneBegin(curTop);
        auto depthShift = [&](float px) { return eyeSign * ctx.slider3D * px; };

        Ui::backdrop(400.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

        // Barra Superior
        float dBar = depthShift(1.8f);
        Ui::pill(10.0f + dBar, 8.0f, 62.0f, 18.0f, 0.25f, Ui::rgba(255, 102, 170));
        drawCenteredText(ctx.textBuf, 41.0f + dBar, 16.0f, 0.36f, ctx.clrWhite, "osu!3DS", 0.30f);

        const char* topTitle = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
            ? LOC(STR_MULTI_ROOMS_UDS)
            : LOC(STR_MULTI_ROOMS_LAN);
        drawText(ctx.textBuf, 78.0f + dBar, 10.0f, 0.44f, ctx.clrWhite, topTitle, 0.30f);

        char roomBadge[32];
        std::snprintf(roomBadge, sizeof(roomBadge), LOC(STR_MULTI_ROOMS_COUNT), static_cast<int>(ctx.multiMgr.discoveredRooms.size()));
        Ui::pill(280.0f + dBar, 8.0f, 58.0f, 18.0f, 0.25f, Ui::rgba(40, 215, 235));
        drawCenteredText(ctx.textBuf, 309.0f + dBar, 16.0f, 0.30f, Ui::rgba(0, 0, 0, 240), roomBadge, 0.30f);

        Ui::pill(344.0f + dBar, 8.0f, 48.0f, 18.0f, 0.25f, Ui::rgba(30, 25, 45, 220));
        renderWifiBars(358.0f + dBar, 10.0f, 3, Ui::rgba(40, 215, 235), Ui::rgba(80, 80, 100));

        // Si no hay salas descubiertas
        if (ctx.multiMgr.discoveredRooms.empty()) {
            float radAnim = 16.0f + 12.0f * (0.5f + 0.5f * std::sin(ctx.animTick * 0.08f));
            float cx = 200.0f + depthShift(1.5f);
            float cy = 88.0f;
            Ui::glow(cx, cy, 0.15f, radAnim * 2.2f, Ui::rgba(40, 215, 235, 60), 4);
            Ui::panel(cx - 24.0f, cy - 24.0f, 48.0f, 48.0f, 0.20f, Ui::rgba(25, 22, 42, 235), Ui::rgba(40, 215, 235));
            renderWifiBars(cx - 9.0f, cy - 6.0f, 3, Ui::rgba(40, 215, 235), Ui::rgba(70, 70, 95));

            if (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS) {
                drawCenteredText(ctx.textBuf, cx, 126.0f, 0.42f, ctx.clrWhite, LOC(STR_MULTI_SEARCH_UDS), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 146.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_NO_NET_REQ), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 166.0f, 0.33f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 182.0f, 0.29f, ctx.clrMuted, LOC(STR_MULTI_ANY_JOIN), 0.25f);
            } else {
                drawCenteredText(ctx.textBuf, cx, 126.0f, 0.42f, ctx.clrWhite, LOC(STR_MULTI_SEARCH_LAN), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 146.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_NO_ACTIVE_ROOMS), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 166.0f, 0.33f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 182.0f, 0.29f, ctx.clrMuted, LOC(STR_MULTI_ROOM_WILL_APPEAR), 0.25f);
            }
        } else {
            // Columna Izquierda: Tarjetas de Salas (hasta 3 visibles)
            int startIdx = 0;
            if (ctx.multiMgr.selectedRoomIndex >= 3) startIdx = ctx.multiMgr.selectedRoomIndex - 2;
            for (int r = 0; r < 3 && (startIdx + r) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++r) {
                int idx = startIdx + r;
                const auto& rm = ctx.multiMgr.discoveredRooms[idx];
                bool isSel = (idx == ctx.multiMgr.selectedRoomIndex);
                float ry = 32.0f + r * 56.0f;
                float rx = 10.0f + depthShift(isSel ? 3.0f : 1.5f);
                float rw = 224.0f;
                float rh = 52.0f;

                u32 borderClr = isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(50, 44, 72);
                u32 bgClr = isSel ? Ui::rgba(36, 26, 54, 245) : Ui::rgba(22, 19, 36, 230);
                Ui::panel(rx, ry, rw, rh, 0.20f, bgClr, borderClr);

                if (isSel) {
                    Ui::pill(rx + 4.0f, ry + 6.0f, 4.0f, rh - 12.0f, 0.25f, Ui::rgba(255, 102, 170));
                }

                drawText(ctx.textBuf, rx + 14.0f, ry + 6.0f, 0.38f, ctx.clrWhite, truncateString(rm.roomTitle, 22).c_str(), 0.25f);

                char subLine[64];
                std::snprintf(subLine, sizeof(subLine), "Host: %s . %s", rm.hostName.c_str(), truncateString(rm.mapTitle, 16).c_str());
                drawText(ctx.textBuf, rx + 14.0f, ry + 22.0f, 0.30f, ctx.clrMuted, subLine, 0.25f);

                char starsBuf[16];
                std::snprintf(starsBuf, sizeof(starsBuf), "%.2f*", rm.mapStars);
                u32 sClr = getStarRatingColor(rm.mapStars);
                Ui::pill(rx + 14.0f, ry + 36.0f, 40.0f, 11.0f, 0.26f, sClr);
                drawCenteredText(ctx.textBuf, rx + 34.0f, ry + 40.0f, 0.24f, Ui::rgba(0, 0, 0, 240), starsBuf, 0.28f);

                char plyBuf[16];
                std::snprintf(plyBuf, sizeof(plyBuf), "%d/%d", rm.playerCount, rm.maxPlayers);
                u32 pClr = (rm.playerCount >= rm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(30, 195, 80);
                Ui::pill(rx + rw - 44.0f, ry + 10.0f, 36.0f, 15.0f, 0.26f, pClr);
                drawCenteredText(ctx.textBuf, rx + rw - 26.0f, ry + 16.5f, 0.28f, ctx.clrWhite, plyBuf, 0.28f);
            }

            // Columna Derecha: Panel de Detalles de la Sala Seleccionada
            float px = 240.0f + depthShift(2.0f);
            float py = 32.0f;
            float pw = 150.0f;
            float ph = 168.0f;
            Ui::panel(px, py, pw, ph, 0.20f, Ui::rgba(22, 19, 36, 245), Ui::rgba(40, 215, 235));

            const auto& selRm = ctx.multiMgr.discoveredRooms[ctx.multiMgr.selectedRoomIndex];
            drawText(ctx.textBuf, px + 8.0f, py + 8.0f, 0.36f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_DETAILS), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 26.0f, 0.30f, ctx.clrWhite, ("Host: " + truncateString(selRm.hostName, 12)).c_str(), 0.25f);

            const char* wcStr = (selRm.winCondition == WIN_SCORE) ? LOC(STR_SCORE) :
                                ((selRm.winCondition == WIN_ACCURACY) ? LOC(STR_ACCURACY) : LOC(STR_MAX_COMBO));
            drawText(ctx.textBuf, px + 8.0f, py + 42.0f, 0.28f, ctx.clrMuted, (std::string(LOC(STR_MULTI_RULE)) + ": " + std::string(wcStr)).c_str(), 0.25f);

            const char* mmStr = (selRm.modMode == MODS_HOST_ONLY) ? "Host Only" : "FreeMod";
            drawText(ctx.textBuf, px + 8.0f, py + 56.0f, 0.28f, ctx.clrMuted, (std::string(LOC(STR_MULTI_MODS_LABEL)) + ": " + std::string(mmStr)).c_str(), 0.25f);

            drawText(ctx.textBuf, px + 8.0f, py + 74.0f, 0.30f, Ui::rgba(255, 195, 45), LOC(STR_MULTI_MAP_LABEL), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 88.0f, 0.29f, ctx.clrWhite, truncateString(selRm.mapTitle, 15).c_str(), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 102.0f, 0.26f, ctx.clrMuted, truncateString(selRm.mapArtist, 17).c_str(), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 116.0f, 0.26f, ctx.clrMuted, truncateString(selRm.mapVersion, 17).c_str(), 0.25f);

            char pingStr[24];
            std::snprintf(pingStr, sizeof(pingStr), (selRm.mode == MultiNetworkMode::NET_MODE_UDS) ? "Ping UDS: %d ms" : "Ping LAN: %d ms", selRm.pingMs);
            drawText(ctx.textBuf, px + 8.0f, py + 138.0f, 0.27f, Ui::rgba(70, 220, 120), pingStr, 0.25f);

            const char* joinHint = (selRm.playerCount >= selRm.maxPlayers) ? LOC(STR_MULTI_ROOM_FULL) : LOC(STR_MULTI_JOIN);
            u32 hintClr = (selRm.playerCount >= selRm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(40, 215, 235);
            drawCenteredText(ctx.textBuf, px + pw * 0.5f, py + 154.0f, 0.30f, hintClr, joinHint, 0.25f);
        }

        // Barra Inferior de Atajos o Toast
        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::glow(200.0f + dToast, 218.0f, 0.88f, 160.0f, Ui::rgba(255, 60, 80, 110), 3);
            Ui::pill(30.0f + dToast, 206.0f, 340.0f, 24.0f, 0.90f, Ui::rgba(35, 12, 22, 245));
            drawRectOutline(30.0f + dToast, 206.0f, 340.0f, 24.0f, 1.2f, Ui::rgba(255, 80, 100), 0.92f);
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 217.0f, 0.35f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        } else {
            float byY = 219.0f;
            drawKeyBadge(ctx.textBuf, 12.0f + dBar, byY, "A", LOC(STR_MULTI_JOIN), Ui::rgba(50, 165, 255));
            drawKeyBadge(ctx.textBuf, 88.0f + dBar, byY, "X", LOC(STR_MULTI_CREATE_ROOM), Ui::rgba(50, 215, 115));
            drawKeyBadge(ctx.textBuf, 156.0f + dBar, byY, "Y", LOC(STR_SEARCH), Ui::rgba(255, 180, 40));
            drawKeyBadge(ctx.textBuf, 228.0f + dBar, byY, "B", LOC(STR_BACK), Ui::rgba(255, 75, 80));
        }
    }

    // Renderizado Pantalla Inferior (Táctil)
    C2D_TargetClear(ctx.bottomScreen, ctx.clrBgDark);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(18, 14, 30), Ui::rgba(10, 8, 18), ctx.clrPrimary, 0.01f);

    const char* bottomTitle = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
        ? LOC(STR_MULTI_ROOMS_UDS)
        : LOC(STR_MULTI_ROOMS_LAN);
    drawText(ctx.textBuf, 10.0f, 8.0f, 0.36f, ctx.clrWhite, bottomTitle, 0.25f);

    // Botón Alternar Modo (UDS / LAN)
    bool isModeUds = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS);
    Ui::panel(148.0f, 4.0f, 74.0f, 22.0f, 0.20f, Ui::rgba(28, 24, 44, 240), isModeUds ? Ui::rgba(40, 215, 235) : Ui::rgba(255, 150, 40));
    drawCenteredText(ctx.textBuf, 185.0f, 13.0f, 0.28f, isModeUds ? Ui::rgba(40, 215, 235) : Ui::rgba(255, 150, 40), isModeUds ? (std::string(LOC(STR_MODE)) + ": UDS").c_str() : (std::string(LOC(STR_MODE)) + ": LAN").c_str(), 0.25f);

    // Botón Refrescar
    Ui::panel(226.0f, 4.0f, 84.0f, 22.0f, 0.20f, Ui::rgba(30, 26, 46, 240), Ui::rgba(40, 215, 235));
    drawCenteredText(ctx.textBuf, 268.0f, 13.0f, 0.29f, Ui::rgba(40, 215, 235), LOC(STR_SEARCH), 0.25f);

    // Contenedor de la lista de salas
    if (ctx.multiMgr.discoveredRooms.empty()) {
        Ui::panel(10.0f, 36.0f, 300.0f, 138.0f, 0.20f, Ui::rgba(22, 19, 36, 240), Ui::rgba(45, 40, 65));
        drawCenteredText(ctx.textBuf, 160.0f, 75.0f, 0.38f, ctx.clrWhite, LOC(STR_MULTI_NO_ROOMS_FOUND), 0.25f);
        drawCenteredText(ctx.textBuf, 160.0f, 98.0f, 0.31f, ctx.clrMuted, LOC(STR_MULTI_CREATE_ROOM_TOUCH), 0.25f);
        drawCenteredText(ctx.textBuf, 160.0f, 116.0f, 0.31f, Ui::rgba(40, 215, 235), LOC(STR_MULTI_ANY_3DS_JOIN), 0.25f);
    } else {
        int startIdx = 0;
        if (ctx.multiMgr.selectedRoomIndex >= 3) startIdx = ctx.multiMgr.selectedRoomIndex - 2;
        for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++k) {
            int idx = startIdx + k;
            const auto& rm = ctx.multiMgr.discoveredRooms[idx];
            bool isSel = (idx == ctx.multiMgr.selectedRoomIndex);
            float cy = 34.0f + k * 48.0f;
            float cx = 10.0f;
            float cw = 300.0f;
            float ch = 44.0f;

            u32 bClr = isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(50, 44, 72);
            u32 bg = isSel ? Ui::rgba(38, 28, 56, 245) : Ui::rgba(22, 19, 36, 230);
            Ui::panel(cx, cy, cw, ch, 0.20f, bg, bClr);

            if (isSel) {
                Ui::pill(cx + 4.0f, cy + 5.0f, 4.0f, ch - 10.0f, 0.25f, Ui::rgba(255, 102, 170));
            }

            drawText(ctx.textBuf, cx + 14.0f, cy + 5.0f, 0.38f, ctx.clrWhite, truncateString(rm.roomTitle, 24).c_str(), 0.25f);

            char subTxt[64];
            std::snprintf(subTxt, sizeof(subTxt), "%s Host: %s . %s", (rm.mode == MultiNetworkMode::NET_MODE_UDS) ? "[UDS]" : "[LAN]", rm.hostName.c_str(), truncateString(rm.mapTitle, 16).c_str());
            drawText(ctx.textBuf, cx + 14.0f, cy + 22.0f, 0.30f, ctx.clrMuted, subTxt, 0.25f);

            char pTxt[16];
            std::snprintf(pTxt, sizeof(pTxt), "%d/%d", rm.playerCount, rm.maxPlayers);
            Ui::pill(cx + cw - 52.0f, cy + 12.0f, 42.0f, 20.0f, 0.26f, isSel ? Ui::rgba(255, 102, 170) : Ui::rgba(40, 215, 235));
            drawCenteredText(ctx.textBuf, cx + cw - 31.0f, cy + 20.0f, 0.30f, isSel ? ctx.clrWhite : Ui::rgba(0, 0, 0, 240), isSel ? LOC(STR_MULTI_JOIN) : pTxt, 0.28f);
        }
    }

    // Barra de Acción Inferior: VOLVER y + CREAR SALA NUEVA estilo multijugador
    Ui::button(10.0f, 186.0f, 68.0f, 44.0f, 0.20f, Ui::rgba(32, 26, 44, 240), Ui::rgba(65, 55, 85), false);
    Ui::pill(16.0f, 192.0f, 18.0f, 18.0f, 0.24f, Ui::rgba(65, 55, 85));
    drawCenteredText(ctx.textBuf, 25.0f, 201.0f, 0.28f, ctx.clrWhite, "B", 0.28f);
    drawTextWithShadow(ctx.textBuf, 38.0f, 193.0f, 0.32f, ctx.clrMuted, LOC(STR_BACK), 0.28f);
    drawTextWithShadow(ctx.textBuf, 38.0f, 207.0f, 0.26f, ctx.clrMuted, LOC(STR_MENU_SHORT), 0.28f);

    Ui::button(84.0f, 186.0f, 226.0f, 44.0f, 0.20f, Ui::rgba(25, 140, 65, 250), Ui::rgba(140, 255, 180), true);
    Ui::pill(92.0f, 196.0f, 24.0f, 22.0f, 0.25f, Ui::rgba(140, 255, 180));
    drawCenteredText(ctx.textBuf, 104.0f, 207.0f, 0.34f, Ui::rgba(10, 45, 20, 245), "X", 0.30f);
    drawTextWithShadow(ctx.textBuf, 124.0f, 197.0f, 0.44f, ctx.clrWhite, LOC(STR_MULTI_CREATE_ROOM), 0.30f);

    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    C3D_FrameEnd(0);
}
