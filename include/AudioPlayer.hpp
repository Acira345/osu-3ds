#pragma once
#include <3ds.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

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
    bool dspReady = false;
    bool isLoaded = false;
    bool isPlaying = false;
    bool isPaused = false;
    bool inLeadIn = false;
    bool eofReached = false;

    int musicChannel = 0;
    u32 sampleRate = 44100;
    u16 musicChannels = 2;
    float speedRate = 1.0f;

    float musicGain = 0.85f;
    float sfxGain = 1.0f;

    s16* audioBuffer[2] = { nullptr, nullptr };
    ndspWaveBuf waveBuf[2];

    u64 totalSamplesPlayed = 0;
    u64 leadInStartTick = 0;
    u64 pauseStartTick = 0;
    int leadInDurationMs = 0;

    std::string loadedMusicPath = "";

    // 0: Normal, 1: Whistle, 2: Clap (Canales DSP 1, 2 y 3)
    WavSample hitSounds[3];

    void applyChannelGain(int chn, float gain) {
        if (!dspReady) return;
        float mix[12];
        std::memset(mix, 0, sizeof(mix));
        mix[0] = gain;
        mix[1] = gain;
        ndspChnSetMix(chn, mix);
    }

    bool loadWavFile(const std::string& path, WavSample& out) {
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
                // Solo PCM sin comprimir (formato 1) y 8/16 bits soportados por NDSP
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
        totalSamplesPlayed = 0;
        applyChannelGain(musicChannel, musicGain);
        fillAndPlayBuffer(0);
        fillAndPlayBuffer(1);
    }

public:
    AudioPlayer() = default;
    ~AudioPlayer() { exitAll(); }

    bool initDSPAndHitsounds() {
        if (!dspReady) {
            if (ndspInit() < 0) return false;
            ndspSetOutputMode(NDSP_OUTPUT_STEREO);
            dspReady = true;

            loadSkinHitsounds(
                "romfs:/drum-hitnormal2.wav",
                "romfs:/drum-hitwhistle2.wav",
                "romfs:/drum-hitclap2.wav"
            );
            setVolumes(85, 100);
        }
        return true;
    }

    void loadSkinHitsounds(const std::string& normalPath, const std::string& whistlePath, const std::string& clapPath) {
        if (!dspReady) return;
        for (int c = 1; c <= 3; ++c) ndspChnReset(c);

        if (!loadWavFile(normalPath, hitSounds[0])) {
            loadWavFile("romfs:/drum-hitnormal2.wav", hitSounds[0]);
        }
        if (!loadWavFile(whistlePath, hitSounds[1])) {
            loadWavFile("romfs:/drum-hitwhistle2.wav", hitSounds[1]);
        }
        if (!loadWavFile(clapPath, hitSounds[2])) {
            loadWavFile("romfs:/drum-hitclap2.wav", hitSounds[2]);
        }
    }

    void setVolumes(int musicVolPct, int sfxVolPct) {
        musicGain = std::max(0, std::min(100, musicVolPct)) / 100.0f;
        sfxGain   = std::max(0, std::min(100, sfxVolPct)) / 100.0f;
        if (dspReady) {
            applyChannelGain(musicChannel, musicGain);
            applyChannelGain(1, sfxGain);
            applyChannelGain(2, sfxGain);
            applyChannelGain(3, sfxGain);
        }
    }

    void playHitSound(int hitSoundMask) {
        triggerWavChannel(1, hitSounds[0]);
        if (hitSoundMask & 2) triggerWavChannel(2, hitSounds[1]);
        if ((hitSoundMask & 8) || (hitSoundMask & 4)) triggerWavChannel(3, hitSounds[2]);
    }

    const std::string& getLoadedMusicPath() const { return loadedMusicPath; }
    bool getIsPlaying() const { return isPlaying; }

    bool startMusic(const std::string& filepath, int leadInMs = 0, float rate = 1.0f) {
        stopMusic();
        if (!dspReady && !initDSPAndHitsounds()) return false;

        if (mp3dec_ex_open(&mp3, filepath.c_str(), MP3D_DO_NOT_SCAN) != 0) {
            return false;
        }

        loadedMusicPath = filepath;
        sampleRate = (mp3.info.hz > 0) ? static_cast<u32>(mp3.info.hz) : 44100;
        musicChannels = (mp3.info.channels == 1) ? 1 : 2;
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
        audioBuffer[0] = static_cast<s16*>(linearAlloc(bufferSize));
        audioBuffer[1] = static_cast<s16*>(linearAlloc(bufferSize));

        std::memset(&waveBuf, 0, sizeof(waveBuf));
        for (int i = 0; i < 2; ++i) {
            waveBuf[i].data_vaddr = audioBuffer[i];
            waveBuf[i].status = NDSP_WBUF_DONE;
        }

        isPlaying = true;
        totalSamplesPlayed = 0;

        if (leadInMs > 0) {
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
        } else if (dspReady) {
            ndspChnSetPaused(musicChannel, false);
        }
    }

    void fillAndPlayBuffer(int index) {
        if (!isLoaded || eofReached) return;
        size_t samplesToRead = AUDIO_BUFFER_FRAMES * musicChannels;
        size_t samplesRead = mp3dec_ex_read(&mp3, audioBuffer[index], samplesToRead);
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
        if (!isPlaying || isPaused) return;

        if (inLeadIn) {
            u64 elapsed = static_cast<u64>((osGetTime() - leadInStartTick) * speedRate);
            if (static_cast<int>(elapsed) >= leadInDurationMs) {
                beginNDSPPlayback();
            }
            return;
        }

        for (int i = 0; i < 2; ++i) {
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
            return elapsed - leadInDurationMs;
        }
        u64 currentSamples = totalSamplesPlayed + ndspChnGetSamplePos(musicChannel);
        return static_cast<int>((currentSamples * 1000) / sampleRate);
    }

    bool isSongFinished() const {
        if (!isPlaying || inLeadIn || isPaused) return false;
        return eofReached &&
               waveBuf[0].status == NDSP_WBUF_DONE &&
               waveBuf[1].status == NDSP_WBUF_DONE;
    }

    void stopMusic() {
        if (isLoaded) {
            isPlaying = false;
            isPaused = false;
            inLeadIn = false;
            loadedMusicPath = "";
            ndspChnReset(musicChannel);
            mp3dec_ex_close(&mp3);
            if (audioBuffer[0]) { linearFree(audioBuffer[0]); audioBuffer[0] = nullptr; }
            if (audioBuffer[1]) { linearFree(audioBuffer[1]); audioBuffer[1] = nullptr; }
            isLoaded = false;
        }
    }

    void exitAll() {
        stopMusic();
        for (int i = 0; i < 3; ++i) hitSounds[i].freeData();
        if (dspReady) {
            ndspExit();
            dspReady = false;
        }
    }
};