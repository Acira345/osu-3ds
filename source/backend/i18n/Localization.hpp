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

    // Controls & Settings Extra
    STR_SET_TAIKO_BG_DESC,
    STR_CTRL_HELP_LIVE,
    STR_CTRL_TOUCH_1TO1,
    STR_CTRL_TOUCH_1TO1_DESC,
    STR_CTRL_REASSIGN_TITLE,
    STR_CTRL_REASSIGN_HINT,
    STR_CTRL_REASSIGN_SUB,

    // Song Select Bottom Cards & Badges
    STR_CARD_TABLET,
    STR_CARD_TOUCH,
    STR_CARD_ROOM,
    STR_CARD_ORDER,
    STR_CARD_UP,
    STR_CARD_DOWN,
    STR_CARD_HOST,
    STR_CARD_GUEST,
    STR_CARD_FILTER,
    STR_CARD_TITLE,
    STR_CARD_ARTIST,
    STR_CARD_STARS,
    STR_CARD_RECORD,
    STR_CARD_STANDARD,
    STR_SONG_NO_BEATMAPS,
    STR_SONG_PUT_FILES,
    STR_SONG_MAP_BY,
    STR_SONG_NO_FILTER_MATCH,
    STR_SONG_ALL,
    STR_SONG_VIEW_TOP,
    STR_SONG_BEST_RECORD,
    STR_RESULTS_VIEW_TOP,
    STR_RESULTS_STRAIN_CURVE,

    // Multiplayer Expanded
    STR_MULTI_SEARCH_UDS,
    STR_MULTI_NO_NET_REQ,
    STR_MULTI_PRESS_X_CREATE,
    STR_MULTI_ANY_JOIN,
    STR_MULTI_SEARCH_LAN,
    STR_MULTI_NO_ACTIVE_ROOMS,
    STR_MULTI_ROOM_WILL_APPEAR,
    STR_MULTI_NO_ROOMS_FOUND,
    STR_MULTI_CREATE_ROOM_TOUCH,
    STR_MULTI_ANY_3DS_JOIN,
    STR_MULTI_DETAILS,
    STR_MULTI_RULE,
    STR_MULTI_MODS_LABEL,
    STR_MULTI_MAP_LABEL,
    STR_MULTI_SLOT_OPEN,
    STR_MULTI_CHANGING_MAP,
    STR_MULTI_HOST_CHOOSING,
    STR_MULTI_NEXT_MAP,
    STR_MULTI_QUEUE_EMPTY,
    STR_MULTI_TAP_TO_ADD,
    STR_MULTI_ROLE,
    STR_MULTI_CLIENT,
    STR_MULTI_TOUCH_PANEL,
    STR_MULTI_WIN_CRITERIA,
    STR_MULTI_WIN_SCORE,
    STR_MULTI_WIN_ACC,
    STR_MULTI_WIN_COMBO,
    STR_MULTI_CHANGE_MAP,
    STR_MULTI_BLOCKED,
    STR_MULTI_ACTIVE_MAP,
    STR_MULTI_DL_ACTIVE,
    STR_MULTI_TAP_CHANGE,
    STR_MULTI_ADD_QUEUE,
    STR_MULTI_ENQUEUE,
    STR_MULTI_TAP_QUEUE,
    STR_MULTI_TAP_OPEN,
    STR_MULTI_MODS_MENU,
    STR_MULTI_SUGGEST_MAP,
    STR_MULTI_FOR_ROOM,
    STR_MULTI_DL_MAP_TAP,
    STR_MULTI_UNREADY,
    STR_MULTI_READY_TAP,
    STR_MULTI_BACK_TO_ROOM,
    STR_MULTI_RESULTS_TITLE,
    STR_MULTI_WAITING_ALL,
    STR_MULTI_RETURN_LOBBY,
    STR_MULTI_MATCH_FINISHED,
    STR_MULTI_WAIT_HOST,
    STR_MULTI_LOADING_RESOURCES,
    STR_MULTI_PREP_MAP,
    STR_MULTI_WAIT_OTHER_CONSOLES,
    STR_MULTI_SAME_TIME,
    STR_MULTI_SYNC_CLOCK,
    STR_MULTI_FAIL,
    STR_MULTI_YOU,
    STR_MULTI_TAB_HITS,
    STR_MULTI_SEL_PLAYER,
    STR_MULTI_REL_ACC,

    // Multiplayer Toasts
    STR_TOAST_HOST_CLOSED,
    STR_TOAST_LOST_CONN,
    STR_TOAST_PLAYER_DC,
    STR_TOAST_PLAYER_LEFT,
    STR_TOAST_PLAYER_FAILED,

    // In-game HUD
    STR_GAME_SPIN,
    STR_GAME_BREAK,
    STR_GAME_SMOKE,
    STR_GAME_WARNING,

    // Extra UI (segunda pasada)
    STR_SPACE,
    STR_NOT_AVAIL,
    STR_MODS_IMPACT,
    STR_MODS_EQUIPPED,
    STR_POINTS,
    STR_MULTI_NEXT_IN_QUEUE,
    STR_MULTI_QUEUE_DONE,
    STR_MULTI_TABS,
    STR_MENU_SHORT,
    STR_BY,
    STR_GAME_CIRCLES_IN,
    STR_TABLET_AREA_FMT,
    STR_PRESS_SHORT,
    STR_PREVIEW,
    STR_KEY_K1,
    STR_KEY_K2,
    STR_KEY_SMOKE,
    STR_KEY_RETRY,
    STR_KEY_DON_L,
    STR_KEY_DON_R,
    STR_KEY_KAT_L,
    STR_KEY_KAT_R,
    STR_KEY_MOVE_L,
    STR_KEY_MOVE_R,
    STR_KEY_DASH,
    STR_KEY_COL_FMT,
    STR_TAIKO_RED,
    STR_TAIKO_BLUE,
    STR_SONG_MULTI_QUEUE,
    STR_SONG_MULTI_PICK,
    STR_SEARCH_PLACEHOLDER,
    STR_SEARCH_SWKBD_HINT,
    STR_CATCH_FRUITS,
    STR_CATCH_DROPS,
    STR_CATCH_DROPLETS,
    STR_WIFI_UPDATING_LIB,
    STR_WIFI_NETWORK,
    STR_WIFI_LIB_UPDATED,
    STR_OFFSET_CALIB_HINT,
    STR_MULTI_ROOMS_COUNT,

    // Mods: nombres cortos (orden de modTable)
    STR_MOD_NAME_0, STR_MOD_NAME_1, STR_MOD_NAME_2, STR_MOD_NAME_3, STR_MOD_NAME_4,
    STR_MOD_NAME_5, STR_MOD_NAME_6, STR_MOD_NAME_7, STR_MOD_NAME_8, STR_MOD_NAME_9,
    STR_MOD_NAME_10, STR_MOD_NAME_11, STR_MOD_NAME_12, STR_MOD_NAME_13, STR_MOD_NAME_14,

    // Mods: descripciones (orden de modTable)
    STR_MOD_DESC_0, STR_MOD_DESC_1, STR_MOD_DESC_2, STR_MOD_DESC_3, STR_MOD_DESC_4,
    STR_MOD_DESC_5, STR_MOD_DESC_6, STR_MOD_DESC_7, STR_MOD_DESC_8, STR_MOD_DESC_9,
    STR_MOD_DESC_10, STR_MOD_DESC_11, STR_MOD_DESC_12, STR_MOD_DESC_13, STR_MOD_DESC_14,

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

// Acceso por indice relativo (p. ej. STR_MOD_NAME_0 + n)
inline const char* getIndexed(StringId base, int offset) {
    return get(static_cast<StringId>(static_cast<int>(base) + offset));
}

} // namespace Localization

#define LOC(id) (Localization::get(Localization::id))
