#include "AppContext.hpp"

void AppContext::init() {
    topScreen      = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topScreenRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottomScreen   = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuf        = C2D_TextBufNew(4096);

    clrBgDark     = C2D_Color32(18, 16, 26, 255);
    clrPlayfield  = C2D_Color32(26, 24, 38, 255);
    clrTabletBg   = C2D_Color32(22, 20, 30, 255);
    clrWhite      = C2D_Color32(255, 255, 255, 255);
    clrMuted      = C2D_Color32(170, 165, 195, 255);
    clrKeyIdle    = C2D_Color32(45, 42, 60, 255);
    clrKeyActive  = C2D_Color32(245, 110, 165, 255);
    clr300        = C2D_Color32(80, 220, 255, 255);
    clr100        = C2D_Color32(100, 240, 120, 255);
    clr50         = C2D_Color32(230, 190, 80, 255);
    clrMiss       = C2D_Color32(240, 70, 70, 255);

    settings.load();

    skinMgr.initAndScan();
    if (settings.skinIndex >= static_cast<int>(skinMgr.skins.size())) {
        settings.skinIndex = 0;
    }

    scoreMgr.load();
    songMgr.initAndScan(&scoreMgr);

    audio.initDSPAndHitsounds();
    audio.setVolumes(settings.musicVolume, settings.sfxVolume);

    multiMgr.enableBgDownload = (settings.multiDlOption != 0);

    // Detectar nombre de usuario configurado en la consola 3DS
    sysUser = "Player3DS";
    u16 utf16_user[16] = {0};
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetConfigInfoBlk2(0x1C, 0x000A0000, utf16_user);
        cfguExit();
        std::string tmp;
        for (int i = 0; i < 10 && utf16_user[i] != 0; ++i) {
            if (utf16_user[i] >= 32 && utf16_user[i] < 127) tmp.push_back(static_cast<char>(utf16_user[i]));
        }
        if (!tmp.empty()) sysUser = tmp;
    }
    multiMgr.localPlayerName = sysUser;

    // Detectar si la consola es New Nintendo 3DS / New 2DS XL (804 MHz Quad-Core + 256 MB RAM)
    isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);
    bgMaxW = isNew3DS ? 400 : 256;
    bgMaxH = isNew3DS ? 240 : 192;

    // Decodificador de fondos en hilo secundario (usa nucleo dedicado en New 3DS)
    bgLoader.start(isNew3DS);
    audio.startAsyncLoader(isNew3DS);
    videoPlayer.init();
    previewDelay = 0;
    bgFade = 255.0f;

    applyActiveSkinAudio();

    // Al arrancar el juego, reproducir siempre en primer lugar la canción emblemática ("Click the circles")
    std::srand(static_cast<unsigned int>(osGetTime()));
    selectedSet = 0;
    selectedDiff = 0;
    carouselPos = 0.0f;

    std::string themePath = "romfs:/circles.mp3";
    bool themeStarted = false;
    FILE* fTheme = std::fopen(themePath.c_str(), "rb");
    if (fTheme) {
        std::fclose(fTheme);
        themeStarted = audio.startMusic(themePath, 0, 1.0f, 0);
        if (themeStarted) {
            audio.setLoop(false);
            bgLoader.cancel();
            bgTex.unload();
        }
    }

    if (!themeStarted && !songMgr.sets.empty()) {
        selectedSet = std::rand() % static_cast<int>(songMgr.sets.size());
        selectedDiff = 0;
        carouselPos = static_cast<float>(selectedSet);
        updateSongPreview();
    }
}

void AppContext::beginFrame() {
    hidScanInput();
    kDown = hidKeysDown();
    kHeld = hidKeysHeld();
    kUp   = hidKeysUp();
    hidCircleRead(&cpad);
    cstick = {0, 0};
    hidCstickRead(&cstick);

    slider3D = osGet3DSliderState();
    if (stereoEnabledState) {
        if (slider3D < 0.02f) {
            stereoEnabledState = false;
            gfxSet3D(false);
        }
    } else {
        if (slider3D > 0.06f) {
            stereoEnabledState = true;
            gfxSet3D(true);
        }
    }
    stereoActive = stereoEnabledState;
    if (analogNavCooldown > 0) analogNavCooldown--;

    isTouching = (kHeld & KEY_TOUCH) != 0;
    touchJustDown = (kDown & KEY_TOUCH) != 0;
    touchJustUp   = (kUp & KEY_TOUCH) != 0;
    if (isTouching) {
        hidTouchRead(&touch);
    }

    animTick++;
    if (appState != lastAppState) {
        lastAppState = appState;
        screenFade = 1.0f;
    } else {
        screenFade = Ui::approach(screenFade, 0.0f, 0.18f);
    }
    C2D_TextBufClear(textBuf);

    const SkinProfile& activeSkin = getActiveSkin();
    clrCursor = C2D_Color32(activeSkin.cursorColor.r, activeSkin.cursorColor.g, activeSkin.cursorColor.b, 255);
    clrPrimary = C2D_Color32(
        activeSkin.comboColors[0].r,
        activeSkin.comboColors[0].g,
        activeSkin.comboColors[0].b,
        235
    );
}

void AppContext::shutdown() {
    videoPlayer.shutdown();
    audio.stopAsyncLoader();
    bgLoader.stop();
    bgTex.unload();
    texSkinCursor.unload();
    texSkinCursorTrail.unload();
    texSkinCircle.unload();
    texSkinOverlay.unload();
    texSkinApproach.unload();
    texSkinComboBurst.unload();
    settings.save();
    audio.exitAll();
    if (textBuf) {
        C2D_TextBufDelete(textBuf);
        textBuf = nullptr;
    }
}

void AppContext::applyActiveSkinAudio() {
    if (skinMgr.skins.empty()) return;
    const auto& sk = skinMgr.skins[settings.skinIndex % skinMgr.skins.size()];
    audio.loadSkinHitsounds(
        sk.hitNormalWav,
        sk.hitWhistleWav,
        sk.hitFinishWav,
        sk.hitClapWav,
        sk.comboBreakWav,
        sk.synthPreset,
        sk.nightcoreKickWav,
        sk.nightcoreClapWav,
        sk.nightcoreHatWav
    );

    if (!sk.cursorPng.empty())           texSkinCursor.loadFromFile(sk.cursorPng, 64, 64);
    else                                 texSkinCursor.unload();

    if (!sk.cursorTrailPng.empty())      texSkinCursorTrail.loadFromFile(sk.cursorTrailPng, 64, 64);
    else                                 texSkinCursorTrail.unload();

    if (!sk.hitCirclePng.empty())        texSkinCircle.loadFromFile(sk.hitCirclePng, 128, 128);
    else                                 texSkinCircle.unload();

    if (!sk.hitCircleOverlayPng.empty()) texSkinOverlay.loadFromFile(sk.hitCircleOverlayPng, 128, 128);
    else                                 texSkinOverlay.unload();

    if (!sk.approachCirclePng.empty())   texSkinApproach.loadFromFile(sk.approachCirclePng, 128, 128);
    else                                 texSkinApproach.unload();

    if (!sk.comboBurstPng.empty())       texSkinComboBurst.loadFromFile(sk.comboBurstPng, 160, 240);
    else                                 texSkinComboBurst.unload();
}

void AppContext::spawnParticles(float osuX, float osuY, u32 color, int count, float minSpd, float maxSpd, float baseSize, int maxLifeFrames) {
    for (int p = 0; p < count; ++p) {
        if (particles.size() >= 64) particles.erase(particles.begin());
        float ang = static_cast<float>(std::rand() % 628) * 0.01f;
        float spd = minSpd + (static_cast<float>(std::rand() % 100) / 100.0f) * (maxSpd - minSpd);
        int lf = std::max(6, maxLifeFrames - (std::rand() % 5));
        particles.push_back({
            osuX * SCALE_FACTOR, osuY * SCALE_FACTOR,
            std::cos(ang) * spd, std::sin(ang) * spd,
            baseSize, color, lf, lf
        });
    }
}

void AppContext::checkComboBurst(int curCombo) {
    if (!settings.comboBursts) return;
    bool isMilestone = false;
    if (curCombo == 30 || curCombo == 60) isMilestone = true;
    else if (curCombo >= 100 && (curCombo % 100 == 0 || curCombo % 50 == 0)) isMilestone = true;

    if (isMilestone && curCombo > lastComboBurstMilestone) {
        lastComboBurstMilestone = curCombo;
        comboBurstTimer = 48;
        comboBurstSide = (comboBurstSide == 0) ? 1 : 0;
        comboBurstValue = curCombo;
        float burstEdgeX = (comboBurstSide == 0) ? 460.0f : 50.0f;
        spawnParticles(burstEdgeX, 220.0f, Ui::rgba(255, 102, 171), 10, 1.2f, 3.2f, 2.5f, 24);
        spawnParticles(burstEdgeX, 220.0f, Ui::rgba(255, 215, 60), 8, 1.0f, 2.8f, 2.2f, 22);
    }
}

void AppContext::triggerShake(float intensity, float frames) {
    if (intensity > shakeIntensity || shakeTimer <= 0.0f) {
        shakeIntensity = intensity;
    }
    if (frames > shakeTimer) {
        shakeTimer = frames;
    }
}

void AppContext::updateSongPreview() {
    if (songMgr.sets.empty()) {
        audio.stopMusic();
        bgTex.unload();
        return;
    }
    selectedSet = std::max(0, std::min(selectedSet, static_cast<int>(songMgr.sets.size()) - 1));
    const auto& curSet = songMgr.sets[selectedSet];
    if (curSet.difficulties.empty()) return;
    selectedDiff = std::max(0, std::min(selectedDiff, static_cast<int>(curSet.difficulties.size()) - 1));
    const auto& curDiff = curSet.difficulties[selectedDiff];

    std::string bgPath = !curDiff.backgroundPath.empty() ? curDiff.backgroundPath : curSet.backgroundPath;
    if (!bgPath.empty()) {
        if (!(bgTex.loaded && bgTex.sourcePath == bgPath)) {
            bgLoader.request(bgPath, bgMaxW, bgMaxH);
        }
    } else {
        bgLoader.cancel();
        bgTex.unload();
    }

    if (!settings.songPreview) {
        audio.stopMusic();
        return;
    }
    const std::string& audioPath = curDiff.audioPath;
    float speedRate = getModsSpeedMultiplier(settings.activeMods);
    bool shouldLoop = (appState == STATE_SONG_SELECT);
    int startOff = (curDiff.previewTimeMs > 0) ? curDiff.previewTimeMs : 0;
    if (!audio.getIsPlaying() || audio.isSongFinished() || audio.getLoadedMusicPath() != audioPath) {
        audio.requestPreview(audioPath, speedRate, startOff, shouldLoop);
    } else {
        audio.setLoop(shouldLoop, startOff);
    }
}

void AppContext::rotateMenuSong() {
    if (!settings.songPreview) return;
    if (songMgr.sets.empty()) {
        std::string themePath = "romfs:/circles.mp3";
        audio.startMusic(themePath, 0, 1.0f, 0);
        audio.setLoop(false);
        bgLoader.cancel();
        bgTex.unload();
        return;
    }

    int nSets = static_cast<int>(songMgr.sets.size());
    if (nSets > 1) {
        int next = selectedSet;
        while (next == selectedSet) {
            next = std::rand() % nSets;
        }
        selectedSet = next;
    } else {
        selectedSet = 0;
    }
    selectedDiff = 0;
    carouselPos = static_cast<float>(selectedSet);
    updateSongPreview();
}

float AppContext::getBpmPulse() {
    float beatLen = 500.0f;
    float offset = 0.0f;
    if (audio.getLoadedMusicPath() == "romfs:/circles.mp3") {
        // Nekodex - Circles (175 BPM)
        beatLen = 60000.0f / 175.0f;
        offset = 0.0f;
    } else if (!songMgr.sets.empty() && selectedSet >= 0 && selectedSet < static_cast<int>(songMgr.sets.size())) {
        const auto& cs = songMgr.sets[selectedSet];
        if (!cs.difficulties.empty()) {
            int dIdx = std::max(0, std::min(selectedDiff, static_cast<int>(cs.difficulties.size()) - 1));
            beatLen = std::max(120.0f, cs.difficulties[dIdx].beatLengthMs / getModsSpeedMultiplier(settings.activeMods));
            offset = cs.difficulties[dIdx].timingOffsetMs;
        }
    }
    float tMs = audio.getIsPlaying()
        ? static_cast<float>(audio.getTimeMs())
        : static_cast<float>(animTick) * (1000.0f / 60.0f);
    float rel = std::fmod(std::max(0.0f, tMs - offset), beatLen) / beatLen;
    return std::pow(1.0f - rel, 3.0f);
}

void AppContext::toggleSearchTag(const std::string& tag) {
    size_t pos = songMgr.searchQuery.find(tag);
    if (pos != std::string::npos) {
        songMgr.searchQuery.erase(pos, tag.length());
        size_t sp = songMgr.searchQuery.find("  ");
        while (sp != std::string::npos) {
            songMgr.searchQuery.erase(sp, 1);
            sp = songMgr.searchQuery.find("  ");
        }
        songMgr.searchQuery = Beatmap::trim(songMgr.searchQuery);
    } else {
        if (tag.rfind("mode:", 0) == 0) {
            const char* modes[] = {"mode:std", "mode:taiko", "mode:catch", "mode:mania"};
            for (const char* m : modes) {
                size_t mp = songMgr.searchQuery.find(m);
                if (mp != std::string::npos) {
                    songMgr.searchQuery.erase(mp, std::strlen(m));
                }
            }
        }
        if (tag == "★4" || tag == "★5") {
            size_t p4 = songMgr.searchQuery.find("★4");
            if (p4 != std::string::npos) songMgr.searchQuery.erase(p4, 2);
            size_t p5 = songMgr.searchQuery.find("★5");
            if (p5 != std::string::npos) songMgr.searchQuery.erase(p5, 2);
        }
        songMgr.searchQuery = Beatmap::trim(songMgr.searchQuery);
        if (!songMgr.searchQuery.empty()) songMgr.searchQuery += " ";
        songMgr.searchQuery += tag;
    }
    songMgr.applySortAndFilter(&scoreMgr);
    selectedSet = 0;
    selectedDiff = 0;
    rankingScroll = 0;
    carouselPos = 0.0f;
    audio.playHitSound(2);
    updateSongPreview();
}

void AppContext::openSystemKeyboard() {
    SwkbdState swkbd;
    char inputBuf[64] = {0};
    std::snprintf(inputBuf, sizeof(inputBuf), "%s", songMgr.searchQuery.c_str());
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 32);
    swkbdSetInitialText(&swkbd, inputBuf);
    swkbdSetHintText(&swkbd, LOC(STR_SEARCH_SWKBD_HINT));
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, LOC(STR_CLEAR), false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, LOC(STR_SEARCH), true);
    SwkbdButton btn = swkbdInputText(&swkbd, inputBuf, sizeof(inputBuf));
    if (btn == SWKBD_BUTTON_RIGHT) {
        songMgr.searchQuery = Beatmap::trim(inputBuf);
    } else if (btn == SWKBD_BUTTON_LEFT) {
        songMgr.searchQuery = "";
    }
    songMgr.applySortAndFilter(&scoreMgr);
    selectedSet = 0;
    selectedDiff = 0;
    carouselPos = 0.0f;
    audio.playHitSound(2);
    updateSongPreview();
}

void AppContext::openSearchKeyboard() {
    searchKeyboardOpen = true;
    searchLastKeyIndex = -1;
    searchKeyFlashTimer = 0;
    searchBackspaceHoldTimer = 0;
    audio.playHitSound(0);
}

bool AppContext::startSelectedBeatmap() {
    if (songMgr.sets.empty()) return false;
    const auto& curSet = songMgr.sets[selectedSet];
    if (curSet.difficulties.empty()) return false;
    const auto& diffEntry = curSet.difficulties[selectedDiff];

    if (!beatmap.loadFromFile(diffEntry.osuPath, false)) return false;
    beatmap.applyMods(settings.activeMods);

    bgLoader.cancel();
    previewDelay = 0;
    if (!beatmap.fullBackgroundPath.empty()) {
        bgTex.loadFromFile(beatmap.fullBackgroundPath, bgMaxW, bgMaxH);
    }
    if (settings.enableVideo && !beatmap.fullVideoPath.empty()) {
        videoPlayer.openVideo(beatmap.fullVideoPath, beatmap.videoOffsetMs);
    } else {
        videoPlayer.closeVideo();
    }

    audio.setVolumes(settings.musicVolume, settings.sfxVolume);
    applyActiveSkinAudio();
    if (settings.beatmapHitsounds && !beatmap.folderPath.empty()) {
        audio.loadBeatmapHitsounds(beatmap.folderPath, beatmap.sampleSetName);
    }
    audio.playHitSound(2);

    bool isTaikoPlay = (settings.gameMode == MODE_TAIKO || diffEntry.mode == 1);
    bool isCatchPlay = (settings.gameMode == MODE_CATCH || diffEntry.mode == 2);
    bool isManiaPlay = (settings.gameMode == MODE_MANIA || diffEntry.mode == 3);
    if (isTaikoPlay) {
        taikoEngine.initFromBeatmap(beatmap);
    } else if (isCatchPlay) {
        catchEngine.initFromBeatmap(beatmap);
    } else if (isManiaPlay) {
        maniaEngine.initFromBeatmap(beatmap);
    }

    int leadIn = std::max(0, beatmap.audioLeadIn);
    if (isTaikoPlay) {
        if (!taikoEngine.notes.empty()) {
            int firstObjTime = taikoEngine.notes.front().time;
            int minStartWait = 1400;
            if (firstObjTime < minStartWait) {
                leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
        }
    } else if (isCatchPlay) {
        if (!catchEngine.fruits.empty()) {
            int firstObjTime = catchEngine.fruits.front().time;
            int minStartWait = 1300;
            if (firstObjTime < minStartWait) {
                leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
        }
    } else if (isManiaPlay) {
        if (!maniaEngine.notes.empty()) {
            int firstObjTime = maniaEngine.notes.front().time;
            int minStartWait = 1200;
            if (firstObjTime < minStartWait) {
                leadIn = std::max(leadIn, minStartWait - firstObjTime);
            }
        }
    } else if (!beatmap.circles.empty()) {
        int firstObjTime = beatmap.circles.front().time;
        int minStartWait = static_cast<int>(beatmap.diff.preempt) + 500;
        if (firstObjTime < minStartWait) {
            leadIn = std::max(leadIn, minStartWait - firstObjTime);
        }
    }

    float speedRate = getModsSpeedMultiplier(settings.activeMods);
    audio.startMusic(beatmap.fullAudioPath, leadIn, speedRate, 0);
    gameplayFallbackStartTick = osGetTime() + leadIn;

    combo = 0;
    prevCombo = 0;
    lastBrokenCombo = 0;
    maxCombo = 0;
    score = 0;
    count300 = count100 = count50 = countMiss = 0;
    k1PressCount = k2PressCount = 0;
    comboScale = 1.0f;
    comboBreakAnim = 0.0f;
    hitErrorTicks.clear();
    allHitErrors.clear();
    judgementHistory.clear();
    hitErrorNeedle = 0.0f;
    lastUnstableRate = 0.0f;
    lastMeanHitError = 0.0f;
    spinnerRPM = 0.0f;
    spinnerClearAnim = 0.0f;
    spinnerBonusAnim = 0.0f;
    lastSpinnerBonus = 0;
    shakeTimer = 0.0f;
    shakeIntensity = 0.0f;
    shakeX = 0.0f;
    shakeY = 0.0f;
    hp = 1.0f;
    stageFailed = false;
    newRecordAchieved = false;
    lastEarnedPP = 0.0f;
    isPaused = false;
    pauseOption = 0;
    cursorOsuX = 256.0f;
    cursorOsuY = 192.0f;
    prevCursorOsuX = 256.0f;
    prevCursorOsuY = 192.0f;
    cursorTrail.clear();
    smokeParticles.clear();
    popups.clear();
    particles.clear();
    int curStartMode = isTaikoPlay ? 1 : isCatchPlay ? 2 : isManiaPlay ? 3 : 0;
    localRecords = scoreMgr.getTopRecords(diffEntry.osuPath, 4, curStartMode);
    overtakeFlashAnim = 0.0f;
    hasOvertakenBest = false;
    skipAvailable = false;
    skipTargetMs = 0;
    inBreakPeriod = false;
    breakRemainingMs = 0;
    breakTotalMs = 0;
    breakNextObjPos = !beatmap.circles.empty() ? Vec2(beatmap.circles.front().x, beatmap.circles.front().y) : Vec2(256.0f, 192.0f);
    inKiai = false;
    prevInKiai = false;
    kiaiGlow = 0.0f;
    kiaiBurstFlash = 0.0f;
    comboBurstTimer = 0;
    comboBurstSide = 0;
    comboBurstValue = 0;
    lastComboBurstMilestone = 0;
    lastNcStep = -999999;
    taikoGuideTimer = 240.0f;
    appState = STATE_PLAYING;
    return true;
}

void AppContext::toggleModByIndex(int idx) {
    if (idx < 0 || idx >= 15) return;
    uint32_t bit = modTable[idx].bit;
    if (multiMgr.inMultiplayer && (bit == MOD_AUTO || bit == MOD_RX || bit == MOD_AP)) {
        // Mods no permitidos en partidas multijugador (Auto, Relax, AutoPilot)
        audio.playHitSound(4);
        return;
    }
    if (settings.activeMods & bit) {
        settings.activeMods &= ~bit;
    } else {
        settings.activeMods |= bit;
        // Incompatibilidades clásicas y de osu!lazer
        if (bit == MOD_EZ) settings.activeMods &= ~MOD_HR;
        if (bit == MOD_HR) settings.activeMods &= ~MOD_EZ;
        if (bit == MOD_HT) settings.activeMods &= ~(MOD_DT | MOD_NC);
        if (bit == MOD_DT) settings.activeMods &= ~(MOD_HT | MOD_NC);
        if (bit == MOD_NC) settings.activeMods &= ~(MOD_HT | MOD_DT);
        if (bit == MOD_SD) settings.activeMods &= ~(MOD_PF | MOD_NF);
        if (bit == MOD_PF) settings.activeMods &= ~(MOD_SD | MOD_NF);
        if (bit == MOD_NF) settings.activeMods &= ~(MOD_SD | MOD_PF);
        if (bit == MOD_AUTO) settings.activeMods &= ~(MOD_RX | MOD_AP);
        if (bit == MOD_RX) settings.activeMods &= ~(MOD_AUTO | MOD_AP);
        if (bit == MOD_AP) settings.activeMods &= ~(MOD_AUTO | MOD_RX);
    }
    if (multiMgr.inMultiplayer) {
        settings.activeMods &= ~(MOD_AUTO | MOD_RX | MOD_AP);
        multiMgr.players[multiMgr.localSlot].activeMods = settings.activeMods;
        if (!multiMgr.isHost()) {
            multiMgr.sendPlayerUpdate();
        } else {
            multiMgr.broadcastLobbyState();
        }
    }
    settings.save();
}

std::string AppContext::findLocalMapFile(const std::string& folder, const std::string& file) {
    std::string res = resolveFilePath(folder, file);
    if (!res.empty()) return res;

    std::string bFile = extractBaseFilename(file);
    std::string bFolder = extractFolderName(folder);

    // 1. Probar rutas alternativas estándar directas y en subcarpetas
    const char* testDirs[] = {
        "sdmc:/3ds/osu-3ds/songs/",
        "sdmc:/osu-3ds/songs/",
        "sdmc:/3ds/osu/Songs/",
        "sdmc:/3ds/osu/songs/",
        "romfs:/songs/",
        "romfs:/"
    };
    for (const char* dir : testDirs) {
        std::string p1 = std::string(dir) + bFolder + "/" + bFile;
        FILE* f1 = std::fopen(p1.c_str(), "rb");
        if (f1) { std::fclose(f1); return p1; }

        std::string p2 = std::string(dir) + bFile;
        FILE* f2 = std::fopen(p2.c_str(), "rb");
        if (f2) { std::fclose(f2); return p2; }
    }

    // 1b. Si en la subcarpeta existe algún archivo .osu, intentar abrirlo
    if (!bFolder.empty()) {
        for (const char* dir : testDirs) {
            std::string fullDir = std::string(dir) + bFolder;
            DIR* d = opendir(fullDir.c_str());
            if (d) {
                struct dirent* ent;
                while ((ent = readdir(d)) != nullptr) {
                    std::string fn = ent->d_name;
                    if (fn.size() >= 5 && fn.substr(fn.size() - 4) == ".osu") {
                        if (fn == bFile) {
                            std::string cand = fullDir + "/" + fn;
                            closedir(d);
                            return cand;
                        }
                    }
                }
                closedir(d);
            }
        }
    }

    // 2. Buscar en la biblioteca de SongManager por nombre de archivo exacto
    if (!bFile.empty()) {
        for (const auto& s : songMgr.sets) {
            for (const auto& d : s.difficulties) {
                if (extractBaseFilename(d.osuPath) == bFile) {
                    return d.osuPath;
                }
            }
        }
    }

    // 3. Buscar en la biblioteca por Título y Dificultad/Versión exacta
    if (!multiMgr.activeBeatmap.title.empty() && !multiMgr.activeBeatmap.version.empty()) {
        for (const auto& s : songMgr.sets) {
            if (s.title == multiMgr.activeBeatmap.title) {
                for (const auto& d : s.difficulties) {
                    if (d.version == multiMgr.activeBeatmap.version) {
                        return d.osuPath;
                    }
                }
            }
        }
    }

    return "";
}

void AppContext::triggerSuddenDeathFail() {
    hp = 0.0f;
    stageFailed = true;
    lastEarnedPP = 0.0f;
    audio.stopMusic();
    videoPlayer.closeVideo();
    int totalJudged = count300 + count100 + count50 + countMiss;
    float curAcc = (totalJudged > 0)
        ? ((count300 * 300.0f + count100 * 100.0f + count50 * 50.0f) / (totalJudged * 300.0f)) * 100.0f
        : 100.0f;
    if (multiMgr.inMultiplayer) {
        multiMgr.matchStarted = false;
        multiMgr.matchInProgressFlag = false;
        multiMgr.matchFinished = true;
        multiMgr.sendLiveGameplay(score, combo, maxCombo, curAcc, 0.0f, "F", count300, count100, count50, countMiss, true, true);
        appState = STATE_MULTI_RESULTS;
    } else {
        appState = STATE_RESULTS;
    }
}

