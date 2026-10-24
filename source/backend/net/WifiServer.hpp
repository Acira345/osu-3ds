#pragma once
#include <3ds.h>
#include <malloc.h>
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
#include "OszExtractor.hpp"
#include "NetCommon.hpp"

// Servidor Web HTTP Local no bloqueante para Nintendo 3DS (Puerto 8080)
class WifiServer {
private:
    u32* socBuffer = nullptr;
    bool socReady = false;
    int listenSock = -1;
    int clientSock = -1;

    enum ClientState {
        CLIENT_IDLE,
        CLIENT_READING_HEADERS,
        CLIENT_RECEIVING_BODY,
        CLIENT_SENDING_RESPONSE,
        CLIENT_WAIT_CLOSE
    };

    ClientState state = CLIENT_IDLE;
    std::string headerBuf;
    std::string sendBuf;
    size_t sendOffset = 0;
    u64 waitCloseTick = 0;

    std::string uploadFileName;
    std::string uploadTempPath;
    bool isSkinUpload = false;
    FILE* outFile = nullptr;

    size_t contentLength = 0;
    size_t bytesReceived = 0;
    std::vector<uint8_t> ioBuffer;

    static constexpr size_t SOC_ALIGN = 0x1000;
    static constexpr size_t SOC_BUFFERSIZE = 0x100000; // 1 MB
    static constexpr size_t IO_CHUNK_SIZE = 32768;
    static constexpr int SERVER_PORT = 8080;

    static void setNonBlocking(int sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
    }

    static std::string urlDecode(const std::string& str) {
        std::string out;
        out.reserve(str.size());
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                char hex[3] = { str[i + 1], str[i + 2], 0 };
                char c = static_cast<char>(std::strtol(hex, nullptr, 16));
                out.push_back(c);
                i += 2;
            } else if (str[i] == '+') {
                out.push_back(' ');
            } else {
                out.push_back(str[i]);
            }
        }
        return out;
    }

    static std::string sanitizeFilename(std::string name) {
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        for (char& c : name) {
            if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        if (name.empty()) name = "beatmap.osz";
        return name;
    }

    void closeClient() {
        if (outFile) {
            std::fclose(outFile);
            outFile = nullptr;
        }
        if (clientSock >= 0) {
            shutdown(clientSock, SHUT_RDWR);
            close(clientSock);
            clientSock = -1;
        }
        state = CLIENT_IDLE;
        headerBuf.clear();
        sendBuf.clear();
        sendOffset = 0;
        contentLength = 0;
        bytesReceived = 0;
    }

    void queueHttpResponse(int statusCode, const char* statusMsg, const char* contentType, const char* body) {
        size_t bodyLen = std::strlen(body);
        char hdr[320];
        std::snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %u\r\n"
            "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n\r\n",
            statusCode, statusMsg, contentType, static_cast<unsigned int>(bodyLen));

        sendBuf.assign(hdr);
        sendBuf.append(body, bodyLen);
        sendOffset = 0;
        state = CLIENT_SENDING_RESPONSE;
    }

    void prepareWebInterface() {
        static const char* htmlBody =
            "<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>osu! 3DS Hub</title>"
            "<script src='https://cdn.jsdelivr.net/npm/mp4box@0.5.2/dist/mp4box.all.min.js' defer></script>"
            "<style>"
            "*{box-sizing:border-box}"
            "body{font-family:system-ui,-apple-system,sans-serif;background:#0f0d18;color:#fff;margin:0;padding:12px 12px 90px;text-align:center}"
            ".app-container{max-width:620px;margin:0 auto}"
            ".topbar{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;background:#181427;border-radius:14px;border:1px solid #362e54;margin-bottom:12px}"
            ".brand{display:flex;align-items:center;gap:8px}"
            ".logo{font-size:1.25rem;font-weight:900;color:#ff73b4;letter-spacing:-0.5px}"
            ".ver{font-size:0.72rem;background:#292244;color:#50dcff;padding:2px 7px;border-radius:6px;font-weight:700}"
            ".status-pill{font-size:0.75rem;background:#152b20;color:#52e87a;padding:4px 9px;border-radius:12px;border:1px solid #235836;font-weight:600}"
            ".tabs{display:flex;gap:6px;background:#141122;padding:5px;border-radius:12px;margin-bottom:14px;border:1px solid #292440}"
            ".tab-btn{flex:1;background:transparent;border:none;color:#9b95b8;padding:9px 6px;border-radius:8px;font-weight:700;font-size:0.8rem;cursor:pointer;transition:0.15s}"
            ".tab-btn:hover{color:#fff}"
            ".tab-btn.active{background:#2a2347;color:#fff;box-shadow:0 2px 8px rgba(0,0,0,0.35);border:1px solid #ff73b4}"
            ".tab-content{display:none}"
            ".tab-content.active{display:block}"
            ".card{background:#181526;border-radius:14px;padding:16px;border:1px solid #362e54;margin-bottom:14px;text-align:left}"
            ".chips-row{display:flex;gap:6px;overflow-x:auto;padding-bottom:8px;margin-bottom:12px;scrollbar-width:none}"
            ".chips-row::-webkit-scrollbar{display:none}"
            ".chip{background:#221d37;border:1px solid #3a3258;color:#aaa5c7;padding:6px 12px;border-radius:18px;font-size:0.78rem;font-weight:600;white-space:nowrap;cursor:pointer;transition:0.15s}"
            ".chip:hover,.chip.active{background:#ff73b4;color:#fff;border-color:#ff73b4}"
            ".search-row{display:flex;gap:8px;margin-bottom:10px}"
            ".search-row input{flex:1;padding:10px 14px;border-radius:10px;border:1px solid #433a68;background:#100d1c;color:#fff;font-size:0.92rem;outline:none}"
            ".search-row input:focus{border-color:#ff73b4}"
            ".btn{background:#ff73b4;color:#fff;border:none;padding:9px 14px;border-radius:8px;font-weight:700;cursor:pointer;font-size:0.86rem;white-space:nowrap}"
            ".btn:disabled{opacity:0.6;cursor:wait}"
            ".btn2{background:#46b4ff;color:#12101a;border:none;padding:10px 16px;border-radius:8px;font-weight:700;cursor:pointer}"
            ".filter-row{display:flex;gap:6px;margin-bottom:12px;justify-content:center;flex-wrap:wrap}"
            ".filter-btn{background:#141122;color:#aaa5c7;border:1px solid #362e54;padding:5px 10px;border-radius:6px;font-size:0.76rem;cursor:pointer;font-weight:600}"
            ".filter-btn.active{background:#ff73b4;color:#fff;border-color:#ff73b4}"
            ".batch-bar{background:#1e1834;border:1px solid #50dcff;border-radius:10px;padding:8px 12px;margin-bottom:12px;display:none;align-items:center;justify-content:space-between;gap:8px}"
            ".batch-btn{background:#50dcff;color:#100d1c;border:none;padding:7px 12px;border-radius:7px;font-weight:700;font-size:0.8rem;cursor:pointer}"
            ".results{text-align:left;max-height:460px;overflow-y:auto;padding-right:4px}"
            ".item{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #2a2442;gap:10px}"
            ".thumb{width:64px;height:48px;border-radius:8px;object-fit:cover;background:#12101a;flex-shrink:0}"
            ".item-info{flex:1;min-width:0}"
            ".item-title{font-weight:700;font-size:0.9rem;color:#ffe066;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
            ".item-sub{font-size:0.76rem;color:#aaa5c7;margin-top:2px}"
            ".diffs{display:flex;flex-wrap:wrap;gap:4px;margin-top:5px}"
            ".diff-pill{font-size:0.68rem;background:#100d1c;color:#50dcff;padding:2px 6px;border-radius:5px;border:1px solid #332b50}"
            ".actions{display:flex;flex-direction:column;gap:5px;align-items:flex-end}"
            ".btn-play{background:#231d37;color:#50dcff;border:1px solid #433a68;padding:6px 9px;border-radius:7px;font-size:0.74rem;font-weight:600;cursor:pointer}"
            ".btn-play:hover{background:#362d55;color:#fff}"
            ".btn-dl{background:#231d37;color:#aaa5c7;text-decoration:none;padding:6px 9px;border-radius:7px;font-size:0.74rem;font-weight:600}"
            ".pack-card{background:#1f1933;border:1px solid #433a68;border-radius:12px;padding:14px;margin-bottom:12px;display:flex;flex-direction:column;gap:8px}"
            ".pack-head{display:flex;justify-content:space-between;align-items:center}"
            ".pack-title{font-size:1rem;font-weight:700;color:#ffe066}"
            ".pack-badge{background:#ff73b4;color:#fff;font-size:0.7rem;font-weight:700;padding:2px 7px;border-radius:5px}"
            ".pack-desc{font-size:0.8rem;color:#aaa5c7}"
            ".pack-list{font-size:0.75rem;color:#50dcff;background:#141122;padding:6px 10px;border-radius:6px}"
            ".drop{display:block;border:2px dashed #ff73b4;border-radius:12px;padding:22px 14px;margin:12px 0;cursor:pointer;background:#131020;text-align:center;transition:0.2s}"
            ".drop:hover{background:#221b38;border-color:#50dcff}"
            "input[type=file]{display:none}"
            ".opt-row{display:flex;align-items:center;justify-content:center;gap:8px;margin:8px 0;padding:8px 12px;background:#131020;border-radius:8px;border:1px solid #332b50;font-size:0.84rem;color:#ffe066;cursor:pointer}"
            ".opt-row input{accent-color:#ff73b4;width:16px;height:16px}"
            ".pc-box{background:#131020;border:1px solid #332b50;border-radius:10px;padding:12px;font-size:0.8rem;color:#ccc8e0;margin-top:10px}"
            ".bottom-bar{position:fixed;bottom:0;left:0;right:0;background:rgba(19,16,32,0.96);backdrop-filter:blur(8px);border-top:1px solid #362e54;padding:8px 16px;display:flex;justify-content:space-between;align-items:center;gap:12px;z-index:99}"
            ".bottom-info{flex:1;min-width:0;text-align:left}"
            "#status{font-weight:700;font-size:0.82rem;color:#64f078;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
            ".bar-bg{background:#100d1c;height:8px;border-radius:4px;overflow:hidden;margin-top:4px;display:none}"
            ".bar-fill{background:linear-gradient(90deg,#ff73b4,#50dcff);height:100%;width:0%;transition:width 0.1s}"
            ".audio-pill{display:none;align-items:center;gap:8px;background:#221d37;border:1px solid #50dcff;padding:4px 10px;border-radius:20px;font-size:0.76rem;color:#50dcff}"
            ".btn-mini{background:none;border:none;color:#fff;cursor:pointer;font-weight:700;padding:0 4px}"
            "</style></head><body>"
            "<div class='app-container'>"
            "<header class='topbar'>"
            "<div class='brand'>"
            "<span class='logo'>osu! 3DS</span>"
            "<span class='ver'>v3.0 Hub</span>"
            "</div>"
            "<div class='status-pill'>🟢 3DS Conectada</div>"
            "</header>"
            "<nav class='tabs'>"
            "<button class='tab-btn active' onclick='switchTab(\"explore\",this)'>🌟 Explorar</button>"
            "<button class='tab-btn' onclick='switchTab(\"search\",this)'>🔍 Buscar</button>"
            "<button class='tab-btn' onclick='switchTab(\"packs\",this)'>📦 Packs</button>"
            "<button class='tab-btn' onclick='switchTab(\"upload\",this)'>📤 Subir / PC</button>"
            "</nav>"
            "<div class='opt-row' style='margin:0 0 12px 0'>"
            "<label style='display:flex;align-items:center;justify-content:center;gap:8px;cursor:pointer;width:100%'>"
            "<input type='checkbox' id='wantVideo'>"
            "<span>🎬 <b>Incluir Video de Fondo</b> <span style='font-size:0.75rem;color:#50dcff'>(Acelerado por Hardware GPU ⚡ 256x128 @ 15fps)</span></span>"
            "</label>"
            "</div>"
            "<!-- TAB 1: EXPLORAR -->"
            "<div id='tab-explore' class='tab-content active'>"
            "<div class='chips-row'>"
            "<button class='chip active' onclick='loadCategory(\"popular\",this)'>🔥 Populares</button>"
            "<button class='chip' onclick='loadCategory(\"anime\",this)'>🌸 Anime</button>"
            "<button class='chip' onclick='loadCategory(\"jrock\",this)'>🎸 J-Rock &amp; Pop</button>"
            "<button class='chip' onclick='loadCategory(\"vocaloid\",this)'>🤖 Vocaloid</button>"
            "<button class='chip' onclick='loadCategory(\"game\",this)'>🎮 Videojuegos</button>"
            "<button class='chip' onclick='loadCategory(\"tvsize\",this)'>⚡ TV Size</button>"
            "</div>"
            "<div class='batch-bar' id='batchBarExplore'>"
            "<span id='batchTextExplore' style='font-size:0.84rem;color:#50dcff'>Canciones en pantalla</span>"
            "<button class='batch-btn' onclick='downloadAllVisible(\"resultsExplore\",\"batchBarExplore\")'>⬇ Descargar TODAS en cola a 3DS</button>"
            "</div>"
            "<div class='results' id='resultsExplore'></div>"
            "</div>"
            "<!-- TAB 2: BUSCAR -->"
            "<div id='tab-search' class='tab-content'>"
            "<div class='search-row'>"
            "<input type='text' id='q' placeholder='Pega enlace (beatmapsets/...) o busca título/artista...'>"
            "<button class='btn' onclick='searchMaps()'>Buscar</button>"
            "</div>"
            "<div class='filter-row'>"
            "<button class='filter-btn active' onclick='setFilter(\"all\",this)'>Todas</button>"
            "<button class='filter-btn' onclick='setFilter(\"easy\",this)'>Facil &lt;2.5&#9733;</button>"
            "<button class='filter-btn' onclick='setFilter(\"mid\",this)'>Normal/Hard 2.5-4.2&#9733;</button>"
            "<button class='filter-btn' onclick='setFilter(\"insane\",this)'>Insane+ 4.2+&#9733;</button>"
            "</div>"
            "<div class='batch-bar' id='batchBarSearch'>"
            "<span id='batchTextSearch' style='font-size:0.84rem;color:#50dcff'>Resultados encontrados</span>"
            "<button class='batch-btn' onclick='downloadAllVisible(\"resultsSearch\",\"batchBarSearch\")'>⬇ Descargar TODAS en cola a 3DS</button>"
            "</div>"
            "<div class='results' id='resultsSearch'><p style='color:#aaa5c7;padding:16px'>Escribe el nombre de una cancion o pega un enlace de osu!</p></div>"
            "</div>"
            "<!-- TAB 3: PACKS -->"
            "<div id='tab-packs' class='tab-content'>"
            "<div id='packsContainer'></div>"
            "</div>"
            "<!-- TAB 4: SUBIR / PC -->"
            "<div id='tab-upload' class='tab-content'>"
            "<label class='drop' id='dropzone'>"
            "<input type='file' id='fileInput' multiple accept='.osz,.osk,.zip'>"
            "<div style='font-size:1.02rem;font-weight:700;color:#50dcff'>Haz clic aqui o arrastra archivos .osz / .osk</div>"
            "<p style='margin:4px 0;font-size:0.82rem;color:#aaa5c7'>Se envian y extraen solos en la 3DS</p>"
            "</label>"
            "<label class='btn2' style='display:block;margin:10px auto;max-width:380px;text-align:center'>"
            "<input type='file' id='dirInput' webkitdirectory directory multiple>"
            "📂 Importar Carpeta de osu! en PC (osu!stable o osu!lazer)"
            "</label>"
            "<div class='pc-box'>"
            "<div>💡 <b>osu!lazer:</b> Presiona <code>Ctrl + E</code> sobre cualquier cancion para exportarla, o selecciona tu carpeta <code>%appdata%\\osu</code>.</div>"
            "<div style='margin-top:6px'>💡 <b>osu!stable:</b> Selecciona tu carpeta <code>%localappdata%\\osu!\\Songs</code> para listar e instalar toda tu biblioteca.</div>"
            "</div>"
            "</div>"
            "</div>"
            "<!-- BARRA INFERIOR STICKY -->"
            "<div class='bottom-bar'>"
            "<div class='bottom-info'>"
            "<div id='status'>Listo para recibir canciones o skins</div>"
            "<div class='bar-bg' id='barBg'><div class='bar-fill' id='barFill'></div></div>"
            "</div>"
            "<div class='audio-pill' id='audioPill'>"
            "<span id='audioTitle' style='max-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap'>Audio</span>"
            "<button class='btn-mini' id='audioPlayBtn' onclick='toggleAudio()'>⏸</button>"
            "<button class='btn-mini' onclick='stopAudio()'>✕</button>"
            "</div>"
            "</div>"
            "<script>"
            "const st=document.getElementById('status'),barBg=document.getElementById('barBg'),barFill=document.getElementById('barFill');"
            "let uploadChain=Promise.resolve();"
            "const crcTable=(()=>{const t=new Uint32Array(256);for(let i=0;i<256;i++){let c=i;for(let k=0;k<8;k++)c=(c&1)?(0xEDB88320^(c>>>1)):(c>>>1);t[i]=c>>>0;}return t;})();"
            "function crc32(u8){let c=0xFFFFFFFF;for(let i=0;i<u8.length;i++)c=crcTable[(c^u8[i])&0xFF]^(c>>>8);return(c^0xFFFFFFFF)>>>0;}"
            "function buildZipBlob(entries){"
            "let bodySz=0,cdSz=0;"
            "for(const it of entries){bodySz+=30+it.nameBytes.length+it.compData.length;cdSz+=46+it.nameBytes.length;}"
            "const out=new Uint8Array(bodySz+cdSz+22),odv=new DataView(out.buffer);"
            "let wOff=0,cdOff=bodySz;"
            "for(const it of entries){"
            "const lfhPos=wOff;"
            "odv.setUint32(wOff,0x04034b50,true);odv.setUint16(wOff+4,20,true);odv.setUint16(wOff+6,it.flags,true);odv.setUint16(wOff+8,it.method,true);"
            "odv.setUint32(wOff+14,it.crc,true);odv.setUint32(wOff+18,it.compSz,true);odv.setUint32(wOff+22,it.uncompSz,true);odv.setUint16(wOff+26,it.nameBytes.length,true);"
            "out.set(it.nameBytes,wOff+30);wOff+=30+it.nameBytes.length;"
            "out.set(it.compData,wOff);wOff+=it.compData.length;"
            "odv.setUint32(cdOff,0x02014b50,true);odv.setUint16(cdOff+4,20,true);odv.setUint16(cdOff+6,20,true);odv.setUint16(cdOff+8,it.flags,true);odv.setUint16(cdOff+10,it.method,true);"
            "odv.setUint32(cdOff+16,it.crc,true);odv.setUint32(cdOff+20,it.compSz,true);odv.setUint32(cdOff+24,it.uncompSz,true);odv.setUint16(cdOff+28,it.nameBytes.length,true);"
            "odv.setUint32(cdOff+42,lfhPos,true);out.set(it.nameBytes,cdOff+46);cdOff+=46+it.nameBytes.length;"
            "}"
            "odv.setUint32(cdOff,0x06054b50,true);odv.setUint16(cdOff+8,entries.length,true);odv.setUint16(cdOff+10,entries.length,true);"
            "odv.setUint32(cdOff+12,cdSz,true);odv.setUint32(cdOff+16,bodySz,true);"
            "return new Blob([out]);}"
            "function assemble3dsv(frames,W,H,fps){"
            "if(!frames||!frames.length)return null;"
            "const count=frames.length,hdrSize=16+count*8;"
            "let totalSize=hdrSize;for(const f of frames)totalSize+=f.length;"
            "const out=new Uint8Array(totalSize),dv=new DataView(out.buffer);"
            "out[0]=0x33;out[1]=0x44;out[2]=0x53;out[3]=0x56;"
            "dv.setUint16(4,W,true);dv.setUint16(6,H,true);dv.setUint16(8,fps*100,true);dv.setUint16(10,0,true);dv.setUint32(12,count,true);"
            "let off=hdrSize;"
            "for(let i=0;i<count;i++){"
            "dv.setUint32(16+i*8,off,true);dv.setUint32(16+i*8+4,frames[i].length,true);"
            "out.set(frames[i],off);off+=frames[i].length;"
            "}"
            "return out;}"
            "async function ensureMp4Box(){"
            "if(typeof MP4Box!=='undefined')return true;"
            "return new Promise((res)=>{"
            "const s=document.createElement('script');"
            "s.src='https://cdn.jsdelivr.net/npm/mp4box@0.5.2/dist/mp4box.all.min.js';"
            "s.onload=()=>res(true);s.onerror=()=>res(false);"
            "document.head.appendChild(s);"
            "setTimeout(()=>res(false),3000);"
            "});}"
            "async function transcodeWebCodecs(vidBytes){"
            "if(typeof VideoDecoder==='undefined')return null;"
            "const hasMp4=await ensureMp4Box();"
            "if(!hasMp4||typeof MP4Box==='undefined')return null;"
            "return new Promise((resolve)=>{"
            "try{"
            "const mp4box=MP4Box.createFile();"
            "let decoder=null;"
            "const W=256,H=128,fps=15,intervalUs=1000000/fps;"
            "let nextTargetUs=0,totalDurSec=0,processedSamples=0,isDone=false;"
            "const frames=[],frameQueue=[];"
            "let processingQueue=false;"
            "const cv=document.createElement('canvas');cv.width=W;cv.height=H;"
            "const ctx=cv.getContext('2d');"
            "const finish=()=>{"
            "if(isDone)return;isDone=true;"
            "try{if(decoder&&decoder.state!=='closed')decoder.close();}catch(e){}"
            "resolve(assemble3dsv(frames,W,H,fps));"
            "};"
            "const failTo=setTimeout(()=>{finish();},35000);"
            "async function processFrameQueue(){"
            "if(processingQueue)return;processingQueue=true;"
            "while(frameQueue.length>0){"
            "const vf=frameQueue.shift();"
            "try{ctx.drawImage(vf,0,0,W,H);vf.close();}catch(e){try{vf.close();}catch(e2){}}"
            "const blob=await new Promise(r=>cv.toBlob(r,'image/jpeg',0.62));"
            "if(blob){frames.push(new Uint8Array(await blob.arrayBuffer()));}"
            "}"
            "processingQueue=false;"
            "}"
            "mp4box.onError=()=>{clearTimeout(failTo);finish();};"
            "mp4box.onReady=(info)=>{"
            "const vTrack=info.videoTracks&&info.videoTracks[0];"
            "if(!vTrack){clearTimeout(failTo);finish();return;}"
            "totalDurSec=Math.min((info.duration/info.timescale)||180,240);"
            "let description=null;"
            "try{"
            "const trak=mp4box.getTrackById(vTrack.id);"
            "for(const entry of trak.mdia.minf.stbl.stsd.entries){"
            "const box=entry.avcC||entry.hvcC||entry.av1C||entry.vpcC;"
            "if(box){const st=new DataStream(undefined,0,DataStream.BIG_ENDIAN);box.write(st);description=new Uint8Array(st.buffer,8);break;}"
            "}"
            "}catch(e){}"
            "const config={codec:vTrack.codec,codedWidth:vTrack.video.width,codedHeight:vTrack.video.height,description:description};"
            "try{"
            "decoder=new VideoDecoder({"
            "output:(vf)=>{"
            "const ts=vf.timestamp;"
            "if(ts>=nextTargetUs){"
            "nextTargetUs+=intervalUs;"
            "frameQueue.push(vf);"
            "processFrameQueue();"
            "}else{vf.close();}"
            "},"
            "error:()=>{finish();}"
            "});"
            "decoder.configure(config);"
            "}catch(e){clearTimeout(failTo);finish();return;}"
            "mp4box.setExtractionOptions(vTrack.id,null,{nbSamples:1000});"
            "mp4box.start();"
            "};"
            "mp4box.onSamples=async(trackId,ref,samples)=>{"
            "for(const s of samples){"
            "if(isDone)break;"
            "const sec=(s.cts/s.timescale);"
            "if(sec>240){isDone=true;break;}"
            "processedSamples++;"
            "if(processedSamples%30===0){"
            "const p=Math.min(99,Math.round((sec/totalDurSec)*100));"
            "barBg.style.display='block';barFill.style.width=p+'%';"
            "st.style.color='#50dcff';st.textContent='⚡ Decodificando por GPU ('+p+'%)...';"
            "}"
            "while(frameQueue.length>8||(decoder&&decoder.decodeQueueSize>12)){"
            "await new Promise(r=>setTimeout(r,6));"
            "}"
            "if(decoder&&decoder.state==='configured'){"
            "decoder.decode(new EncodedVideoChunk({"
            "type:s.is_sync?'key':'delta',"
            "timestamp:(s.cts*1000000)/s.timescale,"
            "duration:(s.duration*1000000)/s.timescale,"
            "data:s.data"
            "}));"
            "}"
            "}"
            "if(decoder&&decoder.state==='configured'){try{await decoder.flush();}catch(e){}}"
            "while(frameQueue.length>0||processingQueue){await new Promise(r=>setTimeout(r,6));}"
            "clearTimeout(failTo);finish();"
            "};"
            "const ab=vidBytes.buffer.slice(vidBytes.byteOffset,vidBytes.byteOffset+vidBytes.byteLength);"
            "ab.fileStart=0;"
            "mp4box.appendBuffer(ab);"
            "mp4box.flush();"
            "}catch(e){finish();}"
            "});}"
            "async function transcodeFallback(vidBytes){"
            "return new Promise((resolve)=>{"
            "const url=URL.createObjectURL(new Blob([vidBytes]));"
            "const v=document.createElement('video');"
            "v.muted=true;v.playsInline=true;v.preload='auto';v.src=url;"
            "const cleanup=()=>{try{v.pause();v.removeAttribute('src');v.load();URL.revokeObjectURL(url);}catch(e){}};"
            "const failTimeout=setTimeout(()=>{cleanup();resolve(null);},15000);"
            "v.onerror=()=>{clearTimeout(failTimeout);cleanup();resolve(null);};"
            "v.onloadedmetadata=async()=>{"
            "clearTimeout(failTimeout);"
            "const dur=Math.min(v.duration||0,240);"
            "if(!dur||!isFinite(dur)||dur<1){cleanup();resolve(null);return;}"
            "const fps=15,W=256,H=128,total=Math.min(3600,Math.max(1,Math.floor(dur*fps)));"
            "const cv=document.createElement('canvas');cv.width=W;cv.height=H;"
            "const ctx=cv.getContext('2d');"
            "const frames=[];"
            "barBg.style.display='block';"
            "for(let i=0;i<total;i++){"
            "v.currentTime=i/fps;"
            "await new Promise(r=>{const to=setTimeout(r,250);v.onseeked=()=>{clearTimeout(to);r();};});"
            "ctx.drawImage(v,0,0,W,H);"
            "const jb=await new Promise(r=>cv.toBlob(r,'image/jpeg',0.62));"
            "if(!jb)continue;"
            "frames.push(new Uint8Array(await jb.arrayBuffer()));"
            "if(i%8===0||i===total-1){const p=Math.round(((i+1)/total)*100);barFill.style.width=p+'%';st.style.color='#ffe066';st.textContent='🎬 Convirtiendo video para 3DS ('+p+'%)...';}"
            "}"
            "cleanup();"
            "resolve(assemble3dsv(frames,W,H,fps));"
            "};});}"
            "async function transcodeTo3dsv(vidBytes){"
            "barBg.style.display='block';barFill.style.width='5%';"
            "st.style.color='#50dcff';st.textContent='⚡ Decodificando video por Hardware (GPU)...';"
            "try{"
            "const v3=await transcodeWebCodecs(vidBytes);"
            "if(v3&&v3.length>64)return v3;"
            "}catch(e){}"
            "return await transcodeFallback(vidBytes);"
            "}"
            "async function optimizeOszFor3ds(blob,name,wantVideo){"
            "if(!name.toLowerCase().endsWith('.osz'))return blob;"
            "try{"
            "const ab=await blob.arrayBuffer(),u8=new Uint8Array(ab),dv=new DataView(ab);"
            "let eocd=-1,minPos=Math.max(0,u8.length-65557);"
            "for(let i=u8.length-22;i>=minPos;i--){if(dv.getUint32(i,true)===0x06054b50){eocd=i;break;}}"
            "if(eocd<0)return blob;"
            "const numEntries=dv.getUint16(eocd+10,true),cdOffset=dv.getUint32(eocd+16,true);"
            "let pos=cdOffset,vidEntry=null,needsRepack=false;"
            "const entries=[];"
            "const dec=new TextDecoder();"
            "for(let e=0;e<numEntries&&pos+46<=u8.length;e++){"
            "if(dv.getUint32(pos,true)!==0x02014b50)break;"
            "const flags=dv.getUint16(pos+8,true),method=dv.getUint16(pos+10,true),crc=dv.getUint32(pos+16,true);"
            "const compSz=dv.getUint32(pos+20,true),uncompSz=dv.getUint32(pos+24,true);"
            "const nLen=dv.getUint16(pos+28,true),xLen=dv.getUint16(pos+30,true),cLen=dv.getUint16(pos+32,true);"
            "const lfhOff=dv.getUint32(pos+42,true);"
            "const nameBytes=u8.subarray(pos+46,pos+46+nLen),fName=dec.decode(nameBytes);"
            "pos+=46+nLen+xLen+cLen;"
            "const lfhN=dv.getUint16(lfhOff+26,true),lfhX=dv.getUint16(lfhOff+28,true);"
            "const dataOff=lfhOff+30+lfhN+lfhX;"
            "if(/\\.(mp4|webm|avi|mkv|flv|wmv)$/i.test(fName)){if(!vidEntry)vidEntry={fName,method,compSz,dataOff};needsRepack=true;continue;}"
            "if(/@2x\\./i.test(fName)){needsRepack=true;continue;}"
            "entries.push({flags:flags&~8,method,crc,compSz,uncompSz,nameBytes,compData:u8.subarray(dataOff,dataOff+compSz)});"
            "}"
            "if(!needsRepack)return blob;"
            "let vid3dsv=null;"
            "if(wantVideo&&vidEntry){"
            "st.style.color='#ffe066';st.textContent='🎬 Extrayendo video de '+name+'...';"
            "let rawVid=null;"
            "const slice=u8.subarray(vidEntry.dataOff,vidEntry.dataOff+vidEntry.compSz);"
            "if(vidEntry.method===0)rawVid=slice;"
            "else if(vidEntry.method===8&&typeof DecompressionStream!=='undefined'){"
            "try{const ds=new Blob([slice]).stream().pipeThrough(new DecompressionStream('deflate-raw'));rawVid=new Uint8Array(await new Response(ds).arrayBuffer());}catch(e){}"
            "}"
            "if(rawVid)vid3dsv=await transcodeTo3dsv(rawVid);"
            "}"
            "if(vid3dsv){"
            "const vn=new TextEncoder().encode('video.3dsv');"
            "entries.push({flags:0,method:0,crc:crc32(vid3dsv),compSz:vid3dsv.length,uncompSz:vid3dsv.length,nameBytes:vn,compData:vid3dsv});"
            "}"
            "return buildZipBlob(entries);"
            "}catch(e){return blob;}}"
            "function uploadBlob(blob,name){"
            "const run=async()=>{"
            "const wantVid=document.getElementById('wantVideo').checked;"
            "let finalBlob=blob;"
            "if(wantVid){try{finalBlob=await optimizeOszFor3ds(blob,name,true);}catch(e){finalBlob=blob;}}"
            "return new Promise((res,rej)=>{"
            "barBg.style.display='block';barFill.style.width='0%';"
            "st.style.color='#50dcff';st.textContent='Enviando '+name+' a la 3DS...';"
            "const xhr=new XMLHttpRequest();"
            "xhr.open('POST','/upload?name='+encodeURIComponent(name),true);"
            "xhr.upload.onprogress=(e)=>{if(e.lengthComputable){const p=Math.round((e.loaded/e.total)*100);barFill.style.width=p+'%';st.textContent='Enviando a la 3DS: '+name+' ('+p+'%)...';}};"
            "xhr.onload=()=>{if(xhr.status===200){barFill.style.width='100%';st.style.color='#64f078';st.textContent='✓ Instalado en tu 3DS: '+name;res();}else{st.style.color='#f04646';st.textContent='Error al guardar en 3DS (HTTP '+xhr.status+')';rej(new Error('Error al guardar en 3DS'));}};"
            "xhr.onerror=()=>{st.style.color='#f04646';st.textContent='Error de conexion Wi-Fi con la 3DS';rej(new Error('Conexion con 3DS fallida'));};"
            "xhr.send(finalBlob);"
            "});};"
            "uploadChain=uploadChain.then(run,run);"
            "return uploadChain;}"
            "document.getElementById('fileInput').addEventListener('change',async(e)=>{"
            "for(const f of e.target.files){await uploadBlob(f,f.name);}"
            "});"
            "document.getElementById('dirInput').addEventListener('change',async(e)=>{"
            "const files=Array.from(e.target.files||[]);if(!files.length)return;"
            "const enc=new TextEncoder(),dec=new TextDecoder();"
            "const oszFiles=files.filter(f=>/\\.(osz|osk)$/i.test(f.name));"
            "for(const f of oszFiles){await uploadBlob(f,f.name);}"
            "const stableGroups=new Map();"
            "for(const f of files){"
            "const rel=(f.webkitRelativePath||f.name).replace(/\\\\/g,'/');"
            "const parts=rel.split('/');"
            "if(parts.some(p=>p.toLowerCase()==='files'||p.toLowerCase()==='cache'))continue;"
            "const folder=parts.length>=2?parts[parts.length-2]:'Beatmap';"
            "if(!stableGroups.has(folder))stableGroups.set(folder,[]);"
            "stableGroups.get(folder).push(f);"
            "}"
            "let stableSent=0;"
            "for(const[folder,flist]of stableGroups.entries()){"
            "if(!flist.some(f=>f.name.toLowerCase().endsWith('.osu')))continue;"
            "stableSent++;"
            "st.style.color='#ffe066';st.textContent='Empaquetando carpeta de osu!stable: '+folder+'...';"
            "const entries=[];const wantVid=document.getElementById('wantVideo').checked;let vidDone=false;"
            "for(const f of flist){"
            "const low=f.name.toLowerCase();"
            "if(low.includes('@2x.'))continue;"
            "if(/\\.(mp4|webm|avi|mkv|flv)$/i.test(low)){"
            "if(wantVid&&!vidDone){vidDone=true;const vBytes=new Uint8Array(await f.arrayBuffer());const v3=await transcodeTo3dsv(vBytes);if(v3){const vn=enc.encode('video.3dsv');entries.push({flags:0,method:0,crc:crc32(v3),compSz:v3.length,uncompSz:v3.length,nameBytes:vn,compData:v3});}}"
            "continue;}"
            "if(!/\\.(osu|ini|mp3|wav|ogg|jpg|jpeg|png|3dsv|mjpg)$/i.test(low))continue;"
            "const u8=new Uint8Array(await f.arrayBuffer()),nb=enc.encode(f.name);"
            "entries.push({flags:0,method:0,crc:crc32(u8),compSz:u8.length,uncompSz:u8.length,nameBytes:nb,compData:u8});"
            "}"
            "if(entries.length){const zBlob=buildZipBlob(entries);await uploadBlob(zBlob,folder+'.osz');}"
            "}"
            "if(stableSent>0)return;"
            "st.style.color='#ffe066';st.textContent='Escaneando base de datos de osu!lazer...';"
            "const lazerSets=new Map();"
            "const cand=files.filter(f=>!f.name.includes('.')&&f.size>120&&f.size<1500000);"
            "for(let i=0;i<cand.length;i++){"
            "const f=cand[i];"
            "const headBuf=await f.slice(0,28).arrayBuffer();"
            "const headStr=dec.decode(headBuf);"
            "if(!headStr.includes('osu file format'))continue;"
            "const txt=dec.decode(await f.slice(0,4096).arrayBuffer());"
            "const mSet=txt.match(/^BeatmapSetID\\s*:\\s*(\\d+)/m);"
            "if(!mSet)continue;"
            "const sid=mSet[1];if(lazerSets.has(sid))continue;"
            "const mTit=txt.match(/^Title\\s*:\\s*(.+)/m),mArt=txt.match(/^Artist\\s*:\\s*(.+)/m),mCre=txt.match(/^Creator\\s*:\\s*(.+)/m);"
            "const hasVid=/^(?:1|Video),/m.test(txt);"
            "lazerSets.set(sid,{id:sid,title:mTit?mTit[1].trim():('Set #'+sid),artist:mArt?mArt[1].trim():'osu!lazer',creator:mCre?mCre[1].trim():'lazer',video:hasVid});"
            "if(lazerSets.size%5===0)st.textContent='Detectados '+lazerSets.size+' mapas en osu!lazer...';"
            "}"
            "const rDiv=document.getElementById('results');rDiv.innerHTML='';"
            "if(lazerSets.size>0){"
            "st.style.color='#64f078';st.textContent='¡Se detectaron '+lazerSets.size+' canciones en tu osu!lazer!';"
            "const barAll=document.createElement('div');barAll.style.cssText='padding:10px;background:#26203a;border:1px solid #50dcff;border-radius:10px;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center';"
            "barAll.innerHTML='<div><b style=\"color:#50dcff\">Biblioteca osu!lazer ('+lazerSets.size+' mapas)</b><div style=\"font-size:0.76rem;color:#aaa5c3\">Puedes instalar todos en lote o elegir abajo</div></div>';"
            "const btnAll=document.createElement('button');btnAll.className='btn';btnAll.textContent='⬇ Instalar TODOS ('+lazerSets.size+')';"
            "btnAll.onclick=async()=>{btnAll.disabled=true;let k=0;for(const s of lazerSets.values()){k++;btnAll.textContent='Instalando '+k+'/'+lazerSets.size+'...';try{const b=await fetchOszMultiMirror(s.id,s.title);const safe=((s.artist||'osu')+' - '+(s.title||'map')).replace(/[^a-zA-Z0-9 _.-]/g,'_');await uploadBlob(b,s.id+' '+safe+'.osz');}catch(e){}}btnAll.textContent='✓ Biblioteca Instalada!';};"
            "barAll.appendChild(btnAll);rDiv.appendChild(barAll);"
            "lazerSets.forEach(s=>renderCard(s,rDiv,false,null));"
            "}else if(!oszFiles.length){st.style.color='#f04646';st.textContent='No se encontraron mapas en la carpeta seleccionada.';}"
            "});"
            "const dz=document.getElementById('dropzone');"
            "dz.addEventListener('dragover',(e)=>{e.preventDefault();});"
            "dz.addEventListener('drop',async(e)=>{e.preventDefault();for(const f of e.dataTransfer.files){await uploadBlob(f,f.name);}});"
            "function parseOsuInput(q){"
            "let m=q.match(/beatmapsets\\/(\\d+)(?:[^#]*#\\w+\\/(\\d+))?/i);"
            "if(m)return{type:'set',id:m[1],bid:m[2]||null};"
            "m=q.match(/(?:osu\\.ppy\\.sh\\/s\\/|catboy\\.best\\/d\\/|nerinyan\\.moe\\/d\\/)(\\d+)/i);"
            "if(m)return{type:'set',id:m[1],bid:null};"
            "m=q.match(/osu\\.ppy\\.sh\\/(?:beatmaps|b)\\/(\\d+)/i);"
            "if(m)return{type:'map',id:m[1],bid:m[1]};"
            "if(/^\\d{4,10}$/.test(q))return{type:'id',id:q,bid:null};"
            "return null;}"
            "async function fetchOszMultiMirror(setId,title){"
            "const wantVid=document.getElementById('wantVideo').checked;"
            "const urls=wantVid?["
            "'https://catboy.best/d/'+setId,"
            "'https://txy1.sayobot.cn/beatmaps/download/full/'+setId,"
            "'https://dl.sayobot.cn/beatmaps/download/full/'+setId,"
            "'https://api.nerinyan.moe/d/'+setId+'?noVideo=0',"
            "'https://catboy.best/d/'+setId+'n',"
            "'https://txy1.sayobot.cn/beatmaps/download/novideo/'+setId"
            "]:["
            "'https://catboy.best/d/'+setId+'n',"
            "'https://catboy.best/d/'+setId,"
            "'https://txy1.sayobot.cn/beatmaps/download/novideo/'+setId,"
            "'https://dl.sayobot.cn/beatmaps/download/novideo/'+setId,"
            "'https://api.nerinyan.moe/d/'+setId+'?noVideo=1'"
            "];"
            "for(let i=0;i<urls.length;i++){"
            "try{st.style.color='#ffe066';st.textContent='Descargando '+title+' (Espejo '+(i+1)+'/'+urls.length+')...';"
            "const r=await fetch(urls[i]);"
            "if(r.ok){const b=await r.blob();if(b.size>2048){const h=new Uint8Array(await b.slice(0,2).arrayBuffer());if(h[0]===0x50&&h[1]===0x4B)return b;}}"
            "}catch(e){}}"
            "throw new Error('No se pudo descargar de los servidores');}"
            "let currentAudio=null,currentAudioId=null,currentAudioBtn=null;"
            "function playPreview(setId,title,btn){"
            "const pill=document.getElementById('audioPill');const aTit=document.getElementById('audioTitle');const aPlay=document.getElementById('audioPlayBtn');"
            "if(currentAudio&&currentAudioId===setId){"
            "if(currentAudio.paused){currentAudio.play().catch(()=>{});if(btn)btn.textContent='⏸ Pausar';if(aPlay)aPlay.textContent='⏸';}"
            "else{currentAudio.pause();if(btn)btn.textContent='▶ Escuchar';if(aPlay)aPlay.textContent='▶';}return;"
            "}"
            "if(currentAudio){currentAudio.pause();if(currentAudioBtn)currentAudioBtn.textContent='▶ Escuchar';}"
            "currentAudioId=setId;currentAudioBtn=btn;"
            "if(btn)btn.textContent='⌛ Cargando...';if(aPlay)aPlay.textContent='⌛';if(aTit)aTit.textContent=title||('Set #'+setId);if(pill)pill.style.display='flex';"
            "const pUrls=['https://b.ppy.sh/preview/'+setId+'.mp3','https://catboy.best/preview/'+setId+'.mp3'];"
            "let idx=0;"
            "function tryAudio(){"
            "if(idx>=pUrls.length){"
            "if(btn&&currentAudioId===setId)btn.textContent='❌ Sin audio';"
            "setTimeout(()=>{if(btn&&currentAudioId===setId)btn.textContent='▶ Escuchar';if(pill&&currentAudioId===setId)pill.style.display='none';if(currentAudioId===setId){currentAudio=null;currentAudioId=null;}},2000);"
            "return;"
            "}"
            "const a=new Audio(pUrls[idx++]);a.volume=0.75;"
            "a.onplaying=()=>{if(btn&&currentAudioId===setId)btn.textContent='⏸ Pausar';if(aPlay)aPlay.textContent='⏸';};"
            "a.onended=()=>{if(btn&&currentAudioId===setId)btn.textContent='▶ Escuchar';if(pill&&currentAudioId===setId)pill.style.display='none';if(currentAudioId===setId){currentAudio=null;currentAudioId=null;}};"
            "a.onerror=()=>{if(currentAudio===a)tryAudio();};"
            "currentAudio=a;"
            "a.play().catch(()=>{if(currentAudio===a)tryAudio();});"
            "}"
            "tryAudio();"
            "}"
            "function toggleAudio(){"
            "if(!currentAudio)return;const aPlay=document.getElementById('audioPlayBtn');"
            "if(currentAudio.paused){currentAudio.play().catch(()=>{});if(aPlay)aPlay.textContent='⏸';if(currentAudioBtn)currentAudioBtn.textContent='⏸ Pausar';}"
            "else{currentAudio.pause();if(aPlay)aPlay.textContent='▶';if(currentAudioBtn)currentAudioBtn.textContent='▶ Escuchar';}"
            "}"
            "function stopAudio(){"
            "if(currentAudio){currentAudio.pause();currentAudio=null;currentAudioId=null;}"
            "if(currentAudioBtn)currentAudioBtn.textContent='▶ Escuchar';"
            "const pill=document.getElementById('audioPill');if(pill)pill.style.display='none';"
            "}"
            "function switchTab(tabId,btn){"
            "document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));"
            "document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"
            "if(btn)btn.classList.add('active');"
            "const target=document.getElementById('tab-'+tabId);if(target)target.classList.add('active');"
            "}"
            "const packsData=["
            "{title:'Pack Esenciales de Inicio',badge:'8 Mapas Variados',desc:'Grandes clasicos del ritmo y mapas mas jugados de la historia.',songs:'Bad Apple!!, No title, Hitorigoto, Tear Rain, My Love, Kimi no Shiranai, Renai Circulation, Chiisana Koi',ids:[17500,320118,598717,9338,163112,8683,351630,24391]},"
            "{title:'Pack Anime Openings & J-Pop',badge:'8 Temas Famosos',desc:'Openings iconicos de Demon Slayer, Naruto, Attack on Titan, Wotakoi y Chainsaw Man.',songs:'Fiction (sumika), Kaibutsu (YOASOBI), Gurenge (LiSA), KICK BACK, Silhouette, Shinzou wo Sasageyo, Blue Bird, unravel',ids:[826106,1361589,961450,1876402,236292,593721,175040,158023]},"
            "{title:'Pack Vocaloid Clasicos',badge:'6 Exitos de Miku',desc:'Himnos inolvidables de Hatsune Miku y productores legendarios.',songs:'Melt, Senbonzakura, World is Mine, Disappearance of Miku, Six Trillion Years, Hibikase',ids:[3860,43701,4867,8404,48259,212588]},"
            "{title:'Pack J-Rock High Energy',badge:'6 Temas Rock',desc:'Canciones rapidas con bateria potente y guitarras energicas.',songs:'Kyouran Hey Kids!!, Haruka Kanata, Baton Road, Kanzen Kankaku Dreamer, The Rumbling, Black Catcher',ids:[372851,8168,595701,10609,1679093,1108263]}"
            "];"
            "function renderPacks(){"
            "const cont=document.getElementById('packsContainer');if(!cont||cont.dataset.done==='1')return;"
            "cont.dataset.done='1';cont.innerHTML='';"
            "packsData.forEach((p,idx)=>{"
            "const card=document.createElement('div');card.className='pack-card';"
            "card.innerHTML='<div class=\"pack-head\"><span class=\"pack-title\">'+p.title+'</span><span class=\"pack-badge\">'+p.badge+'</span></div>'+'<div class=\"pack-desc\">'+p.desc+'</div>'+'<div class=\"pack-list\"><b>Incluye:</b> '+p.songs+'</div>';"
            "const b=document.createElement('button');b.className='btn';b.style.marginTop='6px';b.textContent='⬇ Instalar Pack en 3DS ('+p.ids.length+' canciones)';"
            "b.onclick=()=>installPack(idx,b);"
            "card.appendChild(b);cont.appendChild(card);"
            "});"
            "}"
            "async function installPack(packIdx,btn){"
            "const p=packsData[packIdx];if(!p)return;btn.disabled=true;"
            "for(let i=0;i<p.ids.length;i++){"
            "const id=p.ids[i];btn.textContent='Instalando ('+(i+1)+'/'+p.ids.length+'): Set #'+id+'...';"
            "try{const blob=await fetchOszMultiMirror(id,'Pack #'+id);await uploadBlob(blob,id+'_pack.osz');}catch(e){}"
            "}"
            "btn.disabled=false;btn.textContent='✓ Pack Instalado en 3DS!';"
            "}"
            "let currentFilter='all',currentResults=[],isDirectLinkMode=false;"
            "function getMaxStars(s){"
            "if(!Array.isArray(s.beatmaps)||!s.beatmaps.length)return 0;"
            "let maxS=0;for(const b of s.beatmaps){if(b.mode_int===0||b.mode==='osu'||b.mode===0){const r=Number(b.difficulty_rating)||0;if(r>maxS)maxS=r;}}"
            "return maxS;"
            "}"
            "function matchesFilter(s){"
            "if(currentFilter==='all')return true;"
            "const maxS=getMaxStars(s);if(maxS===0)return true;"
            "if(currentFilter==='easy')return maxS<2.5;"
            "if(currentFilter==='mid')return maxS>=2.5&&maxS<4.2;"
            "if(currentFilter==='insane')return maxS>=4.2;"
            "return true;"
            "}"
            "function setFilter(filt,btn){"
            "currentFilter=filt;"
            "document.querySelectorAll('.filter-btn').forEach(b=>b.classList.remove('active'));"
            "if(btn)btn.classList.add('active');"
            "renderCurrentList('resultsSearch','batchBarSearch','batchTextSearch');"
            "}"
            "function renderCard(s,rDiv,isLink,targetBid){"
            "const d=document.createElement('div');d.className='item'+(isLink?' highlight':'');"
            "const safeTitle=((s.artist||'osu')+' - '+(s.title||'map')).replace(/[^a-zA-Z0-9 _.-]/g,'_');"
            "const img=document.createElement('img');img.className='thumb';"
            "img.src=(s.covers&&s.covers.list)?s.covers.list:'https://assets.ppy.sh/beatmaps/'+s.id+'/covers/list.jpg';"
            "img.onerror=()=>{img.style.display='none';};"
            "d.appendChild(img);"
            "const info=document.createElement('div');info.className='item-info';"
            "let html=isLink?'<div class=\"badge-link\">Link Detectado (Set #'+s.id+')</div>':'';"
            "const vidBadge=s.video?' <span style=\"color:#50dcff;font-size:0.75rem\">[🎬 VIDEO]</span>':'';"
            "let stBadge='';"
            "if(s.status===1||s.status==='ranked'||s.status==='approved')stBadge=' <span style=\"background:#153d26;color:#52e87a;font-size:0.68rem;padding:2px 6px;border-radius:4px;font-weight:700\">★ Ranked</span>';"
            "else if(s.status===4||s.status==='loved')stBadge=' <span style=\"background:#3a1832;color:#ff73b4;font-size:0.68rem;padding:2px 6px;border-radius:4px;font-weight:700\">💖 Loved</span>';"
            "else if(s.status===-2||s.status==='graveyard')stBadge=' <span style=\"background:#2d263f;color:#aaa5c7;font-size:0.68rem;padding:2px 6px;border-radius:4px\">⚰️ Graveyard</span>';"
            "html+='<div class=\"item-title\">'+(s.title||'Beatmap #'+s.id)+vidBadge+stBadge+'</div>';"
            "html+='<div class=\"item-sub\">'+(s.artist||'')+' | Mapper: '+(s.creator||'?')+(s.bpm?' | '+s.bpm+' BPM':'')+'</div>';"
            "if(Array.isArray(s.beatmaps)){"
            "const std=s.beatmaps.filter(b=>b.mode_int===0||b.mode==='osu'||b.mode===0).sort((a,b)=>(a.difficulty_rating||0)-(b.difficulty_rating||0));"
            "if(std.length){html+='<div class=\"diffs\">';"
            "std.slice(0,8).forEach(b=>{"
            "const sel=targetBid&&String(b.id)===String(targetBid)?' sel':'';"
            "const stars=b.difficulty_rating?Number(b.difficulty_rating).toFixed(2):'?';"
            "html+='<span class=\"diff-pill'+sel+'\">★ '+stars+' '+b.version+'</span>';"
            "});html+='</div>';}}"
            "info.innerHTML=html;d.appendChild(info);"
            "const act=document.createElement('div');act.className='actions';"
            "const isBlocked=s.availability&&s.availability.download_disabled;"
            "const b=document.createElement('button');b.className='btn card-btn-dl';b.textContent='⬇ Instalar';"
            "if(isBlocked){b.disabled=true;b.style.background='#362432';b.style.color='#ff94b3';b.textContent='⚠️ Retirado (DMCA)';}"
            "b.triggerDownload=async()=>{"
            "if(isBlocked){st.style.color='#f04646';st.textContent='Este mapa fue retirado de osu! por derechos de autor (DMCA).';return;}"
            "b.disabled=true;b.textContent='Enviando...';"
            "try{const blob=await fetchOszMultiMirror(s.id,s.title||('Set #'+s.id));"
            "await uploadBlob(blob,s.id+' '+safeTitle+'.osz');"
            "b.textContent='✓ Instalado!';b.dataset.done='1';}catch(err){b.disabled=false;b.textContent='Reintentar';st.style.color='#f04646';st.textContent=(err&&err.message)?err.message:'Error al transferir. Reintenta.';throw err;}"
            "};"
            "b.onclick=()=>b.triggerDownload();"
            "const pBtn=document.createElement('button');pBtn.className='btn-play';pBtn.textContent='▶ Escuchar';"
            "pBtn.onclick=()=>playPreview(s.id,(s.title||'Set #'+s.id),pBtn);"
            "const dl=document.createElement('a');dl.className='btn-dl';dl.href='https://catboy.best/d/'+s.id+'n';dl.target='_blank';dl.textContent='Bajar .osz';"
            "act.appendChild(b);act.appendChild(pBtn);act.appendChild(dl);d.appendChild(act);rDiv.appendChild(d);"
            "}"
            "function renderCurrentList(rDivId,batchBarId,batchTextId){"
            "const rDiv=document.getElementById(rDivId);if(!rDiv)return;rDiv.innerHTML='';"
            "const batchBar=document.getElementById(batchBarId);const batchText=document.getElementById(batchTextId);"
            "if(isDirectLinkMode||!currentResults.length){"
            "if(batchBar)batchBar.style.display='none';"
            "if(currentResults.length===1)renderCard(currentResults[0],rDiv,true,null);"
            "return;"
            "}"
            "const filtered=currentResults.filter(matchesFilter);"
            "if(!filtered.length){rDiv.innerHTML='<p style=\"color:#aaa5c7;padding:12px\">No hay mapas con esta dificultad.</p>';if(batchBar)batchBar.style.display='none';return;}"
            "if(batchBar)batchBar.style.display='flex';"
            "if(batchText)batchText.textContent=filtered.length+' canciones ('+currentFilter.toUpperCase()+')';"
            "filtered.forEach(s=>renderCard(s,rDiv,false,null));"
            "}"
            "async function downloadAllVisible(rDivId,batchBarId){"
            "const btns=Array.from(document.querySelectorAll('#'+rDivId+' .item .card-btn-dl'));if(!btns.length)return;"
            "const batchBtn=document.querySelector('#'+batchBarId+' .batch-btn');if(batchBtn)batchBtn.disabled=true;"
            "for(let i=0;i<btns.length;i++){"
            "const b=btns[i];if(b.dataset.done==='1')continue;"
            "if(batchBtn)batchBtn.textContent='Descargando ('+(i+1)+'/'+btns.length+')...';"
            "try{await b.triggerDownload();}catch(e){}"
            "}"
            "if(batchBtn){batchBtn.disabled=false;batchBtn.textContent='✓ Cola completada';}"
            "}"
            "async function lookupSetById(setId){"
            "try{const r=await fetch('https://catboy.best/api/v2/s/'+setId);if(r.ok){const j=await r.json();if(j&&j.id)return j;}}catch(e){}"
            "try{const r=await fetch('https://osu.direct/api/v2/s/'+setId);if(r.ok){const j=await r.json();if(j&&j.id)return j;}}catch(e){}"
            "return {id:setId,title:'Beatmap Set #'+setId,artist:'Enlace Directo',creator:'osu!'};"
            "}"
            "async function searchMaps(){"
            "const q=document.getElementById('q').value.trim();if(!q)return;"
            "const rDiv=document.getElementById('resultsSearch');rDiv.innerHTML='<p style=\"color:#50dcff;padding:12px\">Buscando...</p>';"
            "try{"
            "const parsed=parseOsuInput(q);"
            "if(parsed){"
            "rDiv.innerHTML='<p style=\"color:#50dcff;padding:12px\">Analizando enlace de osu!...</p>';let setObj=null;"
            "if(parsed.type==='map'){"
            "try{const rb=await fetch('https://catboy.best/api/v2/b/'+parsed.id);if(rb.ok){const bData=await rb.json();if(bData&&bData.beatmapset_id)setObj=await lookupSetById(bData.beatmapset_id);}}catch(e){}"
            "}else{setObj=await lookupSetById(parsed.id);}"
            "if(setObj){isDirectLinkMode=true;currentResults=[setObj];renderCurrentList('resultsSearch','batchBarSearch','batchTextSearch');return;}"
            "}"
            "const r=await fetch('https://catboy.best/api/v2/search?mode=0&limit=25&q='+encodeURIComponent(q));"
            "const list=await r.json();"
            "if(!Array.isArray(list)||!list.length){isDirectLinkMode=false;currentResults=[];renderCurrentList('resultsSearch','batchBarSearch','batchTextSearch');rDiv.innerHTML='<p style=\"color:#aaa5c7;padding:12px\">No se encontraron mapas.</p>';return;}"
            "isDirectLinkMode=false;currentResults=list;renderCurrentList('resultsSearch','batchBarSearch','batchTextSearch');"
            "}catch(e){rDiv.innerHTML='<p style=\"color:#f04646;padding:12px\">Error al consultar API online.</p>';}"
            "}"
            "async function loadCategory(cat,btn){"
            "if(btn){document.querySelectorAll('.chips-row .chip').forEach(c=>c.classList.remove('active'));btn.classList.add('active');}"
            "const rDiv=document.getElementById('resultsExplore');rDiv.innerHTML='<p style=\"color:#50dcff;padding:16px\">Cargando canciones...</p>';"
            "const batchBar=document.getElementById('batchBarExplore');if(batchBar)batchBar.style.display='none';"
            "let query='anime';"
            "if(cat==='popular')query='top';"
            "else if(cat==='anime')query='anime';"
            "else if(cat==='jrock')query='rock';"
            "else if(cat==='vocaloid')query='vocaloid';"
            "else if(cat==='game')query='game';"
            "else if(cat==='tvsize')query='tv size';"
            "try{"
            "const r=await fetch('https://catboy.best/api/v2/search?mode=0&limit=20&q='+encodeURIComponent(query));"
            "const list=await r.json();rDiv.innerHTML='';"
            "if(!Array.isArray(list)||!list.length){rDiv.innerHTML='<p style=\"color:#aaa5c7;padding:12px\">No se pudieron cargar canciones.</p>';return;}"
            "if(batchBar){batchBar.style.display='flex';document.getElementById('batchTextExplore').textContent=list.length+' canciones en pantalla';}"
            "list.forEach(s=>renderCard(s,rDiv,false,null));"
            "}catch(e){rDiv.innerHTML='<p style=\"color:#f04646;padding:12px\">Error de conexion con la API.</p>';}"
            "}"
            "const qInput=document.getElementById('q');"
            "qInput.addEventListener('keydown',(e)=>{if(e.key==='Enter')searchMaps();});"
            "qInput.addEventListener('input',()=>{if(parseOsuInput(qInput.value.trim()))searchMaps();});"
            "window.addEventListener('DOMContentLoaded',()=>{renderPacks();loadCategory('popular',null);});"
            "setTimeout(()=>{renderPacks();loadCategory('popular',null);},400);"
            "</script></body></html>";

        queueHttpResponse(200, "OK", "text/html; charset=utf-8", htmlBody);
    }

public:
    enum ServerInitState {
        SERVER_STOPPED,
        SERVER_CONNECTING,
        SERVER_RUNNING
    };

    ServerInitState initState = SERVER_STOPPED;
    u32 retryTimer = 0;

    std::string serverUrl = "";
    std::string statusText = "Inactivo";
    std::string currentFile = "";
    int uploadedCount = 0;
    bool needsRescan = false;

    WifiServer() = default;
    ~WifiServer() { stop(); }

    bool isRunning() const {
        return socReady && listenSock >= 0 && initState == SERVER_RUNNING;
    }

    bool isConnecting() const {
        return initState == SERVER_CONNECTING;
    }

    bool tryInitServer() {
        if (isRunning()) return true;

        u8 wifiSig = osGetWifiStrength();
        if (wifiSig == 0) {
            statusText = "Buscando senal Wi-Fi... (Sin conexion)";
            serverUrl = "";
            return false;
        }

        if (!socReady) {
            if (!NetCommon::initSoc()) {
                Result r = NetCommon::getLastSocResult();
                char errBuf[64];
                std::snprintf(errBuf, sizeof(errBuf), "Iniciando Wi-Fi... (0x%08lX)", (unsigned long)r);
                statusText = errBuf;
                return false;
            }
            socReady = true;
        }

        if (listenSock < 0) {
            listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSock < 0) {
                int err = errno;
                char errBuf[64];
                std::snprintf(errBuf, sizeof(errBuf), "Esperando socket TCP... (err: %d)", err);
                statusText = errBuf;
                if (err == ENODEV) {
                    if (socReady) {
                        NetCommon::exitSoc();
                        socReady = false;
                    }
                }
                return false;
            }

            int opt = 1;
            setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(SERVER_PORT);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(listenSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0 ||
                listen(listenSock, 4) < 0) {
                int err = errno;
                close(listenSock);
                listenSock = -1;
                char errBuf[64];
                std::snprintf(errBuf, sizeof(errBuf), "Error en puerto %d (err: %d)", SERVER_PORT, err);
                statusText = errBuf;
                return false;
            }

            setNonBlocking(listenSock);
        }

        if (ioBuffer.size() < IO_CHUNK_SIZE) {
            ioBuffer.resize(IO_CHUNK_SIZE);
        }

        // Comprobar si el router ya asignó una dirección IP válida
        struct in_addr hostAddr;
        hostAddr.s_addr = gethostid();
        u32 rawIp = hostAddr.s_addr;
        if (rawIp == 0 || rawIp == INADDR_NONE || (rawIp & 0xFF) == 127) {
            serverUrl = "";
            statusText = "Obteniendo direccion IP del router...";
            return false;
        }

        char ipStr[64];
        std::snprintf(ipStr, sizeof(ipStr), "http://%s:%d", inet_ntoa(hostAddr), SERVER_PORT);
        serverUrl = ipStr;
        statusText = "Esperando conexion desde PC o Celular...";
        initState = SERVER_RUNNING;
        return true;
    }

    bool start() {
        if (isRunning()) return true;

        initState = SERVER_CONNECTING;
        retryTimer = 0;
        statusText = "Conectando al servidor Wi-Fi...";
        return tryInitServer();
    }

    void restart() {
        stop();
        start();
    }

    void stop() {
        closeClient();
        if (listenSock >= 0) {
            close(listenSock);
            listenSock = -1;
        }
        if (socReady) {
            NetCommon::exitSoc();
            socReady = false;
        }
        socBuffer = nullptr;
        serverUrl = "";
        initState = SERVER_STOPPED;
        retryTimer = 0;
        statusText = "Servidor detenido";
    }

    float getProgressRatio() const {
        if (state != CLIENT_RECEIVING_BODY || contentLength == 0) return 0.0f;
        return std::min(1.0f, static_cast<float>(bytesReceived) / static_cast<float>(contentLength));
    }

    size_t getBytesReceived() const { return bytesReceived; }
    size_t getContentLength() const { return contentLength; }

    void update() {
        if (!isRunning()) {
            if (initState == SERVER_CONNECTING) {
                retryTimer++;
                // Reintentar de forma suave cada 20 fotogramas (~0.33s)
                if (retryTimer >= 20) {
                    retryTimer = 0;
                    tryInitServer();
                }
            }
            return;
        }

        // Si por alguna razón la URL no se había fijado con la IP correcta, reintentar leer la IP
        if (serverUrl.empty()) {
            struct in_addr hostAddr;
            hostAddr.s_addr = gethostid();
            u32 rawIp = hostAddr.s_addr;
            if (rawIp != 0 && rawIp != INADDR_NONE && (rawIp & 0xFF) != 127) {
                char ipStr[64];
                std::snprintf(ipStr, sizeof(ipStr), "http://%s:%d", inet_ntoa(hostAddr), SERVER_PORT);
                serverUrl = ipStr;
            }
        }

        if (ioBuffer.size() < IO_CHUNK_SIZE) ioBuffer.resize(IO_CHUNK_SIZE);

        // 1. Aceptar nuevo cliente si estamos libres
        if (clientSock < 0) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int s = accept(listenSock, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
            if (s >= 0) {
                clientSock = s;
                setNonBlocking(clientSock);
                state = CLIENT_READING_HEADERS;
                headerBuf.clear();
                bytesReceived = 0;
                contentLength = 0;
            }
            return;
        }

        // 2. Enviar respuesta HTTP pendiente de forma asíncrona sin cortar el buffer TCP de la 3DS
        if (state == CLIENT_SENDING_RESPONSE) {
            while (sendOffset < sendBuf.size()) {
                size_t toSend = std::min(static_cast<size_t>(1024), sendBuf.size() - sendOffset);
                ssize_t n = send(clientSock, sendBuf.data() + sendOffset, toSend, 0);
                if (n > 0) {
                    sendOffset += static_cast<size_t>(n);
                } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    return; // Seguir enviando en el siguiente fotograma
                } else {
                    closeClient();
                    return;
                }
            }
            // Todos los bytes encolados: cerrar escritura suavemente y esperar a que el navegador reciba todo
            shutdown(clientSock, SHUT_WR);
            waitCloseTick = osGetTime();
            state = CLIENT_WAIT_CLOSE;
            return;
        }

        // 3. Esperar confirmación de cierre del cliente (evita TCP Reset en soc:u)
        if (state == CLIENT_WAIT_CLOSE) {
            ssize_t n = recv(clientSock, ioBuffer.data(), 256, 0);
            if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) ||
                (osGetTime() - waitCloseTick > 600)) {
                closeClient();
            }
            return;
        }

        // 4. Leer cabeceras HTTP
        if (state == CLIENT_READING_HEADERS) {
            ssize_t n = recv(clientSock, ioBuffer.data(), 2048, 0);
            if (n > 0) {
                headerBuf.append(reinterpret_cast<const char*>(ioBuffer.data()), static_cast<size_t>(n));
                size_t headerEnd = headerBuf.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    std::string headers = headerBuf.substr(0, headerEnd);
                    std::string bodyRemainder = headerBuf.substr(headerEnd + 4);

                    if (headers.rfind("GET ", 0) == 0) {
                        statusText = "Web abierta! Elige un mapa en tu navegador";
                        prepareWebInterface();
                        return;
                    } else if (headers.rfind("POST /upload", 0) == 0) {
                        uploadFileName = "beatmap.osz";
                        size_t namePos = headers.find("name=");
                        if (namePos != std::string::npos) {
                            size_t endPos = headers.find_first_of(" &\r\n", namePos + 5);
                            std::string rawName = headers.substr(namePos + 5, endPos - (namePos + 5));
                            uploadFileName = sanitizeFilename(urlDecode(rawName));
                        }

                        contentLength = 0;
                        std::string lowerH = headers;
                        for (char& c : lowerH) {
                            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                        }
                        size_t clPos = lowerH.find("content-length:");
                        if (clPos != std::string::npos) {
                            contentLength = static_cast<size_t>(std::strtoul(lowerH.c_str() + clPos + 15, nullptr, 10));
                        }

                        std::string ext = "";
                        if (uploadFileName.size() >= 4) {
                            ext = uploadFileName.substr(uploadFileName.size() - 4);
                            for (char& c : ext) if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                        }
                        isSkinUpload = (ext == ".osk");

                        std::string targetFolder = isSkinUpload
                            ? "sdmc:/3ds/osu-3ds/skins/"
                            : "sdmc:/3ds/osu-3ds/songs/";
                        OszExtractor::ensureDirectoryRecursive(targetFolder);
                        uploadTempPath = targetFolder + uploadFileName;

                        outFile = std::fopen(uploadTempPath.c_str(), "wb");
                        if (!outFile || contentLength == 0) {
                            queueHttpResponse(500, "Internal Server Error", "text/plain", "Error SD");
                            return;
                        }

                        currentFile = uploadFileName;
                        statusText = "Recibiendo: " + uploadFileName;
                        state = CLIENT_RECEIVING_BODY;
                        bytesReceived = 0;

                        if (!bodyRemainder.empty()) {
                            std::fwrite(bodyRemainder.data(), 1, bodyRemainder.size(), outFile);
                            bytesReceived += bodyRemainder.size();
                        }
                    } else {
                        queueHttpResponse(200, "OK", "text/plain", "OK");
                        return;
                    }
                } else if (headerBuf.size() > 8192) {
                    closeClient();
                    return;
                }
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                closeClient();
                return;
            }
        }

        // 5. Recibir cuerpo binario del archivo .osz / .osk usando ioBuffer en heap
        if (state == CLIENT_RECEIVING_BODY) {
            for (int iter = 0; iter < 8 && bytesReceived < contentLength; ++iter) {
                ssize_t n = recv(clientSock, ioBuffer.data(), IO_CHUNK_SIZE, 0);
                if (n > 0) {
                    if (outFile) {
                        std::fwrite(ioBuffer.data(), 1, static_cast<size_t>(n), outFile);
                    }
                    bytesReceived += static_cast<size_t>(n);
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    statusText = "Transferencia interrumpida";
                    closeClient();
                    return;
                } else {
                    break;
                }
            }

            if (bytesReceived >= contentLength) {
                if (outFile) {
                    std::fclose(outFile);
                    outFile = nullptr;
                }

                statusText = "Extrayendo paquete en la SD...";
                if (isSkinUpload) {
                    OszExtractor::extractAllPendingOsk("sdmc:/3ds/osu-3ds/skins", "sdmc:/3ds/osu-3ds/skins");
                } else {
                    OszExtractor::extractAllPendingOsz("sdmc:/3ds/osu-3ds/songs", "sdmc:/3ds/osu-3ds/songs");
                }

                uploadedCount++;
                needsRescan = true;
                statusText = "Instalado: " + currentFile;
                queueHttpResponse(200, "OK", "text/plain", "OK");
            }
        }
    }
};
