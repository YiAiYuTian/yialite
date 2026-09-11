#ifndef YIALITE_AUDIO_MANAGER_H
#define YIALITE_AUDIO_MANAGER_H

#include "audio_adapter.h"
#include "audio_types.h"
#include "../core/core.h"
#include "../core/result.h"

namespace yialite
{

class YIALITE_API AudioManager
{
    FRIEND_ALLOCATOR
public:
    ~AudioManager();

    //tools
    static Result<AudioManager*> create();
    static void destroy(AudioManager* am);

    void update(float dt);

    SoundID load_sound(const char* path);
    void unload_sound(SoundID id);
    void unload_all_sounds();
    bool is_sound_loaded(SoundID id) const;

    VoiceID play(SoundID id, const PlayParams& params = {});
    void stop(VoiceID voice);
    void stop_all_voices();
    void pause(VoiceID voice);
    void resume(VoiceID voice);
    bool is_voice_playing(VoiceID voice) const;

    void set_volume(VoiceID voice, float v);
    void set_pitch(VoiceID voice, float v);
    void set_pan(VoiceID voice, float v);
    void set_loop(VoiceID voice, bool loop);

    void set_spatialization(VoiceID voice, bool enabled);
    void set_voice_position(VoiceID voice, float x, float y, float z);
    void set_voice_velocity(VoiceID voice, float x, float y, float z);
    void set_voice_distance(VoiceID voice, float min_dist, float max_dist);

    void fade_out_and_stop(VoiceID voice, float seconds);

    void create_bus(const char* name);
    void set_bus_volume(const char* name, float v);
    void pause_bus(const char* name);
    void resume_bus(const char* name);
    void remove_bus(const char* name);

    void set_listener(const ListenerState& state);
    void set_doppler_factor(float factor);
    void set_speed_of_sound(float speed);

    size_t get_sound_count() const;
    size_t get_voice_count() const;

    explicit operator bool() const noexcept { return m_adapter != nullptr; }
private:
    AudioManager() noexcept = default;
    AudioManager(AudioManager&&) = delete;
    AudioManager(const AudioManager&) = delete;

    //operators
    AudioManager& operator=(AudioManager&&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
private:
    IAudioAdapter* m_adapter = nullptr;
};

}

#endif
