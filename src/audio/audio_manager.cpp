#include "pch.h"
#include "audio_manager.h"
#include "../backends/miniaudio/audio/miniaudio_adapter.h"
#include "../core/error.h"
#include "../utils/memory/allocator.h"

namespace yialite
{

Result<AudioManager*> AudioManager::create()
{
    AudioManager* am = alloc_obj<AudioManager>();
    if (!am) return Result<AudioManager*>(ErrorCode::OutOfMemory, "Failed to allocate AudioManager");

    IAudioAdapter* adapter = alloc_obj<MiniaudioAdapter>();
    if (!adapter)
    {
        dealloc_obj(am);
        return Result<AudioManager*>(ErrorCode::OutOfMemory, "Failed to allocate MiniaudioAdapter");
    }

    auto init_result = adapter->init();
    if (!init_result)
    {
        dealloc_obj(adapter);
        dealloc_obj(am);
        return Result<AudioManager*>(init_result.error());
    }

    am->m_adapter = adapter;
    return am;
}

AudioManager::~AudioManager()
{
    if (m_adapter)
    {
        m_adapter->destroy();
        dealloc_obj(m_adapter);
        m_adapter = nullptr;
    }
}

void AudioManager::destroy(AudioManager* am)
{
    dealloc_obj(am);
}

void AudioManager::update(float dt)
{
    if (m_adapter) m_adapter->update(dt);
}

SoundID AudioManager::load_sound(const char* path)
{
    return m_adapter ? m_adapter->load_sound(path) : INVALID_SOUND_ID;
}

void AudioManager::unload_sound(SoundID id)
{
    if (m_adapter) m_adapter->unload_sound(id);
}

void AudioManager::unload_all_sounds()
{
    if (m_adapter) m_adapter->unload_all_sounds();
}

bool AudioManager::is_sound_loaded(SoundID id) const
{
    return m_adapter ? m_adapter->is_sound_loaded(id) : false;
}

VoiceID AudioManager::play(SoundID id, const PlayParams& params)
{
    return m_adapter ? m_adapter->play(id, params) : INVALID_VOICE_ID;
}

void AudioManager::stop(VoiceID voice)
{
    if (m_adapter) m_adapter->stop(voice);
}

void AudioManager::stop_all_voices()
{
    if (m_adapter) m_adapter->stop_all_voices();
}

void AudioManager::pause(VoiceID voice)
{
    if (m_adapter) m_adapter->pause(voice);
}

void AudioManager::resume(VoiceID voice)
{
    if (m_adapter) m_adapter->resume(voice);
}

bool AudioManager::is_voice_playing(VoiceID voice) const
{
    return m_adapter ? m_adapter->is_voice_playing(voice) : false;
}

void AudioManager::set_volume(VoiceID voice, float v)
{
    if (m_adapter) m_adapter->set_volume(voice, v);
}

void AudioManager::set_pitch(VoiceID voice, float v)
{
    if (m_adapter) m_adapter->set_pitch(voice, v);
}

void AudioManager::set_pan(VoiceID voice, float v)
{
    if (m_adapter) m_adapter->set_pan(voice, v);
}

void AudioManager::set_loop(VoiceID voice, bool loop)
{
    if (m_adapter) m_adapter->set_loop(voice, loop);
}

void AudioManager::set_spatialization(VoiceID voice, bool enabled)
{
    if (m_adapter) m_adapter->set_spatialization(voice, enabled);
}

void AudioManager::set_voice_position(VoiceID voice, float x, float y, float z)
{
    if (m_adapter) m_adapter->set_voice_position(voice, x, y, z);
}

void AudioManager::set_voice_velocity(VoiceID voice, float x, float y, float z)
{
    if (m_adapter) m_adapter->set_voice_velocity(voice, x, y, z);
}

void AudioManager::set_voice_distance(VoiceID voice, float min_dist, float max_dist)
{
    if (m_adapter) m_adapter->set_voice_distance(voice, min_dist, max_dist);
}

void AudioManager::fade_out_and_stop(VoiceID voice, float seconds)
{
    if (m_adapter) m_adapter->fade_out_and_stop(voice, seconds);
}

void AudioManager::create_bus(const char* name)
{
    if (m_adapter) m_adapter->create_bus(name);
}

void AudioManager::set_bus_volume(const char* name, float v)
{
    if (m_adapter) m_adapter->set_bus_volume(name, v);
}

void AudioManager::pause_bus(const char* name)
{
    if (m_adapter) m_adapter->pause_bus(name);
}

void AudioManager::resume_bus(const char* name)
{
    if (m_adapter) m_adapter->resume_bus(name);
}

void AudioManager::remove_bus(const char* name)
{
    if (m_adapter) m_adapter->remove_bus(name);
}

void AudioManager::set_listener(const ListenerState& state)
{
    if (m_adapter) m_adapter->set_listener(state);
}

void AudioManager::set_doppler_factor(float factor)
{
    if (m_adapter) m_adapter->set_doppler_factor(factor);
}

void AudioManager::set_speed_of_sound(float speed)
{
    if (m_adapter) m_adapter->set_speed_of_sound(speed);
}

size_t AudioManager::get_sound_count() const
{
    return m_adapter ? m_adapter->get_sound_count() : 0;
}

size_t AudioManager::get_voice_count() const
{
    return m_adapter ? m_adapter->get_voice_count() : 0;
}

}
