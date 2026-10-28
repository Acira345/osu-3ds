#include "MultiBrowserScreen.hpp"
#include "backend/i18n/Localization.hpp"

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
        // Selector de Modo de Red (UDS vs LAN) en la barra táctil superior
        if (ctx.touch.py >= 32.0f && ctx.touch.py <= 58.0f) {
            if (ctx.touch.px >= 10.0f && ctx.touch.px <= 155.0f) {
                if (ctx.multiMgr.netMode != MultiNetworkMode::NET_MODE_UDS) {
                    ctx.audio.playHitSound(1);
                    ctx.multiMgr.toggleNetworkMode();
                }
            } else if (ctx.touch.px >= 165.0f && ctx.touch.px <= 310.0f) {
                if (ctx.multiMgr.netMode != MultiNetworkMode::NET_MODE_LAN) {
                    ctx.audio.playHitSound(1);
                    ctx.multiMgr.toggleNetworkMode();
                }
            }
        }

        // Tocar botón refrescar
        if (ctx.touch.px >= 240.0f && ctx.touch.px <= 312.0f && ctx.touch.py >= 4.0f && ctx.touch.py <= 28.0f) {
            ctx.audio.playHitSound(1);
            ctx.multiMgr.refreshBrowser();
        }

        // Tocar tarjetas de salas en la lista
        int startIdx = 0;
        if (ctx.multiMgr.selectedRoomIndex >= 3) startIdx = ctx.multiMgr.selectedRoomIndex - 2;
        for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++k) {
            float cy = 64.0f + k * 42.0f;
            if (ctx.touch.px >= 10.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= cy && ctx.touch.py <= cy + 38.0f) {
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
        if (ctx.touch.px >= 10.0f && ctx.touch.px <= 85.0f && ctx.touch.py >= 196.0f && ctx.touch.py <= 234.0f) {
            ctx.audio.playHitSound(0);
            ctx.multiMgr.stop();
            ctx.appState = STATE_MAIN_MENU;
            return;
        }

        // Botón + CREAR SALA NUEVA [X] (inferior derecho)
        if (ctx.touch.px >= 95.0f && ctx.touch.px <= 310.0f && ctx.touch.py >= 196.0f && ctx.touch.py <= 234.0f) {
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

        // Barra Superior estilizada
        float dBar = depthShift(1.8f);
        Ui::slantRectGrad(8.0f + dBar, 4.0f, 85.0f, 20.0f, -4.0f, 0.25f, Ui::rgba(255, 42, 133), Ui::rgba(215, 25, 105));
        drawCenteredTextWithShadow(ctx.textBuf, 50.0f + dBar, 14.0f, 0.35f, ctx.clrWhite, "osu!3DS", 0.30f);

        const char* topTitle = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS)
            ? LOC(STR_MULTI_TITLE_UDS)
            : LOC(STR_MULTI_TITLE_LAN);
        drawTextWithShadow(ctx.textBuf, 102.0f + dBar, 9.0f, 0.40f, ctx.clrWhite, topTitle, 0.30f);

        char roomBadge[32];
        std::snprintf(roomBadge, sizeof(roomBadge), LOC(STR_MULTI_ROOMS_COUNT), static_cast<int>(ctx.multiMgr.discoveredRooms.size()));
        Ui::pill(300.0f + dBar, 5.0f, 54.0f, 18.0f, 0.25f, Ui::rgba(0, 229, 255));
        drawCenteredText(ctx.textBuf, 327.0f + dBar, 14.0f, 0.30f, Ui::rgba(0, 0, 0, 240), roomBadge, 0.30f);

        Ui::pill(360.0f + dBar, 5.0f, 32.0f, 18.0f, 0.25f, Ui::rgba(30, 25, 45, 220));
        renderWifiBars(370.0f + dBar, 8.0f, 3, Ui::rgba(0, 229, 255), Ui::rgba(80, 80, 100));

        // Si no hay salas descubiertas
        if (ctx.multiMgr.discoveredRooms.empty()) {
            float radAnim = 16.0f + 12.0f * (0.5f + 0.5f * std::sin(ctx.animTick * 0.08f));
            float cx = 200.0f + depthShift(1.5f);
            float cy = 92.0f;
            Ui::glow(cx, cy, 0.15f, radAnim * 2.2f, Ui::rgba(0, 229, 255, 60), 4);
            Ui::panel(cx - 24.0f, cy - 24.0f, 48.0f, 48.0f, 0.20f, Ui::rgba(25, 22, 42, 235), Ui::rgba(0, 229, 255));
            renderWifiBars(cx - 9.0f, cy - 6.0f, 3, Ui::rgba(0, 229, 255), Ui::rgba(70, 70, 95));

            if (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS) {
                drawCenteredText(ctx.textBuf, cx, 130.0f, 0.42f, ctx.clrWhite, LOC(STR_MULTI_SEARCH_UDS), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 148.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_NO_NET_REQ), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 168.0f, 0.34f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 186.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_ANY_JOIN), 0.25f);
            } else {
                drawCenteredText(ctx.textBuf, cx, 130.0f, 0.42f, ctx.clrWhite, LOC(STR_MULTI_SEARCH_LAN), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 148.0f, 0.32f, ctx.clrMuted, LOC(STR_MULTI_NO_ACTIVE_ROOMS), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 168.0f, 0.34f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_PRESS_X_CREATE), 0.25f);
                drawCenteredText(ctx.textBuf, cx, 186.0f, 0.30f, ctx.clrMuted, LOC(STR_MULTI_ROOM_WILL_APPEAR), 0.25f);
            }
        } else {
            // Columna Izquierda: Tarjetas de Salas (hasta 4 horizontales bien ordenadas)
            int startIdx = 0;
            if (ctx.multiMgr.selectedRoomIndex >= 4) startIdx = ctx.multiMgr.selectedRoomIndex - 3;
            for (int r = 0; r < 4 && (startIdx + r) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++r) {
                int idx = startIdx + r;
                const auto& rm = ctx.multiMgr.discoveredRooms[idx];
                bool isSel = (idx == ctx.multiMgr.selectedRoomIndex);
                float ry = 30.0f + r * 44.0f;
                float rx = 10.0f + depthShift(isSel ? 3.0f : 1.5f);
                float rw = 226.0f;
                float rh = 40.0f;

                u32 borderClr = isSel ? Ui::rgba(0, 229, 255) : Ui::rgba(45, 38, 65);
                u32 bgClr = isSel ? Ui::rgba(20, 34, 58, 255) : Ui::rgba(18, 22, 38, 230);
                Ui::panel(rx, ry, rw, rh, 0.20f, bgClr, borderClr);

                if (isSel) {
                    C2D_DrawRectSolid(rx, ry, 0.25f, 4.0f, rh, Ui::rgba(0, 229, 255));
                }

                drawTextWithShadow(ctx.textBuf, rx + 10.0f, ry + 4.0f, 0.38f, ctx.clrWhite, truncateString(rm.roomTitle, 20).c_str(), 0.25f);

                char subLine[64];
                std::snprintf(subLine, sizeof(subLine), "Host: %s . %s", rm.hostName.c_str(), truncateString(rm.mapTitle, 14).c_str());
                drawText(ctx.textBuf, rx + 10.0f, ry + 20.0f, 0.28f, ctx.clrMuted, subLine, 0.25f);

                char starsBuf[16];
                std::snprintf(starsBuf, sizeof(starsBuf), "%.2f*", rm.mapStars);
                u32 sClr = getStarRatingColor(rm.mapStars);
                Ui::pill(rx + 154.0f, ry + 5.0f, 32.0f, 13.0f, 0.26f, sClr);
                drawCenteredText(ctx.textBuf, rx + 170.0f, ry + 11.5f, 0.24f, Ui::rgba(0, 0, 0, 240), starsBuf, 0.28f);

                char plyBuf[16];
                std::snprintf(plyBuf, sizeof(plyBuf), "%d/%d", rm.playerCount, rm.maxPlayers);
                u32 pClr = (rm.playerCount >= rm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(74, 222, 128);
                Ui::pill(rx + rw - 34.0f, ry + 5.0f, 26.0f, 13.0f, 0.26f, pClr);
                drawCenteredText(ctx.textBuf, rx + rw - 21.0f, ry + 11.5f, 0.26f, Ui::rgba(0, 0, 0, 240), plyBuf, 0.28f);
            }

            // Columna Derecha: Panel de Detalles de la Sala Seleccionada
            float px = 242.0f + depthShift(2.0f);
            float py = 30.0f;
            float pw = 148.0f;
            float ph = 176.0f;
            Ui::panel(px, py, pw, ph, 0.20f, Ui::rgba(13, 17, 31, 255), Ui::rgba(0, 229, 255));

            const auto& selRm = ctx.multiMgr.discoveredRooms[ctx.multiMgr.selectedRoomIndex];
            drawTextWithShadow(ctx.textBuf, px + 8.0f, py + 6.0f, 0.36f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_DETAILS_HEADER), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 24.0f, 0.30f, ctx.clrWhite, ("Host: " + truncateString(selRm.hostName, 12)).c_str(), 0.25f);

            const char* wcStr = (selRm.winCondition == WIN_SCORE) ? LOC(STR_MULTI_WIN_SCORE) :
                                ((selRm.winCondition == WIN_ACCURACY) ? LOC(STR_MULTI_WIN_ACC) : LOC(STR_MULTI_WIN_COMBO));
            char ruleLine[64];
            std::snprintf(ruleLine, sizeof(ruleLine), LOC(STR_MULTI_RULE_FMT), wcStr);
            drawText(ctx.textBuf, px + 8.0f, py + 40.0f, 0.28f, ctx.clrMuted, ruleLine, 0.25f);

            const char* mmStr = (selRm.modMode == MODS_HOST_ONLY) ? "Host Only" : "FreeMod";
            char modsLine[64];
            std::snprintf(modsLine, sizeof(modsLine), "%s %s", LOC(STR_MULTI_MODS_LABEL), mmStr);
            drawText(ctx.textBuf, px + 8.0f, py + 54.0f, 0.28f, ctx.clrMuted, modsLine, 0.25f);

            drawText(ctx.textBuf, px + 8.0f, py + 72.0f, 0.30f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_ACTIVE_MAP), 0.25f);
            drawTextWithShadow(ctx.textBuf, px + 8.0f, py + 86.0f, 0.32f, ctx.clrWhite, truncateString(selRm.mapTitle, 15).c_str(), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 100.0f, 0.26f, ctx.clrMuted, truncateString(selRm.mapArtist, 17).c_str(), 0.25f);
            drawText(ctx.textBuf, px + 8.0f, py + 114.0f, 0.26f, ctx.clrMuted, truncateString(selRm.mapVersion, 17).c_str(), 0.25f);

            char pingStr[24];
            std::snprintf(pingStr, sizeof(pingStr), (selRm.mode == MultiNetworkMode::NET_MODE_UDS) ? LOC(STR_MULTI_PING_UDS_FMT) : LOC(STR_MULTI_PING_LAN_FMT), selRm.pingMs);
            drawText(ctx.textBuf, px + 8.0f, py + 134.0f, 0.27f, Ui::rgba(74, 222, 128), pingStr, 0.25f);

            const char* joinHint = (selRm.playerCount >= selRm.maxPlayers) ? LOC(STR_MULTI_ROOM_FULL) : LOC(STR_MULTI_TAP_ENTER);
            u32 hintClr = (selRm.playerCount >= selRm.maxPlayers) ? Ui::rgba(215, 50, 60) : Ui::rgba(0, 229, 255);
            Ui::pill(px + 8.0f, py + 152.0f, pw - 16.0f, 18.0f, 0.25f, Ui::rgba(20, 25, 45));
            drawCenteredText(ctx.textBuf, px + pw * 0.5f, py + 161.0f, 0.28f, hintClr, joinHint, 0.28f);
        }

        // Barra Inferior de Atajos
        Ui::gradV(0.0f, 214.0f, 400.0f, 26.0f, 0.3f, Ui::rgba(14, 11, 26, 120), Ui::rgba(14, 11, 26, 250));
        float byY = 220.0f;
        drawKeyBadge(ctx.textBuf, 12.0f + dBar, byY, "A", LOC(STR_MULTI_BADGE_JOIN), Ui::rgba(0, 229, 255));
        drawKeyBadge(ctx.textBuf, 112.0f + dBar, byY, "X", LOC(STR_MULTI_BADGE_CREATE), Ui::rgba(255, 209, 102));
        drawKeyBadge(ctx.textBuf, 196.0f + dBar, byY, "Y", LOC(STR_MULTI_BADGE_REFRESH), Ui::rgba(74, 222, 128));
        drawKeyBadge(ctx.textBuf, 276.0f + dBar, byY, "SELECT", LOC(STR_MULTI_BADGE_NETMODE), Ui::rgba(255, 102, 170));
        drawKeyBadge(ctx.textBuf, 356.0f + dBar, byY, "B", LOC(STR_MULTI_BADGE_BACK), Ui::rgba(148, 163, 184));

        if (ctx.multiMgr.hasToastNotice()) {
            float dToast = depthShift(4.0f);
            Ui::panel(50.0f + dToast, 212.0f, 300.0f, 24.0f, 0.90f, Ui::rgba(180, 20, 40, 240), Ui::rgba(255, 100, 120));
            drawCenteredTextWithShadow(ctx.textBuf, 200.0f + dToast, 224.0f, 0.34f, Ui::rgba(255, 230, 230), ctx.multiMgr.toastNotice.c_str(), 0.95f);
        }

        Ui::drawTransition(400.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);
    }

    // PANTALLA INFERIOR TÁCTIL (320x240)
    C2D_TargetClear(ctx.bottomScreen, ctx.clrTabletBg);
    C2D_SceneBegin(ctx.bottomScreen);

    Ui::backdrop(320.0f, 240.0f, static_cast<float>(ctx.animTick), Ui::rgba(15, 11, 26), Ui::rgba(10, 7, 18), ctx.clrPrimary, 0.02f);

    drawTextWithShadow(ctx.textBuf, 10.0f, 8.0f, 0.40f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_BROWSER_TITLE), 0.20f);
    Ui::button(240.0f, 4.0f, 72.0f, 22.0f, 0.20f, Ui::rgba(22, 28, 48), Ui::rgba(0, 229, 255), false);
    drawCenteredTextWithShadow(ctx.textBuf, 276.0f, 15.0f, 0.30f, Ui::rgba(0, 229, 255), LOC(STR_MULTI_BADGE_REFRESH), 0.25f);

    // Selector Táctil de Modo de Red
    bool isUds = (ctx.multiMgr.netMode == MultiNetworkMode::NET_MODE_UDS);
    Ui::panel(10.0f, 32.0f, 146.0f, 24.0f, 0.20f, isUds ? Ui::rgba(0, 229, 255) : Ui::rgba(20, 25, 45), isUds ? Ui::rgba(255, 255, 255) : 0);
    drawCenteredTextWithShadow(ctx.textBuf, 83.0f, 44.0f, 0.32f, isUds ? Ui::rgba(0, 0, 0, 240) : ctx.clrWhite, LOC(STR_MULTI_TAB_UDS), 0.25f);

    Ui::panel(164.0f, 32.0f, 146.0f, 24.0f, 0.20f, !isUds ? Ui::rgba(0, 229, 255) : Ui::rgba(20, 25, 45), !isUds ? Ui::rgba(255, 255, 255) : 0);
    drawCenteredTextWithShadow(ctx.textBuf, 237.0f, 44.0f, 0.32f, !isUds ? Ui::rgba(0, 0, 0, 240) : ctx.clrWhite, LOC(STR_MULTI_TAB_LAN), 0.25f);

    // Lista táctil de salas encontradas
    if (ctx.multiMgr.discoveredRooms.empty()) {
        Ui::panel(10.0f, 62.0f, 300.0f, 126.0f, 0.15f, Ui::rgba(18, 22, 38, 230), Ui::rgba(45, 38, 65));
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 110.0f, 0.36f, ctx.clrMuted, LOC(STR_MULTI_SEARCHING_NEARBY), 0.25f);
        drawCenteredTextWithShadow(ctx.textBuf, 160.0f, 130.0f, 0.32f, Ui::rgba(255, 209, 102), LOC(STR_MULTI_CREATE_ROOM_PROMPT), 0.25f);
    } else {
        int startIdx = 0;
        if (ctx.multiMgr.selectedRoomIndex >= 3) startIdx = ctx.multiMgr.selectedRoomIndex - 2;
        for (int k = 0; k < 3 && (startIdx + k) < static_cast<int>(ctx.multiMgr.discoveredRooms.size()); ++k) {
            int idx = startIdx + k;
            float cy = 64.0f + k * 42.0f;
            bool isSel = (idx == ctx.multiMgr.selectedRoomIndex);
            const auto& rm = ctx.multiMgr.discoveredRooms[idx];

            u32 bColor = isSel ? Ui::rgba(0, 229, 255) : Ui::rgba(35, 30, 55);
            u32 cColor = isSel ? Ui::rgba(20, 34, 58) : Ui::rgba(18, 22, 38);
            Ui::panel(10.0f, cy, 300.0f, 38.0f, 0.20f, cColor, bColor);

            drawTextWithShadow(ctx.textBuf, 18.0f, cy + 4.0f, 0.36f, ctx.clrWhite, truncateString(rm.roomTitle, 24).c_str(), 0.25f);
            char infBuf[64];
            std::snprintf(infBuf, sizeof(infBuf), "Host: %s · %s", rm.hostName.c_str(), truncateString(rm.mapTitle, 16).c_str());
            drawText(ctx.textBuf, 18.0f, cy + 20.0f, 0.28f, ctx.clrMuted, infBuf, 0.25f);

            char plPill[16];
            std::snprintf(plPill, sizeof(plPill), "%d/%d", rm.playerCount, rm.maxPlayers);
            Ui::pill(260.0f, cy + 9.0f, 42.0f, 20.0f, 0.25f, isSel ? Ui::rgba(0, 229, 255) : Ui::rgba(30, 40, 65));
            drawCenteredText(ctx.textBuf, 281.0f, cy + 19.0f, 0.30f, isSel ? Ui::rgba(0, 0, 0, 240) : ctx.clrWhite, plPill, 0.28f);
        }
    }

    // Botones de acción inferior
    Ui::button(10.0f, 196.0f, 76.0f, 38.0f, 0.20f, Ui::rgba(28, 22, 38), Ui::rgba(148, 163, 184), false);
    drawCenteredTextWithShadow(ctx.textBuf, 48.0f, 215.0f, 0.34f, ctx.clrWhite, LOC(STR_MULTI_BACK_ARROW), 0.25f);

    Ui::button(94.0f, 196.0f, 216.0f, 38.0f, 0.20f, Ui::rgba(255, 209, 102), Ui::rgba(255, 240, 160), true);
    drawCenteredTextWithShadow(ctx.textBuf, 202.0f, 215.0f, 0.40f, Ui::rgba(0, 0, 0, 240), LOC(STR_MULTI_BTN_CREATE_ROOM), 0.25f);

    if (ctx.isTouching) {
        Ui::glow(ctx.touch.px, ctx.touch.py, 0.95f, 10.0f, Ui::withAlpha(Ui::rgba(0, 229, 255), 130), 3);
        C2D_DrawCircleSolid(ctx.touch.px, ctx.touch.py, 0.96f, 4.0f, Ui::rgba(0, 229, 255));
    }
    Ui::drawTransition(320.0f, 240.0f, ctx.screenFade, ctx.clrPrimary);

    C3D_FrameEnd(0);
}
