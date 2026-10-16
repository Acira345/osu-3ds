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
            "<title>osu! 3DS - Gestor Wi-Fi v2</title>"
            "<style>"
            "body{font-family:system-ui,-apple-system,sans-serif;background:#12101a;color:#fff;margin:0;padding:16px;text-align:center}"
            ".card{max-width:580px;margin:0 auto 16px;background:#1e1b2c;border-radius:14px;padding:18px;border:1px solid #3a3456;box-shadow:0 6px 20px rgba(0,0,0,0.4)}"
            "h1{color:#ff73b4;margin:0 0 6px;font-size:1.45rem}p{color:#aaa5c3;margin:6px 0;font-size:0.9rem}"
            ".drop{display:block;border:2px dashed #ff73b4;border-radius:12px;padding:20px 12px;margin:14px 0;cursor:pointer;background:#171522;transition:0.2s}"
            ".drop:hover{background:#252038;border-color:#50dcff}"
            "input[type=file]{display:none}"
            ".btn{background:#ff73b4;color:#fff;border:none;padding:9px 14px;border-radius:8px;font-weight:700;cursor:pointer;font-size:0.88rem;white-space:nowrap}"
            ".btn:disabled{opacity:0.6;cursor:wait}"
            ".btn2{background:#46b4ff;color:#fff;border:none;padding:10px 16px;border-radius:8px;font-weight:700;cursor:pointer}"
            ".btn-dl{background:#2e2942;color:#aaa5c3;text-decoration:none;padding:7px 10px;border-radius:7px;font-size:0.76rem;font-weight:600}"
            ".bar-bg{background:#12101a;height:14px;border-radius:7px;overflow:hidden;margin:12px 0;display:none}"
            ".bar-fill{background:linear-gradient(90deg,#ff73b4,#50dcff);height:100%;width:0%;transition:width 0.1s}"
            ".search-row{display:flex;gap:8px;margin-top:10px}"
            ".search-row input{flex:1;padding:10px;border-radius:8px;border:1px solid #4a4368;background:#12101a;color:#fff;font-size:0.92rem}"
            ".results{text-align:left;margin-top:12px;max-height:420px;overflow-y:auto}"
            ".item{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #2e2942;gap:10px}"
            ".item.highlight{background:#26203a;border:1px solid #ff73b4;border-radius:10px;margin-bottom:8px}"
            ".thumb{width:64px;height:48px;border-radius:7px;object-fit:cover;background:#12101a;flex-shrink:0}"
            ".item-info{flex:1;min-width:0}"
            ".item-title{font-weight:700;font-size:0.92rem;color:#ffe066;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
            ".item-sub{font-size:0.78rem;color:#aaa5c3;margin-top:2px}"
            ".diffs{display:flex;flex-wrap:wrap;gap:4px;margin-top:5px}"
            ".diff-pill{font-size:0.7rem;background:#12101a;color:#50dcff;padding:2px 6px;border-radius:5px;border:1px solid #35304d}"
            ".diff-pill.sel{background:#ff73b4;color:#fff;border-color:#ff73b4;font-weight:700}"
            ".badge-link{display:inline-block;background:#ff73b4;color:#fff;font-size:0.7rem;font-weight:700;padding:2px 7px;border-radius:5px;margin-bottom:4px}"
            ".actions{display:flex;flex-direction:column;gap:5px;align-items:flex-end}"
            "#status{font-weight:700;color:#64f078;margin-top:8px;min-height:22px}"
            "</style></head><body>"
            "<div class='card'>"
            "<h1>osu! 3DS Wi-Fi Manager <span style='font-size:0.75rem;color:#50dcff'>v2.0</span></h1>"
            "<p>Envia archivos <b>.osz</b> (Canciones) o <b>.osk</b> (Skins) directo a tu Nintendo 3DS</p>"
            "<label class='drop' id='dropzone'>"
            "<input type='file' id='fileInput' multiple accept='.osz,.osk,.zip'>"
            "<div style='font-size:1.05rem;font-weight:700;color:#50dcff'>Haz clic aqui o arrastra archivos .osz / .osk</div>"
            "<p>Se descomprimiran e instalaran automaticamente en la consola</p>"
            "</label>"
            "<div class='bar-bg' id='barBg'><div class='bar-fill' id='barFill'></div></div>"
            "<div id='status'>Listo para recibir canciones o skins</div>"
            "</div>"
            "<div class='card'>"
            "<h1 style='font-size:1.25rem;color:#50dcff'>Buscador Online / Pegar Link de osu!</h1>"
            "<p>Pega cualquier <b>link de osu!</b> (ej: <code>osu.ppy.sh/beatmapsets/...</code>) o escribe el nombre de la cancion</p>"
            "<div class='search-row'>"
            "<input type='text' id='q' placeholder='Pega aqui un link de osu! o escribe ej: Yoasobi...'>"
            "<button class='btn2' onclick='searchMaps()'>Buscar</button>"
            "</div>"
            "<div class='results' id='results'></div>"
            "</div>"
            "<script>"
            "const st=document.getElementById('status'),barBg=document.getElementById('barBg'),barFill=document.getElementById('barFill');"
            "let uploadChain=Promise.resolve();"
            "function uploadBlob(blob,name){"
            "const run=()=>new Promise((res,rej)=>{"
            "barBg.style.display='block';barFill.style.width='0%';"
            "st.style.color='#50dcff';st.textContent='Enviando '+name+' a la 3DS...';"
            "const xhr=new XMLHttpRequest();"
            "xhr.open('POST','/upload?name='+encodeURIComponent(name),true);"
            "xhr.upload.onprogress=(e)=>{if(e.lengthComputable){const p=Math.round((e.loaded/e.total)*100);barFill.style.width=p+'%';st.textContent='Enviando a la 3DS: '+name+' ('+p+'%)...';}};"
            "xhr.onload=()=>{if(xhr.status===200){barFill.style.width='100%';st.style.color='#64f078';st.textContent='Instalado con exito en tu 3DS: '+name;res();}else{st.style.color='#f04646';st.textContent='Error al instalar '+name;rej();}};"
            "xhr.onerror=()=>{st.style.color='#f04646';st.textContent='Error de conexion con la 3DS';rej();};"
            "xhr.send(blob);"
            "});"
            "uploadChain=uploadChain.then(run,run);"
            "return uploadChain;}"
            "document.getElementById('fileInput').addEventListener('change',async(e)=>{"
            "for(const f of e.target.files){await uploadBlob(f,f.name);}"
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
            "const urls=["
            "'https://catboy.best/d/'+setId+'n',"
            "'https://catboy.best/d/'+setId,"
            "'https://api.nerinyan.moe/d/'+setId+'?noVideo=1',"
            "'https://osu.direct/api/d/'+setId+'?noVideo=1',"
            "'https://txy1.sayobot.cn/beatmaps/download/novideo/'+setId"
            "];"
            "for(let i=0;i<urls.length;i++){"
            "try{st.style.color='#ffe066';st.textContent='Descargando '+title+' (Espejo '+(i+1)+'/'+urls.length+')...';"
            "const r=await fetch(urls[i]);"
            "if(r.ok){const b=await r.blob();if(b.size>1024)return b;}"
            "}catch(e){}}"
            "throw new Error('Todos los espejos fallaron');}"
            "function renderCard(s,rDiv,isLink,targetBid){"
            "const d=document.createElement('div');d.className='item'+(isLink?' highlight':'');"
            "const safeTitle=((s.artist||'osu')+' - '+(s.title||'map')).replace(/[^a-zA-Z0-9 _.-]/g,'_');"
            "const img=document.createElement('img');img.className='thumb';"
            "img.src=(s.covers&&s.covers.list)?s.covers.list:'https://assets.ppy.sh/beatmaps/'+s.id+'/covers/list.jpg';"
            "img.onerror=()=>{img.style.display='none';};"
            "d.appendChild(img);"
            "const info=document.createElement('div');info.className='item-info';"
            "let html=isLink?'<div class=\"badge-link\">Cancion detectada por Link (Set #'+s.id+')</div>':'';"
            "html+='<div class=\"item-title\">'+(s.title||'Beatmap #'+s.id)+'</div>';"
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
            "const b=document.createElement('button');b.className='btn';b.textContent='⬇ Instalar en 3DS';"
            "b.onclick=async()=>{b.disabled=true;b.textContent='Descargando...';"
            "try{const blob=await fetchOszMultiMirror(s.id,s.title||('Set #'+s.id));"
            "b.textContent='Enviando a 3DS...';"
            "await uploadBlob(blob,s.id+' '+safeTitle+'.osz');"
            "b.textContent='✓ Instalado!';}catch(err){b.disabled=false;b.textContent='Reintentar';st.style.color='#f04646';st.textContent='Bloqueo de navegador: pulsa Bajar .osz y arrastralo al cuadro de arriba';}};"
            "const dl=document.createElement('a');dl.className='btn-dl';dl.href='https://catboy.best/d/'+s.id+'n';dl.target='_blank';dl.textContent='Bajar .osz a PC/Cel';"
            "act.appendChild(b);act.appendChild(dl);d.appendChild(act);rDiv.appendChild(d);}"
            "async function lookupSetById(setId){"
            "try{const r=await fetch('https://catboy.best/api/v2/s/'+setId);if(r.ok){const j=await r.json();if(j&&j.id)return j;}}catch(e){}"
            "try{const r=await fetch('https://osu.direct/api/v2/s/'+setId);if(r.ok){const j=await r.json();if(j&&j.id)return j;}}catch(e){}"
            "return {id:setId,title:'Beatmap Set #'+setId,artist:'Enlace Directo',creator:'osu!'};}"
            "async function searchMaps(){"
            "const q=document.getElementById('q').value.trim();if(!q)return;"
            "const rDiv=document.getElementById('results');rDiv.innerHTML='<p>Buscando...</p>';"
            "try{const parsed=parseOsuInput(q);"
            "if(parsed){"
            "rDiv.innerHTML='<p>Analizando enlace de osu!...</p>';"
            "let setObj=null;"
            "if(parsed.type==='map'){"
            "try{const rb=await fetch('https://catboy.best/api/v2/b/'+parsed.id);"
            "if(rb.ok){const bData=await rb.json();if(bData&&bData.beatmapset_id)setObj=await lookupSetById(bData.beatmapset_id);}}catch(e){}"
            "}else{setObj=await lookupSetById(parsed.id);}"
            "if(setObj){rDiv.innerHTML='';renderCard(setObj,rDiv,true,parsed.bid);return;}}"
            "const r=await fetch('https://catboy.best/api/v2/search?mode=0&limit=15&q='+encodeURIComponent(q));"
            "const list=await r.json();rDiv.innerHTML='';"
            "if(!Array.isArray(list)||!list.length){rDiv.innerHTML='<p>No se encontraron mapas.</p>';return;}"
            "list.forEach(s=>renderCard(s,rDiv,false,null));"
            "}catch(e){rDiv.innerHTML='<p>Error al consultar API online.</p>';}}"
            "const qInput=document.getElementById('q');"
            "qInput.addEventListener('keydown',(e)=>{if(e.key==='Enter')searchMaps();});"
            "qInput.addEventListener('input',()=>{if(parseOsuInput(qInput.value.trim()))searchMaps();});"
            "</script></body></html>";

        queueHttpResponse(200, "OK", "text/html; charset=utf-8", htmlBody);
    }

public:
    std::string serverUrl = "";
    std::string statusText = "Inactivo";
    std::string currentFile = "";
    int uploadedCount = 0;
    bool needsRescan = false;

    WifiServer() = default;
    ~WifiServer() { stop(); }

    bool start() {
        if (socReady) return true;

        socBuffer = static_cast<u32*>(memalign(SOC_ALIGN, SOC_BUFFERSIZE));
        if (!socBuffer) {
            statusText = "Error: Sin memoria para sockets";
            return false;
        }

        if (socInit(socBuffer, SOC_BUFFERSIZE) != 0) {
            free(socBuffer);
            socBuffer = nullptr;
            statusText = "Error al iniciar Wi-Fi (socInit)";
            return false;
        }
        socReady = true;

        listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listenSock < 0) {
            stop();
            statusText = "Error al crear socket TCP";
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
            stop();
            statusText = "Error en bind/listen puerto 8080";
            return false;
        }

        setNonBlocking(listenSock);

        if (ioBuffer.size() < IO_CHUNK_SIZE) {
            ioBuffer.resize(IO_CHUNK_SIZE);
        }

        struct in_addr hostAddr;
        hostAddr.s_addr = gethostid();
        char ipStr[64];
        std::snprintf(ipStr, sizeof(ipStr), "http://%s:%d", inet_ntoa(hostAddr), SERVER_PORT);
        serverUrl = ipStr;
        statusText = "Esperando conexion desde PC o Celular...";
        return true;
    }

    void stop() {
        closeClient();
        if (listenSock >= 0) {
            close(listenSock);
            listenSock = -1;
        }
        if (socReady) {
            socExit();
            socReady = false;
        }
        if (socBuffer) {
            free(socBuffer);
            socBuffer = nullptr;
        }
        serverUrl = "";
        statusText = "Servidor detenido";
    }

    bool isRunning() const { return socReady && listenSock >= 0; }

    float getProgressRatio() const {
        if (state != CLIENT_RECEIVING_BODY || contentLength == 0) return 0.0f;
        return std::min(1.0f, static_cast<float>(bytesReceived) / static_cast<float>(contentLength));
    }

    size_t getBytesReceived() const { return bytesReceived; }
    size_t getContentLength() const { return contentLength; }

    void update() {
        if (!isRunning()) return;
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
