#pragma once
#include <3ds.h>
#include <cstdint>
#include <cstring>

enum AppLanguage {
    LANG_ES = 0, // Español
    LANG_EN = 1, // English
    LANG_RU = 2, // Русский
    LANG_PL = 3, // Polski
    LANG_COUNT = 4
};

namespace Localization {

enum StringId {
    // General
    STR_BACK,
    STR_SAVE,
    STR_DEFAULT,
    STR_CALIBRATE,
    STR_RESET,
    STR_READY,
    STR_SEARCH,
    STR_DELETE,
    STR_CLEAR,
    STR_DONE,
    STR_RESUME,
    STR_RETRY,
    STR_SONGS,
    STR_SCORE,
    STR_ACCURACY,
    STR_MAX_COMBO,
    STR_DIFFICULTY,
    STR_STARS,
    STR_OBJECTS,
    STR_MODE,
    STR_SUMMARY,
    STR_LOCAL_RANKING,
    STR_EXPAND,
    STR_COLLAPSE,
    STR_ON,
    STR_OFF,
    STR_SELECT_ACTION,
    STR_MOVE_ACTION,
    STR_TOUCH_ACTION,

    // Main Menu
    STR_MENU_PLAY,
    STR_MENU_MULTI,
    STR_MENU_WIFI,
    STR_MENU_SETTINGS,
    STR_MENU_EXIT,
    STR_MENU_PILL_SOLO,
    STR_MENU_PILL_MULTI,
    STR_MENU_PILL_WIFI,
    STR_MENU_PILL_SETTINGS,
    STR_MENU_PILL_EXIT,
    STR_MENU_MUSIC_NOW,

    // Settings Tabs
    STR_TAB_TABLET,
    STR_TAB_CONTROLS,
    STR_TAB_AUDIO,
    STR_TAB_GAME,
    STR_TAB_VISUAL,

    // Settings Items
    STR_SET_INPUT_MODE,
    STR_SET_INPUT_MODE_DESC,
    STR_SET_TABLET_SIZE,
    STR_SET_POS_X,
    STR_SET_POS_Y,
    STR_SET_MUSIC_VOL,
    STR_SET_SFX_VOL,
    STR_SET_OFFSET,
    STR_SET_MAP_HITSOUNDS,
    STR_SET_MAP_HITSOUNDS_DESC,
    STR_SET_LANGUAGE,
    STR_SET_LANGUAGE_DESC,
    STR_SET_MAIN_MODE,
    STR_SET_MAIN_MODE_DESC,
    STR_SET_ACTIVE_SKIN,
    STR_SET_SNAKING,
    STR_SET_SNAKING_DESC,
    STR_SET_BURSTS,
    STR_SET_BURSTS_DESC,
    STR_SET_HIT300,
    STR_SET_HIT300_DESC,
    STR_SET_TAIKO_BG,
    STR_SET_CURSOR_SIZE,
    STR_SET_BG_DIM,
    STR_SET_VIDEOS,
    STR_SET_VIDEOS_DESC,
    STR_SET_MULTI_DL,
    STR_SET_MULTI_DL_DESC,
    STR_SET_HELP_BAR,

    // Settings Controls Tab
    STR_CTRL_LIVE_TEST,
    STR_CTRL_ASSIGNING,
    STR_CTRL_PRESS_ANY,
    STR_CTRL_CANCEL_HINT,
    STR_CTRL_ASSIGN_KEY_TITLE,
    STR_CTRL_TAP_TO_ASSIGN,

    // Calibration
    STR_CALIB_TITLE,
    STR_CALIB_DESC,
    STR_CALIB_APPLY_HINT,
    STR_CALIB_STATUS_FMT,

    // Song Selection
    STR_SONG_SELECT_TITLE,
    STR_SONG_NO_RECORD,
    STR_SONG_PLAY_NOW,
    STR_SONG_DIFF_DENSITY,
    STR_SONG_NO_RECORDS_YET,
    STR_SONG_PLAY_TO_RECORD,
    STR_SONG_CLOSE_HINT,
    STR_SONG_NO_SEARCH_RESULTS,
    STR_SONG_TAP_SEARCH_CLEAR,
    STR_SONG_FILTER_BADGE,
    STR_SEARCH_KEYBOARD_HINT,
    STR_DELETE_CONFIRM_TITLE,
    STR_DELETE_CONFIRM_DESC,

    // Mods
    STR_MODS_TITLE,
    STR_MODS_MULTIPLIER,
    STR_MODS_INFO,
    STR_MODS_DIFF_REDUCTION,
    STR_MODS_DIFF_INCREASE,
    STR_MODS_ASSIST,
    STR_MODS_CONTROLS_HINT,
    STR_MODS_NOT_ONLINE,

    // Playing & Pause
    STR_GAME_PAUSED,
    STR_PAUSE_RESUME,
    STR_PAUSE_RETRY,
    STR_PAUSE_QUIT,
    STR_SKIP_HINT,
    STR_WATCH_OUT,

    // Results
    STR_RESULTS_TITLE,
    STR_RESULTS_NEW_RECORD,
    STR_RESULTS_FAILED,
    STR_RESULTS_RETRY_HINT,
    STR_RESULTS_TAB_STRAIN,
    STR_RESULTS_TAB_RANKING,
    STR_RESULTS_NO_SCORES_SD,
    STR_RESULTS_CLEARED_DESC,
    STR_RESULTS_FAILED_DESC,

    // Wi-Fi Screen
    STR_WIFI_HEADER,
    STR_WIFI_TITLE,
    STR_WIFI_BROWSER_PROMPT,
    STR_WIFI_IP_CONNECTING,
    STR_WIFI_IP_NONE,
    STR_WIFI_STEP1,
    STR_WIFI_STEP2,
    STR_WIFI_FOOTER_FMT,
    STR_WIFI_CONNECTING,
    STR_WIFI_SEARCHING,
    STR_WIFI_SIG_EXCELLENT,
    STR_WIFI_SIG_GOOD,
    STR_WIFI_SIG_WEAK,
    STR_WIFI_SIG_NONE,
    STR_WIFI_SCAN_MOBILE,
    STR_WIFI_AIM_CAM,
    STR_WIFI_OPEN_WEB,
    STR_WIFI_DL_AUDIO,
    STR_WIFI_STAR_FILTER,
    STR_WIFI_RESCAN_BTN,
    STR_WIFI_RESCAN_SUB,
    STR_WIFI_BACK_BTN,
    STR_WIFI_SAVE_SUB,

    // Multiplayer
    STR_MULTI_ROOMS_UDS,
    STR_MULTI_ROOMS_LAN,
    STR_MULTI_CREATE_ROOM,
    STR_MULTI_JOIN,
    STR_MULTI_ROOM_FULL,
    STR_MULTI_WAITING,
    STR_MULTI_READY_TAG,
    STR_MULTI_START_MATCH,
    STR_MULTI_DOWNLOAD_MAP,
    STR_MULTI_ALL_READY,
    STR_MULTI_WAITING_PLAYERS,
    STR_MULTI_WAITING_HOST,

    STR_COUNT
};

inline AppLanguage& currentLanguage() {
    static AppLanguage s_lang = LANG_ES;
    return s_lang;
}

inline AppLanguage getLanguage() {
    return currentLanguage();
}

inline void setLanguage(AppLanguage lang) {
    if (lang >= 0 && lang < LANG_COUNT) {
        currentLanguage() = lang;
    }
}

inline const char* getLanguageName(int lang) {
    switch (lang) {
        case LANG_ES: return "Español";
        case LANG_EN: return "English";
        case LANG_RU: return "Русский";
        case LANG_PL: return "Polski";
        default:      return "English";
    }
}

inline AppLanguage detectConsoleLanguage() {
    u8 sysLang = CFG_LANGUAGE_EN;
    u8 countryId = 0;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetSystemLanguage(&sysLang);
        CFGU_GetConfigInfoBlk2(1, 0x000B0000, &countryId);
        cfguExit();
    }
    if (sysLang == CFG_LANGUAGE_ES) {
        return LANG_ES;
    }
    if (sysLang == CFG_LANGUAGE_RU) {
        return LANG_RU;
    }
    // Poland country detection (country ID 100 on European 3DS)
    if (countryId == 100) {
        return LANG_PL;
    }
    if (countryId != 0 && R_SUCCEEDED(cfguInit())) {
        u16 codeStr = 0;
        if (R_SUCCEEDED(CFGU_GetCountryCodeString(countryId, &codeStr))) {
            char c0 = static_cast<char>(codeStr & 0xFF);
            char c1 = static_cast<char>((codeStr >> 8) & 0xFF);
            if ((c0 == 'P' || c0 == 'p') && (c1 == 'L' || c1 == 'l')) {
                cfguExit();
                return LANG_PL;
            }
        }
        cfguExit();
    }
    return LANG_EN;
}

// Translations table [LANG_COUNT][STR_COUNT]
extern const char* const STRINGS[LANG_COUNT][STR_COUNT];

inline const char* get(StringId id) {
    if (id < 0 || id >= STR_COUNT) return "";
    int lang = static_cast<int>(currentLanguage());
    if (lang < 0 || lang >= LANG_COUNT) lang = LANG_ES;
    return STRINGS[lang][id];
}

} // namespace Localization

#define LOC(id) (Localization::get(Localization::id))
