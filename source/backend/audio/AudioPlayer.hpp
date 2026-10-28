#pragma once
#include <3ds.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#include "minimp3_ex.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"

constexpr size_t AUDIO_BUFFER_COUNT = 3;
constexpr size_t AUDIO_BUFFER_FRAMES = 4096;

struct WavSample {
    u8* data = nullptr;
    u32 byteSize = 0;
    u32 nsamples = 0;
    u32 sampleRate = 44100;
    u16 channels = 1;
    u16 bitsPerSample = 16;
    ndspWaveBuf waveBuf;

    void freeData() {
        if (data) {
            linearFree(data);
            data = nullptr;
        }
    }
};

class AudioPlayer {
private:
    mp3dec_ex_t mp3;
    stb_vorbis* vorbisStream = nullptr;
    bool usingVorbis = false;

    bool dspReady = false;
    bool isLoaded = false;
    bool isPlaying = false;
    bool isPaused = false;
    bool inLeadIn = false;
    bool eofReached = false;
    bool loopEnabled = false;
    int loopOffsetMs = 0;

    int musicChannel = 0;
    u32 sampleRate = 44100;
    u16 musicChannels = 2;
    float speedRate = 1.0f;

    float musicGain = 0.85f;
    float sfxGain = 1.0f;

    s16* audioBuffer[AUDIO_BUFFER_COUNT] = { nullptr, nullptr, nullptr };
    ndspWaveBuf waveBuf[AUDIO_BUFFER_COUNT];
    bool preloadedBufReady[AUDIO_BUFFER_COUNT] = { false, false, false };

    u64 totalSamplesPlayed = 0;
    u64 seekOffsetSamples = 0;
    u64 leadInStartTick = 0;
    u64 pauseStartTick = 0;
    u64 musicStartTick = 0;
    u64 musicPlayheadOffsetMs = 0;
    int leadInDurationMs = 0;
    mutable int lastReportedTimeMs = -999999;

    std::string loadedMusicPath = "";

    // 0: Normal, 1: Whistle, 2: Finish, 3: Clap, 4: ComboBreak (Canales DSP 1..5)
    WavSample hitSounds[5];

    // Canales DSP 6..9: Efectos rítmicos Nightcore (Kick, Clap, Hat, Crash)
    WavSample ncKick;
    WavSample ncClap;
    WavSample ncHat;
    WavSample ncCrash;

    // =========================================================================
    // Cargador asíncrono de vistas previas de canciones (Preview Audio)
    // Permite que la decodificación y lectura de audio desde la SD ocurra en
    // un hilo secundario sin congelar la interfaz ni los 60 FPS del selector.
    // =========================================================================
    struct PreparedPreview {
        mp3dec_ex_t mp3;
        stb_vorbis* vorbisStream = nullptr;
        bool usingVorbis = false;
        u32 sampleRate = 44100;
        u16 musicChannels = 2;
        float speedRate = 1.0f;
        u64 seekOffsetSamples = 0;
        std::string loadedMusicPath = "";
        s16* audioBuffer[AUDIO_BUFFER_COUNT] = { nullptr, nullptr, nullptr };
        size_t bufferSamples[AUDIO_BUFFER_COUNT] = { 0, 0, 0 };
        int startOffsetMs = 0;
        bool loop = false;
        bool ready = false;
        u32 id = 0;

        PreparedPreview() {
            reset();
        }

        void reset() {
            std::memset(&mp3, 0, sizeof(mp3));
            vorbisStream = nullptr;
            usingVorbis = false;
            sampleRate = 44100;
            musicChannels = 2;
            speedRate = 1.0f;
            seekOffsetSamples = 0;
            loadedMusicPath.clear();
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                audioBuffer[i] = nullptr;
                bufferSamples[i] = 0;
            }
            startOffsetMs = 0;
            loop = false;
            ready = false;
            id = 0;
        }

        void release() {
            if (usingVorbis) {
                if (vorbisStream) {
                    stb_vorbis_close(vorbisStream);
                    vorbisStream = nullptr;
                }
                usingVorbis = false;
            } else {
                if (mp3.file.buffer) {
                    mp3dec_ex_close(&mp3);
                }
                std::memset(&mp3, 0, sizeof(mp3));
            }
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                if (audioBuffer[i]) { linearFree(audioBuffer[i]); audioBuffer[i] = nullptr; }
                bufferSamples[i] = 0;
            }
            ready = false;
            loadedMusicPath.clear();
        }
    };

    Thread previewWorker = nullptr;
    LightLock previewLock;
    LightEvent previewWake;
    volatile bool previewQuit = false;
    volatile bool previewLoading = false;
    std::string reqPreviewPath = "";
    float reqPreviewRate = 1.0f;
    int reqPreviewStartOff = 0;
    bool reqPreviewLoop = false;
    u32 reqPreviewId = 0;
    PreparedPreview preparedPreview;

    static void previewThreadMain(void* arg) {
        static_cast<AudioPlayer*>(arg)->runPreviewWorker();
    }

    void runPreviewWorker() {
        while (!previewQuit) {
            LightEvent_Wait(&previewWake);
            if (previewQuit) break;

            LightLock_Lock(&previewLock);
            std::string path = reqPreviewPath;
            float rate = reqPreviewRate;
            int startOff = reqPreviewStartOff;
            bool loop = reqPreviewLoop;
            u32 id = reqPreviewId;
            LightLock_Unlock(&previewLock);

            if (path.empty()) continue;

            PreparedPreview p;
            p.usingVorbis = false;
            p.vorbisStream = nullptr;
            p.seekOffsetSamples = 0;
            p.speedRate = std::max(0.5f, std::min(2.0f, rate));
            p.startOffsetMs = startOff;
            p.loop = loop;

            bool tryOggFirst = hasOggHeaderOrExt(path);
            if (tryOggFirst) {
                int err = 0;
                p.vorbisStream = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
                if (p.vorbisStream) {
                    stb_vorbis_info vinfo = stb_vorbis_get_info(p.vorbisStream);
                    p.sampleRate = (vinfo.sample_rate > 0) ? vinfo.sample_rate : 44100;
                    p.musicChannels = (vinfo.channels == 1) ? 1 : 2;
                    p.usingVorbis = true;
                }
            }

            if (!p.usingVorbis) {
                std::memset(&p.mp3, 0, sizeof(p.mp3));
                if (mp3dec_ex_open(&p.mp3, path.c_str(), MP3D_SEEK_TO_SAMPLE) == 0 && p.mp3.info.hz > 0) {
                    p.sampleRate = static_cast<u32>(p.mp3.info.hz);
                    p.musicChannels = (p.mp3.info.channels == 1) ? 1 : 2;
                } else {
                    int err = 0;
                    p.vorbisStream = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
                    if (!p.vorbisStream) {
                        p.release();
                        continue;
                    }
                    stb_vorbis_info vinfo = stb_vorbis_get_info(p.vorbisStream);
                    p.sampleRate = (vinfo.sample_rate > 0) ? vinfo.sample_rate : 44100;
                    p.musicChannels = (vinfo.channels == 1) ? 1 : 2;
                    p.usingVorbis = true;
                }
            }

            if (previewQuit || id != reqPreviewId) {
                p.release();
                continue;
            }

            if (startOff > 0) {
                if (p.usingVorbis && p.vorbisStream) {
                    unsigned int targetFrame = static_cast<unsigned int>((static_cast<u64>(startOff) * p.sampleRate) / 1000ULL);
                    if (stb_vorbis_seek(p.vorbisStream, targetFrame)) {
                        int act = stb_vorbis_get_sample_offset(p.vorbisStream);
                        p.seekOffsetSamples = (act >= 0) ? static_cast<u64>(act) : targetFrame;
                    }
                } else if (p.sampleRate > 0 && p.musicChannels > 0) {
                    uint64_t targetSample = (static_cast<uint64_t>(startOff) * p.sampleRate * p.musicChannels) / 1000ULL;
                    if (mp3dec_ex_seek(&p.mp3, targetSample) == 0) {
                        p.seekOffsetSamples = p.mp3.cur_sample / p.musicChannels;
                    }
                }
            }

            if (previewQuit || id != reqPreviewId) {
                p.release();
                continue;
            }

            size_t bufferSize = AUDIO_BUFFER_FRAMES * p.musicChannels * sizeof(s16);
            bool allocOk = true;
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                p.audioBuffer[i] = static_cast<s16*>(linearAlloc(bufferSize));
                if (!p.audioBuffer[i]) allocOk = false;
            }
            if (!allocOk) {
                p.release();
                continue;
            }

            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                size_t samplesToRead = AUDIO_BUFFER_FRAMES * p.musicChannels;
                size_t samplesRead = 0;
                if (p.usingVorbis && p.vorbisStream) {
                    int frames = stb_vorbis_get_samples_short_interleaved(
                        p.vorbisStream, p.musicChannels, p.audioBuffer[i], static_cast<int>(samplesToRead)
                    );
                    if (frames > 0) samplesRead = static_cast<size_t>(frames) * p.musicChannels;
                } else {
                    samplesRead = mp3dec_ex_read(&p.mp3, p.audioBuffer[i], samplesToRead);
                }
                if (samplesRead > 0) {
                    DSP_FlushDataCache(p.audioBuffer[i], samplesRead * sizeof(s16));
                }
                p.bufferSamples[i] = samplesRead;
            }

            p.loadedMusicPath = path;
            p.id = id;
            p.ready = true;

            LightLock_Lock(&previewLock);
            if (id == reqPreviewId && !previewQuit) {
                preparedPreview.release();
                preparedPreview = p;
                p.reset();
            } else {
                p.release();
            }
            LightLock_Unlock(&previewLock);
        }
    }

    void applyChannelGain(int chn, float gain) {
        if (!dspReady) return;
        float mix[12];
        std::memset(mix, 0, sizeof(mix));
        mix[0] = gain;
        mix[1] = gain;
        ndspChnSetMix(chn, mix);
    }

    static bool hasOggHeaderOrExt(const std::string& path) {
        if (path.size() >= 4) {
            std::string ext = path.substr(path.size() - 4);
            for (char& c : ext) if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            if (ext == ".ogg") return true;
        }
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        char magic[4] = {0};
        size_t n = std::fread(magic, 1, 4, f);
        std::fclose(f);
        return (n == 4 && std::memcmp(magic, "OggS", 4) == 0);
    }

    bool loadOggSample(const std::string& path, WavSample& out) {
        int ch = 0, rate = 0;
        short* decoded = nullptr;
        int frames = stb_vorbis_decode_filename(path.c_str(), &ch, &rate, &decoded);
        if (frames <= 0 || !decoded || ch <= 0) {
            if (decoded) free(decoded);
            return false;
        }

        u32 totalBytes = static_cast<u32>(frames) * static_cast<u32>(ch) * sizeof(s16);
        if (totalBytes > 2 * 1024 * 1024) {
            free(decoded);
            return false;
        }

        out.freeData();
        out.data = static_cast<u8*>(linearAlloc(totalBytes));
        if (out.data) {
            std::memcpy(out.data, decoded, totalBytes);
            DSP_FlushDataCache(out.data, totalBytes);
            out.byteSize = totalBytes;
            out.nsamples = static_cast<u32>(frames);
            out.sampleRate = static_cast<u32>(rate);
            out.channels = static_cast<u16>(ch);
            out.bitsPerSample = 16;
        }
        free(decoded);
        return out.data != nullptr;
    }

    bool loadWavFile(const std::string& path, WavSample& out) {
        if (path.empty()) return false;
        if (hasOggHeaderOrExt(path)) {
            return loadOggSample(path, out);
        }

        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;

        char magic[4];
        if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "RIFF", 4) != 0) {
            std::fclose(f);
            return false;
        }
        std::fseek(f, 4, SEEK_CUR);
        if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "WAVE", 4) != 0) {
            std::fclose(f);
            return false;
        }

        u16 audioFormat = 1;
        while (!std::feof(f)) {
            char chunkId[4];
            u32 chunkSize = 0;
            if (std::fread(chunkId, 1, 4, f) != 4) break;
            if (std::fread(&chunkSize, 4, 1, f) != 1) break;

            if (std::memcmp(chunkId, "fmt ", 4) == 0) {
                std::fread(&audioFormat, 2, 1, f);
                std::fread(&out.channels, 2, 1, f);
                std::fread(&out.sampleRate, 4, 1, f);
                std::fseek(f, 6, SEEK_CUR);
                std::fread(&out.bitsPerSample, 2, 1, f);
                if (chunkSize > 16) std::fseek(f, chunkSize - 16, SEEK_CUR);
            } else if (std::memcmp(chunkId, "data", 4) == 0) {
                if (audioFormat != 1 || (out.bitsPerSample != 8 && out.bitsPerSample != 16) || chunkSize > 2 * 1024 * 1024) {
                    break;
                }
                out.freeData();
                out.data = static_cast<u8*>(linearAlloc(chunkSize));
                if (out.data) {
                    std::fread(out.data, 1, chunkSize, f);
                    DSP_FlushDataCache(out.data, chunkSize);
                    out.byteSize = chunkSize;
                    u32 bytesPerSample = (out.bitsPerSample / 8) * out.channels;
                    out.nsamples = (bytesPerSample > 0) ? (chunkSize / bytesPerSample) : 0;
                }
                break;
            } else {
                std::fseek(f, chunkSize, SEEK_CUR);
            }
        }
        std::fclose(f);
        return out.data != nullptr;
    }

    // Genera el sonido de Combo Break en memoria si el skin no incluye combobreak.wav
    void generateComboBreakSample(WavSample& out) {
        const u32 rate = 44100;
        const u32 frames = rate * 24 / 100; // 240 ms
        const u32 totalBytes = frames * sizeof(s16);
        out.freeData();
        out.data = static_cast<u8*>(linearAlloc(totalBytes));
        if (!out.data) return;

        s16* pcm = reinterpret_cast<s16*>(out.data);
        u32 rng = 0x13579BDFu;
        float phase1 = 0.0f, phase2 = 0.0f;
        for (u32 i = 0; i < frames; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(rate);
            float env = std::exp(-t * 14.0f);
            float crashEnv = std::exp(-t * 28.0f);
            // Tono descendente + destello de ruptura
            float f1 = 520.0f - t * 1150.0f;
            if (f1 < 110.0f) f1 = 110.0f;
            float f2 = 980.0f - t * 1900.0f;
            if (f2 < 180.0f) f2 = 180.0f;
            phase1 += (2.0f * 3.14159265f * f1) / rate;
            phase2 += (2.0f * 3.14159265f * f2) / rate;
            rng = rng * 1664525u + 1013904223u;
            float noise = (static_cast<int>(rng >> 16) - 32768) / 32768.0f;
            float val = (std::sin(phase1) * 0.55f + std::sin(phase2) * 0.30f) * env + noise * 0.45f * crashEnv;
            int sample = static_cast<int>(val * 24000.0f);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            pcm[i] = static_cast<s16>(sample);
        }
        DSP_FlushDataCache(out.data, totalBytes);
        out.byteSize = totalBytes;
        out.nsamples = frames;
        out.sampleRate = rate;
        out.channels = 1;
        out.bitsPerSample = 16;
    }

    // Genera hitsounds distintivos para los presets integrados (1: WhiteCat, 2: Rafis, 3: Neon Synth)
    void generatePresetSample(int preset, int slot, WavSample& out) {
        const u32 rate = 44100;
        const u32 frames = (slot == 2) ? (rate * 14 / 100) : (rate * 8 / 100);
        const u32 totalBytes = frames * sizeof(s16);
        out.freeData();
        out.data = static_cast<u8*>(linearAlloc(totalBytes));
        if (!out.data) return;

        s16* pcm = reinterpret_cast<s16*>(out.data);
        u32 rng = 0x2468ACE1u + static_cast<u32>(preset * 97 + slot * 31);
        float phase = 0.0f, phaseB = 0.0f;
        for (u32 i = 0; i < frames; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(rate);
            rng = rng * 1664525u + 1013904223u;
            float noise = (static_cast<int>(rng >> 16) - 32768) / 32768.0f;
            float val = 0.0f;

            if (preset == 1) {
                // WhiteCat Minimal: clic de madera corto y limpio
                if (slot == 0) {
                    float f = 780.0f * std::exp(-t * 45.0f) + 190.0f;
                    phase += (2.0f * 3.14159265f * f) / rate;
                    val = std::sin(phase) * std::exp(-t * 58.0f) + noise * 0.22f * std::exp(-t * 120.0f);
                } else if (slot == 1) {
                    phase += (2.0f * 3.14159265f * 1650.0f) / rate;
                    val = std::sin(phase) * std::exp(-t * 45.0f);
                } else if (slot == 2) {
                    phase += (2.0f * 3.14159265f * 520.0f) / rate;
                    val = (std::sin(phase) * 0.5f + noise * 0.5f) * std::exp(-t * 28.0f);
                } else {
                    phase += (2.0f * 3.14159265f * 1150.0f) / rate;
                    val = (std::sin(phase) * 0.45f + noise * 0.55f) * std::exp(-t * 52.0f);
                }
            } else if (preset == 2) {
                // Rafis Cyan: ping cristalino de alta precisión
                if (slot == 0) {
                    phase += (2.0f * 3.14159265f * 1040.0f) / rate;
                    phaseB += (2.0f * 3.14159265f * 2080.0f) / rate;
                    val = (std::sin(phase) * 0.7f + std::sin(phaseB) * 0.3f) * std::exp(-t * 50.0f);
                } else if (slot == 1) {
                    phase += (2.0f * 3.14159265f * 2350.0f) / rate;
                    val = std::sin(phase) * std::exp(-t * 42.0f);
                } else if (slot == 2) {
                    phase += (2.0f * 3.14159265f * 880.0f) / rate;
                    val = (std::sin(phase) * 0.4f + noise * 0.6f) * std::exp(-t * 24.0f);
                } else {
                    val = noise * std::exp(-t * 48.0f) * 0.85f;
                }
            } else {
                // Neon Synth: pulso electrónico synthwave
                if (slot == 0) {
                    float f = 620.0f * std::exp(-t * 32.0f) + 140.0f;
                    phase += (2.0f * 3.14159265f * f) / rate;
                    val = std::sin(phase) * std::exp(-t * 44.0f);
                } else if (slot == 1) {
                    float f = 1400.0f + t * 4200.0f;
                    phase += (2.0f * 3.14159265f * f) / rate;
                    val = std::sin(phase) * std::exp(-t * 40.0f) * 0.8f;
                } else if (slot == 2) {
                    float f = 340.0f * std::exp(-t * 18.0f) + 90.0f;
                    phase += (2.0f * 3.14159265f * f) / rate;
                    val = (std::sin(phase) * 0.65f + noise * 0.35f) * std::exp(-t * 22.0f);
                } else {
                    phase += (2.0f * 3.14159265f * 920.0f) / rate;
                    val = (std::sin(phase) * 0.5f + noise * 0.5f) * std::exp(-t * 46.0f);
                }
            }

            int sample = static_cast<int>(val * 26000.0f);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            pcm[i] = static_cast<s16>(sample);
        }
        DSP_FlushDataCache(out.data, totalBytes);
        out.byteSize = totalBytes;
        out.nsamples = frames;
        out.sampleRate = rate;
        out.channels = 1;
        out.bitsPerSample = 16;
    }

    void generateNightcoreSamples() {
        const u32 rate = 44100;

        // 1. Kick (170 ms): Punchy 808/909 dance kick
        {
            const u32 frames = rate * 17 / 100;
            const u32 totalBytes = frames * sizeof(s16);
            ncKick.freeData();
            ncKick.data = static_cast<u8*>(linearAlloc(totalBytes));
            if (ncKick.data) {
                s16* pcm = reinterpret_cast<s16*>(ncKick.data);
                float phase = 0.0f;
                for (u32 i = 0; i < frames; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(rate);
                    float f = 48.0f + 140.0f * std::exp(-t * 36.0f);
                    phase += (2.0f * 3.14159265f * f) / rate;
                    float env = std::exp(-t * 18.0f);
                    float click = (t < 0.004f) ? ((1.0f - t / 0.004f) * 0.45f) : 0.0f;
                    float val = (std::sin(phase) * 0.85f + click) * env;
                    int sample = static_cast<int>(val * 28000.0f);
                    pcm[i] = static_cast<s16>(std::max(-32768, std::min(32767, sample)));
                }
                DSP_FlushDataCache(ncKick.data, totalBytes);
                ncKick.byteSize = totalBytes;
                ncKick.nsamples = frames;
                ncKick.sampleRate = rate;
                ncKick.channels = 1;
                ncKick.bitsPerSample = 16;
            }
        }

        // 2. Clap (160 ms): Crisp TR-909 multi-burst dance handclap
        {
            const u32 frames = rate * 16 / 100;
            const u32 totalBytes = frames * sizeof(s16);
            ncClap.freeData();
            ncClap.data = static_cast<u8*>(linearAlloc(totalBytes));
            if (ncClap.data) {
                s16* pcm = reinterpret_cast<s16*>(ncClap.data);
                u32 rng = 0x51A2B3C4u;
                float phaseBody = 0.0f;
                for (u32 i = 0; i < frames; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(rate);
                    rng = rng * 1664525u + 1013904223u;
                    float noise = (static_cast<int>(rng >> 16) - 32768) / 32768.0f;

                    float envBurst = 0.0f;
                    if (t < 0.009f) envBurst = std::exp(-t * 280.0f);
                    else if (t >= 0.011f && t < 0.020f) envBurst = std::exp(-(t - 0.011f) * 280.0f);
                    else if (t >= 0.022f && t < 0.031f) envBurst = std::exp(-(t - 0.022f) * 280.0f);

                    float envTail = (t >= 0.028f) ? std::exp(-(t - 0.028f) * 32.0f) : 0.0f;
                    phaseBody += (2.0f * 3.14159265f * 950.0f) / rate;
                    float body = std::sin(phaseBody) * 0.35f;

                    float val = (noise * 0.85f + body) * (envBurst * 0.75f + envTail * 0.65f);
                    int sample = static_cast<int>(val * 27000.0f);
                    pcm[i] = static_cast<s16>(std::max(-32768, std::min(32767, sample)));
                }
                DSP_FlushDataCache(ncClap.data, totalBytes);
                ncClap.byteSize = totalBytes;
                ncClap.nsamples = frames;
                ncClap.sampleRate = rate;
                ncClap.channels = 1;
                ncClap.bitsPerSample = 16;
            }
        }

        // 3. Hat (45 ms): Tight metallic closed hi-hat
        {
            const u32 frames = rate * 45 / 1000;
            const u32 totalBytes = frames * sizeof(s16);
            ncHat.freeData();
            ncHat.data = static_cast<u8*>(linearAlloc(totalBytes));
            if (ncHat.data) {
                s16* pcm = reinterpret_cast<s16*>(ncHat.data);
                u32 rng = 0x87654321u;
                float p1 = 0.0f, p2 = 0.0f;
                for (u32 i = 0; i < frames; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(rate);
                    rng = rng * 1664525u + 1013904223u;
                    float noise = (static_cast<int>(rng >> 16) - 32768) / 32768.0f;
                    p1 += (2.0f * 3.14159265f * 4800.0f) / rate;
                    p2 += (2.0f * 3.14159265f * 7200.0f) / rate;
                    float metallic = (std::sin(p1) + std::sin(p2)) * 0.35f;
                    float env = std::exp(-t * 95.0f);
                    float val = (noise * 0.65f + metallic) * env;
                    int sample = static_cast<int>(val * 22000.0f);
                    pcm[i] = static_cast<s16>(std::max(-32768, std::min(32767, sample)));
                }
                DSP_FlushDataCache(ncHat.data, totalBytes);
                ncHat.byteSize = totalBytes;
                ncHat.nsamples = frames;
                ncHat.sampleRate = rate;
                ncHat.channels = 1;
                ncHat.bitsPerSample = 16;
            }
        }

        // 4. Crash (320 ms): Open dance cymbal
        {
            const u32 frames = rate * 32 / 100;
            const u32 totalBytes = frames * sizeof(s16);
            ncCrash.freeData();
            ncCrash.data = static_cast<u8*>(linearAlloc(totalBytes));
            if (ncCrash.data) {
                s16* pcm = reinterpret_cast<s16*>(ncCrash.data);
                u32 rng = 0x99887766u;
                float p1 = 0.0f, p2 = 0.0f;
                for (u32 i = 0; i < frames; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(rate);
                    rng = rng * 1664525u + 1013904223u;
                    float noise = (static_cast<int>(rng >> 16) - 32768) / 32768.0f;
                    p1 += (2.0f * 3.14159265f * 3200.0f) / rate;
                    p2 += (2.0f * 3.14159265f * 5900.0f) / rate;
                    float metal = (std::sin(p1) + std::sin(p2)) * 0.30f;
                    float env = std::exp(-t * 11.0f);
                    float val = (noise * 0.70f + metal) * env;
                    int sample = static_cast<int>(val * 24000.0f);
                    pcm[i] = static_cast<s16>(std::max(-32768, std::min(32767, sample)));
                }
                DSP_FlushDataCache(ncCrash.data, totalBytes);
                ncCrash.byteSize = totalBytes;
                ncCrash.nsamples = frames;
                ncCrash.sampleRate = rate;
                ncCrash.channels = 1;
                ncCrash.bitsPerSample = 16;
            }
        }
    }

    void triggerWavChannel(int chn, WavSample& wav) {
        if (!dspReady || !wav.data || wav.nsamples == 0 || sfxGain <= 0.001f) return;

        ndspChnWaveBufClear(chn);
        ndspChnSetInterp(chn, NDSP_INTERP_LINEAR);
        ndspChnSetRate(chn, wav.sampleRate);

        u16 format = NDSP_FORMAT_MONO_PCM16;
        if (wav.channels == 2) {
            format = (wav.bitsPerSample == 8) ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_STEREO_PCM16;
        } else {
            format = (wav.bitsPerSample == 8) ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_MONO_PCM16;
        }
        ndspChnSetFormat(chn, format);
        applyChannelGain(chn, sfxGain);

        std::memset(&wav.waveBuf, 0, sizeof(ndspWaveBuf));
        wav.waveBuf.data_vaddr = wav.data;
        wav.waveBuf.nsamples = wav.nsamples;
        wav.waveBuf.status = NDSP_WBUF_FREE;

        ndspChnWaveBufAdd(chn, &wav.waveBuf);
    }

    void beginNDSPPlayback() {
        inLeadIn = false;
        totalSamplesPlayed = seekOffsetSamples;
        musicStartTick = osGetTime();
        applyChannelGain(musicChannel, musicGain);
        for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
            if (preloadedBufReady[i] && waveBuf[i].nsamples > 0) {
                preloadedBufReady[i] = false;
                waveBuf[i].status = NDSP_WBUF_FREE;
                ndspChnWaveBufAdd(musicChannel, &waveBuf[i]);
            } else {
                preloadedBufReady[i] = false;
                fillAndPlayBuffer(i);
            }
        }
    }

public:
    AudioPlayer() = default;
    ~AudioPlayer() {
        stopAsyncLoader();
        exitAll();
    }

    bool startAsyncLoader(bool isNew3DS = false) {
        LightLock_Init(&previewLock);
        LightEvent_Init(&previewWake, RESET_ONESHOT);
        previewQuit = false;
        previewLoading = false;

        s32 prio = 0x30;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        s32 workerPrio = std::min<s32>(0x3F, prio + 1);

        if (isNew3DS) {
            // En New 3DS / New 2DS XL ejecutamos en el núcleo 2 dedicado a 804 MHz
            previewWorker = threadCreate(previewThreadMain, this, 64 * 1024, workerPrio, 2, false);
        }
        if (!previewWorker) previewWorker = threadCreate(previewThreadMain, this, 64 * 1024, workerPrio, 1, false);
        if (!previewWorker) previewWorker = threadCreate(previewThreadMain, this, 64 * 1024, workerPrio, -2, false);
        return previewWorker != nullptr;
    }

    void stopAsyncLoader() {
        if (!previewWorker) return;
        previewQuit = true;
        LightLock_Lock(&previewLock);
        reqPreviewPath.clear();
        reqPreviewId++;
        preparedPreview.release();
        LightEvent_Signal(&previewWake);
        LightLock_Unlock(&previewLock);

        threadJoin(previewWorker, U64_MAX);
        threadFree(previewWorker);
        previewWorker = nullptr;
    }

    void setLoop(bool loop, int offsetMs = -1) {
        loopEnabled = loop;
        if (offsetMs >= 0) {
            loopOffsetMs = offsetMs;
        }
    }
    bool getLoop() const { return loopEnabled; }
    int getLoopOffsetMs() const { return loopOffsetMs; }

    // Solicita la carga y reproducción de la vista previa de una canción sin bloquear el hilo principal
    void requestPreview(const std::string& filepath, float rate = 1.0f, int startOffsetMs = 0, bool loop = false, bool forceReload = false) {
        if (filepath.empty()) {
            cancelPreview();
            stopMusicInternal();
            return;
        }
        if (!forceReload && isPlaying && !isSongFinished() && loadedMusicPath == filepath) {
            setLoop(loop, startOffsetMs);
            return;
        }

        if (!previewWorker) {
            startMusic(filepath, 0, rate, startOffsetMs);
            setLoop(loop, startOffsetMs);
            return;
        }

        LightLock_Lock(&previewLock);
        if (forceReload || isSongFinished() || reqPreviewPath != filepath || reqPreviewStartOff != startOffsetMs || reqPreviewLoop != loop) {
            reqPreviewPath = filepath;
            reqPreviewRate = rate;
            reqPreviewStartOff = startOffsetMs;
            reqPreviewLoop = loop;
            reqPreviewId++;
            previewLoading = true;
            preparedPreview.release();
            LightEvent_Signal(&previewWake);
        }
        LightLock_Unlock(&previewLock);
    }

    void cancelPreview() {
        LightLock_Lock(&previewLock);
        reqPreviewPath.clear();
        reqPreviewId++;
        previewLoading = false;
        preparedPreview.release();
        LightLock_Unlock(&previewLock);
    }

    bool isPreviewLoading() {
        LightLock_Lock(&previewLock);
        bool loading = previewLoading && !reqPreviewPath.empty();
        LightLock_Unlock(&previewLock);
        return loading;
    }

    void pollPreview() {
        if (!dspReady) return;
        PreparedPreview p;
        bool haveNew = false;

        LightLock_Lock(&previewLock);
        if (preparedPreview.ready && preparedPreview.id == reqPreviewId) {
            p = preparedPreview;
            preparedPreview.reset();
            previewLoading = false;
            haveNew = true;
        }
        LightLock_Unlock(&previewLock);

        if (haveNew) {
            stopMusicInternal();

            mp3 = p.mp3;
            vorbisStream = p.vorbisStream;
            usingVorbis = p.usingVorbis;
            sampleRate = p.sampleRate;
            musicChannels = p.musicChannels;
            speedRate = p.speedRate;
            seekOffsetSamples = p.seekOffsetSamples;
            loadedMusicPath = p.loadedMusicPath;
            isLoaded = true;
            isPaused = false;
            eofReached = false;
            inLeadIn = false;
            loopEnabled = p.loop;
            loopOffsetMs = p.startOffsetMs;

            ndspChnReset(musicChannel);
            ndspChnSetInterp(musicChannel, NDSP_INTERP_POLYPHASE);
            ndspChnSetRate(musicChannel, static_cast<float>(sampleRate) * speedRate);
            ndspChnSetFormat(
                musicChannel,
                (musicChannels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16
            );
            applyChannelGain(musicChannel, musicGain);

            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                audioBuffer[i] = p.audioBuffer[i];
                p.audioBuffer[i] = nullptr;
            }

            std::memset(&waveBuf, 0, sizeof(waveBuf));
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                waveBuf[i].data_vaddr = audioBuffer[i];
                if (p.bufferSamples[i] > 0) {
                    waveBuf[i].nsamples = p.bufferSamples[i] / musicChannels;
                    waveBuf[i].status = NDSP_WBUF_FREE;
                    ndspChnWaveBufAdd(musicChannel, &waveBuf[i]);
                } else {
                    waveBuf[i].nsamples = 0;
                    waveBuf[i].status = NDSP_WBUF_DONE;
                }
            }

            isPlaying = true;
            totalSamplesPlayed = seekOffsetSamples;
            lastReportedTimeMs = -999999;
            musicStartTick = osGetTime();
            musicPlayheadOffsetMs = (p.startOffsetMs > 0) ? static_cast<u64>(p.startOffsetMs) : 0;
        }
    }

    bool initDSPAndHitsounds() {
        if (!dspReady) {
            if (ndspInit() < 0) return false;
            ndspSetOutputMode(NDSP_OUTPUT_STEREO);
            dspReady = true;

            generateNightcoreSamples();

            loadSkinHitsounds(
                "romfs:/drum-hitnormal2.wav",
                "romfs:/drum-hitwhistle2.wav",
                "romfs:/drum-hitclap2.wav",
                "romfs:/drum-hitclap2.wav",
                "",
                0
            );
            setVolumes(85, 100);
        }
        return true;
    }

    void loadSkinHitsounds(
        const std::string& normalPath,
        const std::string& whistlePath,
        const std::string& finishPath,
        const std::string& clapPath,
        const std::string& comboBreakPath = "",
        int synthPreset = 0,
        const std::string& ncKickPath = "",
        const std::string& ncClapPath = "",
        const std::string& ncHatPath = ""
    ) {
        if (!dspReady) return;
        for (int c = 1; c <= 9; ++c) ndspChnReset(c);

        if (synthPreset > 0) {
            for (int s = 0; s < 4; ++s) {
                generatePresetSample(synthPreset, s, hitSounds[s]);
            }
        } else {
            if (!loadWavFile(normalPath, hitSounds[0])) {
                loadWavFile("romfs:/drum-hitnormal2.wav", hitSounds[0]);
            }
            if (!loadWavFile(whistlePath, hitSounds[1])) {
                loadWavFile("romfs:/drum-hitwhistle2.wav", hitSounds[1]);
            }
            if (!loadWavFile(finishPath, hitSounds[2])) {
                loadWavFile("romfs:/drum-hitclap2.wav", hitSounds[2]);
            }
            if (!loadWavFile(clapPath, hitSounds[3])) {
                loadWavFile("romfs:/drum-hitclap2.wav", hitSounds[3]);
            }
        }

        if (comboBreakPath.empty() || !loadWavFile(comboBreakPath, hitSounds[4])) {
            generateComboBreakSample(hitSounds[4]);
        }

        // Cargar o regenerar muestras para Nightcore
        if (!ncKickPath.empty()) loadWavFile(ncKickPath, ncKick);
        if (!ncClapPath.empty()) loadWavFile(ncClapPath, ncClap);
        if (!ncHatPath.empty())  loadWavFile(ncHatPath, ncHat);
        if (!ncKick.data || !ncClap.data || !ncHat.data) {
            generateNightcoreSamples();
        }
    }

    // Carga los hitsounds propios del beatmap si existen en la carpeta de la canción
    bool loadBeatmapHitsounds(const std::string& folderPath, const std::string& sampleSet = "Normal") {
        if (!dspReady || folderPath.empty()) return false;

        std::string setLow = sampleSet;
        for (char& c : setLow) if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (setLow.empty() || setLow == "none") setLow = "normal";

        auto findSound = [&](const std::vector<std::string>& candidates) -> std::string {
            for (const auto& name : candidates) {
                std::string fullPath = folderPath + "/" + name;
                FILE* f = std::fopen(fullPath.c_str(), "rb");
                if (f) {
                    std::fclose(f);
                    return fullPath;
                }
            }
            return "";
        };

        // 0: HitNormal
        std::string normalFile = findSound({
            setLow + "-hitnormal.wav", setLow + "-hitnormal.ogg",
            "hitnormal.wav", "hitnormal.ogg",
            "normal-hitnormal.wav", "normal-hitnormal.ogg",
            "soft-hitnormal.wav", "soft-hitnormal.ogg"
        });
        if (!normalFile.empty()) loadWavFile(normalFile, hitSounds[0]);

        // 1: HitWhistle
        std::string whistleFile = findSound({
            setLow + "-hitwhistle.wav", setLow + "-hitwhistle.ogg",
            "hitwhistle.wav", "hitwhistle.ogg",
            "normal-hitwhistle.wav", "normal-hitwhistle.ogg",
            "soft-hitwhistle.wav", "soft-hitwhistle.ogg"
        });
        if (!whistleFile.empty()) loadWavFile(whistleFile, hitSounds[1]);

        // 2: HitFinish
        std::string finishFile = findSound({
            setLow + "-hitfinish.wav", setLow + "-hitfinish.ogg",
            "hitfinish.wav", "hitfinish.ogg",
            "normal-hitfinish.wav", "normal-hitfinish.ogg",
            "soft-hitfinish.wav", "soft-hitfinish.ogg"
        });
        if (!finishFile.empty()) loadWavFile(finishFile, hitSounds[2]);

        // 3: HitClap
        std::string clapFile = findSound({
            setLow + "-hitclap.wav", setLow + "-hitclap.ogg",
            "hitclap.wav", "hitclap.ogg",
            "normal-hitclap.wav", "normal-hitclap.ogg",
            "soft-hitclap.wav", "soft-hitclap.ogg"
        });
        if (!clapFile.empty()) loadWavFile(clapFile, hitSounds[3]);

        // 4: ComboBreak
        std::string cbFile = findSound({ "combobreak.wav", "combobreak.ogg" });
        if (!cbFile.empty()) loadWavFile(cbFile, hitSounds[4]);

        return (!normalFile.empty() || !whistleFile.empty() || !finishFile.empty() || !clapFile.empty() || !cbFile.empty());
    }

    void setVolumes(int musicVolPct, int sfxVolPct) {
        musicGain = std::max(0, std::min(100, musicVolPct)) / 100.0f;
        sfxGain   = std::max(0, std::min(100, sfxVolPct)) / 100.0f;
        if (dspReady) {
            applyChannelGain(musicChannel, musicGain);
            for (int c = 1; c <= 5; ++c) applyChannelGain(c, sfxGain);
            for (int c = 6; c <= 9; ++c) applyChannelGain(c, sfxGain * 0.80f);
        }
    }

    void playHitSound(int hitSoundMask) {
        triggerWavChannel(1, hitSounds[0]);
        if (hitSoundMask & 2) triggerWavChannel(2, hitSounds[1]);
        if (hitSoundMask & 4) triggerWavChannel(3, hitSounds[2]);
        if (hitSoundMask & 8) triggerWavChannel(4, hitSounds[3]);
    }

    void playTaikoDon(bool isBig = false) {
        triggerWavChannel(1, hitSounds[0]); // Don (Parche central grave)
        if (isBig) {
            triggerWavChannel(3, hitSounds[2]); // Finish / Platillo para Gran Don
        }
    }

    void playTaikoKatsu(bool isBig = false) {
        triggerWavChannel(2, hitSounds[1]); // Katsu (Aro de madera agudo)
        if (isBig) {
            triggerWavChannel(3, hitSounds[2]); // Finish / Platillo para Gran Katsu
        }
    }

    void playTaikoBalloonPop() {
        triggerWavChannel(3, hitSounds[2]);
        triggerWavChannel(4, hitSounds[3]);
    }

    void playComboBreak() {
        triggerWavChannel(5, hitSounds[4]);
    }

    // Efectos de ritmo Nightcore (Canales DSP 6..9)
    void playNcKick()  { triggerWavChannel(6, ncKick);  }
    void playNcClap()  { triggerWavChannel(7, ncClap);  }
    void playNcHat()   { triggerWavChannel(8, ncHat);   }
    void playNcCrash() { triggerWavChannel(9, ncCrash); }

    const std::string& getLoadedMusicPath() const { return loadedMusicPath; }
    bool getIsPlaying() const { return isPlaying; }

    bool startMusic(const std::string& filepath, int leadInMs = 0, float rate = 1.0f, int startOffsetMs = 0) {
        cancelPreview();
        stopMusicInternal();
        if (filepath.empty()) return false;
        if (!dspReady && !initDSPAndHitsounds()) return false;

        usingVorbis = false;
        vorbisStream = nullptr;
        seekOffsetSamples = 0;

        bool tryOggFirst = hasOggHeaderOrExt(filepath);
        if (tryOggFirst) {
            int err = 0;
            vorbisStream = stb_vorbis_open_filename(filepath.c_str(), &err, nullptr);
            if (vorbisStream) {
                stb_vorbis_info vinfo = stb_vorbis_get_info(vorbisStream);
                sampleRate = (vinfo.sample_rate > 0) ? vinfo.sample_rate : 44100;
                musicChannels = (vinfo.channels == 1) ? 1 : 2;
                usingVorbis = true;
            }
        }

        if (!usingVorbis) {
            if (mp3dec_ex_open(&mp3, filepath.c_str(), MP3D_SEEK_TO_SAMPLE) == 0 && mp3.info.hz > 0) {
                sampleRate = static_cast<u32>(mp3.info.hz);
                musicChannels = (mp3.info.channels == 1) ? 1 : 2;
            } else {
                // Fallback a Ogg Vorbis por si el archivo tiene extensión .mp3 pero contenido .ogg
                int err = 0;
                vorbisStream = stb_vorbis_open_filename(filepath.c_str(), &err, nullptr);
                if (!vorbisStream) return false;
                stb_vorbis_info vinfo = stb_vorbis_get_info(vorbisStream);
                sampleRate = (vinfo.sample_rate > 0) ? vinfo.sample_rate : 44100;
                musicChannels = (vinfo.channels == 1) ? 1 : 2;
                usingVorbis = true;
            }
        }

        // Si se pide saltar al PreviewTime (menú de inicio / selector de canciones), buscar de forma instantánea
        if (startOffsetMs > 0 && leadInMs <= 0) {
            if (usingVorbis && vorbisStream) {
                unsigned int targetFrame = static_cast<unsigned int>((static_cast<u64>(startOffsetMs) * sampleRate) / 1000ULL);
                if (stb_vorbis_seek(vorbisStream, targetFrame)) {
                    int act = stb_vorbis_get_sample_offset(vorbisStream);
                    seekOffsetSamples = (act >= 0) ? static_cast<u64>(act) : targetFrame;
                }
            } else if (sampleRate > 0 && musicChannels > 0) {
                uint64_t targetSample = (static_cast<uint64_t>(startOffsetMs) * sampleRate * musicChannels) / 1000ULL;
                if (mp3dec_ex_seek(&mp3, targetSample) == 0) {
                    seekOffsetSamples = mp3.cur_sample / musicChannels;
                }
            }
        }

        loadedMusicPath = filepath;
        speedRate = std::max(0.5f, std::min(2.0f, rate));
        isLoaded = true;
        isPaused = false;
        eofReached = false;

        ndspChnReset(musicChannel);
        ndspChnSetInterp(musicChannel, NDSP_INTERP_POLYPHASE);
        ndspChnSetRate(musicChannel, static_cast<float>(sampleRate) * speedRate);
        ndspChnSetFormat(
            musicChannel,
            (musicChannels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16
        );
        applyChannelGain(musicChannel, musicGain);

        size_t bufferSize = AUDIO_BUFFER_FRAMES * musicChannels * sizeof(s16);
        std::memset(&waveBuf, 0, sizeof(waveBuf));
        for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
            audioBuffer[i] = static_cast<s16*>(linearAlloc(bufferSize));
            waveBuf[i].data_vaddr = audioBuffer[i];
            waveBuf[i].status = NDSP_WBUF_DONE;
            preloadedBufReady[i] = false;
        }

        isPlaying = true;
        totalSamplesPlayed = seekOffsetSamples;
        lastReportedTimeMs = -999999;
        musicStartTick = 0;
        musicPlayheadOffsetMs = (startOffsetMs > 0 && leadInMs <= 0) ? static_cast<u64>(startOffsetMs) : 0;

        if (leadInMs > 0) {
            // Pre-decodificar los buffers iniciales en RAM durante la pantalla de carga
            // para evitar cualquier bajón de FPS cuando termine el lead-in e inicie la música.
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                if (!eofReached && audioBuffer[i]) {
                    size_t samplesToRead = AUDIO_BUFFER_FRAMES * musicChannels;
                    size_t samplesRead = 0;
                    if (usingVorbis && vorbisStream) {
                        int frames = stb_vorbis_get_samples_short_interleaved(
                            vorbisStream, musicChannels, audioBuffer[i], static_cast<int>(samplesToRead)
                        );
                        if (frames > 0) samplesRead = static_cast<size_t>(frames) * musicChannels;
                    } else {
                        samplesRead = mp3dec_ex_read(&mp3, audioBuffer[i], samplesToRead);
                    }
                    if (samplesRead > 0) {
                        DSP_FlushDataCache(audioBuffer[i], samplesRead * sizeof(s16));
                        waveBuf[i].nsamples = samplesRead / musicChannels;
                        preloadedBufReady[i] = true;
                    } else {
                        eofReached = true;
                    }
                }
            }
            inLeadIn = true;
            leadInDurationMs = leadInMs;
            leadInStartTick = osGetTime();
        } else {
            beginNDSPPlayback();
        }
        return true;
    }

    void pauseMusic() {
        if (!isPlaying || isPaused) return;
        isPaused = true;
        pauseStartTick = osGetTime();
        if (!inLeadIn && dspReady) {
            ndspChnSetPaused(musicChannel, true);
        }
    }

    void resumeMusic() {
        if (!isPlaying || !isPaused) return;
        isPaused = false;
        if (inLeadIn) {
            leadInStartTick += (osGetTime() - pauseStartTick);
        } else {
            if (musicStartTick > 0) {
                musicStartTick += (osGetTime() - pauseStartTick);
            }
            if (dspReady) {
                ndspChnSetPaused(musicChannel, false);
            }
        }
    }

    bool seekToMs(int targetMs) {
        if (!isLoaded || sampleRate == 0) return false;

        // Si el salto nos sitúa en tiempo de espera previo (Lead-In)
        lastReportedTimeMs = -999999;
        if (targetMs < 0) {
            inLeadIn = true;
            leadInDurationMs = -targetMs;
            leadInStartTick = osGetTime();
            musicStartTick = 0;
            musicPlayheadOffsetMs = 0;
            totalSamplesPlayed = 0;
            seekOffsetSamples = 0;
            if (usingVorbis && vorbisStream) {
                stb_vorbis_seek(vorbisStream, 0);
            } else {
                mp3dec_ex_seek(&mp3, 0);
            }
            if (dspReady) {
                ndspChnReset(musicChannel);
            }
            return true;
        }

        inLeadIn = false;
        targetMs = std::max(0, targetMs);
        musicStartTick = osGetTime();
        musicPlayheadOffsetMs = static_cast<u64>(targetMs);

        if (usingVorbis && vorbisStream) {
            unsigned int targetFrame = static_cast<unsigned int>((static_cast<u64>(targetMs) * sampleRate) / 1000ULL);
            stb_vorbis_seek(vorbisStream, targetFrame);
            int act = stb_vorbis_get_sample_offset(vorbisStream);
            if (act >= 0) targetFrame = static_cast<unsigned int>(act);
            seekOffsetSamples = targetFrame;
            totalSamplesPlayed = targetFrame;
        } else if (musicChannels > 0) {
            uint64_t targetSample = (static_cast<uint64_t>(targetMs) * sampleRate * musicChannels) / 1000ULL;
            if (mp3dec_ex_seek(&mp3, targetSample) == 0) {
                seekOffsetSamples = mp3.cur_sample / musicChannels;
                totalSamplesPlayed = seekOffsetSamples;
            } else {
                seekOffsetSamples = (static_cast<u64>(targetMs) * sampleRate) / 1000ULL;
                totalSamplesPlayed = seekOffsetSamples;
            }
        } else {
            seekOffsetSamples = (static_cast<u64>(targetMs) * sampleRate) / 1000ULL;
            totalSamplesPlayed = seekOffsetSamples;
        }

        eofReached = false;
        if (dspReady) {
            ndspChnReset(musicChannel);
            ndspChnSetInterp(musicChannel, NDSP_INTERP_POLYPHASE);
            ndspChnSetRate(musicChannel, static_cast<float>(sampleRate) * speedRate);
            ndspChnSetFormat(
                musicChannel,
                (musicChannels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16
            );
            applyChannelGain(musicChannel, musicGain);
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                waveBuf[i].status = NDSP_WBUF_DONE;
                waveBuf[i].nsamples = 0;
            }
            for (size_t i = 0; i < 2 && i < AUDIO_BUFFER_COUNT; ++i) {
                fillAndPlayBuffer(i);
            }
        }
        return true;
    }

    void fillAndPlayBuffer(int index) {
        if (!isLoaded || eofReached) return;
        size_t samplesToRead = AUDIO_BUFFER_FRAMES * musicChannels;
        size_t samplesRead = 0;

        if (usingVorbis && vorbisStream) {
            int frames = stb_vorbis_get_samples_short_interleaved(
                vorbisStream, musicChannels, audioBuffer[index], static_cast<int>(samplesToRead)
            );
            if (frames > 0) {
                samplesRead = static_cast<size_t>(frames) * musicChannels;
            }
        } else {
            samplesRead = mp3dec_ex_read(&mp3, audioBuffer[index], samplesToRead);
        }

        // Bucle sin interrupción (Seamless Loop): si se alcanza el final de la pista y el bucle está activo,
        // rebobinar de inmediato al punto de inicio (PreviewTime o 0) y completar el resto del búfer actual.
        if (samplesRead < samplesToRead && loopEnabled && sampleRate > 0 && musicChannels > 0) {
            if (usingVorbis && vorbisStream) {
                unsigned int targetFrame = static_cast<unsigned int>((static_cast<u64>(loopOffsetMs) * sampleRate) / 1000ULL);
                if (!stb_vorbis_seek(vorbisStream, targetFrame)) {
                    stb_vorbis_seek(vorbisStream, 0);
                    targetFrame = 0;
                }
                int act = stb_vorbis_get_sample_offset(vorbisStream);
                if (act >= 0) targetFrame = static_cast<unsigned int>(act);
                seekOffsetSamples = targetFrame;
            } else {
                uint64_t targetSample = (static_cast<uint64_t>(loopOffsetMs) * sampleRate * musicChannels) / 1000ULL;
                if (mp3dec_ex_seek(&mp3, targetSample) == 0) {
                    seekOffsetSamples = mp3.cur_sample / musicChannels;
                } else {
                    mp3dec_ex_seek(&mp3, 0);
                    seekOffsetSamples = 0;
                }
            }

            totalSamplesPlayed = seekOffsetSamples;
            musicStartTick = osGetTime();
            musicPlayheadOffsetMs = static_cast<u64>(loopOffsetMs);
            lastReportedTimeMs = loopOffsetMs;

            size_t remaining = samplesToRead - samplesRead;
            size_t extra = 0;
            if (usingVorbis && vorbisStream) {
                int frames = stb_vorbis_get_samples_short_interleaved(
                    vorbisStream, musicChannels, audioBuffer[index] + samplesRead, static_cast<int>(remaining)
                );
                if (frames > 0) extra = static_cast<size_t>(frames) * musicChannels;
            } else {
                extra = mp3dec_ex_read(&mp3, audioBuffer[index] + samplesRead, remaining);
            }
            samplesRead += extra;
        }

        if (samplesRead == 0) {
            eofReached = true;
            return;
        }

        DSP_FlushDataCache(audioBuffer[index], samplesRead * sizeof(s16));
        waveBuf[index].nsamples = samplesRead / musicChannels;
        waveBuf[index].status = NDSP_WBUF_FREE;
        ndspChnWaveBufAdd(musicChannel, &waveBuf[index]);
    }

    void update() {
        pollPreview();
        if (!isPlaying || isPaused) return;

        if (inLeadIn) {
            u64 elapsed = static_cast<u64>((osGetTime() - leadInStartTick) * speedRate);
            if (static_cast<int>(elapsed) >= leadInDurationMs) {
                beginNDSPPlayback();
            }
            return;
        }

        for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
            if (waveBuf[i].status == NDSP_WBUF_DONE) {
                if (waveBuf[i].nsamples > 0) {
                    totalSamplesPlayed += waveBuf[i].nsamples;
                    waveBuf[i].nsamples = 0;
                }
                if (!eofReached) {
                    fillAndPlayBuffer(i);
                }
            }
        }
    }

    int getTimeMs() const {
        if (!isPlaying || sampleRate == 0) return 0;
        if (inLeadIn) {
            u64 now = isPaused ? pauseStartTick : osGetTime();
            int elapsed = static_cast<int>((now - leadInStartTick) * speedRate);
            int t = elapsed - leadInDurationMs;
            lastReportedTimeMs = t;
            return t;
        }
        u64 currentSamples = totalSamplesPlayed + ndspChnGetSamplePos(musicChannel);
        int t = static_cast<int>((currentSamples * 1000) / sampleRate);

        // Watchdog de tiempo real para emuladores (Citra / Azahar) ante pérdida o estancamiento de muestras DSP
        if (musicStartTick > 0) {
            u64 nowTick = isPaused ? pauseStartTick : osGetTime();
            int wallT = static_cast<int>((nowTick - musicStartTick) * speedRate) + static_cast<int>(musicPlayheadOffsetMs);
            if (t == 0 && wallT > 100) {
                t = wallT;
            } else if (wallT > t + 500) {
                t = std::max(t, wallT - 100);
            }
        }

        if (t < lastReportedTimeMs && (lastReportedTimeMs - t) < 300) {
            t = lastReportedTimeMs;
        } else {
            lastReportedTimeMs = t;
        }
        return t;
    }

    bool isSongFinished() const {
        if (!isPlaying || inLeadIn || isPaused) return false;
        if (loopEnabled) return false;
        if (!eofReached) return false;
        for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
            if (waveBuf[i].status != NDSP_WBUF_DONE) return false;
        }
        return true;
    }

    void stopMusicInternal() {
        if (isLoaded) {
            isPlaying = false;
            isPaused = false;
            inLeadIn = false;
            loopEnabled = false;
            loopOffsetMs = 0;
            musicStartTick = 0;
            musicPlayheadOffsetMs = 0;
            lastReportedTimeMs = -999999;
            loadedMusicPath = "";
            ndspChnReset(musicChannel);
            if (usingVorbis) {
                if (vorbisStream) {
                    stb_vorbis_close(vorbisStream);
                    vorbisStream = nullptr;
                }
                usingVorbis = false;
            } else {
                if (mp3.file.buffer) {
                    mp3dec_ex_close(&mp3);
                }
                std::memset(&mp3, 0, sizeof(mp3));
            }
            for (size_t i = 0; i < AUDIO_BUFFER_COUNT; ++i) {
                if (audioBuffer[i]) { linearFree(audioBuffer[i]); audioBuffer[i] = nullptr; }
            }
            isLoaded = false;
        }
    }

    void stopMusic() {
        cancelPreview();
        stopMusicInternal();
    }

    void exitAll() {
        stopAsyncLoader();
        stopMusicInternal();
        for (int i = 0; i < 5; ++i) hitSounds[i].freeData();
        ncKick.freeData();
        ncClap.freeData();
        ncHat.freeData();
        ncCrash.freeData();
        if (dspReady) {
            ndspExit();
            dspReady = false;
        }
    }
};