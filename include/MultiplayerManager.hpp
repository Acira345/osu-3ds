#pragma once
#include <3ds.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include "Beatmap.hpp"
#include "NetCommon.hpp"
#include "OszExtractor.hpp"

// ============================================================================
// CONSTANTES Y PUERTOS DE RED MULTIJUGADOR (UDS / LAN)
// ============================================================================
static constexpr int MULTI_MAX_PLAYERS = 4;
static constexpr int MULTI_MAX_QUEUE   = 4; // Optimizado a 4 para garantizar que todo paquete quepa en un frame UDS (<= 1478 bytes)
static constexpr uint16_t MULTI_UDP_PORT = 34567;
static constexpr uint16_t MULTI_TCP_PORT = 34568;
static constexpr uint32_t MULTI_MAGIC_UDP = 0x4F535533; // "OSU3"
static constexpr uint32_t MULTI_MAGIC_TCP = 0x4F535544; // "OSUD"

// Constantes exclusivas de UDS (Local Wireless 3DS)
static constexpr u32 UDS_APP_WLANCOMMID  = 0x4F535533; // Identificador de comunicación inalámbrica local osu!3DS
static constexpr u8  UDS_DATA_CHANNEL    = 1;          // Canal de filtrado de datos
static constexpr u32 UDS_RECV_BUF_SIZE   = UDS_DEFAULT_RECVBUFSIZE; // 0x2E30 (~11 KB)
static const char*   UDS_PASSPHRASE      = "osu3ds_uds";

// Reglas de la sala
enum WinCondition : uint8_t {
    WIN_SCORE    = 0, // Gana por Puntuación Total
    WIN_ACCURACY = 1, // Gana por Precisión (Acc %)
    WIN_COMBO    = 2  // Gana por Combo Máximo
};

enum ModMode : uint8_t {
    MODS_HOST_ONLY = 0, // Todos juegan con los mods impuestos por el Host
    MODS_FREEMOD   = 1  // Cada jugador elige libremente sus mods (HD, HR, EZ, NF)
};

enum PlayerStatus : uint8_t {
    SLOT_EMPTY         = 0,
    STATUS_NOT_READY   = 1, // Esperando / Configurando
    STATUS_READY       = 2, // Listo para jugar
    STATUS_MISSING_MAP = 3, // No tiene el mapa instalado en la SD
    STATUS_DOWNLOADING = 4, // Descargando mapa por UDS / TCP
    STATUS_LOADED      = 5  // Recursos cargados en RAM, listo para sincronizar inicio
};

// ============================================================================
// ESTRUCTURA: CANCIÓN EN COLA (MAP QUEUE)
// ============================================================================
struct QueuedBeatmap {
    std::string title;
    std::string artist;
    std::string version;
    std::string creator;
    std::string folderPath;
    std::string osuFilename;
    std::string audioFilename;
    float starRating;
    float bpm;
    int lengthSec;
    int suggestedBySlot;

    QueuedBeatmap()
        : title("Desconocido"), artist("Desconocido"), version("Normal"),
          creator(""), folderPath(""), osuFilename(""), audioFilename("audio.mp3"),
          starRating(1.0f), bpm(120.0f), lengthSec(90), suggestedBySlot(0) {}

    QueuedBeatmap(const std::string& _t, const std::string& _a, const std::string& _v,
                  const std::string& _c, const std::string& _fp, const std::string& _of,
                  const std::string& _af, float _sr, float _bpm, int _len, int _slot)
        : title(_t), artist(_a), version(_v), creator(_c),
          folderPath(_fp), osuFilename(_of), audioFilename(_af),
          starRating(_sr), bpm(_bpm), lengthSec(_len), suggestedBySlot(_slot) {}
};

// ============================================================================
// ESTRUCTURA: JUGADOR EN LA SALA / PARTIDA
// ============================================================================
struct NetworkPlayer {
    bool active;
    bool isHost;
    bool isLocal;
    char name[24];
    uint32_t ip;
    uint16_t port;
    PlayerStatus status;
    uint32_t activeMods;
    uint8_t downloadProgress; // 0..100 %
    uint32_t lastPacketTick;
    uint16_t pingMs;

    // Estadísticas de juego en vivo y resultados
    int32_t score;
    int16_t combo;
    int16_t maxCombo;
    float accuracy;
    float hp;
    char grade[4];
    int16_t count300;
    int16_t count100;
    int16_t count50;
    int16_t countMiss;
    bool isFinished;
    bool isFailed;

    NetworkPlayer()
        : active(false), isHost(false), isLocal(false), ip(0), port(0),
          status(SLOT_EMPTY), activeMods(MOD_NONE), downloadProgress(0),
          lastPacketTick(0), pingMs(10),
          score(0), combo(0), maxCombo(0), accuracy(100.0f), hp(1.0f),
          count300(0), count100(0), count50(0), countMiss(0),
          isFinished(false), isFailed(false)
    {
        name[0] = '\0';
        std::strcpy(grade, "SS");
    }

    NetworkPlayer(int _slot, bool _isHost, bool _isLocal, const char* _name)
        : active(true), isHost(_isHost), isLocal(_isLocal), ip(0), port(0),
          status(STATUS_NOT_READY), activeMods(MOD_NONE), downloadProgress(0),
          lastPacketTick(0), pingMs(8),
          score(0), combo(0), maxCombo(0), accuracy(100.0f), hp(1.0f),
          count300(0), count100(0), count50(0), countMiss(0),
          isFinished(false), isFailed(false)
    {
        std::strncpy(name, _name ? _name : "Jugador", sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        std::strcpy(grade, "SS");
    }

    void resetForMatch() {
        score = 0;
        combo = 0;
        maxCombo = 0;
        accuracy = 100.0f;
        hp = 1.0f;
        count300 = 0;
        count100 = 0;
        count50 = 0;
        countMiss = 0;
        isFinished = false;
        isFailed = false;
        std::strcpy(grade, "SS");
    }
};

// ============================================================================
// PAQUETES BINARIOS DE RED (UDP & UDS PACKETS)
// Todos empaquetados estrictamente para cumplir límites de tamaño de trama UDS (<= 1478 bytes)
// ============================================================================
#pragma pack(push, 1)

enum PacketType : uint8_t {
    PKT_DISCOVER_PING  = 1,
    PKT_LOBBY_STATE    = 2,
    PKT_PLAYER_UPDATE  = 3,
    PKT_START_MATCH    = 4,
    PKT_LIVE_GAMEPLAY  = 5,
    PKT_QUEUE_ADD      = 6,
    PKT_MATCH_FINISH   = 7,
    PKT_FILE_REQ       = 8,  // Solicitud de descarga de mapa por UDS
    PKT_FILE_HEADER    = 9,  // Cabecera de metadatos de mapa para UDS
    PKT_FILE_CHUNK     = 10, // Fragmento de archivo binario transmitido por UDS
    PKT_FILE_ACK       = 11, // Confirmación / solicitud de siguiente ráfaga de chunks UDS
    PKT_ROOM_CLOSED    = 12, // El host ha cerrado la sala (devuelve a invitados al menú/browser)
    PKT_PLAYER_LEAVE   = 13, // Un jugador abandona voluntariamente la sala
    PKT_RETURN_LOBBY   = 14, // Sincronización para volver de resultados al lobby todos juntos
    PKT_MATCH_LOADED   = 15, // Jugador cargó recursos y está listo para sincronizar inicio
    PKT_MATCH_GO       = 16  // Host ordena inicio simultáneo de gameplay
};

struct NetHeader {
    uint32_t magic; // MULTI_MAGIC_UDP
    uint8_t type;
    uint8_t senderSlot;
    uint16_t seq;
};

struct NetSlotInfo {
    uint8_t active;
    uint8_t isHost;
    uint8_t status;
    uint8_t downloadProgress;
    char name[20];
    uint32_t ip;
    uint32_t mods;
    uint16_t pingMs;
};

struct NetQueueItem {
    char title[28];
    char artist[20];
    char version[20];
    char folder[56];
    char osuFile[64];
    char audioFile[24];
    float starRating;
    float bpm;
    uint16_t lengthSec;
};

// Estado completo del Lobby enviado por el Host
struct PacketLobbyState {
    NetHeader header;
    char roomTitle[28];
    uint8_t hostSlot;
    uint8_t winCondition;
    uint8_t modMode;
    uint8_t hostRotate;
    uint8_t matchInProgress; // 1 si la partida está en curso / empezando, 0 si estamos en el lobby
    uint8_t hostChangingSong; // 1 si el anfitrión está eligiendo/cambiando canción, 0 normal
    uint32_t matchStartCountdown;

    // Mapa Activo
    char mapTitle[40];
    char mapArtist[28];
    char mapVersion[28];
    char mapCreator[20];
    char mapFolder[64];
    char mapOsuFile[80];
    char mapAudioFile[32];
    float mapStars;
    float mapBpm;
    uint16_t mapLengthSec;

    // 4 Slots
    NetSlotInfo slots[MULTI_MAX_PLAYERS];

    // Fila de canciones (MapQueue)
    uint8_t queueCount;
    NetQueueItem queue[MULTI_MAX_QUEUE];
};
static_assert(sizeof(PacketLobbyState) <= 1478, "PacketLobbyState exceeds UDS MTU (1478 bytes)");

// Actualización enviada por cada Cliente
struct PacketPlayerUpdate {
    NetHeader header;
    char name[20];
    uint8_t status;
    uint8_t downloadProgress;
    uint32_t mods;
};

// Inicio sincronizado de partida
struct PacketStartMatch {
    NetHeader header;
    uint32_t startTick;
};

// Sincronización en vivo de Gameplay
struct PacketLiveGameplay {
    NetHeader header;
    int32_t score;
    int16_t combo;
    int16_t maxCombo;
    float accuracy;
    float hp;
    char grade[4];
    int16_t c300;
    int16_t c100;
    int16_t c50;
    int16_t cMiss;
    uint8_t isFinished;
    uint8_t isFailed;
};

// Petición para encolar canción
struct PacketQueueAdd {
    NetHeader header;
    NetQueueItem item;
};

// Orden de inicio simultáneo de partida enviada por el Host
struct PacketMatchGo {
    NetHeader header;
    uint32_t startDelayMs;
};

// ============================================================================
// PAQUETES EXCLUSIVOS DE TRANSFERENCIA DE MAPAS POR UDS
// ============================================================================
struct PacketFileReq {
    NetHeader header;
};

struct PacketFileHeader {
    NetHeader header;
    uint32_t totalBytes;
    uint32_t totalChunks;
    char folderName[64];
    char osuFileName[96];
    uint32_t osuFileSize;
    char audioFileName[48];
    uint32_t audioFileSize;
    char bgFileName[48];
    uint32_t bgFileSize;
};
static_assert(sizeof(PacketFileHeader) <= 1478, "PacketFileHeader exceeds UDS MTU (1478 bytes)");

struct PacketFileChunk {
    NetHeader header;
    uint32_t chunkIndex;
    uint32_t totalChunks;
    uint16_t chunkSize;
    uint8_t  fileTarget; // 0 = .osu, 1 = audio (.mp3/.ogg)
    uint8_t  payload[1024];
};

struct PacketFileAck {
    NetHeader header;
    uint32_t requestedChunkIndex;
};

// Cabecera para transferencia clásica de archivos por TCP (Modo LAN)
struct TcpFileHeader {
    uint32_t magic; // MULTI_MAGIC_TCP
    uint32_t totalBytes;
    char folderName[64];
    char osuFileName[96];
    uint32_t osuFileSize;
    char audioFileName[48];
    uint32_t audioFileSize;
};

// Metadatos de difusión en Beacons UDS (máx 200 bytes permitidos por el SO 3DS)
struct UdsBeaconData {
    uint32_t magic;           // MULTI_MAGIC_UDP
    char     hostName[16];
    char     roomTitle[24];
    char     mapTitle[32];
    char     mapArtist[24];
    char     mapVersion[20];
    float    mapStars;
    uint8_t  playerCount;
    uint8_t  maxPlayers;
    uint8_t  winCondition;
    uint8_t  modMode;
    uint8_t  matchInProgress;
};

#pragma pack(pop)

// Verificación estática en compilación: asegurar que ningún paquete exceda el MTU UDS de 1478 bytes
static_assert(sizeof(PacketLobbyState) <= 1478, "PacketLobbyState excede UDS_DATAFRAME_MAXSIZE");
static_assert(sizeof(PacketFileChunk) <= 1478, "PacketFileChunk excede UDS_DATAFRAME_MAXSIZE");
static_assert(sizeof(UdsBeaconData) <= 200, "UdsBeaconData excede el limite de appdata de 200 bytes");

// ============================================================================
// FUNCIONES DE RESOLUCIÓN DE RUTAS DE ARCHIVOS DE BEATMAPS
// ============================================================================
inline std::string extractBaseFilename(const std::string& path) {
    if (path.empty()) return "";
    size_t slash = path.find_last_of("/\\");
    return (slash != std::string::npos) ? path.substr(slash + 1) : path;
}

inline std::string extractFolderName(const std::string& folderPath) {
    if (folderPath.empty()) return "beatmap";
    std::string cleaned = folderPath;
    while (!cleaned.empty() && (cleaned.back() == '/' || cleaned.back() == '\\')) {
        cleaned.pop_back();
    }
    size_t slash = cleaned.find_last_of("/\\");
    return (slash != std::string::npos) ? cleaned.substr(slash + 1) : cleaned;
}

inline std::string resolveFilePath(const std::string& folder, const std::string& file) {
    if (file.empty()) return "";
    FILE* f = std::fopen(file.c_str(), "rb");
    if (f) { std::fclose(f); return file; }

    std::string baseFile = extractBaseFilename(file);
    std::string baseFolder = extractFolderName(folder);

    if (!folder.empty()) {
        std::string p = folder;
        if (p.back() != '/' && p.back() != '\\') p += "/";
        p += baseFile;
        f = std::fopen(p.c_str(), "rb");
        if (f) { std::fclose(f); return p; }
    }

    const char* baseSearchPaths[] = {
        "sdmc:/3ds/osu-3ds/songs/",
        "sdmc:/osu-3ds/songs/",
        "sdmc:/3ds/osu/Songs/",
        "sdmc:/3ds/osu/songs/",
        "romfs:/songs/",
        "romfs:/"
    };
    for (const char* bp : baseSearchPaths) {
        std::string pSd = std::string(bp) + baseFolder + "/" + baseFile;
        f = std::fopen(pSd.c_str(), "rb");
        if (f) { std::fclose(f); return pSd; }

        std::string pDirect = std::string(bp) + baseFile;
        f = std::fopen(pDirect.c_str(), "rb");
        if (f) { std::fclose(f); return pDirect; }
    }

    return "";
}

// ============================================================================
// ESTRUCTURA: SALA DESCUBIERTA (BROWSER / ROOM LIST)
// ============================================================================
enum MultiNetworkMode : uint8_t {
    NET_MODE_UDS = 0, // Inalámbrico 3DS (UDS nativo - Sin router, consola a consola)
    NET_MODE_LAN = 1  // Wi-Fi Local (Sockets TCP/UDP - Misma red local / Emulador)
};

struct DiscoveredRoom {
    MultiNetworkMode mode;
    udsNetworkStruct udsNetStruct;
    uint32_t hostIp;
    uint16_t hostPort;
    std::string roomTitle;
    std::string hostName;
    std::string mapTitle;
    std::string mapArtist;
    std::string mapVersion;
    float mapStars;
    int playerCount;
    int maxPlayers;
    WinCondition winCondition;
    ModMode modMode;
    uint32_t lastSeenTick;
    uint16_t pingMs;

    DiscoveredRoom()
        : mode(NET_MODE_UDS), hostIp(0), hostPort(MULTI_UDP_PORT), roomTitle("SALA MULTIJUGADOR"),
          hostName("Host"), mapTitle("Desconocido"), mapArtist("Desconocido"), mapVersion("Normal"),
          mapStars(1.0f), playerCount(1), maxPlayers(MULTI_MAX_PLAYERS),
          winCondition(WIN_SCORE), modMode(MODS_FREEMOD),
          lastSeenTick(0), pingMs(8)
    {
        std::memset(&udsNetStruct, 0, sizeof(udsNetStruct));
    }

    DiscoveredRoom(uint32_t _ip, uint16_t _port, const std::string& _title,
                   const std::string& _hname, const std::string& _mtitle,
                   const std::string& _martist, const std::string& _mver,
                   float _stars, int _count, int _maxP,
                   WinCondition _wc, ModMode _mm, uint32_t _tick, uint16_t _ping)
        : mode(NET_MODE_LAN), hostIp(_ip), hostPort(_port), roomTitle(_title), hostName(_hname),
          mapTitle(_mtitle), mapArtist(_martist), mapVersion(_mver),
          mapStars(_stars), playerCount(_count), maxPlayers(_maxP),
          winCondition(_wc), modMode(_mm), lastSeenTick(_tick), pingMs(_ping)
    {
        std::memset(&udsNetStruct, 0, sizeof(udsNetStruct));
    }
};

// ============================================================================
// ESTRUCTURA: CLASIFICACIÓN DE JUGADOR (LEADERBOARD / PODIO)
// ============================================================================
struct PlayerRank {
    int slot;
    int rank;
    const NetworkPlayer* player;

    PlayerRank() : slot(0), rank(1), player(nullptr) {}
    PlayerRank(int _s, int _r, const NetworkPlayer* _p) : slot(_s), rank(_r), player(_p) {}
};

// ============================================================================
// CLASE PRINCIPAL: MULTIPLAYER MANAGER (SOPORTE UDS & LAN)
// ============================================================================
class MultiplayerManager {
private:
    // Estado de sockets LAN
    bool socSessionActive = false;
    int udpSock = -1;
    int tcpListenSock = -1;
    int tcpClientSock = -1;
    int tcpTransferSock = -1;
    struct sockaddr_in bcastAddr;

    // Estado UDS (Native 3DS Wireless)
    bool udsInitialized = false;
    udsBindContext udsBindCtx;
    udsNetworkStruct activeUdsNetwork;
    u16 localNodeId = 0;
    uint32_t lastUdsScanTick = 0;

    // Hilo en segundo plano para escaneo no bloqueante de Beacons UDS (60 FPS y audio fluidos)
    Thread udsScanThread = nullptr;
    LightLock udsRoomsLock;
    LightEvent udsScanWake;
    volatile bool udsScanQuit = false;
    volatile bool udsScanActive = false;
    std::vector<DiscoveredRoom> udsDiscoveredRooms;
    bool udsRoomsUpdated = false;

    // Transferencia de archivos por UDS (Host/Peer sirviendo archivo)
    FILE* udsSrvOsuFile = nullptr;
    FILE* udsSrvAudioFile = nullptr;
    FILE* udsSrvBgFile = nullptr;
    uint32_t udsSrvOsuChunks = 0;
    uint32_t udsSrvAudioChunks = 0;
    uint32_t udsSrvBgChunks = 0;
    uint32_t udsSrvTotalChunks = 0;
    size_t udsSrvOsuSize = 0;
    size_t udsSrvAudioSize = 0;
    size_t udsSrvBgSize = 0;
    u16 udsSrvTargetNode = 0;

    // Transferencia de archivos por UDS (Peer descargando)
    FILE* udsDlOsuFile = nullptr;
    FILE* udsDlAudioFile = nullptr;
    FILE* udsDlBgFile = nullptr;
    std::string udsDlCurrentOsuPath = "";
    std::string udsDlCurrentAudioPath = "";
    std::string udsDlCurrentBgPath = "";
    uint32_t udsDlExpectedChunk = 0;
    uint32_t udsDlTotalChunks = 0;
    uint32_t udsDlTotalBytes = 0;
    size_t udsDlBytesWritten = 0;
    uint32_t udsDlLastChunkTick = 0;
    u16 udsDlSourceNode = UDS_HOST_NETWORKNODEID;
    uint32_t nodeDisconnectStartTick[MULTI_MAX_PLAYERS] = { 0, 0, 0, 0 };

    // Estado de transferencia TCP clásica (Modo LAN)
    enum TcpTransferState {
        TCP_IDLE,
        TCP_CONNECTING,
        TCP_READING_HEADER,
        TCP_READING_PAYLOAD,
        TCP_SERVING_SEND
    };
    TcpTransferState tcpState = TCP_IDLE;
    TcpFileHeader currentDlHeader;
    std::string currentDlOsuPath = "";
    std::string currentDlAudioPath = "";
    size_t dlBytesReceived = 0;
    FILE* dlOsuFile = nullptr;
    FILE* dlAudioFile = nullptr;
    size_t dlOsuBytesWritten = 0;
    size_t dlAudioBytesWritten = 0;

    FILE* srvOsuFile = nullptr;
    FILE* srvAudioFile = nullptr;
    size_t srvTotalSent = 0;
    size_t srvTotalToSend = 0;
    TcpFileHeader srvHeader;

    uint16_t packetSeq = 0;
    uint32_t lastBroadcastTick = 0;
    int gameplaySyncFrameCount = 0;

    static void setNonBlocking(int s) {
        int flags = fcntl(s, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(s, F_SETFL, flags | O_NONBLOCK);
        }
    }

public:
    MultiNetworkMode netMode = NET_MODE_UDS; // Modo por defecto: UDS 3DS Inalámbrico nativo

    bool isHosting = false;
    bool inMultiplayer = false;
    bool inBrowserMode = false;
    int localSlot = 0;
    int hostSlot = 0;

    std::string localPlayerName = "Player3DS";
    std::string roomTitle = "SALA INALAMBRICA 3DS";
    uint32_t connectedHostIp = 0;

    WinCondition winCondition = WIN_ACCURACY;
    ModMode modMode = MODS_FREEMOD;
    bool hostRotate = true;

    QueuedBeatmap activeBeatmap;
    std::string hostExactOsuPath;
    std::string hostExactAudioPath;
    std::vector<QueuedBeatmap> mapQueue;
    NetworkPlayer players[MULTI_MAX_PLAYERS];

    std::vector<DiscoveredRoom> discoveredRooms;
    int selectedRoomIndex = 0;
    uint32_t lastBrowserPingTick = 0;

    bool matchStarted = false;
    bool matchInProgressFlag = false;
    bool matchFinished = false;
    uint32_t matchStartCountdown = 0;

    std::string lastDownloadedOsuPath = "";
    std::string lastDownloadedAudioPath = "";
    std::string lastDownloadedBgPath = "";
    bool enableBgDownload = true;
    bool shouldReturnToLobby = false;
    bool hostChangingSong = false;
    bool matchGo = false;
    uint32_t matchGoDelayMs = 0;
    uint32_t lastHostPacketTick = 0;
    std::string toastNotice = "";
    uint32_t toastNoticeExpireTick = 0;

    void setToastNotice(const std::string& msg, uint32_t durationMs = 3500) {
        toastNotice = msg;
        toastNoticeExpireTick = osGetTime() + durationMs;
    }

    bool hasToastNotice() const {
        return (!toastNotice.empty() && osGetTime() < toastNoticeExpireTick);
    }

    bool areAllPlayersFinished() const {
        if (!inMultiplayer) return true;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && !players[i].isFinished) {
                return false;
            }
        }
        return true;
    }

    bool areAllPlayersLoaded() const {
        if (!inMultiplayer) return true;
        int count = 0;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active) {
                if (players[i].status != STATUS_LOADED) return false;
                count++;
            }
        }
        return count > 0;
    }

    int getLoadedPlayerCount() const {
        int count = 0;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && players[i].status == STATUS_LOADED) count++;
        }
        return count;
    }

    void sendMatchLoaded() {
        players[localSlot].status = STATUS_LOADED;
        NetHeader pkt;
        pkt.magic = MULTI_MAGIC_UDP;
        pkt.type = PKT_MATCH_LOADED;
        pkt.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.seq = ++packetSeq;
        if (isHost()) {
            broadcastLobbyState();
        } else {
            sendPacketToHost(&pkt, sizeof(pkt));
        }
    }

    void sendMatchGo(uint32_t delayMs = 500) {
        matchGo = true;
        matchGoDelayMs = delayMs;
        PacketMatchGo pkt;
        pkt.header.magic = MULTI_MAGIC_UDP;
        pkt.header.type = PKT_MATCH_GO;
        pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.header.seq = ++packetSeq;
        pkt.startDelayMs = delayMs;
        for (int burst = 0; burst < 4; ++burst) {
            sendPacketToAllClients(&pkt, sizeof(pkt));
        }
    }

    void notifyRoomClosed() {
        if (!isHost() || !inMultiplayer) return;
        NetHeader pkt;
        pkt.magic = MULTI_MAGIC_UDP;
        pkt.type = PKT_ROOM_CLOSED;
        pkt.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.seq = ++packetSeq;
        for (int burst = 0; burst < 8; ++burst) {
            sendPacketToAllClients(&pkt, sizeof(pkt));
            svcSleepThread(5 * 1000 * 1000LL); // 5 ms entre ráfagas
        }
        svcSleepThread(80 * 1000 * 1000LL); // 80 ms para asegurar emisión por radio antes del cierre del BSSID
    }

    void notifyPlayerLeave() {
        if (isHost() || !inMultiplayer) return;
        NetHeader pkt;
        pkt.magic = MULTI_MAGIC_UDP;
        pkt.type = PKT_PLAYER_LEAVE;
        pkt.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.seq = ++packetSeq;
        for (int burst = 0; burst < 8; ++burst) {
            sendPacketToHost(&pkt, sizeof(pkt));
            svcSleepThread(5 * 1000 * 1000LL);
        }
        svcSleepThread(60 * 1000 * 1000LL);
    }

    void sendReturnToLobby() {
        if (!isHost() || !inMultiplayer) return;
        NetHeader pkt;
        pkt.magic = MULTI_MAGIC_UDP;
        pkt.type = PKT_RETURN_LOBBY;
        pkt.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.seq = ++packetSeq;
        for (int burst = 0; burst < 6; ++burst) {
            sendPacketToAllClients(&pkt, sizeof(pkt));
        }
    }

    bool isAnyoneDownloading() const {
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && players[i].status == STATUS_DOWNLOADING) {
                return true;
            }
        }
        return false;
    }

    bool isAnyClientDownloading() const {
        return isAnyoneDownloading();
    }

    MultiplayerManager() {
        LightLock_Init(&udsRoomsLock);
        LightEvent_Init(&udsScanWake, RESET_ONESHOT);
        std::memset(&udsBindCtx, 0, sizeof(udsBindCtx));
        std::memset(&activeUdsNetwork, 0, sizeof(activeUdsNetwork));
    }

    ~MultiplayerManager() {
        stopUdsScanThread(true);
        stop();
    }

    static void udsScanThreadEntry(void* arg) {
        static_cast<MultiplayerManager*>(arg)->udsScanLoop();
    }

    void startUdsScanThread() {
        if (udsScanThread) {
            udsScanActive = true;
            LightEvent_Signal(&udsScanWake);
            return;
        }
        udsScanQuit = false;
        udsScanActive = true;
        s32 prio = 0x30;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        s32 workerPrio = std::min<s32>(0x3F, prio + 2);

        udsScanThread = threadCreate(udsScanThreadEntry, this, 32 * 1024, workerPrio, 1, false);
        if (!udsScanThread) {
            udsScanThread = threadCreate(udsScanThreadEntry, this, 32 * 1024, workerPrio, -2, false);
        }
    }

    void stopUdsScanThread(bool terminate = false) {
        udsScanActive = false;
        if (!udsScanThread) return;
        if (terminate) {
            udsScanQuit = true;
            LightEvent_Signal(&udsScanWake);
            threadJoin(udsScanThread, U64_MAX);
            threadFree(udsScanThread);
            udsScanThread = nullptr;
        }
    }

    void udsScanLoop() {
        size_t scanBufSize = 0x4000;
        void* scanBuf = std::malloc(scanBufSize);

        while (!udsScanQuit) {
            if (!udsScanActive || !udsInitialized || !inBrowserMode || netMode != NET_MODE_UDS) {
                LightEvent_Wait(&udsScanWake);
                if (udsScanQuit) break;
                if (!udsScanActive || !udsInitialized || !inBrowserMode || netMode != NET_MODE_UDS) {
                    continue;
                }
            }

            if (scanBuf) {
                std::memset(scanBuf, 0, scanBufSize);
                udsNetworkScanInfo* scanNetworks = nullptr;
                size_t totalNetworks = 0;
                Result res = udsScanBeacons(scanBuf, scanBufSize, &scanNetworks, &totalNetworks,
                                            UDS_APP_WLANCOMMID, 0, nullptr, false);

                if (R_SUCCEEDED(res) && scanNetworks && totalNetworks > 0) {
                    uint32_t now = osGetTime();
                    std::vector<DiscoveredRoom> freshRooms;
                    freshRooms.reserve(totalNetworks);

                    for (size_t i = 0; i < totalNetworks; ++i) {
                        UdsBeaconData bData;
                        std::memset(&bData, 0, sizeof(bData));
                        size_t actualSize = 0;
                        Result appRes = udsGetNetworkStructApplicationData(&scanNetworks[i].network, &bData, sizeof(bData), &actualSize);
                        if (R_SUCCEEDED(appRes) && bData.magic == MULTI_MAGIC_UDP) {
                            DiscoveredRoom nr;
                            nr.mode = NET_MODE_UDS;
                            nr.udsNetStruct = scanNetworks[i].network;
                            nr.roomTitle = bData.roomTitle;
                            nr.hostName = bData.hostName;
                            nr.mapTitle = bData.mapTitle;
                            nr.mapArtist = bData.mapArtist;
                            nr.mapVersion = bData.mapVersion;
                            nr.mapStars = bData.mapStars;
                            nr.playerCount = bData.playerCount;
                            nr.maxPlayers = bData.maxPlayers;
                            nr.winCondition = static_cast<WinCondition>(bData.winCondition);
                            nr.modMode = static_cast<ModMode>(bData.modMode);
                            nr.lastSeenTick = now;
                            nr.pingMs = 5;
                            freshRooms.push_back(nr);
                        }
                    }

                    if (scanNetworks) {
                        std::free(scanNetworks);
                        scanNetworks = nullptr;
                    }

                    LightLock_Lock(&udsRoomsLock);
                    for (const auto& fr : freshRooms) {
                        bool found = false;
                        for (auto& er : udsDiscoveredRooms) {
                            if (er.mode == NET_MODE_UDS &&
                                std::memcmp(er.udsNetStruct.host_macaddress, fr.udsNetStruct.host_macaddress, 6) == 0) {
                                er.udsNetStruct = fr.udsNetStruct;
                                er.roomTitle = fr.roomTitle;
                                er.hostName = fr.hostName;
                                er.mapTitle = fr.mapTitle;
                                er.mapArtist = fr.mapArtist;
                                er.mapVersion = fr.mapVersion;
                                er.mapStars = fr.mapStars;
                                er.playerCount = fr.playerCount;
                                er.maxPlayers = fr.maxPlayers;
                                er.winCondition = fr.winCondition;
                                er.modMode = fr.modMode;
                                er.lastSeenTick = now;
                                er.pingMs = fr.pingMs;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            udsDiscoveredRooms.push_back(fr);
                        }
                    }
                    for (auto it = udsDiscoveredRooms.begin(); it != udsDiscoveredRooms.end(); ) {
                        if (now - it->lastSeenTick > 5000) {
                            it = udsDiscoveredRooms.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    udsRoomsUpdated = true;
                    LightLock_Unlock(&udsRoomsLock);
                } else if (scanNetworks) {
                    std::free(scanNetworks);
                }
            }

            for (int s = 0; s < 12 && !udsScanQuit && udsScanActive; ++s) {
                svcSleepThread(100ULL * 1000000ULL);
            }
        }

        if (scanBuf) {
            std::free(scanBuf);
        }
    }

    bool isHost() const { return isHosting && (localSlot == hostSlot); }

    void setNetworkMode(MultiNetworkMode mode) {
        if (netMode == mode) return;
        if (mode == NET_MODE_LAN) {
            exitUdsService();
        }
        netMode = mode;
        if (inBrowserMode) {
            startBrowser(localPlayerName);
        }
    }

    void toggleNetworkMode() {
        setNetworkMode((netMode == NET_MODE_UDS) ? NET_MODE_LAN : NET_MODE_UDS);
    }

    // ========================================================================
    // INICIALIZACIÓN Y DETENCIÓN DE SERVICIOS
    // ========================================================================
    bool initUdsService() {
        if (udsInitialized) return true;

        if (NetCommon::isSocReady()) {
            NetCommon::exitSoc();
        }

        Result res = udsInit(0x4000, localPlayerName.substr(0, 10).c_str());
        if (R_FAILED(res)) {
            udsInitialized = false;
            return false;
        }

        udsInitialized = true;
        return true;
    }

    void disconnectUds() {
        closeUdsDownload();
        closeUdsServerTransfer();
        if (isHosting) {
            udsDestroyNetwork();
        } else if (inMultiplayer) {
            udsDisconnectNetwork();
        }
        if (udsBindCtx.event != 0) {
            udsUnbind(&udsBindCtx);
            std::memset(&udsBindCtx, 0, sizeof(udsBindCtx));
        }
        std::memset(&activeUdsNetwork, 0, sizeof(activeUdsNetwork));
    }

    void exitUdsService() {
        stopUdsScanThread(false);
        disconnectUds();
        if (udsInitialized) {
            udsExit();
            udsInitialized = false;
        }
    }

    bool initSocHelper() {
        if (!NetCommon::initSoc()) return false;
        socSessionActive = true;
        return true;
    }

    void stopSession(bool fullExit = false) {
        stopUdsScanThread(fullExit);

        bool hadSoc = (udpSock >= 0 || tcpListenSock >= 0 || tcpClientSock >= 0);
        inBrowserMode = false;
        connectedHostIp = 0;
        closeTcp();
        if (udpSock >= 0) {
            close(udpSock);
            udpSock = -1;
        }
        if (socSessionActive || hadSoc) {
            NetCommon::exitSoc();
            socSessionActive = false;
        }

        disconnectUds();
        if (fullExit) {
            exitUdsService();
        }

        inMultiplayer = false;
        isHosting = false;
        matchStarted = false;
        matchInProgressFlag = false;
        matchFinished = false;
        lastHostPacketTick = 0;
        mapQueue.clear();
        std::memset(nodeDisconnectStartTick, 0, sizeof(nodeDisconnectStartTick));
        lastDownloadedOsuPath.clear();
        lastDownloadedAudioPath.clear();
        udsDlCurrentOsuPath.clear();
        udsDlCurrentAudioPath.clear();
        currentDlOsuPath.clear();
        currentDlAudioPath.clear();
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            players[i] = NetworkPlayer();
        }
    }

    void stop() {
        stopSession(true);
    }

    bool startBrowser(const std::string& playerName) {
        stopSession(false);
        localPlayerName = playerName.empty() ? "Jugador3DS" : playerName;
        inBrowserMode = true;
        inMultiplayer = false;
        isHosting = false;
        LightLock_Lock(&udsRoomsLock);
        discoveredRooms.clear();
        selectedRoomIndex = 0;
        LightLock_Unlock(&udsRoomsLock);

        if (netMode == NET_MODE_UDS) {
            if (!initUdsService()) {
                netMode = NET_MODE_LAN;
            }
        }

        if (netMode == NET_MODE_LAN) {
            stopUdsScanThread(false);
            if (!initSocHelper() || !setupSockets()) {
                stop();
                return false;
            }
        }

        lastBrowserPingTick = osGetTime();
        lastUdsScanTick = 0;
        if (netMode == NET_MODE_UDS) {
            startUdsScanThread();
        } else {
            sendDiscoverPing(true);
        }
        return true;
    }

    void refreshBrowser() {
        if (!inBrowserMode) return;
        LightLock_Lock(&udsRoomsLock);
        discoveredRooms.clear();
        udsDiscoveredRooms.clear();
        udsRoomsUpdated = true;
        selectedRoomIndex = 0;
        LightLock_Unlock(&udsRoomsLock);

        lastBrowserPingTick = osGetTime();
        lastUdsScanTick = 0;

        if (netMode == NET_MODE_UDS) {
            startUdsScanThread();
            LightEvent_Signal(&udsScanWake);
        } else if (udpSock >= 0) {
            sendDiscoverPing(true);
        }
    }

    void updateBrowser() {
        if (!inBrowserMode) return;
        uint32_t now = osGetTime();

        if (netMode == NET_MODE_UDS) {
            if (!udsScanActive) {
                startUdsScanThread();
            }
            LightLock_Lock(&udsRoomsLock);
            if (udsRoomsUpdated) {
                discoveredRooms = udsDiscoveredRooms;
                udsRoomsUpdated = false;
            }
            LightLock_Unlock(&udsRoomsLock);
        } else {
            // Modo LAN: Broadcast ping cada 800 ms
            if (now - lastBrowserPingTick > 800) {
                sendDiscoverPing(true);
                lastBrowserPingTick = now;
            }
            processLanBrowserPackets();

            // Limpiar salas inactivas (> 4.5 segundos sin beacon)
            for (auto it = discoveredRooms.begin(); it != discoveredRooms.end(); ) {
                if (now - it->lastSeenTick > 4500) {
                    it = discoveredRooms.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (selectedRoomIndex >= static_cast<int>(discoveredRooms.size())) {
            selectedRoomIndex = discoveredRooms.empty() ? 0 : static_cast<int>(discoveredRooms.size() - 1);
        }
        if (selectedRoomIndex < 0) selectedRoomIndex = 0;
    }

    bool joinSelectedRoom() {
        LightLock_Lock(&udsRoomsLock);
        if (!inBrowserMode || discoveredRooms.empty() || selectedRoomIndex < 0 || selectedRoomIndex >= static_cast<int>(discoveredRooms.size())) {
            LightLock_Unlock(&udsRoomsLock);
            return false;
        }
        const auto room = discoveredRooms[selectedRoomIndex];
        LightLock_Unlock(&udsRoomsLock);

        std::string pName = localPlayerName;
        stopSession(false);

        if (room.mode == NET_MODE_UDS) {
            return startClient(pName, 0, &room.udsNetStruct);
        } else {
            return startClient(pName, room.hostIp, nullptr);
        }
    }

    bool joinRoom(size_t index) {
        if (!inBrowserMode || index >= discoveredRooms.size()) return false;
        selectedRoomIndex = static_cast<int>(index);
        return joinSelectedRoom();
    }

    // Inicia como Anfitrión (Host)
    bool startHost(const std::string& hostName, const QueuedBeatmap& initialMap) {
        stopSession(false);
        localPlayerName = hostName.empty() ? "PlayerOne" : hostName;
        connectedHostIp = 0;
        isHosting = true;
        inMultiplayer = true;
        inBrowserMode = false;
        matchInProgressFlag = false;
        matchStarted = false;
        localSlot = 0;
        hostSlot = 0;
        activeBeatmap = initialMap;
        hostExactOsuPath = initialMap.osuFilename;
        hostExactAudioPath = initialMap.audioFilename;
        activeBeatmap.folderPath = extractFolderName(initialMap.folderPath);
        activeBeatmap.osuFilename = extractBaseFilename(initialMap.osuFilename);
        activeBeatmap.audioFilename = extractBaseFilename(initialMap.audioFilename);
        mapQueue.clear();

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            players[i] = NetworkPlayer();
        }
        players[0] = NetworkPlayer(0, true, true, localPlayerName.c_str());
        players[0].status = STATUS_READY;
        players[0].downloadProgress = 100;
        players[0].ip = (netMode == NET_MODE_LAN) ? gethostid() : 0;

        if (netMode == NET_MODE_UDS) {
            if (!initUdsService()) {
                netMode = NET_MODE_LAN;
            }
        }

        if (netMode == NET_MODE_UDS) {
            udsGenerateDefaultNetworkStruct(&activeUdsNetwork, UDS_APP_WLANCOMMID, 0, MULTI_MAX_PLAYERS);
            Result res = udsCreateNetwork(&activeUdsNetwork, UDS_PASSPHRASE, std::strlen(UDS_PASSPHRASE) + 1,
                                          &udsBindCtx, UDS_DATA_CHANNEL, UDS_RECV_BUF_SIZE);
            if (R_FAILED(res)) {
                disconnectUds();
                netMode = NET_MODE_LAN;
            } else {
                localNodeId = UDS_HOST_NETWORKNODEID;
                updateUdsBeacon();
                broadcastLobbyState();
                return true;
            }
        }

        // Si estamos en LAN o UDS falló
        if (!initSocHelper() || !setupSockets()) {
            stop();
            return false;
        }

        broadcastLobbyState();
        return true;
    }

    // Inicia como Cliente conectado a una sala
    bool startClient(const std::string& playerName, uint32_t targetHostIp = 0, const udsNetworkStruct* targetUdsNet = nullptr) {
        stopSession(false);
        localPlayerName = playerName.empty() ? "TuNombre" : playerName;
        connectedHostIp = targetHostIp;
        isHosting = false;
        inMultiplayer = true;
        inBrowserMode = false;
        matchInProgressFlag = false;
        matchStarted = false;
        lastHostPacketTick = osGetTime();
        localSlot = 1;
        hostSlot = 0;
        mapQueue.clear();

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            players[i] = NetworkPlayer();
        }
        players[localSlot] = NetworkPlayer(localSlot, false, true, localPlayerName.c_str());
        players[localSlot].status = STATUS_NOT_READY;

        if (targetUdsNet != nullptr) {
            netMode = NET_MODE_UDS;
        }

        if (netMode == NET_MODE_UDS && targetUdsNet != nullptr) {
            if (!initUdsService()) {
                stop();
                return false;
            }
            activeUdsNetwork = *targetUdsNet;
            Result res = udsConnectNetwork(&activeUdsNetwork, UDS_PASSPHRASE, std::strlen(UDS_PASSPHRASE) + 1,
                                           &udsBindCtx, UDS_BROADCAST_NETWORKNODEID,
                                           UDSCONTYPE_Client, UDS_DATA_CHANNEL, UDS_RECV_BUF_SIZE);
            if (R_FAILED(res)) {
                stop();
                return false;
            }

            udsConnectionStatus constatus;
            if (R_SUCCEEDED(udsGetConnectionStatus(&constatus))) {
                localNodeId = constatus.cur_NetworkNodeID;
                if (localNodeId >= 2 && localNodeId <= MULTI_MAX_PLAYERS) {
                    localSlot = localNodeId - 1;
                }
            }

            sendPlayerUpdate();
            return true;
        }

        // Modo LAN
        netMode = NET_MODE_LAN;
        if (!initSocHelper() || !setupSockets()) {
            stop();
            return false;
        }

        players[localSlot].ip = gethostid();
        sendPlayerUpdate();
        return true;
    }

private:
    void updateUdsBeacon() {
        if (!isHost() || !udsInitialized) return;
        UdsBeaconData bData;
        std::memset(&bData, 0, sizeof(bData));
        bData.magic = MULTI_MAGIC_UDP;
        std::strncpy(bData.hostName, localPlayerName.c_str(), sizeof(bData.hostName) - 1);
        std::strncpy(bData.roomTitle, roomTitle.c_str(), sizeof(bData.roomTitle) - 1);
        std::strncpy(bData.mapTitle, activeBeatmap.title.c_str(), sizeof(bData.mapTitle) - 1);
        std::strncpy(bData.mapArtist, activeBeatmap.artist.c_str(), sizeof(bData.mapArtist) - 1);
        std::strncpy(bData.mapVersion, activeBeatmap.version.c_str(), sizeof(bData.mapVersion) - 1);
        bData.mapStars = activeBeatmap.starRating;
        bData.playerCount = static_cast<uint8_t>(getActivePlayerCount());
        bData.maxPlayers = MULTI_MAX_PLAYERS;
        bData.winCondition = static_cast<uint8_t>(winCondition);
        bData.modMode = static_cast<uint8_t>(modMode);
        bData.matchInProgress = (matchStarted || matchInProgressFlag) ? 1 : 0;

        udsSetApplicationData(&bData, sizeof(bData));
    }

    bool setupSockets() {
        udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSock < 0) return false;

        int opt = 1;
        setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

        struct sockaddr_in bindAddr;
        std::memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(MULTI_UDP_PORT);
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(udpSock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0) {
            close(udpSock);
            udpSock = -1;
            return false;
        }
        setNonBlocking(udpSock);

        std::memset(&bcastAddr, 0, sizeof(bcastAddr));
        bcastAddr.sin_family = AF_INET;
        bcastAddr.sin_port = htons(MULTI_UDP_PORT);
        bcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

        tcpListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpListenSock >= 0) {
            int topt = 1;
            setsockopt(tcpListenSock, SOL_SOCKET, SO_REUSEADDR, &topt, sizeof(topt));
            struct sockaddr_in tcpAddr;
            std::memset(&tcpAddr, 0, sizeof(tcpAddr));
            tcpAddr.sin_family = AF_INET;
            tcpAddr.sin_port = htons(MULTI_TCP_PORT);
            tcpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(tcpListenSock, reinterpret_cast<struct sockaddr*>(&tcpAddr), sizeof(tcpAddr)) == 0) {
                listen(tcpListenSock, 4);
                setNonBlocking(tcpListenSock);
            }
        }
        return true;
    }

    void closeTcpDownload() {
        if (dlOsuFile) { std::fclose(dlOsuFile); dlOsuFile = nullptr; }
        if (dlAudioFile) { std::fclose(dlAudioFile); dlAudioFile = nullptr; }
        if (tcpClientSock >= 0) {
            shutdown(tcpClientSock, SHUT_RDWR);
            close(tcpClientSock);
            tcpClientSock = -1;
        }
        dlBytesReceived = 0;
        dlOsuBytesWritten = 0;
        dlAudioBytesWritten = 0;
        tcpState = TCP_IDLE;
    }

    void closeTcpServerTransfer() {
        if (srvOsuFile) { std::fclose(srvOsuFile); srvOsuFile = nullptr; }
        if (srvAudioFile) { std::fclose(srvAudioFile); srvAudioFile = nullptr; }
        if (tcpTransferSock >= 0) {
            shutdown(tcpTransferSock, SHUT_RDWR);
            close(tcpTransferSock);
            tcpTransferSock = -1;
        }
        srvTotalSent = 0;
        srvTotalToSend = 0;
    }

    void closeTcp() {
        closeTcpDownload();
        closeTcpServerTransfer();
        if (tcpListenSock >= 0) {
            close(tcpListenSock);
            tcpListenSock = -1;
        }
    }

    void closeUdsServerTransfer() {
        if (udsSrvOsuFile) { std::fclose(udsSrvOsuFile); udsSrvOsuFile = nullptr; }
        if (udsSrvAudioFile) { std::fclose(udsSrvAudioFile); udsSrvAudioFile = nullptr; }
        if (udsSrvBgFile) { std::fclose(udsSrvBgFile); udsSrvBgFile = nullptr; }
        udsSrvOsuChunks = 0;
        udsSrvAudioChunks = 0;
        udsSrvBgChunks = 0;
        udsSrvTotalChunks = 0;
        udsSrvTargetNode = 0;
    }

    void closeUdsDownload() {
        if (udsDlOsuFile) { std::fclose(udsDlOsuFile); udsDlOsuFile = nullptr; }
        if (udsDlAudioFile) { std::fclose(udsDlAudioFile); udsDlAudioFile = nullptr; }
        if (udsDlBgFile) { std::fclose(udsDlBgFile); udsDlBgFile = nullptr; }
        udsDlExpectedChunk = 0;
        udsDlTotalChunks = 0;
        udsDlTotalBytes = 0;
        udsDlBytesWritten = 0;
    }

public:
    // ========================================================================
    // ENVÍO DE PAQUETES (UNIFICADO UDS & LAN)
    // ========================================================================
    void sendPacketToHost(const void* data, size_t len) {
        if (netMode == NET_MODE_UDS && udsInitialized) {
            udsSendTo(UDS_HOST_NETWORKNODEID, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, data, len);
            return;
        }
        if (udpSock < 0) return;
        if (connectedHostIp != 0) {
            struct sockaddr_in hostAddr;
            std::memset(&hostAddr, 0, sizeof(hostAddr));
            hostAddr.sin_family = AF_INET;
            hostAddr.sin_port = htons(MULTI_UDP_PORT);
            hostAddr.sin_addr.s_addr = connectedHostIp;
            sendto(udpSock, data, len, 0, reinterpret_cast<struct sockaddr*>(&hostAddr), sizeof(hostAddr));
        }
        sendto(udpSock, data, len, 0, reinterpret_cast<struct sockaddr*>(&bcastAddr), sizeof(bcastAddr));
    }

    void sendPacketToAllClients(const void* data, size_t len) {
        if (!isHost()) return;
        if (netMode == NET_MODE_UDS && udsInitialized) {
            udsSendTo(UDS_BROADCAST_NETWORKNODEID, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, data, len);
            return;
        }
        if (udpSock < 0) return;
        sendto(udpSock, data, len, 0, reinterpret_cast<struct sockaddr*>(&bcastAddr), sizeof(bcastAddr));
        for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && players[i].ip != 0) {
                struct sockaddr_in cliAddr;
                std::memset(&cliAddr, 0, sizeof(cliAddr));
                cliAddr.sin_family = AF_INET;
                cliAddr.sin_port = htons(players[i].port ? players[i].port : MULTI_UDP_PORT);
                cliAddr.sin_addr.s_addr = players[i].ip;
                sendto(udpSock, data, len, 0, reinterpret_cast<struct sockaddr*>(&cliAddr), sizeof(cliAddr));
            }
        }
    }

    void sendDiscoverPing(bool isBrowserQuery = false) {
        if (netMode == NET_MODE_UDS) return;
        if (udpSock < 0) return;
        NetHeader hdr;
        hdr.magic = MULTI_MAGIC_UDP;
        hdr.type = PKT_DISCOVER_PING;
        hdr.senderSlot = isBrowserQuery ? 0xFF : static_cast<uint8_t>(localSlot);
        hdr.seq = ++packetSeq;
        if (!isBrowserQuery && connectedHostIp != 0) {
            sendPacketToHost(&hdr, sizeof(hdr));
        } else {
            sendto(udpSock, &hdr, sizeof(hdr), 0, reinterpret_cast<struct sockaddr*>(&bcastAddr), sizeof(bcastAddr));
        }
    }

    void broadcastLobbyState() {
        if (!isHost()) return;
        PacketLobbyState pkt;
        std::memset(&pkt, 0, sizeof(pkt));

        pkt.header.magic = MULTI_MAGIC_UDP;
        pkt.header.type = PKT_LOBBY_STATE;
        pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.header.seq = ++packetSeq;

        std::strncpy(pkt.roomTitle, roomTitle.c_str(), sizeof(pkt.roomTitle) - 1);
        pkt.hostSlot = static_cast<uint8_t>(hostSlot);
        pkt.winCondition = static_cast<uint8_t>(winCondition);
        pkt.modMode = static_cast<uint8_t>(modMode);
        pkt.hostRotate = hostRotate ? 1 : 0;
        pkt.matchInProgress = (isHosting && (matchStarted || matchInProgressFlag)) ? 1 : 0;
        pkt.hostChangingSong = (isHosting && hostChangingSong) ? 1 : 0;
        pkt.matchStartCountdown = matchStartCountdown;

        std::string bFolder = extractFolderName(activeBeatmap.folderPath);
        std::string bOsu = extractBaseFilename(activeBeatmap.osuFilename);
        std::string bAudio = extractBaseFilename(activeBeatmap.audioFilename);

        std::strncpy(pkt.mapTitle, activeBeatmap.title.c_str(), sizeof(pkt.mapTitle) - 1);
        std::strncpy(pkt.mapArtist, activeBeatmap.artist.c_str(), sizeof(pkt.mapArtist) - 1);
        std::strncpy(pkt.mapVersion, activeBeatmap.version.c_str(), sizeof(pkt.mapVersion) - 1);
        std::strncpy(pkt.mapCreator, activeBeatmap.creator.c_str(), sizeof(pkt.mapCreator) - 1);
        std::strncpy(pkt.mapFolder, bFolder.c_str(), sizeof(pkt.mapFolder) - 1);
        std::strncpy(pkt.mapOsuFile, bOsu.c_str(), sizeof(pkt.mapOsuFile) - 1);
        std::strncpy(pkt.mapAudioFile, bAudio.c_str(), sizeof(pkt.mapAudioFile) - 1);
        pkt.mapStars = activeBeatmap.starRating;
        pkt.mapBpm = activeBeatmap.bpm;
        pkt.mapLengthSec = static_cast<uint16_t>(activeBeatmap.lengthSec);

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            pkt.slots[i].active = players[i].active ? 1 : 0;
            pkt.slots[i].isHost = (i == hostSlot) ? 1 : 0;
            pkt.slots[i].status = static_cast<uint8_t>(players[i].status);
            pkt.slots[i].downloadProgress = players[i].downloadProgress;
            std::strncpy(pkt.slots[i].name, players[i].name, sizeof(pkt.slots[i].name) - 1);
            pkt.slots[i].ip = players[i].ip;
            pkt.slots[i].mods = players[i].activeMods;
            pkt.slots[i].pingMs = players[i].pingMs;
        }

        pkt.queueCount = static_cast<uint8_t>(std::min(static_cast<size_t>(MULTI_MAX_QUEUE), mapQueue.size()));
        for (uint8_t q = 0; q < pkt.queueCount; ++q) {
            std::string qFolder = extractFolderName(mapQueue[q].folderPath);
            std::string qOsu = extractBaseFilename(mapQueue[q].osuFilename);
            std::string qAudio = extractBaseFilename(mapQueue[q].audioFilename);

            std::strncpy(pkt.queue[q].title, mapQueue[q].title.c_str(), sizeof(pkt.queue[q].title) - 1);
            std::strncpy(pkt.queue[q].artist, mapQueue[q].artist.c_str(), sizeof(pkt.queue[q].artist) - 1);
            std::strncpy(pkt.queue[q].version, mapQueue[q].version.c_str(), sizeof(pkt.queue[q].version) - 1);
            std::strncpy(pkt.queue[q].folder, qFolder.c_str(), sizeof(pkt.queue[q].folder) - 1);
            std::strncpy(pkt.queue[q].osuFile, qOsu.c_str(), sizeof(pkt.queue[q].osuFile) - 1);
            std::strncpy(pkt.queue[q].audioFile, qAudio.c_str(), sizeof(pkt.queue[q].audioFile) - 1);
            pkt.queue[q].starRating = mapQueue[q].starRating;
            pkt.queue[q].bpm = mapQueue[q].bpm;
            pkt.queue[q].lengthSec = static_cast<uint16_t>(mapQueue[q].lengthSec);
        }

        sendPacketToAllClients(&pkt, sizeof(pkt));
    }

    void sendPlayerUpdate() {
        PacketPlayerUpdate pkt;
        pkt.header.magic = MULTI_MAGIC_UDP;
        pkt.header.type = PKT_PLAYER_UPDATE;
        pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.header.seq = ++packetSeq;

        std::strncpy(pkt.name, localPlayerName.c_str(), sizeof(pkt.name) - 1);
        pkt.name[sizeof(pkt.name) - 1] = '\0';
        pkt.status = static_cast<uint8_t>(players[localSlot].status);
        pkt.downloadProgress = players[localSlot].downloadProgress;
        pkt.mods = players[localSlot].activeMods;

        sendPacketToHost(&pkt, sizeof(pkt));
    }

    void sendStartMatch() {
        if (!isHost()) return;
        PacketStartMatch pkt;
        pkt.header.magic = MULTI_MAGIC_UDP;
        pkt.header.type = PKT_START_MATCH;
        pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.header.seq = ++packetSeq;
        pkt.startTick = osGetTime() + 1500;

        matchStarted = true;
        matchInProgressFlag = true;
        matchStartCountdown = pkt.startTick;

        if (netMode == NET_MODE_UDS) {
            updateUdsBeacon();
        }

        for (int burst = 0; burst < 6; ++burst) {
            sendPacketToAllClients(&pkt, sizeof(pkt));
        }
        broadcastLobbyState();
    }

    void sendLiveGameplay(int32_t score, int16_t combo, int16_t maxCombo, float acc, float hp,
                          const char* grade, int16_t c300, int16_t c100, int16_t c50, int16_t cMiss,
                          bool isFinished, bool isFailed) {
        NetworkPlayer& lp = players[localSlot];
        lp.score = score;
        lp.combo = combo;
        lp.maxCombo = maxCombo;
        lp.accuracy = acc;
        lp.hp = hp;
        std::strncpy(lp.grade, (grade && grade[0] != '\0') ? grade : "SS", sizeof(lp.grade) - 1);
        lp.grade[sizeof(lp.grade) - 1] = '\0';
        lp.count300 = c300;
        lp.count100 = c100;
        lp.count50 = c50;
        lp.countMiss = cMiss;
        lp.isFinished = isFinished;
        lp.isFailed = isFailed;

        PacketLiveGameplay pkt;
        pkt.header.magic = MULTI_MAGIC_UDP;
        pkt.header.type = PKT_LIVE_GAMEPLAY;
        pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
        pkt.header.seq = ++packetSeq;

        pkt.score = score;
        pkt.combo = combo;
        pkt.maxCombo = maxCombo;
        pkt.accuracy = acc;
        pkt.hp = hp;
        std::strncpy(pkt.grade, lp.grade, sizeof(pkt.grade) - 1);
        pkt.grade[sizeof(pkt.grade) - 1] = '\0';
        pkt.c300 = c300;
        pkt.c100 = c100;
        pkt.c50 = c50;
        pkt.cMiss = cMiss;
        pkt.isFinished = isFinished ? 1 : 0;
        pkt.isFailed = isFailed ? 1 : 0;

        if (isHost()) {
            sendPacketToAllClients(&pkt, sizeof(pkt));
        } else {
            sendPacketToHost(&pkt, sizeof(pkt));
        }
    }

    void requestQueueMap(const QueuedBeatmap& qm) {
        if (isHost()) {
            enqueueBeatmap(qm);
        } else {
            PacketQueueAdd pkt;
            std::memset(&pkt, 0, sizeof(pkt));
            pkt.header.magic = MULTI_MAGIC_UDP;
            pkt.header.type = PKT_QUEUE_ADD;
            pkt.header.senderSlot = static_cast<uint8_t>(localSlot);
            pkt.header.seq = ++packetSeq;

            std::string qFolder = extractFolderName(qm.folderPath);
            std::string qOsu = extractBaseFilename(qm.osuFilename);
            std::string qAudio = extractBaseFilename(qm.audioFilename);

            std::strncpy(pkt.item.title, qm.title.c_str(), sizeof(pkt.item.title) - 1);
            std::strncpy(pkt.item.artist, qm.artist.c_str(), sizeof(pkt.item.artist) - 1);
            std::strncpy(pkt.item.version, qm.version.c_str(), sizeof(pkt.item.version) - 1);
            std::strncpy(pkt.item.folder, qFolder.c_str(), sizeof(pkt.item.folder) - 1);
            std::strncpy(pkt.item.osuFile, qOsu.c_str(), sizeof(pkt.item.osuFile) - 1);
            std::strncpy(pkt.item.audioFile, qAudio.c_str(), sizeof(pkt.item.audioFile) - 1);
            pkt.item.starRating = qm.starRating;
            pkt.item.bpm = qm.bpm;
            pkt.item.lengthSec = static_cast<uint16_t>(qm.lengthSec);

            sendPacketToHost(&pkt, sizeof(pkt));
        }
    }

    void enqueueBeatmap(const QueuedBeatmap& qm) {
        if (mapQueue.size() < MULTI_MAX_QUEUE) {
            QueuedBeatmap cleanMap = qm;
            cleanMap.folderPath = extractFolderName(qm.folderPath);
            cleanMap.osuFilename = extractBaseFilename(qm.osuFilename);
            cleanMap.audioFilename = extractBaseFilename(qm.audioFilename);
            mapQueue.push_back(cleanMap);
            if (isHost()) broadcastLobbyState();
        }
    }

    // ========================================================================
    // AUTO-DESCARGA DE CANCIONES (UDS / TCP)
    // ========================================================================
    void startMapDownload(uint32_t hostIp = 0) {
        if (netMode == NET_MODE_UDS && udsInitialized) {
            if (players[localSlot].status == STATUS_DOWNLOADING) return;
            closeUdsDownload();

            u16 targetNode = UDS_HOST_NETWORKNODEID;
            if (isHost()) {
                int srcSlot = activeBeatmap.suggestedBySlot;
                if (srcSlot <= 0 || srcSlot >= MULTI_MAX_PLAYERS || !players[srcSlot].active ||
                    players[srcSlot].status == STATUS_MISSING_MAP || players[srcSlot].status == STATUS_DOWNLOADING) {
                    srcSlot = -1;
                    for (int s = 1; s < MULTI_MAX_PLAYERS; ++s) {
                        if (players[s].active && players[s].status != STATUS_MISSING_MAP && players[s].status != STATUS_DOWNLOADING) {
                            srcSlot = s;
                            break;
                        }
                    }
                }
                if (srcSlot > 0) {
                    targetNode = static_cast<u16>(srcSlot + 1);
                } else {
                    return;
                }
            }

            udsDlSourceNode = targetNode;

            PacketFileReq req;
            req.header.magic = MULTI_MAGIC_UDP;
            req.header.type = PKT_FILE_REQ;
            req.header.senderSlot = static_cast<uint8_t>(localSlot);
            req.header.seq = ++packetSeq;
            udsSendTo(udsDlSourceNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &req, sizeof(req));

            players[localSlot].status = STATUS_DOWNLOADING;
            players[localSlot].downloadProgress = 0;
            udsDlLastChunkTick = osGetTime();
            if (isHost()) {
                broadcastLobbyState();
            } else {
                sendPlayerUpdate();
            }
            return;
        }

        // Modo LAN clásico (TCP)
        if (tcpState != TCP_IDLE) return;
        if (hostIp == 0) hostIp = connectedHostIp;
        if (hostIp == 0) return;

        closeTcpDownload();
        tcpClientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpClientSock < 0) return;
        setNonBlocking(tcpClientSock);

        struct sockaddr_in srvAddr;
        std::memset(&srvAddr, 0, sizeof(srvAddr));
        srvAddr.sin_family = AF_INET;
        srvAddr.sin_port = htons(MULTI_TCP_PORT);
        srvAddr.sin_addr.s_addr = hostIp;

        int res = connect(tcpClientSock, reinterpret_cast<struct sockaddr*>(&srvAddr), sizeof(srvAddr));
        if (res < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
            close(tcpClientSock);
            tcpClientSock = -1;
            return;
        }

        tcpState = TCP_CONNECTING;
        dlBytesReceived = 0;
        dlOsuBytesWritten = 0;
        dlAudioBytesWritten = 0;
        players[localSlot].status = STATUS_DOWNLOADING;
        players[localSlot].downloadProgress = 0;
        sendPlayerUpdate();
    }

private:
    void handleUdsFileReq(u16 srcNode) {
        if (!udsInitialized) return;

        std::string osuFullPath = hostExactOsuPath;
        if (osuFullPath.empty()) {
            osuFullPath = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.osuFilename);
        }
        std::string audioFullPath = hostExactAudioPath;
        if (audioFullPath.empty()) {
            audioFullPath = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.audioFilename);
        }

        closeUdsServerTransfer();
        if (!osuFullPath.empty()) udsSrvOsuFile = std::fopen(osuFullPath.c_str(), "rb");
        if (!audioFullPath.empty()) udsSrvAudioFile = std::fopen(audioFullPath.c_str(), "rb");

        // Buscar imagen de fondo en el archivo .osu si la descarga de fondo está habilitada
        std::string bgFilename = "";
        std::string bgFullPath = "";
        if (enableBgDownload && !osuFullPath.empty()) {
            std::ifstream ofile(osuFullPath.c_str());
            if (ofile.is_open()) {
                std::string l;
                bool inEvents = false;
                while (std::getline(ofile, l)) {
                    if (!l.empty() && l.back() == '\r') l.pop_back();
                    if (l == "[Events]") { inEvents = true; continue; }
                    if (inEvents) {
                        if (!l.empty() && l[0] == '[') break;
                        if (l.rfind("0,0,", 0) == 0 || l.rfind("0,", 0) == 0 || l.rfind("Image,", 0) == 0) {
                            size_t q1 = l.find('"');
                            size_t q2 = (q1 != std::string::npos) ? l.find('"', q1 + 1) : std::string::npos;
                            if (q1 != std::string::npos && q2 != std::string::npos) {
                                bgFilename = l.substr(q1 + 1, q2 - q1 - 1);
                                break;
                            }
                        }
                    }
                }
                ofile.close();
            }
            if (!bgFilename.empty()) {
                size_t slashPos = osuFullPath.find_last_of("/\\");
                std::string folderDir = (slashPos != std::string::npos) ? osuFullPath.substr(0, slashPos + 1) : "";
                bgFullPath = folderDir + extractBaseFilename(bgFilename);
                udsSrvBgFile = std::fopen(bgFullPath.c_str(), "rb");
                if (!udsSrvBgFile) {
                    bgFilename.clear();
                    bgFullPath.clear();
                }
            }
        }

        size_t osuSz = 0, audioSz = 0, bgSz = 0;
        if (udsSrvOsuFile) {
            std::fseek(udsSrvOsuFile, 0, SEEK_END);
            osuSz = static_cast<size_t>(std::ftell(udsSrvOsuFile));
            std::fseek(udsSrvOsuFile, 0, SEEK_SET);
        }
        if (udsSrvAudioFile) {
            std::fseek(udsSrvAudioFile, 0, SEEK_END);
            audioSz = static_cast<size_t>(std::ftell(udsSrvAudioFile));
            std::fseek(udsSrvAudioFile, 0, SEEK_SET);
        }
        if (udsSrvBgFile) {
            std::fseek(udsSrvBgFile, 0, SEEK_END);
            bgSz = static_cast<size_t>(std::ftell(udsSrvBgFile));
            std::fseek(udsSrvBgFile, 0, SEEK_SET);
        }

        udsSrvOsuSize = osuSz;
        udsSrvAudioSize = audioSz;
        udsSrvBgSize = bgSz;
        udsSrvOsuChunks = (static_cast<uint32_t>(osuSz) + 1023) / 1024;
        udsSrvAudioChunks = (static_cast<uint32_t>(audioSz) + 1023) / 1024;
        udsSrvBgChunks = (static_cast<uint32_t>(bgSz) + 1023) / 1024;
        udsSrvTotalChunks = udsSrvOsuChunks + udsSrvAudioChunks + udsSrvBgChunks;
        udsSrvTargetNode = srcNode;

        PacketFileHeader fh;
        std::memset(&fh, 0, sizeof(fh));
        fh.header.magic = MULTI_MAGIC_UDP;
        fh.header.type = PKT_FILE_HEADER;
        fh.header.senderSlot = static_cast<uint8_t>(localSlot);
        fh.header.seq = ++packetSeq;
        fh.totalBytes = static_cast<uint32_t>(osuSz + audioSz + bgSz);
        fh.totalChunks = udsSrvTotalChunks;

        std::string bFolder = extractFolderName(activeBeatmap.folderPath);
        std::string bOsu = extractBaseFilename(activeBeatmap.osuFilename);
        std::string bAudio = extractBaseFilename(activeBeatmap.audioFilename);
        std::string bBg = extractBaseFilename(bgFilename);

        std::strncpy(fh.folderName, bFolder.c_str(), sizeof(fh.folderName) - 1);
        std::strncpy(fh.osuFileName, bOsu.c_str(), sizeof(fh.osuFileName) - 1);
        fh.osuFileSize = static_cast<uint32_t>(osuSz);
        std::strncpy(fh.audioFileName, bAudio.c_str(), sizeof(fh.audioFileName) - 1);
        fh.audioFileSize = static_cast<uint32_t>(audioSz);
        std::strncpy(fh.bgFileName, bBg.c_str(), sizeof(fh.bgFileName) - 1);
        fh.bgFileSize = static_cast<uint32_t>(bgSz);

        udsSendTo(srcNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &fh, sizeof(fh));
    }

    void handleUdsFileAck(uint32_t reqChunk) {
        if (!udsInitialized || udsSrvTargetNode == 0) return;

        // Enviar ráfaga controlada de hasta 2 chunks UDS (2 KB) por cada ACK para evitar saturación de buffer UDS
        for (int b = 0; b < 2 && (reqChunk + b) < udsSrvTotalChunks; ++b) {
            uint32_t cIdx = reqChunk + b;
            PacketFileChunk chunk;
            chunk.header.magic = MULTI_MAGIC_UDP;
            chunk.header.type = PKT_FILE_CHUNK;
            chunk.header.senderSlot = static_cast<uint8_t>(localSlot);
            chunk.header.seq = ++packetSeq;
            chunk.chunkIndex = cIdx;
            chunk.totalChunks = udsSrvTotalChunks;

            if (cIdx < udsSrvOsuChunks) {
                chunk.fileTarget = 0;
                size_t offset = static_cast<size_t>(cIdx) * 1024;
                if (udsSrvOsuFile) {
                    std::fseek(udsSrvOsuFile, offset, SEEK_SET);
                    size_t toRead = std::min(static_cast<size_t>(1024), udsSrvOsuSize - offset);
                    chunk.chunkSize = static_cast<uint16_t>(std::fread(chunk.payload, 1, toRead, udsSrvOsuFile));
                } else {
                    chunk.chunkSize = 0;
                }
            } else if (cIdx < udsSrvOsuChunks + udsSrvAudioChunks) {
                chunk.fileTarget = 1;
                size_t offset = static_cast<size_t>(cIdx - udsSrvOsuChunks) * 1024;
                if (udsSrvAudioFile) {
                    std::fseek(udsSrvAudioFile, offset, SEEK_SET);
                    size_t toRead = std::min(static_cast<size_t>(1024), udsSrvAudioSize - offset);
                    chunk.chunkSize = static_cast<uint16_t>(std::fread(chunk.payload, 1, toRead, udsSrvAudioFile));
                } else {
                    chunk.chunkSize = 0;
                }
            } else {
                chunk.fileTarget = 2;
                size_t offset = static_cast<size_t>(cIdx - (udsSrvOsuChunks + udsSrvAudioChunks)) * 1024;
                if (udsSrvBgFile) {
                    std::fseek(udsSrvBgFile, offset, SEEK_SET);
                    size_t toRead = std::min(static_cast<size_t>(1024), udsSrvBgSize - offset);
                    chunk.chunkSize = static_cast<uint16_t>(std::fread(chunk.payload, 1, toRead, udsSrvBgFile));
                } else {
                    chunk.chunkSize = 0;
                }
            }

            size_t sendLen = sizeof(NetHeader) + sizeof(uint32_t) * 2 + sizeof(uint16_t) + sizeof(uint8_t) + chunk.chunkSize;
            udsSendTo(udsSrvTargetNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &chunk, sendLen);
        }
    }

    void handleUdsFileHeader(const PacketFileHeader* fh, u16 senderNode = 0) {
        if (!udsInitialized || players[localSlot].status != STATUS_DOWNLOADING) return;

        std::string folder = extractFolderName(fh->folderName);
        std::string targetDir = "sdmc:/3ds/osu-3ds/songs/" + folder + "/";
        OszExtractor::ensureDirectoryRecursive(targetDir);

        std::string osuFile = extractBaseFilename(fh->osuFileName);
        std::string audioFile = extractBaseFilename(fh->audioFileName);

        closeUdsDownload();
        std::string osuPath = targetDir + osuFile;
        std::string audioPath = targetDir + audioFile;
        udsDlCurrentOsuPath = osuPath;
        udsDlCurrentAudioPath = audioPath;
        udsDlCurrentBgPath.clear();

        udsDlOsuFile = std::fopen(osuPath.c_str(), "wb");
        udsDlAudioFile = std::fopen(audioPath.c_str(), "wb");
        if (fh->bgFileSize > 0 && fh->bgFileName[0] != '\0') {
            std::string bgFile = extractBaseFilename(fh->bgFileName);
            udsDlCurrentBgPath = targetDir + bgFile;
            udsDlBgFile = std::fopen(udsDlCurrentBgPath.c_str(), "wb");
        }

        udsDlExpectedChunk = 0;
        udsDlTotalChunks = fh->totalChunks;
        udsDlTotalBytes = fh->totalBytes;
        udsDlBytesWritten = 0;
        udsDlLastChunkTick = osGetTime();
        if (senderNode > 0) {
            udsDlSourceNode = senderNode;
        }

        players[localSlot].status = STATUS_DOWNLOADING;
        players[localSlot].downloadProgress = 0;

        PacketFileAck ack;
        ack.header.magic = MULTI_MAGIC_UDP;
        ack.header.type = PKT_FILE_ACK;
        ack.header.senderSlot = static_cast<uint8_t>(localSlot);
        ack.header.seq = ++packetSeq;
        ack.requestedChunkIndex = 0;
        udsSendTo(udsDlSourceNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &ack, sizeof(ack));
    }

    void handleUdsFileChunk(const PacketFileChunk* chunk) {
        if (!udsInitialized || players[localSlot].status != STATUS_DOWNLOADING) return;

        if (chunk->chunkIndex == udsDlExpectedChunk) {
            if (chunk->fileTarget == 0 && udsDlOsuFile) {
                std::fwrite(chunk->payload, 1, chunk->chunkSize, udsDlOsuFile);
            } else if (chunk->fileTarget == 1 && udsDlAudioFile) {
                std::fwrite(chunk->payload, 1, chunk->chunkSize, udsDlAudioFile);
            } else if (chunk->fileTarget == 2 && udsDlBgFile) {
                std::fwrite(chunk->payload, 1, chunk->chunkSize, udsDlBgFile);
            }

            udsDlBytesWritten += chunk->chunkSize;
            udsDlExpectedChunk++;
            udsDlLastChunkTick = osGetTime();

            if (udsDlTotalChunks > 0) {
                float pct = (static_cast<float>(udsDlExpectedChunk) / static_cast<float>(udsDlTotalChunks)) * 100.0f;
                players[localSlot].downloadProgress = static_cast<uint8_t>(std::min(100.0f, pct));
            }

            if (udsDlExpectedChunk >= udsDlTotalChunks) {
                // Descarga UDS completa con éxito
                closeUdsDownload();
                lastDownloadedOsuPath = udsDlCurrentOsuPath;
                lastDownloadedAudioPath = udsDlCurrentAudioPath;
                lastDownloadedBgPath = udsDlCurrentBgPath;
                players[localSlot].status = isHost() ? STATUS_READY : STATUS_NOT_READY;
                players[localSlot].downloadProgress = 100;
                if (isHost()) {
                    hostExactOsuPath = udsDlCurrentOsuPath;
                    hostExactAudioPath = udsDlCurrentAudioPath;
                    broadcastLobbyState();
                } else {
                    sendPlayerUpdate();
                }
            } else if ((udsDlExpectedChunk % 2) == 0) {
                // Solicitar siguiente bloque de 2 chunks
                PacketFileAck ack;
                ack.header.magic = MULTI_MAGIC_UDP;
                ack.header.type = PKT_FILE_ACK;
                ack.header.senderSlot = static_cast<uint8_t>(localSlot);
                ack.header.seq = ++packetSeq;
                ack.requestedChunkIndex = udsDlExpectedChunk;
                udsSendTo(udsDlSourceNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &ack, sizeof(ack));
            }
        }
    }

    void updateTcpDownload() {
        if (tcpState == TCP_IDLE) return;

        if (tcpState == TCP_CONNECTING) {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(tcpClientSock, &wset);
            struct timeval tv = {0, 0};
            int sel = select(tcpClientSock + 1, nullptr, &wset, nullptr, &tv);
            if (sel > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(tcpClientSock, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) {
                    tcpState = TCP_READING_HEADER;
                    dlBytesReceived = 0;
                } else {
                    closeTcpDownload();
                    players[localSlot].status = STATUS_MISSING_MAP;
                    sendPlayerUpdate();
                }
            }
            return;
        }

        if (tcpState == TCP_READING_HEADER) {
            uint8_t* ptr = reinterpret_cast<uint8_t*>(&currentDlHeader) + dlBytesReceived;
            size_t needed = sizeof(currentDlHeader) - dlBytesReceived;
            ssize_t n = recv(tcpClientSock, ptr, needed, 0);
            if (n > 0) {
                dlBytesReceived += static_cast<size_t>(n);
                if (dlBytesReceived >= sizeof(currentDlHeader)) {
                    if (currentDlHeader.magic == MULTI_MAGIC_TCP && currentDlHeader.totalBytes > 0) {
                        std::string folder = extractFolderName(currentDlHeader.folderName);
                        std::string targetDir = "sdmc:/3ds/osu-3ds/songs/" + folder + "/";
                        OszExtractor::ensureDirectoryRecursive(targetDir);

                        std::string osuFile = extractBaseFilename(currentDlHeader.osuFileName);
                        std::string audioFile = extractBaseFilename(currentDlHeader.audioFileName);

                        if (dlOsuFile) { std::fclose(dlOsuFile); dlOsuFile = nullptr; }
                        if (dlAudioFile) { std::fclose(dlAudioFile); dlAudioFile = nullptr; }

                        std::string osuPath = targetDir + osuFile;
                        std::string audioPath = targetDir + audioFile;
                        currentDlOsuPath = osuPath;
                        currentDlAudioPath = audioPath;
                        dlOsuFile = std::fopen(osuPath.c_str(), "wb");
                        dlAudioFile = std::fopen(audioPath.c_str(), "wb");

                        tcpState = TCP_READING_PAYLOAD;
                        dlBytesReceived = 0;
                        dlOsuBytesWritten = 0;
                        dlAudioBytesWritten = 0;
                    } else {
                        closeTcpDownload();
                        players[localSlot].status = STATUS_MISSING_MAP;
                        sendPlayerUpdate();
                    }
                }
            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            } else {
                closeTcpDownload();
                players[localSlot].status = STATUS_MISSING_MAP;
                sendPlayerUpdate();
            }
            return;
        }

        if (tcpState == TCP_READING_PAYLOAD) {
            uint8_t chunk[16384];
            for (int iter = 0; iter < 4; ++iter) {
                ssize_t n = recv(tcpClientSock, chunk, sizeof(chunk), 0);
                if (n > 0) {
                    size_t left = static_cast<size_t>(n);
                    size_t offset = 0;

                    if (dlOsuBytesWritten < currentDlHeader.osuFileSize) {
                        size_t toWriteOsu = std::min(left, static_cast<size_t>(currentDlHeader.osuFileSize - dlOsuBytesWritten));
                        if (dlOsuFile && toWriteOsu > 0) {
                            std::fwrite(chunk + offset, 1, toWriteOsu, dlOsuFile);
                        }
                        dlOsuBytesWritten += toWriteOsu;
                        offset += toWriteOsu;
                        left -= toWriteOsu;
                    }

                    if (left > 0 && dlAudioBytesWritten < currentDlHeader.audioFileSize) {
                        size_t toWriteAudio = std::min(left, static_cast<size_t>(currentDlHeader.audioFileSize - dlAudioBytesWritten));
                        if (dlAudioFile && toWriteAudio > 0) {
                            std::fwrite(chunk + offset, 1, toWriteAudio, dlAudioFile);
                        }
                        dlAudioBytesWritten += toWriteAudio;
                    }

                    dlBytesReceived += static_cast<size_t>(n);
                    if (currentDlHeader.totalBytes > 0) {
                        float pct = (static_cast<float>(dlBytesReceived) / static_cast<float>(currentDlHeader.totalBytes)) * 100.0f;
                        players[localSlot].downloadProgress = static_cast<uint8_t>(std::min(100.0f, pct));
                    }
                } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;
                } else {
                    if (dlBytesReceived < currentDlHeader.totalBytes) {
                        closeTcpDownload();
                        players[localSlot].status = STATUS_MISSING_MAP;
                        sendPlayerUpdate();
                    }
                    break;
                }
            }

            if (dlBytesReceived >= currentDlHeader.totalBytes) {
                if (currentDlHeader.audioFileSize > 0 && dlAudioBytesWritten > 0) {
                    closeTcpDownload();
                    lastDownloadedOsuPath = currentDlOsuPath;
                    lastDownloadedAudioPath = currentDlAudioPath;
                    players[localSlot].status = STATUS_NOT_READY;
                    players[localSlot].downloadProgress = 100;
                    sendPlayerUpdate();
                } else {
                    closeTcpDownload();
                    players[localSlot].status = STATUS_MISSING_MAP;
                    players[localSlot].downloadProgress = 0;
                    sendPlayerUpdate();
                }
            }
        }
    }

    void updateTcpServer() {
        if (tcpListenSock < 0) return;

        if (tcpTransferSock < 0) {
            struct sockaddr_in cliAddr;
            socklen_t cliLen = sizeof(cliAddr);
            int s = accept(tcpListenSock, reinterpret_cast<struct sockaddr*>(&cliAddr), &cliLen);
            if (s >= 0) {
                setNonBlocking(s);
                tcpTransferSock = s;

                std::string osuFullPath = hostExactOsuPath;
                if (osuFullPath.empty()) {
                    osuFullPath = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.osuFilename);
                }
                std::string audioFullPath = hostExactAudioPath;
                if (audioFullPath.empty()) {
                    audioFullPath = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.audioFilename);
                }

                if (srvOsuFile) { std::fclose(srvOsuFile); srvOsuFile = nullptr; }
                if (srvAudioFile) { std::fclose(srvAudioFile); srvAudioFile = nullptr; }

                if (!osuFullPath.empty()) srvOsuFile = std::fopen(osuFullPath.c_str(), "rb");
                if (!audioFullPath.empty()) srvAudioFile = std::fopen(audioFullPath.c_str(), "rb");

                size_t osuSz = 0, audioSz = 0;
                if (srvOsuFile) {
                    std::fseek(srvOsuFile, 0, SEEK_END);
                    osuSz = static_cast<size_t>(std::ftell(srvOsuFile));
                    std::fseek(srvOsuFile, 0, SEEK_SET);
                }
                if (srvAudioFile) {
                    std::fseek(srvAudioFile, 0, SEEK_END);
                    audioSz = static_cast<size_t>(std::ftell(srvAudioFile));
                    std::fseek(srvAudioFile, 0, SEEK_SET);
                }

                std::memset(&srvHeader, 0, sizeof(srvHeader));
                srvHeader.magic = MULTI_MAGIC_TCP;
                srvHeader.totalBytes = static_cast<uint32_t>(osuSz + audioSz);

                std::string baseFolder = extractFolderName(activeBeatmap.folderPath);
                std::string baseOsu = extractBaseFilename(activeBeatmap.osuFilename);
                std::string baseAudio = extractBaseFilename(activeBeatmap.audioFilename);

                std::strncpy(srvHeader.folderName, baseFolder.c_str(), sizeof(srvHeader.folderName) - 1);
                std::strncpy(srvHeader.osuFileName, baseOsu.c_str(), sizeof(srvHeader.osuFileName) - 1);
                srvHeader.osuFileSize = static_cast<uint32_t>(osuSz);
                std::strncpy(srvHeader.audioFileName, baseAudio.c_str(), sizeof(srvHeader.audioFileName) - 1);
                srvHeader.audioFileSize = static_cast<uint32_t>(audioSz);

                srvTotalSent = 0;
                srvTotalToSend = sizeof(srvHeader) + srvHeader.totalBytes;
                send(tcpTransferSock, &srvHeader, sizeof(srvHeader), 0);
            }
            return;
        }

        if (tcpTransferSock >= 0) {
            uint8_t buffer[16384];
            for (int iter = 0; iter < 4; ++iter) {
                size_t readLen = 0;
                if (srvOsuFile && !std::feof(srvOsuFile)) {
                    readLen = std::fread(buffer, 1, sizeof(buffer), srvOsuFile);
                } else if (srvAudioFile && !std::feof(srvAudioFile)) {
                    readLen = std::fread(buffer, 1, sizeof(buffer), srvAudioFile);
                }

                if (readLen > 0) {
                    ssize_t sent = send(tcpTransferSock, buffer, readLen, 0);
                    if (sent > 0) {
                        srvTotalSent += static_cast<size_t>(sent);
                    } else if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    } else {
                        closeTcpServerTransfer();
                        return;
                    }
                } else {
                    closeTcpServerTransfer();
                    return;
                }
            }
        }
    }

    void processLanBrowserPackets() {
        if (udpSock < 0) return;
        uint8_t recvBuf[2048];
        struct sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        uint32_t now = osGetTime();

        while (true) {
            ssize_t n = recvfrom(udpSock, recvBuf, sizeof(recvBuf), 0,
                                 reinterpret_cast<struct sockaddr*>(&fromAddr), &fromLen);
            if (n <= 0) break;
            if (n < static_cast<ssize_t>(sizeof(NetHeader))) continue;

            const NetHeader* hdr = reinterpret_cast<const NetHeader*>(recvBuf);
            if (hdr->magic != MULTI_MAGIC_UDP) continue;

            if (hdr->type == PKT_LOBBY_STATE && n >= static_cast<ssize_t>(sizeof(PacketLobbyState))) {
                const auto* st = reinterpret_cast<const PacketLobbyState*>(recvBuf);
                uint32_t fromIp = fromAddr.sin_addr.s_addr;
                uint16_t fromPort = ntohs(fromAddr.sin_port);

                int count = 0;
                for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
                    if (st->slots[i].active) count++;
                }
                std::string hName = "Host";
                if (st->hostSlot < MULTI_MAX_PLAYERS && st->slots[st->hostSlot].active) {
                    hName = st->slots[st->hostSlot].name;
                }

                bool found = false;
                for (size_t i = 0; i < discoveredRooms.size(); ++i) {
                    if (discoveredRooms[i].mode == NET_MODE_LAN && discoveredRooms[i].hostIp == fromIp) {
                        discoveredRooms[i].roomTitle = st->roomTitle;
                        discoveredRooms[i].hostName = hName;
                        discoveredRooms[i].mapTitle = st->mapTitle;
                        discoveredRooms[i].mapArtist = st->mapArtist;
                        discoveredRooms[i].mapVersion = st->mapVersion;
                        discoveredRooms[i].mapStars = st->mapStars;
                        discoveredRooms[i].playerCount = count;
                        discoveredRooms[i].winCondition = static_cast<WinCondition>(st->winCondition);
                        discoveredRooms[i].modMode = static_cast<ModMode>(st->modMode);
                        discoveredRooms[i].lastSeenTick = now;
                        discoveredRooms[i].pingMs = st->slots[st->hostSlot].pingMs;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    discoveredRooms.push_back(DiscoveredRoom(
                        fromIp, fromPort, st->roomTitle, hName,
                        st->mapTitle, st->mapArtist, st->mapVersion,
                        st->mapStars, count, MULTI_MAX_PLAYERS,
                        static_cast<WinCondition>(st->winCondition),
                        static_cast<ModMode>(st->modMode),
                        now, st->slots[st->hostSlot].pingMs
                    ));
                }
            }
        }
    }

public:
    // ========================================================================
    // BUCLE PRINCIPAL DE RED (INVOCADO CADA FRAME A 60 FPS)
    // ========================================================================
    void update(bool duringMatch = false) {
        if (!inMultiplayer) return;
        uint32_t curTick = osGetTime();

        // 1. Procesamiento de paquetes UDS (Modo Inalámbrico 3DS)
        if (netMode == NET_MODE_UDS && udsInitialized) {
            // Comprobar si hay paquetes disponibles mediante el evento del bind antes de llamar a IPC
            if (udsWaitDataAvailable(&udsBindCtx, false, false)) {
                uint8_t recvBuf[2048];
                size_t actualSize = 0;
                u16 srcNodeId = 0;

                while (true) {
                    actualSize = 0;
                    srcNodeId = 0;
                    Result res = udsPullPacket(&udsBindCtx, recvBuf, sizeof(recvBuf), &actualSize, &srcNodeId);
                    if (R_FAILED(res) || actualSize == 0) break;
                    if (actualSize < sizeof(NetHeader)) continue;

                    const NetHeader* hdr = reinterpret_cast<const NetHeader*>(recvBuf);
                    if (hdr->magic != MULTI_MAGIC_UDP) continue;

                    handlePacket(hdr, recvBuf, actualSize, srcNodeId, 0, 0);
                }
            }

            // Monitoreo continuo del hardware UDS (se comprueba en cada frame sin depender de eventos aislados)
            udsWaitConnectionStatusEvent(true, false);
            udsConnectionStatus constatus;
            if (R_SUCCEEDED(udsGetConnectionStatus(&constatus))) {
                if (isHost()) {
                    bool anyDisconnected = false;
                    for (int s = 1; s < MULTI_MAX_PLAYERS; ++s) {
                        u16 nodeMask = 1 << s;
                        if (players[s].active) {
                            if (!(constatus.node_bitmask & nodeMask)) {
                                if (nodeDisconnectStartTick[s] == 0) {
                                    nodeDisconnectStartTick[s] = curTick;
                                } else if (curTick - nodeDisconnectStartTick[s] > 1800) {
                                    char dcMsg[64];
                                    std::snprintf(dcMsg, sizeof(dcMsg), "%s se ha desconectado.", players[s].name);
                                    setToastNotice(dcMsg, 3500);
                                    players[s].active = false;
                                    players[s].status = SLOT_EMPTY;
                                    players[s].name[0] = '\0';
                                    nodeDisconnectStartTick[s] = 0;
                                    anyDisconnected = true;
                                }
                            } else {
                                nodeDisconnectStartTick[s] = 0;
                            }
                        } else {
                            nodeDisconnectStartTick[s] = 0;
                        }
                    }
                    if (anyDisconnected) {
                        updateUdsBeacon();
                        broadcastLobbyState();
                    }
                } else {
                    // Cliente UDS: Comprobar si la consola fue desconectada o el host cerró la sala
                    if (constatus.cur_NetworkNodeID == 0 || !(constatus.node_bitmask & 0x1)) {
                        if (nodeDisconnectStartTick[0] == 0) {
                            nodeDisconnectStartTick[0] = curTick;
                        } else if (curTick - nodeDisconnectStartTick[0] > 700) {
                            // Host UDS desconectado o sala cerrada (confirmado tras 700 ms continuos)
                            setToastNotice("La sala ha sido cerrada por el anfitrion.", 4000);
                            stop();
                            return;
                        }
                    } else {
                        nodeDisconnectStartTick[0] = 0;
                    }
                }
            }

            // Comprobar retransmisión por timeout durante descarga UDS (600 ms)
            if (players[localSlot].status == STATUS_DOWNLOADING && udsDlTotalChunks > 0) {
                if (curTick - udsDlLastChunkTick > 600) {
                    PacketFileAck ack;
                    ack.header.magic = MULTI_MAGIC_UDP;
                    ack.header.type = PKT_FILE_ACK;
                    ack.header.senderSlot = static_cast<uint8_t>(localSlot);
                    ack.header.seq = ++packetSeq;
                    ack.requestedChunkIndex = udsDlExpectedChunk;
                    udsSendTo(udsDlSourceNode, UDS_DATA_CHANNEL, UDS_SENDFLAG_Default, &ack, sizeof(ack));
                    udsDlLastChunkTick = curTick;
                }
            }
        }
        // 2. Procesamiento de paquetes LAN (Sockets UDP/TCP)
        else if (netMode == NET_MODE_LAN && udpSock >= 0) {
            uint8_t recvBuf[2048];
            struct sockaddr_in fromAddr;
            socklen_t fromLen = sizeof(fromAddr);

            while (true) {
                ssize_t n = recvfrom(udpSock, recvBuf, sizeof(recvBuf), 0,
                                     reinterpret_cast<struct sockaddr*>(&fromAddr), &fromLen);
                if (n <= 0) break;
                if (n < static_cast<ssize_t>(sizeof(NetHeader))) continue;

                const NetHeader* hdr = reinterpret_cast<const NetHeader*>(recvBuf);
                if (hdr->magic != MULTI_MAGIC_UDP) continue;

                handlePacket(hdr, recvBuf, static_cast<size_t>(n), 0, fromAddr.sin_addr.s_addr, ntohs(fromAddr.sin_port));
            }

            updateTcpDownload();
            updateTcpServer();
        }

        // 3. Tareas periódicas del Host
        if (isHost()) {
            if (curTick - lastBroadcastTick > 500) {
                broadcastLobbyState();
                lastBroadcastTick = curTick;
            }

            if (netMode == NET_MODE_LAN) {
                for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                    if (players[i].active && (curTick - players[i].lastPacketTick > 20000)) {
                        players[i].active = false;
                        players[i].status = SLOT_EMPTY;
                        players[i].name[0] = '\0';
                        broadcastLobbyState();
                    }
                }
            }
        } else {
            if (curTick - lastBroadcastTick > 500) {
                sendPlayerUpdate();
                lastBroadcastTick = curTick;
            }

            // Watchdog general para Clientes (si el host no envía ningún paquete durante 5 segundos)
            if (lastHostPacketTick != 0 && (curTick - lastHostPacketTick > 5000)) {
                setToastNotice("Conexion perdida con el anfitrion.", 4000);
                stop();
                return;
            }
        }

        // 4. Sincronización en vivo de Gameplay cada 15 frames durante la canción
        if (duringMatch) {
            gameplaySyncFrameCount++;
            if (gameplaySyncFrameCount >= 15) {
                gameplaySyncFrameCount = 0;
                sendLiveGameplay(
                    players[localSlot].score,
                    players[localSlot].combo,
                    players[localSlot].maxCombo,
                    players[localSlot].accuracy,
                    players[localSlot].hp,
                    players[localSlot].grade,
                    players[localSlot].count300,
                    players[localSlot].count100,
                    players[localSlot].count50,
                    players[localSlot].countMiss,
                    players[localSlot].isFinished,
                    players[localSlot].isFailed
                );
            }
        }
    }

private:
    void handlePacket(const NetHeader* hdr, const uint8_t* data, size_t len,
                      u16 srcNodeId, uint32_t fromIp, uint16_t fromPort) {
        // Descartar de inmediato paquetes enviados por esta misma consola en UDS
        if (netMode == NET_MODE_UDS && srcNodeId == localNodeId) return;

        uint32_t now = osGetTime();

        // Determinar remitente (UDS utiliza NetworkNodeID directamente)
        int resolvedSlot = -1;
        if (netMode == NET_MODE_UDS && srcNodeId > 0) {
            resolvedSlot = srcNodeId - 1;
            if (resolvedSlot == localSlot && !inBrowserMode) return;
        } else {
            if (!inBrowserMode && hdr->senderSlot == localSlot) return;
        }

        if (!isHost() && inMultiplayer) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s == hostSlot || s == 0) {
                lastHostPacketTick = now;
            }
        }

        // --- PAQUETES UDS DE TRANSFERENCIA DE MAPAS ---
        if (hdr->type == PKT_FILE_REQ) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS) {
                players[s].lastPacketTick = now;
            }
            handleUdsFileReq(srcNodeId);
            return;
        }
        if (hdr->type == PKT_FILE_HEADER && len >= sizeof(PacketFileHeader)) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS) {
                players[s].lastPacketTick = now;
            }
            handleUdsFileHeader(reinterpret_cast<const PacketFileHeader*>(data), srcNodeId);
            return;
        }
        if (hdr->type == PKT_FILE_CHUNK && len >= (sizeof(NetHeader) + 11)) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS) {
                players[s].lastPacketTick = now;
            }
            handleUdsFileChunk(reinterpret_cast<const PacketFileChunk*>(data));
            return;
        }
        if (hdr->type == PKT_FILE_ACK && len >= sizeof(PacketFileAck)) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS) {
                players[s].lastPacketTick = now;
            }
            const auto* ack = reinterpret_cast<const PacketFileAck*>(data);
            handleUdsFileAck(ack->requestedChunkIndex);
            return;
        }

        // --- PAQUETES DE CONTROL DE SALA ---
        if (hdr->type == PKT_ROOM_CLOSED) {
            setToastNotice("El anfitrion ha cerrado la sala.", 4000);
            stop();
            return;
        }

        if (isHost() && hdr->type == PKT_PLAYER_LEAVE) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s > 0 && s < MULTI_MAX_PLAYERS && players[s].active) {
                char lMsg[64];
                std::snprintf(lMsg, sizeof(lMsg), "%s ha salido de la sala.", players[s].name);
                setToastNotice(lMsg);
                players[s].active = false;
                players[s].status = SLOT_EMPTY;
                players[s].name[0] = '\0';
                if (netMode == NET_MODE_UDS) updateUdsBeacon();
                broadcastLobbyState();
            }
            return;
        }

        if (!isHost() && hdr->type == PKT_RETURN_LOBBY) {
            matchStarted = false;
            matchInProgressFlag = false;
            players[localSlot].status = STATUS_NOT_READY;
            shouldReturnToLobby = true;
            return;
        }

        // --- PAQUETES DE SALA Y CONTROL ---
        if (isHost()) {
            if (hdr->type == PKT_DISCOVER_PING) {
                if (hdr->senderSlot == 0xFF) {
                    broadcastLobbyState();
                    return;
                }
                int assignSlot = -1;
                for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                    if (players[i].active && players[i].ip == fromIp && fromIp != 0) {
                        assignSlot = i;
                        break;
                    }
                }
                if (assignSlot == -1) {
                    for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                        if (!players[i].active) {
                            assignSlot = i;
                            break;
                        }
                    }
                }
                if (assignSlot > 0) {
                    if (!players[assignSlot].active) {
                        players[assignSlot].active = true;
                        players[assignSlot].status = STATUS_NOT_READY;
                        players[assignSlot].downloadProgress = 0;
                    }
                    players[assignSlot].ip = fromIp;
                    players[assignSlot].port = fromPort;
                    players[assignSlot].lastPacketTick = now;
                    if (players[assignSlot].name[0] == '\0') {
                        std::snprintf(players[assignSlot].name, sizeof(players[assignSlot].name), "Jugador %d", assignSlot + 1);
                    }
                    if (netMode == NET_MODE_UDS) {
                        updateUdsBeacon();
                    }
                    broadcastLobbyState();
                }
                return;
            }

            if (hdr->type == PKT_PLAYER_UPDATE && len >= sizeof(PacketPlayerUpdate)) {
                const auto* pu = reinterpret_cast<const PacketPlayerUpdate*>(data);
                int s = -1;

                if (netMode == NET_MODE_UDS && srcNodeId > 0 && srcNodeId <= MULTI_MAX_PLAYERS) {
                    s = srcNodeId - 1;
                } else {
                    if (pu->name[0] != '\0') {
                        for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                            if (players[i].active && std::strcmp(players[i].name, pu->name) == 0) {
                                s = i;
                                break;
                            }
                        }
                    }
                    if (s == -1 && fromIp != 0) {
                        for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                            if (players[i].active && players[i].ip == fromIp) {
                                s = i;
                                break;
                            }
                        }
                    }
                    if (s == -1 && hdr->senderSlot > 0 && hdr->senderSlot < MULTI_MAX_PLAYERS && !players[hdr->senderSlot].active) {
                        s = hdr->senderSlot;
                    }
                    if (s == -1) {
                        for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                            if (!players[i].active) {
                                s = i;
                                break;
                            }
                        }
                    }
                }

                if (s > 0 && s < MULTI_MAX_PLAYERS) {
                    players[s].active = true;
                    players[s].ip = fromIp;
                    players[s].port = fromPort;
                    players[s].lastPacketTick = now;
                    players[s].status = static_cast<PlayerStatus>(pu->status);
                    players[s].downloadProgress = pu->downloadProgress;
                    if (modMode == MODS_FREEMOD) {
                        players[s].activeMods = pu->mods;
                    }
                    if (pu->name[0] != '\0') {
                        std::strncpy(players[s].name, pu->name, sizeof(players[s].name) - 1);
                        players[s].name[sizeof(players[s].name) - 1] = '\0';
                    }
                    broadcastLobbyState();
                }
                return;
            }

            if (hdr->type == PKT_QUEUE_ADD && len >= sizeof(PacketQueueAdd)) {
                const auto* qa = reinterpret_cast<const PacketQueueAdd*>(data);
                int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
                if (s > 0 && s < MULTI_MAX_PLAYERS && players[s].active) {
                    players[s].lastPacketTick = now;
                }
                QueuedBeatmap qm(
                    qa->item.title, qa->item.artist, qa->item.version,
                    "", qa->item.folder, qa->item.osuFile, qa->item.audioFile,
                    qa->item.starRating, qa->item.bpm, qa->item.lengthSec,
                    s
                );
                enqueueBeatmap(qm);
                broadcastLobbyState();
                return;
            }
        }

        // --- CLIENTE RECIBE ESTADO DEL LOBBY DEL HOST ---
        if (!isHost() && hdr->type == PKT_LOBBY_STATE && len >= sizeof(PacketLobbyState)) {
            const auto* st = reinterpret_cast<const PacketLobbyState*>(data);
            if (fromIp != 0) connectedHostIp = fromIp;
            roomTitle = st->roomTitle;
            hostSlot = st->hostSlot;
            winCondition = static_cast<WinCondition>(st->winCondition);
            modMode = static_cast<ModMode>(st->modMode);
            hostRotate = (st->hostRotate != 0);
            hostChangingSong = (st->hostChangingSong != 0);

            activeBeatmap.title = st->mapTitle;
            activeBeatmap.artist = st->mapArtist;
            activeBeatmap.version = st->mapVersion;
            activeBeatmap.creator = st->mapCreator;
            activeBeatmap.folderPath = st->mapFolder;
            activeBeatmap.osuFilename = st->mapOsuFile;
            activeBeatmap.audioFilename = st->mapAudioFile;
            activeBeatmap.starRating = st->mapStars;
            activeBeatmap.bpm = st->mapBpm;
            activeBeatmap.lengthSec = st->mapLengthSec;

            int foundSlot = -1;
            if (netMode == NET_MODE_UDS && localNodeId >= 2 && localNodeId <= MULTI_MAX_PLAYERS) {
                foundSlot = localNodeId - 1;
            } else {
                for (int i = 1; i < MULTI_MAX_PLAYERS; ++i) {
                    if (st->slots[i].active && i != hostSlot) {
                        if (std::strcmp(st->slots[i].name, localPlayerName.c_str()) == 0) {
                            foundSlot = i;
                            break;
                        }
                    }
                }
                if (foundSlot == -1 && localSlot > 0 && localSlot < MULTI_MAX_PLAYERS && st->slots[localSlot].active) {
                    foundSlot = localSlot;
                }
            }
            if (foundSlot != -1) {
                localSlot = foundSlot;
            }

            for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
                if (i == localSlot) {
                    players[i].active = true;
                    players[i].isLocal = true;
                    players[i].isHost = false;
                    std::strncpy(players[i].name, localPlayerName.c_str(), sizeof(players[i].name) - 1);
                    players[i].name[sizeof(players[i].name) - 1] = '\0';
                    players[i].ip = st->slots[i].ip;
                    players[i].pingMs = (netMode == NET_MODE_UDS) ? 5 : st->slots[i].pingMs;
                } else {
                    players[i].active = (st->slots[i].active != 0);
                    players[i].isLocal = false;
                    players[i].isHost = (st->slots[i].isHost != 0);
                    players[i].ip = st->slots[i].ip;
                    players[i].pingMs = (netMode == NET_MODE_UDS) ? 5 : st->slots[i].pingMs;
                    std::strncpy(players[i].name, st->slots[i].name, sizeof(players[i].name) - 1);
                    players[i].name[sizeof(players[i].name) - 1] = '\0';
                    players[i].status = static_cast<PlayerStatus>(st->slots[i].status);
                    players[i].downloadProgress = st->slots[i].downloadProgress;
                    players[i].activeMods = st->slots[i].mods;
                }
            }

            mapQueue.clear();
            for (uint8_t q = 0; q < st->queueCount && q < MULTI_MAX_QUEUE; ++q) {
                QueuedBeatmap qm(
                    st->queue[q].title, st->queue[q].artist, st->queue[q].version,
                    "", st->queue[q].folder, st->queue[q].osuFile, st->queue[q].audioFile,
                    st->queue[q].starRating, st->queue[q].bpm, st->queue[q].lengthSec, 0
                );
                mapQueue.push_back(qm);
            }

            if (modMode == MODS_HOST_ONLY) {
                players[localSlot].activeMods = players[hostSlot].activeMods;
            }
            return;
        }

        if (hdr->type == PKT_START_MATCH && len >= sizeof(PacketStartMatch)) {
            const auto* sm = reinterpret_cast<const PacketStartMatch*>(data);
            matchStarted = true;
            matchStartCountdown = sm->startTick;
            return;
        }

        if (hdr->type == PKT_LIVE_GAMEPLAY && len >= sizeof(PacketLiveGameplay)) {
            const auto* lg = reinterpret_cast<const PacketLiveGameplay*>(data);
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS && s != localSlot) {
                NetworkPlayer& p = players[s];
                p.score = lg->score;
                p.combo = lg->combo;
                p.maxCombo = lg->maxCombo;
                p.accuracy = lg->accuracy;
                p.hp = lg->hp;
                std::strncpy(p.grade, lg->grade, sizeof(p.grade) - 1);
                p.grade[sizeof(p.grade) - 1] = '\0';
                p.count300 = lg->c300;
                p.count100 = lg->c100;
                p.count50 = lg->c50;
                p.countMiss = lg->cMiss;
                p.isFinished = (lg->isFinished != 0);
                bool wasFailed = p.isFailed;
                p.isFailed = (lg->isFailed != 0);
                p.lastPacketTick = now;

                if (!wasFailed && p.isFailed) {
                    setToastNotice(std::string(p.name) + " ha fallado!", 2500);
                }
            }
            return;
        }

        if (hdr->type == PKT_MATCH_LOADED) {
            int s = (netMode == NET_MODE_UDS && srcNodeId > 0) ? (srcNodeId - 1) : hdr->senderSlot;
            if (s >= 0 && s < MULTI_MAX_PLAYERS && players[s].active) {
                players[s].status = STATUS_LOADED;
                players[s].lastPacketTick = now;
                if (isHost()) {
                    broadcastLobbyState();
                }
            }
            return;
        }

        if (hdr->type == PKT_MATCH_GO && len >= sizeof(PacketMatchGo)) {
            const auto* mg = reinterpret_cast<const PacketMatchGo*>(data);
            matchGo = true;
            matchGoDelayMs = mg->startDelayMs;
            return;
        }
    }

public:
    // ========================================================================
    // GESTIÓN DE SALA: LISTO, INICIAR, AVANZAR COLA Y ROTACIÓN DE HOST
    // ========================================================================
    void toggleLocalReady() {
        if (players[localSlot].status == STATUS_READY) {
            players[localSlot].status = STATUS_NOT_READY;
        } else if (players[localSlot].status == STATUS_NOT_READY) {
            players[localSlot].status = STATUS_READY;
        }
        sendPlayerUpdate();
        if (isHost()) broadcastLobbyState();
    }

    void setActiveBeatmap(const QueuedBeatmap& qm) {
        if (!isHost()) return;
        if (isAnyClientDownloading()) return; // No permitir cambiar mapa mientras un jugador está descargando
        activeBeatmap = qm;
        hostExactOsuPath = qm.osuFilename;
        hostExactAudioPath = qm.audioFilename;
        activeBeatmap.folderPath = extractFolderName(qm.folderPath);
        activeBeatmap.osuFilename = extractBaseFilename(qm.osuFilename);
        activeBeatmap.audioFilename = extractBaseFilename(qm.audioFilename);

        bool hostHasMap = false;
        if (!activeBeatmap.osuFilename.empty()) {
            std::string resolved = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.osuFilename);
            if (!resolved.empty()) {
                hostHasMap = true;
                hostExactOsuPath = resolved;
            }
        }

        players[hostSlot].status = hostHasMap ? STATUS_READY : STATUS_MISSING_MAP;
        players[hostSlot].downloadProgress = hostHasMap ? 100 : 0;

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && i != hostSlot) {
                players[i].status = STATUS_NOT_READY;
                players[i].downloadProgress = 0;
            }
        }

        if (netMode == NET_MODE_UDS) {
            updateUdsBeacon();
        }
        broadcastLobbyState();
    }

    void skipToNextQueueMap() {
        if (!isHost() || mapQueue.empty()) return;
        activeBeatmap = mapQueue.front();
        hostExactOsuPath = mapQueue.front().osuFilename;
        hostExactAudioPath = mapQueue.front().audioFilename;
        mapQueue.erase(mapQueue.begin());

        bool hostHasMap = false;
        if (!activeBeatmap.osuFilename.empty()) {
            std::string resolved = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.osuFilename);
            if (!resolved.empty()) {
                hostHasMap = true;
                hostExactOsuPath = resolved;
            }
        }

        players[hostSlot].status = hostHasMap ? STATUS_READY : STATUS_MISSING_MAP;
        players[hostSlot].downloadProgress = hostHasMap ? 100 : 0;

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && i != hostSlot) {
                players[i].status = STATUS_NOT_READY;
                players[i].downloadProgress = 0;
            }
        }

        if (netMode == NET_MODE_UDS) {
            updateUdsBeacon();
        }
        broadcastLobbyState();
    }

    bool areAllPlayersReady() const {
        if (!isHost()) return false;
        if (players[hostSlot].status != STATUS_READY) return false;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active && i != hostSlot) {
                if (players[i].status != STATUS_READY) return false;
            }
        }
        return true;
    }

    int getActivePlayerCount() const {
        int c = 0;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active) c++;
        }
        return c;
    }

    int getReadyPlayerCount() const {
        int c = 0;
        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active) {
                if (i == hostSlot || players[i].status == STATUS_READY) c++;
            }
        }
        return c;
    }

    void cycleWinCondition() {
        if (!isHost()) return;
        winCondition = static_cast<WinCondition>((static_cast<int>(winCondition) + 1) % 3);
        broadcastLobbyState();
    }

    void setWinConditionExplicit(WinCondition wc) {
        if (!isHost()) return;
        winCondition = wc;
        broadcastLobbyState();
    }

    void toggleModMode() {
        if (!isHost()) return;
        modMode = (modMode == MODS_HOST_ONLY) ? MODS_FREEMOD : MODS_HOST_ONLY;
        if (modMode == MODS_HOST_ONLY) {
            for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
                if (players[i].active) players[i].activeMods = players[hostSlot].activeMods;
            }
        }
        broadcastLobbyState();
    }

    void toggleHostRotate() {
        if (!isHost()) return;
        hostRotate = !hostRotate;
        broadcastLobbyState();
    }

    void advanceQueueAndRotateHost() {
        if (!isHost()) return;

        if (!mapQueue.empty()) {
            activeBeatmap = mapQueue.front();
            hostExactOsuPath = mapQueue.front().osuFilename;
            hostExactAudioPath = mapQueue.front().audioFilename;
            mapQueue.erase(mapQueue.begin());
        }

        // Rotación de Host solo permitida en LAN (en UDS el Nodo 1 es AP fijado por hardware 3DS)
        if (hostRotate && netMode != NET_MODE_UDS) {
            int nextHost = (hostSlot + 1) % MULTI_MAX_PLAYERS;
            int attempts = 0;
            while (!players[nextHost].active && attempts < MULTI_MAX_PLAYERS) {
                nextHost = (nextHost + 1) % MULTI_MAX_PLAYERS;
                attempts++;
            }
            if (players[nextHost].active) {
                players[hostSlot].isHost = false;
                hostSlot = nextHost;
                players[hostSlot].isHost = true;
                isHosting = (localSlot == hostSlot);
            }
        }

        bool hostHasMap = false;
        if (!activeBeatmap.osuFilename.empty()) {
            std::string resolved = resolveFilePath(activeBeatmap.folderPath, activeBeatmap.osuFilename);
            if (!resolved.empty()) {
                hostHasMap = true;
                hostExactOsuPath = resolved;
            }
        }

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active) {
                if (i == hostSlot) {
                    players[i].status = hostHasMap ? STATUS_READY : STATUS_MISSING_MAP;
                    players[i].downloadProgress = hostHasMap ? 100 : 0;
                } else {
                    players[i].status = STATUS_NOT_READY;
                    players[i].downloadProgress = 0;
                }
                players[i].resetForMatch();
            }
        }

        matchStarted = false;
        matchInProgressFlag = false;
        matchFinished = false;

        if (isHost()) {
            if (netMode == NET_MODE_UDS) {
                updateUdsBeacon();
            }
            broadcastLobbyState();
        } else {
            sendPlayerUpdate();
        }
    }

    // ========================================================================
    // CÁLCULO DE CLASIFICACIÓN (LEADERBOARD EN VIVO Y PODIO DE RESULTADOS)
    // ========================================================================
    std::vector<PlayerRank> getLeaderboard() const {
        std::vector<PlayerRank> list;
        list.reserve(MULTI_MAX_PLAYERS);

        for (int i = 0; i < MULTI_MAX_PLAYERS; ++i) {
            if (players[i].active) {
                list.push_back(PlayerRank(i, 1, &players[i]));
            }
        }

        std::sort(list.begin(), list.end(), [&](const PlayerRank& a, const PlayerRank& b) {
            if (a.player->isFailed != b.player->isFailed) {
                return !a.player->isFailed;
            }
            if (winCondition == WIN_ACCURACY) {
                if (std::fabs(a.player->accuracy - b.player->accuracy) > 0.01f) {
                    return a.player->accuracy > b.player->accuracy;
                }
                return a.player->score > b.player->score;
            } else if (winCondition == WIN_COMBO) {
                if (a.player->maxCombo != b.player->maxCombo) {
                    return a.player->maxCombo > b.player->maxCombo;
                }
                return a.player->score > b.player->score;
            } else {
                return a.player->score > b.player->score;
            }
        });

        for (size_t r = 0; r < list.size(); ++r) {
            list[r].rank = static_cast<int>(r + 1);
        }

        return list;
    }
};
