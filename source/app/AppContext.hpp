#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <dirent.h>
#include "Beatmap.hpp"
#include "AudioPlayer.hpp"
#include "SongManager.hpp"
#include "Settings.hpp"
#include "SkinManager.hpp"
#include "ScoreManager.hpp"
#include "Replay.hpp"
#include "WifiServer.hpp"
#include "TextureLoader.hpp"
#include "VideoPlayer.hpp"
#include "UiKit.hpp"
#include "qrcodegen.h"
#include "DirectDownloader.hpp"
#include "ModeTaiko.hpp"
#include "ModeMania.hpp"
#include "ModeCatch.hpp"
#include "NetCommon.hpp"
#include "MultiplayerManager.hpp"
#include "Playfield.hpp"
#include "Mods.hpp"
#include "GameplayEffects.hpp"
#include "AppState.hpp"
#include "QrRenderer.hpp"
#include "Draw.hpp"

enum MultiPickerAction {
    PICKER_ACTION_ACTIVE = 0,
    PICKER_ACTION_QUEUE  = 1
};

struct HitJudgementEvent {
    int time;
    HitState state;
    float errMs;
};

struct AppContext {
    bool shouldQuit = false;

    // Objetivos de renderizado y buffer de texto
    C3D_RenderTarget* topScreen      = nullptr;
    C3D_RenderTarget* topScreenRight = nullptr;
    C3D_RenderTarget* bottomScreen   = nullptr;
    C2D_TextBuf textBuf              = nullptr;

    // Paleta de colores base y colores dinámicos de Skin
    u32 clrBgDark    = 0;
    u32 clrPlayfield = 0;
    u32 clrTabletBg  = 0;
    u32 clrWhite     = 0;
    u32 clrMuted     = 0;
    u32 clrKeyIdle   = 0;
    u32 clrKeyActive = 0;
    u32 clr300       = 0;
    u32 clr100       = 0;
    u32 clr50        = 0;
    u32 clrMiss      = 0;
    u32 clrCursor    = 0;
    u32 clrPrimary   = 0;

    // Servicios del backend y motores de modos de juego
    GameSettings settings;
    SkinManager skinMgr;
    ScoreManager scoreMgr;
    SongManager songMgr;
    Beatmap beatmap;
    ModeTaiko taikoEngine;
    ModeMania maniaEngine;
    ModeCatch catchEngine;
    AudioPlayer audio;
    WifiServer wifiServer;
    MultiplayerManager multiMgr;

    // Estado de Multijugador
    std::string sysUser = "Player3DS";
    bool multiQueuePickerOpen = false;
    int multiQueuePickerIndex = 0;
    int multiQueuePickerDiff = 0;
    MultiPickerAction multiPickerAction = PICKER_ACTION_ACTIVE;
    int multiResultsTab = 0;
    bool multiSyncLoaded = false;
    int multiSyncLeadIn = 0;

    // Texturas GPU dinámicas y reproductor de video
    LoadedTexture bgTex;

    // osu!standard
    LoadedTexture texSkinCursor;
    LoadedTexture texSkinCursorTrail;
    LoadedTexture texSkinCircle;
    LoadedTexture texSkinOverlay;
    LoadedTexture texSkinApproach;
    LoadedTexture texSkinComboBurst;
    LoadedTexture texSkinSliderBall;
    LoadedTexture texSkinSliderFollow;
    LoadedTexture texSkinReverseArrow;
    LoadedTexture texSkinHit0;
    LoadedTexture texSkinHit50;
    LoadedTexture texSkinHit100;
    LoadedTexture texSkinHit300;

    // osu!taiko
    LoadedTexture texTaikoDon;
    LoadedTexture texTaikoKat;
    LoadedTexture texTaikoDonBig;
    LoadedTexture texTaikoKatBig;
    LoadedTexture texTaikoHitCircle;
    LoadedTexture texTaikoHitCircleOverlay;
    LoadedTexture texTaikoDrum;
    LoadedTexture texTaikoDrumDon;
    LoadedTexture texTaikoDrumKat;

    // osu!mania
    LoadedTexture texManiaNote1;
    LoadedTexture texManiaNote2;
    LoadedTexture texManiaNoteS;
    LoadedTexture texManiaStageLight;
    LoadedTexture texManiaStageBottom;

    // osu!catch
    LoadedTexture texCatchFruitApple;
    LoadedTexture texCatchFruitOrange;
    LoadedTexture texCatchFruitGrape;
    LoadedTexture texCatchFruitStrawberry;
    LoadedTexture texCatchFruitBanana;
    LoadedTexture texCatchFruitDrop;
    LoadedTexture texCatchPlate;
    LoadedTexture texCatchCatcher;

    bool isNew3DS = false;
    int bgMaxW = 256;
    int bgMaxH = 192;

    AsyncTextureLoader bgLoader;
    VideoPlayer videoPlayer;
    int previewDelay = 0;
    float bgFade = 255.0f;

    // Navegación y estado de menús
    AppState appState = STATE_MAIN_MENU;
    AppState settingsReturnState = STATE_MAIN_MENU;
    AppState wifiReturnState = STATE_MAIN_MENU;
    AppState lastAppState = STATE_MAIN_MENU;
    float screenFade = 1.0f;

    int mainMenuOption = 0;
    bool mainMenuExpanded = false;
    float mainMenuExpandAnim = 0.0f;
    int settingsOption = 0;
    int capturingKeyIndex = -1;
    int settingsControlsMode = MODE_STD;
    float taikoGuideTimer = 0.0f;
    int selectedSet = 0;
    int selectedDiff = 0;

    bool modMenuOpen = false;
    int modCursor = 0;
    int modTab = 0;
    bool deleteConfirmOpen = false;

    bool searchKeyboardOpen = false;
    int searchLastKeyIndex = -1;
    int searchKeyFlashTimer = 0;
    int searchBackspaceHoldTimer = 0;
    float footerCardLift[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int footerCardHover = -1;

    int songSelectTab = 0;
    bool songInfoHidden = false;
    bool rankingExpanded = false;
    int rankingScroll = 0;
    bool selectingReplayMode = false;
    int selectedRecordIdx = 0;
    float songSelectCardAnim = 1.0f;

    // Transición estilo osu!lazer y carga escalonada al iniciar una canción
    bool inSongIntro = false;
    int songIntroTick = 0;
    int songIntroLoadStage = 0;
    int songIntroLeadInMs = 1500;
    std::string songIntroOsuPath = "";
    std::string songIntroTitle = "";
    std::string songIntroArtist = "";
    std::string songIntroVersion = "";
    std::string songIntroCreator = "";
    float songIntroStars = 0.0f;
    float songIntroCS = 5.0f, songIntroAR = 5.0f, songIntroOD = 5.0f, songIntroHP = 5.0f;
    int songIntroMode = 0;

    bool calibrationMode = false;
    u64 calibStartTick = 0;
    int calibLastBeatIndex = -1;
    std::vector<int> calibSamples;

    // Estado de juego activo
    bool isPaused = false;
    int pauseOption = 0;
    bool stageFailed = false;
    bool newRecordAchieved = false;
    float lastEarnedPP = 0.0f;
    float hp = 1.0f;

    float cursorOsuX = 256.0f;
    float cursorOsuY = 192.0f;
    float prevCursorOsuX = 256.0f;
    float prevCursorOsuY = 192.0f;
    std::vector<TrailPoint> cursorTrail;
    std::vector<SmokeParticle> smokeParticles;
    u64 gameplayFallbackStartTick = 0;
    int combo = 0;
    int prevCombo = 0;
    int lastBrokenCombo = 0;
    int maxCombo = 0;
    int score = 0;
    int count300 = 0, count100 = 0, count50 = 0, countMiss = 0;
    int k1PressCount = 0, k2PressCount = 0;
    float comboScale = 1.0f;
    float comboBreakAnim = 0.0f;
    std::vector<HitPopup> popups;
    std::vector<GameplayParticle> particles;
    std::vector<HitErrorTick> hitErrorTicks;
    std::vector<float> allHitErrors;
    std::vector<HitJudgementEvent> judgementHistory;
    float hitErrorNeedle = 0.0f;
    float lastUnstableRate = 0.0f;
    float lastMeanHitError = 0.0f;
    float spinnerRPM = 0.0f;
    float spinnerClearAnim = 0.0f;
    float spinnerBonusAnim = 0.0f;
    int lastSpinnerBonus = 0;
    float shakeTimer = 0.0f;
    float shakeIntensity = 0.0f;
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    int animTick = 0;
    float carouselPos = 0.0f;

    std::vector<ScoreRecord> localRecords;
    float overtakeFlashAnim = 0.0f;
    bool hasOvertakenBest = false;
    bool skipAvailable = false;
    int skipTargetMs = 0;
    bool inBreakPeriod = false;
    int breakRemainingMs = 0;
    int breakTotalMs = 0;
    Vec2 breakNextObjPos = {256.0f, 192.0f};
    bool inKiai = false;
    bool prevInKiai = false;
    float kiaiGlow = 0.0f;
    float kiaiBurstFlash = 0.0f;
    int comboBurstTimer = 0;
    int comboBurstSide = 0;
    int comboBurstValue = 0;
    int lastComboBurstMilestone = 0;
    int lastNcStep = -999999;

    // Estado de entrada por fotograma y 3D
    bool stereoEnabledState = false;
    int analogNavCooldown = 0;
    u32 kDown = 0;
    u32 kHeld = 0;
    u32 kUp   = 0;
    circlePosition cpad = {0, 0};
    circlePosition cstick = {0, 0};
    float slider3D = 0.0f;
    bool stereoActive = false;
    touchPosition touch = {0, 0};
    bool isTouching = false;
    bool touchJustDown = false;
    bool touchJustUp   = false;

    const SkinProfile& getActiveSkin() const {
        return skinMgr.skins[settings.skinIndex % skinMgr.skins.size()];
    }

    // Ciclo de vida y utilidades compartidas
    void init();
    void beginFrame();
    void shutdown();

    void applyActiveSkinAudio();
    void spawnParticles(float osuX, float osuY, u32 color, int count, float minSpd, float maxSpd, float baseSize, int maxLifeFrames);
    void checkComboBurst(int curCombo);
    void triggerShake(float intensity, float frames);
    void updateSongPreview();
    void rotateMenuSong();
    float getBpmPulse();
    void toggleSearchTag(const std::string& tag);
    void openSystemKeyboard();
    void openSearchKeyboard();
    bool startSelectedBeatmap();
    void toggleModByIndex(int idx);
    std::string findLocalMapFile(const std::string& folder, const std::string& file);
    // Sistema de repeticiones (Replays)
    ReplayData currentReplay;
    bool isReplayPlaying = false;
    size_t replayPlaybackIndex = 0;
    std::string replayToastMessage = "";
    int replayToastTimer = 0;

    void triggerSuddenDeathFail();
    bool playReplay(const ReplayData& rep);
    bool playReplayFromFile(const std::string& path);
};
