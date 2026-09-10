#include "app.h"

#include "yialite.h"
#include "../thirdparty/imgui/imgui.h"

#include <chrono>

class FrameTimer
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    FrameTimer()
    {
        m_last = Clock::now();
    }

    float tick()
    {
        TimePoint now = Clock::now();
        std::chrono::duration<float> delta = now - m_last;
        m_dt = delta.count();
        m_last = now;
        return m_dt;
    }
    float get_dt() const { return m_dt; }
private:
    TimePoint m_last;
    float m_dt = 0.0f;
};

namespace yialite
{

WindowManager* g_win_mgr = nullptr;
EventManager*  g_evt_mgr = nullptr;
AudioManager*  g_ado_mgr = nullptr;

ListenerState ls;

Vector2f voice_pos = { 300.0f, 400.0f };

#define SPEED 200.0f

static void update_listener(float dt)
{
	// if(is_key_pressed(Scancode::A)) ls.x -= SPEED * dt;
	// if(is_key_pressed(Scancode::D)) ls.x += SPEED * dt;
	// if(is_key_pressed(Scancode::W)) ls.y -= SPEED * dt;
	// if(is_key_pressed(Scancode::S)) ls.y += SPEED * dt;
	// if(is_key_pressed(Scancode::Q)) ls.z -= SPEED * dt;
	// if(is_key_pressed(Scancode::E)) ls.z += SPEED * dt;
}

int g_counter = 1;

class TestEventCallback : public EventListener
{
public:
	TestEventCallback() : EventListener(g_evt_mgr) 
	{
		subscribe_auto(
			[](const KeyEvent& e)
			{
				if(e.key == Keycode::F && e.down)
				{
					log(LogLevel::Info, "F key: {}", g_counter++);
					e.consumed = true;
				}
			}
		);
	}
	~TestEventCallback() override = default;
};

class NewEvent : public EventBase<NewEvent>
{
public:
	int counter = 1;
};

class MemTestClass
{
public:
	MemTestClass() = default;
	MemTestClass(int a, int b, int c) : a(a), b(b), c(c) {}

	int a = 1;
	int b = 2;
	int c = 3;
};

App::~App()
{
	AudioManager::destroy(m_audio_manager);
    Context::destroy(m_context);
}

Result<App*> App::create()
{
	App* app = nullptr;
	app = ALLOCATE_OBJECT(App);

    ContextConfig context_config;
    context_config.window_config.title = "YiaLiteTest";
    context_config.window_config.width = 1280;
    context_config.window_config.height = 720;
    context_config.window_config.flags = detail::to_window_flags_(WindowFlags::Resizable);
    context_config.enable_devui = true;

	auto ctx_result = Context::create(context_config);
	if(!ctx_result)
	{
		log(LogLevel::Err, "Failed to create context");
		return Result<App*>(ErrorCode::InitFailed);
	}
    app->m_context = ctx_result.value();

	g_win_mgr = app->m_context->win_mgr;
	g_evt_mgr = app->m_context->evt_mgr;

    set_logger_enabled(true);
    set_log_level(LogLevel::Trace);

	g_evt_mgr->subscribe([app](const QuitEvent& e){ app->is_running = false; });
	g_evt_mgr->subscribe(
		[app](const KeyEvent& e)
		{
			auto& window = *g_win_mgr->get_window(e.win_id);

			int w = window.get_width();
			int h = window.get_height();
			if (e.key == Keycode::ESCAPE && !e.down) app->is_running = false;
			else if (e.key == Keycode::X && e.down)
			{
				NewEvent e;
				e.counter = 10;
				g_evt_mgr->publish(e);
			}
			else if(e.key == Keycode::A && e.down)
			{
				w += 10;
				h += 10;
				window.set_width(w);
				window.set_height(h);
			}
			else if(e.key == Keycode::D && e.down)
			{
				w -= 10;
				h -= 10;
				window.set_width(w);
				window.set_height(h);
			}
			else if(e.key == Keycode::W && e.down)
			{
				WindowConfig config;
				config.title = "New Window";
				config.width = 800;
				config.height = 600;
				config.flags = detail::to_window_flags_(WindowFlags::Resizable);
				g_win_mgr->create_window(config);
			}
			else if(e.key == Keycode::Q && e.down)
			{
				auto* test = ALLOCATE_OBJECT(TestEventCallback);
				app->m_test_callbacks.emplace_back(test);
			}
			else if(e.key == Keycode::E && e.down)
			{
				auto* test = app->m_test_callbacks.back();
				app->m_test_callbacks.pop_back();
				DEALLOCATE_OBJECT(test);
			}
			else if (e.key == Keycode::P && e.down)
			{
				constexpr yialite::DialogFileFilter file_filters[] = {
					{ "PNG images",  "png" },
					{ "JPEG images", "jpg;jpeg" },
					{ "All images",  "png;jpg;jpeg" },
					{ "All files",   "*" }
				};
				constexpr int filter_count = sizeof(file_filters) / sizeof(file_filters[0]);
				g_win_mgr->get_window(e.win_id)->show_open_file_dialog(
					[](const char* const* files, int filter_idx)
					{
						if (!files || !files[0]) return;
						for (int i = 0; files[i]; i++)
						{
						}
					}, file_filters, filter_count
				);
			}
		}
	);
	g_evt_mgr->subscribe(
		[app](const MouseButtonEvent& e)
		{
			if(e.btn == MouseButton::MIDDLE && !e.down) app->is_running = false;
		}
	);
	g_evt_mgr->subscribe(
		[app](const WindowCloseRequestedEvent& e)
		{
			if(e.win_id == g_win_mgr->get_first_window()->get_id())
			{
				app->is_running = false;
				return;
			}
			g_win_mgr->destroy_window(e.win_id);
		}
	);
	g_evt_mgr->subscribe(
		[](const NewEvent& e)
		{
			log(LogLevel::Info, "new event: {}", e.counter);
		}
	);
	g_evt_mgr->callback_once(
		[](const WindowResizeEvent& e)
		{
			log(LogLevel::Trace, "w: {}, h: {}", e.w, e.h);
		}
	);

    log(LogLevel::Info, "App initialize successful");

	auto am_result = AudioManager::create();
	if(!am_result)
	{
		log(LogLevel::Err, "Failed to create audio manager");
		return Result<App*>(ErrorCode::InitFailed);
	}
	app->m_audio_manager = am_result.value();
	g_ado_mgr = app->m_audio_manager;
	
	auto id = g_ado_mgr->load_sound(R"(D:\VScodeProject\CppProject\SomeSmallTests\hurry_up_and_run.ogg)");
	PlayParams pm;
	pm.loop = true;
	pm.volume = 1.0f;
	pm.pitch = 1.0f;
	pm.pan = 0.0f;
	pm.spatial = true;
	pm.x = voice_pos.x;
	pm.y = voice_pos.y;
	pm.z = 0.0f;
	pm.min_distance = 100.0f;
	pm.max_distance = 500.0f;
	g_ado_mgr->play(id, pm);

	ls.x = 100.0f;
	ls.y = 100.0f;
	ls.z = 0.0f;
	g_ado_mgr->set_listener(ls);

	Allocator::print_all_memory_info();
	log(LogLevel::Trace, "All: {}", Allocator::get_alloc_size());
	log(LogLevel::Trace, "Requested: {}", Allocator::get_alloc_requested_size());

	// int* raw_array = static_cast<int*>(allocate_raw(sizeof(int) * 10));
	// int* int_array = allocate_array<int>(10);
	// MemTestClass* class_array = allocate_array<MemTestClass>(10);
	// MemTestClass* mem_test_class = allocate<MemTestClass>(std::source_location::current(), 4, 5, 6);
	// MemTestClass* mem_test_class_2 = ALLOCATE_OBJECT(MemTestClass);
	// MemTestClass* mem_test_class_3 = ALLOCATE_OBJECT(MemTestClass, 4, 5, 6);
	// MemTestClass* mem_test_class_array_2 = ALLOCATE_ARRAY(MemTestClass, 10);

	// log(LogLevel::Info, "mem_test_class_2 a: {}", mem_test_class_2->a);
	// log(LogLevel::Info, "mem_test_class_2 b: {}", mem_test_class_2->b);
	// log(LogLevel::Info, "mem_test_class_2 c: {}", mem_test_class_2->c);
	// log(LogLevel::Info, "===================================");
	// log(LogLevel::Info, "mem_test_class_3 a: {}", mem_test_class_3->a);
	// log(LogLevel::Info, "mem_test_class_3 b: {}", mem_test_class_3->b);
	// log(LogLevel::Info, "mem_test_class_3 c: {}", mem_test_class_3->c);
	// log(LogLevel::Info, "===================================");

	// for (int i = 0; i < 10; i++)
	// {
	// 	log(LogLevel::Info, "mem_test_class_array_2 a: {}", mem_test_class_array_2[i].a);
	// 	log(LogLevel::Info, "mem_test_class_array_2 b: {}", mem_test_class_array_2[i].b);
	// 	log(LogLevel::Info, "mem_test_class_array_2 c: {}", mem_test_class_array_2[i].c);
	// 	log(LogLevel::Info, "===================================");
	// }

	// for (int i = 0; i < 10; i++)
	// {
	// 	log(LogLevel::Info, "raw_array: {}", raw_array[i]);
	// 	log(LogLevel::Info, "int_array: {}", int_array[i]);
	// 	log(LogLevel::Info, "class_array a: {}", class_array[i].a);
	// 	log(LogLevel::Info, "class_array b: {}", class_array[i].b);
	// 	log(LogLevel::Info, "class_array c: {}", class_array[i].c);
	// 	log(LogLevel::Info, "===================================");
	// }
	// log(LogLevel::Info, "mem_test_class a: {}", mem_test_class->a);
	// log(LogLevel::Info, "mem_test_class b: {}", mem_test_class->b);
	// log(LogLevel::Info, "mem_test_class c: {}", mem_test_class->c);

	// deallocate_raw(raw_array);
	// deallocate_array(int_array);
	// deallocate_array(class_array);
	// deallocate(mem_test_class);
	// DEALLOCATE_OBJECT(mem_test_class_2);
	// DEALLOCATE_OBJECT(mem_test_class_3);
	// DEALLOCATE_ARRAY(mem_test_class_array_2);

	return Result<App*>(app);
}

void App::destroy(App *app)
{
	DEALLOCATE_OBJECT(app);
}

int App::run(int argc, char** argv)
{
	FrameTimer ft;
    while (is_running)
    {
		float dt = ft.tick();

        g_evt_mgr->poll_event();
		g_ado_mgr->update(dt);
        m_context->devui->on_update();

		update_listener(dt);
		g_ado_mgr->set_listener(ls);
        
        ImGui::ShowDemoWindow(nullptr);
        ImGui::ShowMetricsWindow(nullptr);

		// if(is_mouse_button_pressed(MouseButtonFlags::LMASK))
		// {
		// 	log(LogLevel::Trace, "Mouse pressed");
		// }
		// if(is_key_pressed(Scancode::A))
		// {
		// 	log(LogLevel::Trace, "A pressed");
		// }

        m_context->renderer2d->begin_draw_f(FCOLOR_GRAY);

        //render
		// Vector2f ls_pos = { ls.x, ls.y };
		// m_context->renderer2d->draw_line_f(voice_pos, ls_pos, FCOLOR_RED);
		// log(LogLevel::Trace, "distance: {}", distance(voice_pos, ls_pos));

        m_context->devui->on_render();
        m_context->renderer2d->end_draw();
    }

    return 0;
}

}