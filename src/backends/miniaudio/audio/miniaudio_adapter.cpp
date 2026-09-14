#include "pch.h"
#include "miniaudio_adapter.h"
#include "../../../core/error.h"
#include "../../../core/logger.h"
#include "../../../utils/utility.h"
#include "../../../utils/memory/allocator.h"

namespace yialite
{

static void* miniaudio_malloc(size_t sz, void* p_user_data)
{
    (void)p_user_data;
    return alloc_raw(sz);
}

static void* miniaudio_realloc(void* p, size_t sz, void* p_user_data)
{
    (void)p_user_data;
    return realloc_raw(p, sz);
}

static void miniaudio_free(void* p, void* p_user_data)
{
    (void)p_user_data;
    dealloc_raw(p);
}

MiniaudioAdapter::MiniaudioAdapter() noexcept
{
    memset(&m_engine, 0, sizeof(m_engine));
    memset(&m_resource_manager, 0, sizeof(m_resource_manager));
}

Result<void> MiniaudioAdapter::init()
{
    if (m_inited) return ok();

    ma_result result;
    ma_resource_manager_config rmc;
    ma_engine_config ec;

    ma_decoding_backend_vtable* p_custom_backend_vtable[] = {
        ma_decoding_backend_libvorbis,
        nullptr
    };

    rmc = ma_resource_manager_config_init();
    rmc.ppCustomDecodingBackendVTables = p_custom_backend_vtable;
    rmc.customDecodingBackendCount     = sizeof(p_custom_backend_vtable) / sizeof(p_custom_backend_vtable[0]);
    rmc.pCustomDecodingBackendUserData = nullptr;

    result = ma_resource_manager_init(&rmc, &m_resource_manager);
    if (result != MA_SUCCESS) return Result<void>(ErrorCode::AudioEngineInitFailed, ma_result_description(result));

    ec = ma_engine_config_init();
    ec.pResourceManager = &m_resource_manager;
    ec.allocationCallbacks.onMalloc   = miniaudio_malloc;
    ec.allocationCallbacks.onRealloc  = miniaudio_realloc;
    ec.allocationCallbacks.onFree     = miniaudio_free;

    result = ma_engine_init(&ec, &m_engine);
    if (result != MA_SUCCESS)
    {
        ma_resource_manager_uninit(&m_resource_manager);
        return Result<void>(ErrorCode::AudioEngineInitFailed, ma_result_description(result));
    }

    m_inited = true;
    Logger::info("MiniaudioAdapter initialized successfully");
    return ok();
}

void MiniaudioAdapter::destroy()
{
    if (m_inited)
    {
        stop_all_voices();
        unload_all_sounds();
        for (auto& pair : m_buses)
        {
            ma_sound_group_uninit(pair.second);
            dealloc_obj(pair.second);
        }
        m_buses.clear();
        ma_engine_uninit(&m_engine);
        ma_resource_manager_uninit(&m_resource_manager);
        m_inited = false;
    }
}

void MiniaudioAdapter::update(float dt)
{
    if (!m_inited) return;

    size_t voice_count = m_voices.size();
    for (size_t i = 0; i < voice_count; ++i)
    {
        VoiceData& vd = m_voices[i];
        if (!vd.active) continue;

        bool should_release = false;
        if (vd.fade_timer > 0.0f)
        {
            vd.fade_timer -= dt;
            if (vd.fade_timer <= 0.0f) should_release = true;
        }

        if (!should_release && !vd.paused && !ma_sound_is_playing(vd.sound)) should_release = true;

        if(should_release) release_voice(i);
    }

    if (need_shrink(m_voices))
    {
        m_voices.shrink_to_fit();
        voice_count = m_voices.size();
        
        List<Uint64> valid_free;
        valid_free.reserve(m_free_indices.size());
        for (auto idx : m_free_indices)
        {
            if (idx < voice_count) valid_free.push_back(idx);
        }
        m_free_indices.swap(valid_free);
    }
}

SoundID MiniaudioAdapter::load_sound(const char* path)
{
    if (!path) return INVALID_SOUND_ID;
    
    SoundID id = ++m_next_sound_id;
    m_sound_paths.emplace(std::move(id), String(path));
    return id;
}

void MiniaudioAdapter::unload_sound(SoundID id)
{
    m_sound_paths.remove(id);
}

void MiniaudioAdapter::unload_all_sounds()
{
    m_sound_paths.clear();
}

bool MiniaudioAdapter::is_sound_loaded(SoundID id) const
{
    return m_sound_paths.contains_key(id);
}

VoiceID MiniaudioAdapter::play(SoundID id, const PlayParams& params)
{
    String* path = m_sound_paths.find(id);
    if (!path) return INVALID_VOICE_ID;

    ma_sound_group* group = find_bus(params.bus);

    ma_sound* snd = alloc_obj<ma_sound>();
    ma_result result = ma_sound_init_from_file(
        &m_engine, path->c_str(), 0, group, nullptr, snd
    );
    if (result != MA_SUCCESS)
    {
        dealloc_obj(snd);
        Logger::error("Failed to play sound (id={}): {}", id, ma_result_description(result));
        return INVALID_VOICE_ID;
    }

    if (params.loop)
        ma_sound_set_looping(snd, MA_TRUE);

    float vol = std::clamp(params.volume, 0.0f, 1.0f);
    ma_sound_set_volume(snd, vol);
    ma_sound_set_pitch(snd, params.pitch);
    ma_sound_set_pan(snd, std::clamp(params.pan, -1.0f, 1.0f));

    if (params.spatial)
    {
        ma_sound_set_spatialization_enabled(snd, MA_TRUE);
        
        ma_sound_set_attenuation_model(snd, ma_attenuation_model_linear);

        ma_sound_set_position(snd, params.x, params.y, params.z);
        ma_sound_set_min_distance(snd, params.min_distance);
        ma_sound_set_max_distance(snd, params.max_distance);
        ma_sound_set_doppler_factor(snd, m_doppler_factor);
    }
    else
    {
        ma_sound_set_spatialization_enabled(snd, MA_FALSE);
    }

    if (params.fade_in_time > 0.0f)
    {
        ma_sound_set_fade_in_milliseconds(
            snd, 0.0f, vol,
            static_cast<ma_uint64>(params.fade_in_time * 1000.0f)
        );
    }

    ma_sound_start(snd);

    Uint64 idx;
    if (!m_free_indices.empty())
    {
        idx = m_free_indices.back();
        m_free_indices.pop_back();
        m_voices[idx] = VoiceData{};
    }
    else
    {
        m_voices.emplace_back(VoiceData{});
        idx = m_voices.size() - 1;
    }

    VoiceData& vd = m_voices[idx];
    vd.sound     = snd;
    vd.source_id = id;
    vd.paused    = false;
    vd.fade_timer = -1.0f;
    vd.active    = true;

    return VoiceID{ idx, vd.gen };
}

void MiniaudioAdapter::stop(VoiceID voice)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    release_voice(voice.index);
}

void MiniaudioAdapter::stop_all_voices()
{
    for (size_t i = 0; i < m_voices.size(); ++i)
    {
        if (m_voices[i].active) release_voice(i);
    }
}

void MiniaudioAdapter::pause(VoiceID voice)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_stop(vd->sound);
    vd->paused = true;
}

void MiniaudioAdapter::resume(VoiceID voice)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_start(vd->sound);
    vd->paused = false;
}

bool MiniaudioAdapter::is_voice_playing(VoiceID voice) const
{
    const VoiceData* vd = find_voice(voice);
    if (!vd) return false;

    return ma_sound_is_playing(vd->sound);
}

void MiniaudioAdapter::set_volume(VoiceID voice, float v)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_volume(vd->sound, std::clamp(v, 0.0f, 1.0f));
}

void MiniaudioAdapter::set_pitch(VoiceID voice, float v)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_pitch(vd->sound, v);
}

void MiniaudioAdapter::set_pan(VoiceID voice, float v)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_pan(vd->sound, std::clamp(v, -1.0f, 1.0f));
}

void MiniaudioAdapter::set_loop(VoiceID voice, bool loop)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_looping(vd->sound, loop ? MA_TRUE : MA_FALSE);
}

void MiniaudioAdapter::set_spatialization(VoiceID voice, bool enabled)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_spatialization_enabled(vd->sound, enabled ? MA_TRUE : MA_FALSE);
}

void MiniaudioAdapter::set_voice_position(VoiceID voice, float x, float y, float z)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_position(vd->sound, x, y, z);
}

void MiniaudioAdapter::set_voice_velocity(VoiceID voice, float x, float y, float z)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_velocity(vd->sound, x, y, z);
}

void MiniaudioAdapter::set_voice_distance(VoiceID voice, float min_dist, float max_dist)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_min_distance(vd->sound, min_dist);
    ma_sound_set_max_distance(vd->sound, max_dist);
}

void MiniaudioAdapter::fade_out_and_stop(VoiceID voice, float seconds)
{
    VoiceData* vd = find_voice(voice);
    if (!vd) return;
    
    ma_sound_set_fade_in_milliseconds(
        vd->sound, -1.0f, 0.0f,
        static_cast<ma_uint32>(seconds * 1000.0f)
    );
    vd->fade_timer = seconds;
}

void MiniaudioAdapter::create_bus(const char* name)
{
    if (!name) return;
    String key(name);
    if (m_buses.find(key)) return;

    ma_sound_group* group = alloc_obj<ma_sound_group>();
    ma_result result = ma_sound_group_init(&m_engine, 0, nullptr, group);
    if (result != MA_SUCCESS)
    {
        dealloc_obj(group);
        Logger::error("Failed to create bus '{}'", name);
        return;
    }
    m_buses.emplace(std::move(key), std::move(group));
}

void MiniaudioAdapter::set_bus_volume(const char* name, float v)
{
    ma_sound_group* group = find_bus(name);
    if (group) ma_sound_group_set_volume(group, std::clamp(v, 0.0f, 1.0f));
}

void MiniaudioAdapter::pause_bus(const char* name)
{
    ma_sound_group* group = find_bus(name);
    if (group) ma_sound_group_stop(group);
}

void MiniaudioAdapter::resume_bus(const char* name)
{
    ma_sound_group* group = find_bus(name);
    if (group) ma_sound_group_start(group);
}

void MiniaudioAdapter::remove_bus(const char* name)
{
    if (!name) return;
    String key(name);
    ma_sound_group** group = m_buses.find(key);
    if (!group) return;
    
    ma_sound_group_uninit(*group);
    dealloc_obj(*group);
    m_buses.remove(key);
}

void MiniaudioAdapter::set_listener(const ListenerState& state)
{
    ma_engine_listener_set_position (&m_engine, 0, state.x, state.y, state.z);
    ma_engine_listener_set_direction(&m_engine, 0, state.forward_x, state.forward_y, state.forward_z);
    ma_engine_listener_set_world_up (&m_engine, 0, state.up_x, state.up_y, state.up_z);
    ma_engine_listener_set_velocity (&m_engine, 0, state.velocity_x, state.velocity_y, state.velocity_z);
}

void MiniaudioAdapter::set_doppler_factor(float factor)
{
    m_doppler_factor = factor;
    for (size_t i = 0; i < m_voices.size(); ++i)
    {
        if (m_voices[i].active) ma_sound_set_doppler_factor(m_voices[i].sound, factor);
    }
}

void MiniaudioAdapter::set_speed_of_sound(float speed)
{
    ma_spatializer_listener_set_speed_of_sound(&m_engine.listeners[0], speed);
}

size_t MiniaudioAdapter::get_sound_count() const
{
    return m_sound_paths.size();
}

size_t MiniaudioAdapter::get_voice_count() const
{
    size_t count = 0;
    for (size_t i = 0; i < m_voices.size(); ++i)
    {
        if (m_voices[i].active) ++count;
    }
    return count;
}

ma_sound_group* MiniaudioAdapter::find_bus(const char* name)
{
    if (!name || std::strcmp(name, "master") == 0) return nullptr;
    ma_sound_group** group = m_buses.find(String(name));
    return group ? *group : nullptr;
}

MiniaudioAdapter::VoiceData* MiniaudioAdapter::find_voice(VoiceID id)
{
    if (id == INVALID_VOICE_ID) return nullptr;

    size_t idx = id.index;
    if (idx >= m_voices.size()) return nullptr;

    auto& vd = m_voices[idx];
    if (!vd.active || vd.gen != id.gen) return nullptr;
    return &vd;
}

const MiniaudioAdapter::VoiceData* MiniaudioAdapter::find_voice(VoiceID id) const
{
    if (id == INVALID_VOICE_ID) return nullptr;

    size_t idx = id.index;
    if (idx >= m_voices.size()) return nullptr;

    auto& vd = m_voices[idx];
    if (!vd.active || vd.gen != id.gen) return nullptr;
    return &vd;
}

void MiniaudioAdapter::release_voice(size_t index)
{
    VoiceData& vd = m_voices[index];
    if (!vd.active) return;
    
    ma_sound_uninit(vd.sound);
    dealloc_obj(vd.sound);
    vd.sound     = nullptr;
    vd.active    = false;
    vd.paused    = false;
    vd.fade_timer = -1.0f;
    vd.gen++;
    
    m_free_indices.push_back(index);
}

}
