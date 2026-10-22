#pragma once
#include <3ds.h>
#include <malloc.h>
#include <cstdlib>
#include <cstdint>

// Gestor común de sockets para compartir socInit/socExit entre WifiServer y MultiplayerManager
namespace NetCommon {
    inline u32*& getSocBuffer() {
        static u32* s_buffer = nullptr;
        return s_buffer;
    }

    inline int& getSocRefCount() {
        static int s_refCount = 0;
        return s_refCount;
    }

    inline bool isSocReady() {
        return (getSocRefCount() > 0 && getSocBuffer() != nullptr);
    }

    inline Result& getLastSocResult() {
        static Result s_lastResult = 0;
        return s_lastResult;
    }

    inline bool initSoc() {
        if (isSocReady()) {
            getSocRefCount()++;
            return true;
        }

        constexpr size_t SOC_ALIGN = 0x1000;
        constexpr size_t SOC_BUFFERSIZE = 0x100000; // 1 MB buffer para 3DS

        // Intentar reservar 1 MB, o reducir a 512 KB / 256 KB si la memoria del montón está fragmentada
        size_t allocSize = SOC_BUFFERSIZE;
        u32* buf = static_cast<u32*>(memalign(SOC_ALIGN, allocSize));
        if (!buf) {
            allocSize = 0x80000; // 512 KB
            buf = static_cast<u32*>(memalign(SOC_ALIGN, allocSize));
        }
        if (!buf) {
            allocSize = 0x40000; // 256 KB
            buf = static_cast<u32*>(memalign(SOC_ALIGN, allocSize));
        }
        if (!buf) {
            getLastSocResult() = -1;
            return false;
        }

        Result res = socInit(buf, allocSize);
        getLastSocResult() = res;
        if (res != 0) {
            free(buf);
            return false;
        }

        getSocBuffer() = buf;
        getSocRefCount() = 1;
        return true;
    }

    inline void exitSoc() {
        if (getSocRefCount() <= 0) return;

        getSocRefCount()--;
        if (getSocRefCount() == 0) {
            socExit();
            if (getSocBuffer()) {
                free(getSocBuffer());
                getSocBuffer() = nullptr;
            }
        }
    }

    inline void forceExitSoc() {
        if (getSocRefCount() > 0 || getSocBuffer() != nullptr) {
            socExit();
            if (getSocBuffer()) {
                free(getSocBuffer());
                getSocBuffer() = nullptr;
            }
            getSocRefCount() = 0;
        }
    }
}
