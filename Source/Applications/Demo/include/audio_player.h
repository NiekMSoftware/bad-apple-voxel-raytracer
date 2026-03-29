#pragma once

// ============================================================================
// AudioPlayer
//
// Minimal wrapper around miniaudio for playing a single audio file.
// Supports play, pause, stop, and seeking to a time in seconds so it
// can be kept in sync with BadApplePlayer.
//
// Usage:
//   AudioPlayer audio;
//   audio.load("assets/bad_apple.ogg");   // or .mp3 / .wav / .flac
//   audio.play();
//   audio.seekTo(timeSeconds);
//   audio.pause();
//
// miniaudio decodes and plays entirely on its own background thread.
// All public methods are safe to call from the main/UI thread.
//
// Reference: miniaudio documentation, "High Level API"
//   https://miniaud.io/docs/manual/index.html#HighLevelAPI
// ============================================================================

#include "miniaudio.h"
#include <string>
#include <cstdio>

namespace demo {

    class AudioPlayer
    {
    public:
        AudioPlayer()  = default;
        ~AudioPlayer() { unload(); }

        // Non-copyable — owns an ma_engine and ma_sound.
        AudioPlayer(const AudioPlayer&)            = delete;
        AudioPlayer& operator=(const AudioPlayer&) = delete;

        // ----------------------------------------------------------------
        // load
        //
        // Initialises the miniaudio engine (if not already done) and loads
        // the audio file at 'path'.  Supported formats: WAV, MP3, OGG,
        // FLAC — whatever miniaudio was compiled with (all by default).
        //
        // Returns true on success.
        // ----------------------------------------------------------------
        bool load(const std::string& path)
        {
            unload();

            // Initialise engine with default settings (system default device,
            // default sample rate, default channel count).
            // Reference: ma_engine_init(), miniaudio.h
            ma_result result = ma_engine_init(nullptr, &m_engine);
            if (result != MA_SUCCESS)
            {
                printf("AudioPlayer: ma_engine_init failed (%d)\n", result);
                return false;
            }
            m_engineInitialised = true;

            // Load the sound file.  MA_SOUND_FLAG_STREAM avoids loading the
            // entire file into memory up front, which matters for a long video
            // soundtrack.
            // Reference: ma_sound_init_from_file(), miniaudio.h
            result = ma_sound_init_from_file(
                &m_engine,
                path.c_str(),
                MA_SOUND_FLAG_STREAM,
                nullptr,   // no sound group
                nullptr,   // no fence
                &m_sound);

            if (result != MA_SUCCESS)
            {
                printf("AudioPlayer: failed to load '%s' (%d)\n",
                       path.c_str(), result);
                ma_engine_uninit(&m_engine);
                m_engineInitialised = false;
                return false;
            }

            m_bLoaded = true;
            printf("AudioPlayer: loaded '%s'\n", path.c_str());
            return true;
        }

        // ----------------------------------------------------------------
        // unload — stops playback and frees all miniaudio resources.
        // ----------------------------------------------------------------
        void unload()
        {
            if (m_bLoaded)
            {
                ma_sound_uninit(&m_sound);
                m_bLoaded = false;
            }
            if (m_engineInitialised)
            {
                ma_engine_uninit(&m_engine);
                m_engineInitialised = false;
            }
        }

        // ----------------------------------------------------------------
        // Playback controls
        // ----------------------------------------------------------------
        void play()
        {
            if (!m_bLoaded) return;
            ma_sound_start(&m_sound);
        }

        void pause()
        {
            if (!m_bLoaded) return;
            ma_sound_stop(&m_sound);
        }

        void stop()
        {
            if (!m_bLoaded) return;
            ma_sound_stop(&m_sound);
            ma_sound_seek_to_pcm_frame(&m_sound, 0);
        }

        // ----------------------------------------------------------------
        // seekTo
        //
        // Seeks to 'seconds' from the start of the file.
        // Converts to PCM frames using the engine sample rate.
        //
        // Reference: ma_sound_seek_to_pcm_frame(), miniaudio.h
        // ----------------------------------------------------------------
        void seekTo(const float seconds)
        {
            if (!m_bLoaded) return;
            const ma_uint32 sampleRate = ma_engine_get_sample_rate(&m_engine);
            const auto frame      = static_cast<ma_uint64>(seconds * static_cast<float>(sampleRate));
            ma_sound_seek_to_pcm_frame(&m_sound, frame);
        }

        // ----------------------------------------------------------------
        // setLooping
        // ----------------------------------------------------------------
        void setLooping(const bool loop)
        {
            if (!m_bLoaded) return;
            ma_sound_set_looping(&m_sound, loop ? MA_TRUE : MA_FALSE);
        }

        // ----------------------------------------------------------------
        // setVolume  [0.0 .. 1.0]
        // ----------------------------------------------------------------
        void setVolume(const float volume)
        {
            if (!m_bLoaded) return;
            ma_sound_set_volume(&m_sound, volume);
        }

        [[nodiscard]] bool isLoaded()   const { return m_bLoaded; }
        [[nodiscard]] bool isPlaying()  const
        {
            if (!m_bLoaded) return false;
            return ma_sound_is_playing(&m_sound) == MA_TRUE;
        }

        [[nodiscard]] float getCursorSeconds() const
        {
            if (!m_bLoaded) return 0.0f;
            float cursor = 0.0f;
            ma_sound_get_cursor_in_seconds(&m_sound, &cursor);
            return cursor;
        }

    private:
        ma_engine m_engine{};
        ma_sound  m_sound{};

        bool m_bLoaded            = false;
        bool m_engineInitialised  = false;
    };

}  // namespace demo