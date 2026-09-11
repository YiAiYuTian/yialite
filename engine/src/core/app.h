#ifndef YLE_APP_H
#define YLE_APP_H

#include "core/result.h"
#include "utils/containers/yia_list.h"

namespace yialite
{

class Context;
class AudioManager;
class TestEventCallback;

class App
{
    FRIEND_ALLOCATOR
public:
    ~App();

    static Result<App*> create();
    static void destroy(App* app);

    int run(int argc, char** argv);
private:
    App() noexcept = default;
private:
    bool is_running = true;
    Context* m_context = nullptr;
    AudioManager* m_audio_manager = nullptr;

    List<TestEventCallback*> m_test_callbacks;
};

}

#endif
