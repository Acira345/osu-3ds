#include <3ds.h>
#include <citro2d.h>
#include "AppContext.hpp"
#include "MainMenuScreen.hpp"
#include "WifiTransferScreen.hpp"
#include "SettingsScreen.hpp"
#include "SongSelectScreen.hpp"
#include "ResultsScreen.hpp"
#include "MultiBrowserScreen.hpp"
#include "MultiLobbyScreen.hpp"
#include "ModsSelectScreen.hpp"
#include "MultiResultsScreen.hpp"
#include "MultiSyncScreen.hpp"
#include "GameplayScreen.hpp"
#include "SplashScreen.hpp"

// Aumentar pila del hilo principal en la 3DS a 256 KB (por defecto libctru asigna solo 32 KB)
extern "C" {
    u32 __stacksize__ = 0x40000;
}

int main(int argc, char* argv[]) {
    osSetSpeedupEnable(true); // Activar 804 MHz + Cache L2 en consolas New Nintendo 3DS / New 2DS XL
    gfxInitDefault();
    gfxSet3D(false);          // Se activa dinamicamente solo cuando el slider 3D fisico > 0
    romfsInit();
    acInit();                 // Iniciar servicio de acceso a red para mantener activa la conexion Wi-Fi
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
    C2D_Prepare();

    AppContext ctx;
    ctx.init();
    runSplashScreen(ctx);

    while (aptMainLoop()) {
        ctx.beginFrame();

        switch (ctx.appState) {
            case STATE_MAIN_MENU:
                runMainMenuScreen(ctx);
                break;
            case STATE_WIFI_TRANSFER:
                runWifiTransferScreen(ctx);
                break;
            case STATE_SETTINGS:
                runSettingsScreen(ctx);
                break;
            case STATE_SONG_SELECT:
                runSongSelectScreen(ctx);
                break;
            case STATE_RESULTS:
                runResultsScreen(ctx);
                break;
            case STATE_MULTI_BROWSER:
                runMultiBrowserScreen(ctx);
                break;
            case STATE_MULTI_LOBBY:
                runMultiLobbyScreen(ctx);
                break;
            case STATE_MODS_SELECT:
                runModsSelectScreen(ctx);
                break;
            case STATE_MULTI_RESULTS:
                runMultiResultsScreen(ctx);
                break;
            case STATE_MULTI_SYNC:
                runMultiSyncScreen(ctx);
                break;
            case STATE_PLAYING:
                runGameplayScreen(ctx);
                break;
        }

        if (ctx.shouldQuit) {
            break;
        }
    }

    ctx.shutdown();
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    acExit();
    gfxExit();
    osSetSpeedupEnable(false);
    return 0;
}
