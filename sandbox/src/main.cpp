#include "core/app.h"
#include "core/log.h"
#include "utils/memory/allocator.h"

#if defined(_DEBUG) && defined(_WIN32)
	#include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
#if defined(_DEBUG) && defined(_WIN32)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
	auto app_result = yialite::App::create();
	if(!app_result)
	{
		log(yialite::LogLevel::Err, "Failed to create app");
		return -1;
	}
	yialite::App* app = app_result.value();
	app->run(argc, argv);
	yialite::App::destroy(app);

    return 0;
}
