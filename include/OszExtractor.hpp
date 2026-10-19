#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

// Descompresor ligero y autónomo de archivos .osz (formato ZIP: STORED y DEFLATE RFC 1951)
// Diseñado específicamente para Nintendo 3DS sin dependencias externas.
namespace OszExtractor {

struct BitStream {
    const uint8_t* data;
    size_t size;
    size_t bytePos;
    uint32_t bitBuf;
    int bitCount;

    BitStream(const uint8_t* d, size_t s)
        : data(d), size(s), bytePos(0), bitBuf(0), bitCount(0) {}

    inline void fillBits(int n) {
        while (bitCount < n) {
            uint8_t nextByte = (bytePos < size) ? data[bytePos++] : 0;
            bitBuf |= (static_cast<uint32_t>(nextByte) << bitCount);
            bitCount += 8;
        }
    }

    inline uint32_t peekBits(int n) {
        fillBits(n);
        return bitBuf & ((1u << n) - 1u);
    }

    inline void dropBits(int n) {
        bitBuf >>= n;
        bitCount -= n;
    }

    inline uint32_t readBits(int n) {
        if (n == 0) return 0;
        uint32_t val = peekBits(n);
        dropBits(n);
        return val;
    }

    inline void alignToByte() {
        int discard = bitCount & 7;
        dropBits(discard);
    }
};

struct HuffmanTable {
    uint16_t counts[16];
    uint16_t symbols[288];

    bool build(const uint8_t* lengths, int numSymbols) {
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < numSymbols; ++i) {
            if (lengths[i] > 15) return false;
            counts[lengths[i]]++;
        }
        counts[0] = 0;

        uint16_t offsets[16];
        uint16_t sum = 0;
        for (int len = 1; len <= 15; ++len) {
            offsets[len] = sum;
            sum += counts[len];
        }

        for (int i = 0; i < numSymbols; ++i) {
            uint8_t len = lengths[i];
            if (len != 0) {
                symbols[offsets[len]++] = static_cast<uint16_t>(i);
            }
        }
        return true;
    }

    int decode(BitStream& bs) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= 15; ++len) {
            code |= static_cast<int>(bs.readBits(1));
            int count = counts[len];
            if (code - count < first) {
                return symbols[index + (code - first)];
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

// Descomprime un bloque DEFLATE en un archivo de salida usando una ventana deslizante de 32 KB
inline bool inflateToFile(const uint8_t* compData, size_t compSize, FILE* outFile) {
    static const uint16_t lenBase[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const uint8_t lenExtra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const uint16_t distBase[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577
    };
    static const uint8_t distExtra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };
    static const uint8_t clOrder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    std::vector<uint8_t> window(32768, 0);
    size_t winPos = 0;

    BitStream bs(compData, compSize);
    HuffmanTable litLenTable, distTable, codeLenTable;

    bool finalBlock = false;
    while (!finalBlock) {
        finalBlock = (bs.readBits(1) != 0);
        uint32_t btype = bs.readBits(2);

        if (btype == 0) {
            // Bloque sin comprimir
            bs.alignToByte();
            uint16_t len = static_cast<uint16_t>(bs.readBits(16));
            uint16_t nlen = static_cast<uint16_t>(bs.readBits(16));
            if ((len ^ ~nlen) & 0xFFFF) return false;

            for (uint16_t i = 0; i < len; ++i) {
                uint8_t b = static_cast<uint8_t>(bs.readBits(8));
                window[winPos++] = b;
                if (winPos == 32768) {
                    if (std::fwrite(window.data(), 1, 32768, outFile) != 32768) return false;
                    winPos = 0;
                }
            }
        } else if (btype == 1 || btype == 2) {
            if (btype == 1) {
                uint8_t fixedLit[288];
                uint8_t fixedDist[32];
                for (int i = 0; i <= 143; ++i) fixedLit[i] = 8;
                for (int i = 144; i <= 255; ++i) fixedLit[i] = 9;
                for (int i = 256; i <= 279; ++i) fixedLit[i] = 7;
                for (int i = 280; i <= 287; ++i) fixedLit[i] = 8;
                for (int i = 0; i < 32; ++i) fixedDist[i] = 5;
                litLenTable.build(fixedLit, 288);
                distTable.build(fixedDist, 32);
            } else {
                int hlit = static_cast<int>(bs.readBits(5)) + 257;
                int hdist = static_cast<int>(bs.readBits(5)) + 1;
                int hclen = static_cast<int>(bs.readBits(4)) + 4;

                uint8_t clLengths[19] = {0};
                for (int i = 0; i < hclen; ++i) {
                    clLengths[clOrder[i]] = static_cast<uint8_t>(bs.readBits(3));
                }
                if (!codeLenTable.build(clLengths, 19)) return false;

                uint8_t lengths[320] = {0};
                int idx = 0;
                while (idx < hlit + hdist) {
                    int sym = codeLenTable.decode(bs);
                    if (sym < 0) return false;
                    if (sym < 16) {
                        lengths[idx++] = static_cast<uint8_t>(sym);
                    } else if (sym == 16) {
                        if (idx == 0) return false;
                        int rep = static_cast<int>(bs.readBits(2)) + 3;
                        uint8_t prev = lengths[idx - 1];
                        while (rep-- > 0 && idx < hlit + hdist) lengths[idx++] = prev;
                    } else if (sym == 17) {
                        int rep = static_cast<int>(bs.readBits(3)) + 3;
                        while (rep-- > 0 && idx < hlit + hdist) lengths[idx++] = 0;
                    } else if (sym == 18) {
                        int rep = static_cast<int>(bs.readBits(7)) + 11;
                        while (rep-- > 0 && idx < hlit + hdist) lengths[idx++] = 0;
                    }
                }
                if (!litLenTable.build(lengths, hlit)) return false;
                if (!distTable.build(lengths + hlit, hdist)) return false;
            }

            while (true) {
                int sym = litLenTable.decode(bs);
                if (sym < 0) return false;
                if (sym < 256) {
                    window[winPos++] = static_cast<uint8_t>(sym);
                    if (winPos == 32768) {
                        if (std::fwrite(window.data(), 1, 32768, outFile) != 32768) return false;
                        winPos = 0;
                    }
                } else if (sym == 256) {
                    break;
                } else {
                    int lenIdx = sym - 257;
                    if (lenIdx < 0 || lenIdx >= 29) return false;
                    int length = lenBase[lenIdx] + static_cast<int>(bs.readBits(lenExtra[lenIdx]));

                    int distSym = distTable.decode(bs);
                    if (distSym < 0 || distSym >= 30) return false;
                    int distance = distBase[distSym] + static_cast<int>(bs.readBits(distExtra[distSym]));

                    for (int i = 0; i < length; ++i) {
                        uint8_t b = window[(winPos - distance) & 32767];
                        window[winPos++] = b;
                        if (winPos == 32768) {
                            if (std::fwrite(window.data(), 1, 32768, outFile) != 32768) return false;
                            winPos = 0;
                        }
                    }
                }
            }
        } else {
            return false;
        }
    }

    if (winPos > 0) {
        if (std::fwrite(window.data(), 1, winPos, outFile) != winPos) return false;
    }
    return true;
}

inline uint16_t readLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline void ensureDirectoryRecursive(const std::string& path) {
    std::string accum;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        accum.push_back(c);
        if ((c == '/' || c == '\\') && accum.size() > 1 && accum.back() == '/') {
            if (accum.find(":/") != accum.size() - 2) {
                mkdir(accum.substr(0, accum.size() - 1).c_str(), 0777);
            }
        }
    }
    mkdir(path.c_str(), 0777);
}

inline bool shouldExtractFile(const std::string& filename) {
    // Extraer archivos de mapas (.osu), configuración de skins (.ini), audio (.mp3, .wav, .ogg),
    // video optimizado (.3dsv, .mjpg) e imágenes de fondo/skins (.jpg, .jpeg, .png), omitiendo duplicados @2x
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string lowName = filename;
    for (char& c : lowName) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    if (lowName.find("@2x.") != std::string::npos) return false;

    std::string ext = lowName.substr(dot);
    return (ext == ".osu" || ext == ".ini" || ext == ".mp3" || ext == ".wav" ||
            ext == ".ogg" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".3dsv" || ext == ".mjpg");
}

// Extrae un archivo .osz o .osk en destFolder usando el Central Directory del ZIP
inline bool extractOszFile(const std::string& oszPath, const std::string& destFolder) {
    FILE* f = std::fopen(oszPath.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    if (fileSize < 22) {
        std::fclose(f);
        return false;
    }

    // Buscar el End of Central Directory (EOCD) al final del archivo
    long searchSize = (fileSize < 65557) ? fileSize : 65557;
    std::vector<uint8_t> tail(searchSize);
    std::fseek(f, fileSize - searchSize, SEEK_SET);
    if (std::fread(tail.data(), 1, searchSize, f) != static_cast<size_t>(searchSize)) {
        std::fclose(f);
        return false;
    }

    long eocdOffset = -1;
    for (long i = searchSize - 22; i >= 0; --i) {
        if (tail[i] == 0x50 && tail[i + 1] == 0x4B && tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
            eocdOffset = i;
            break;
        }
    }
    if (eocdOffset < 0) {
        std::fclose(f);
        return false;
    }

    uint16_t numEntries = readLE16(&tail[eocdOffset + 10]);
    uint32_t cdSize     = readLE32(&tail[eocdOffset + 12]);
    uint32_t cdOffset   = readLE32(&tail[eocdOffset + 16]);

    if (cdOffset + cdSize > static_cast<uint32_t>(fileSize)) {
        std::fclose(f);
        return false;
    }

    std::vector<uint8_t> cdBuf(cdSize);
    std::fseek(f, cdOffset, SEEK_SET);
    if (std::fread(cdBuf.data(), 1, cdSize, f) != cdSize) {
        std::fclose(f);
        return false;
    }

    ensureDirectoryRecursive(destFolder);

    size_t pos = 0;
    int extractedCount = 0;
    for (uint16_t entry = 0; entry < numEntries && pos + 46 <= cdSize; ++entry) {
        if (readLE32(&cdBuf[pos]) != 0x02014B50) break;

        uint16_t method     = readLE16(&cdBuf[pos + 10]);
        uint32_t compSize   = readLE32(&cdBuf[pos + 20]);
        uint16_t nameLen    = readLE16(&cdBuf[pos + 28]);
        uint16_t extraLen   = readLE16(&cdBuf[pos + 30]);
        uint16_t commentLen = readLE16(&cdBuf[pos + 32]);
        uint32_t lfhOffset  = readLE32(&cdBuf[pos + 42]);

        if (pos + 46 + nameLen > cdSize) break;
        std::string entryName(reinterpret_cast<const char*>(&cdBuf[pos + 46]), nameLen);
        pos += 46 + nameLen + extraLen + commentLen;

        // Aplanar subcarpetas o limpiar caracteres inválidos en FAT32
        size_t slash = entryName.find_last_of("/\\");
        std::string baseName = (slash != std::string::npos) ? entryName.substr(slash + 1) : entryName;
        if (baseName.empty() || !shouldExtractFile(baseName)) continue;

        for (char& c : baseName) {
            if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }

        // Leer cabecera local (LFH)
        uint8_t lfh[30];
        std::fseek(f, lfhOffset, SEEK_SET);
        if (std::fread(lfh, 1, 30, f) != 30 || readLE32(lfh) != 0x04034B50) continue;

        uint16_t lfhNameLen  = readLE16(&lfh[26]);
        uint16_t lfhExtraLen = readLE16(&lfh[28]);
        uint32_t dataOffset  = lfhOffset + 30 + lfhNameLen + lfhExtraLen;

        // Para DEFLATE (que carga en RAM), limitar a 20 MB; para STORED (streaming de 32 KB), permitir hasta 64 MB
        uint32_t maxAllowedSize = (method == 0) ? (64 * 1024 * 1024) : (20 * 1024 * 1024);
        if (compSize > maxAllowedSize) continue;

        std::string outPath = destFolder;
        if (!outPath.empty() && outPath.back() != '/') outPath += '/';
        outPath += baseName;

        FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) continue;

        std::fseek(f, dataOffset, SEEK_SET);
        if (method == 0) {
            // STORED (sin compresión)
            std::vector<uint8_t> copyBuf(32768);
            uint32_t remaining = compSize;
            bool ok = true;
            while (remaining > 0) {
                uint32_t chunk = (remaining > 32768) ? 32768 : remaining;
                if (std::fread(copyBuf.data(), 1, chunk, f) != chunk ||
                    std::fwrite(copyBuf.data(), 1, chunk, out) != chunk) {
                    ok = false;
                    break;
                }
                remaining -= chunk;
            }
            if (ok) extractedCount++;
        } else if (method == 8) {
            // DEFLATE
            std::vector<uint8_t> compBuf(compSize);
            if (std::fread(compBuf.data(), 1, compSize, f) == compSize) {
                if (inflateToFile(compBuf.data(), compSize, out)) {
                    extractedCount++;
                }
            }
        }
        std::fclose(out);
    }

    std::fclose(f);
    return extractedCount > 0;
}

// Escanea una carpeta en busca de archivos con la extensión indicada (.osz o .osk), los descomprime y elimina el paquete
inline int extractAllPendingArchives(const std::string& searchFolder, const std::string& destRoot, const std::string& targetExt = ".osz") {
    DIR* dir = opendir(searchFolder.c_str());
    if (!dir) return 0;

    int extractedSets = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 4) {
            std::string ext = name.substr(name.size() - 4);
            for (char& c : ext) {
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            }
            if (ext == targetExt) {
                std::string folderName = name.substr(0, name.size() - 4);
                for (char& c : folderName) {
                    if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                        c = '_';
                    }
                }
                std::string fullArchPath = searchFolder;
                if (!fullArchPath.empty() && fullArchPath.back() != '/') fullArchPath += '/';
                fullArchPath += name;

                std::string destDir = destRoot;
                if (!destDir.empty() && destDir.back() != '/') destDir += '/';
                destDir += folderName;

                if (extractOszFile(fullArchPath, destDir)) {
                    std::remove(fullArchPath.c_str());
                    extractedSets++;
                }
            }
        }
    }
    closedir(dir);
    return extractedSets;
}

inline int extractAllPendingOsz(const std::string& searchFolder, const std::string& songsRoot) {
    return extractAllPendingArchives(searchFolder, songsRoot, ".osz");
}

inline int extractAllPendingOsk(const std::string& searchFolder, const std::string& skinsRoot) {
    return extractAllPendingArchives(searchFolder, skinsRoot, ".osk");
}

inline bool removeDirectoryRecursive(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return std::remove(path.c_str()) == 0;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        std::string sub = path;
        if (!sub.empty() && sub.back() != '/') sub += '/';
        sub += entry->d_name;
        struct stat st;
        if (stat(sub.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                removeDirectoryRecursive(sub);
            } else {
                std::remove(sub.c_str());
            }
        }
    }
    closedir(dir);
    return rmdir(path.c_str()) == 0;
}

} // namespace OszExtractor
