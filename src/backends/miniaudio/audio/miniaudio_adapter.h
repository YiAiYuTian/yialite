#ifndef YIALITE_MINIAUDIO_ADAPTER_H
#define YIALITE_MINIAUDIO_ADAPTER_H

#include "../../../audio/audio_adapter.h"
#include "../../../audio/audio_types.h"
#include "../../../utils/containers/yia_hashmap.h"
#include "../../../utils/containers/yia_list.h"
#include "../../../utils/string/yia_string.h"
#include "../../../core/result.h"

#include <miniaudio.h>
#include <miniaudio_libvorbis.h>

namespace yialite
{

class MiniaudioAdapter : public IAudioAdapter
{
public:
    MiniaudioAdapter() noexcept;
    ~MiniaudioAdapter() override = default;

    Result<void> init() override;
    void destroy() override;
    
    void update(float dt) override;

    SoundID load_sound(const char* path) override;
    void unload_sound(SoundID id) override;
    void unload_all_sounds() override;
    bool is_sound_loaded(SoundID id) const override;

    VoiceID play(SoundID id, const PlayParams& params) override;
    void stop(VoiceID voice) override;
    void stop_all_voices() override;
    void pause(VoiceID voice) override;
    void resume(VoiceID voice) override;
    bool is_voice_playing(VoiceID voice) const override;

    void set_volume(VoiceID voice, float v) override;
    void set_pitch(VoiceID voice, float v) override;
    void set_pan(VoiceID voice, float v) override;
    void set_loop(VoiceID voice, bool loop) override;

    void set_spatialization(VoiceID voice, bool enabled) override;
    void set_voice_position(VoiceID voice, float x, float y, float z) override;
    void set_voice_velocity(VoiceID voice, float x, float y, float z) override;
    void set_voice_distance(VoiceID voice, float min_dist, float max_dist) override;

    void fade_out_and_stop(VoiceID voice, float seconds) override;

    void create_bus(const char* name) override;
    void set_bus_volume(const char* name, float v) override;
    void pause_bus(const char* name) override;
    void resume_bus(const char* name) override;
    void remove_bus(const char* name) override;

    void set_listener(const ListenerState& state) override;
    void set_doppler_factor(float factor) override;
    void set_speed_of_sound(float speed) override;

    size_t get_sound_count() const override;
    size_t get_voice_count() const override;

private:
    struct VoiceData
    {
        ma_sound* sound      = nullptr;
        SoundID  source_id   = INVALID_SOUND_ID;
        bool     paused      = false;
        float    fade_timer  = -1.0f;
        bool     active      = false;
        Uint64   gen         = 0;
    };
private:
    ma_sound_group* find_bus(const char* name);
    VoiceData* find_voice(VoiceID id);
    const VoiceData* find_voice(VoiceID id) const;
    void release_voice(size_t index);
private:
    ma_engine           m_engine;
    ma_resource_manager m_resource_manager;

    HashMap<SoundID, String>         m_sound_paths;
    List<VoiceData>                  m_voices;
    List<Uint64>                     m_free_indices;
    HashMap<String, ma_sound_group*> m_buses;

    SoundID m_next_sound_id = INVALID_SOUND_ID;
    float   m_doppler_factor = 1.0f;
    bool    m_inited = false;
};

}

#endif
