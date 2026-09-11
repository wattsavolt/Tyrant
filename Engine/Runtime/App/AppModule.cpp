#include "AppModule.h"

#if TYR_EDITOR
#include "Editor.h"
#endif

namespace tyr
{
	typedef TYR_IMPORT tyr::AppBase* (TYR_STDCALL* CreateApp)();

	tyr::AppBase* LoadApp(const tyr::LibraryLoader& loader)
	{
		CreateApp createApp = static_cast<CreateApp>(loader.GetProcessAddress("CreateApp"));
		return createApp();
	}

	AppModule::AppModule()
	{
#if TYR_EDITOR
		m_App = new tyr::Editor();
#else
		tyr::LibraryLoader loader;
		const char* libName = TYR_TO_LITERAL(TYR_APP_LIB_NAME);
		TYR_ASSERT(loader.Load(libName));
		m_App = LoadApp(loader);
#endif
	}

	AppModule::~AppModule()
	{
		delete m_App;
#if !TYR_EDITOR
		loader.Unload();
#endif
	}

	void AppModule::Initialize()
	{
		m_App->Initialize();
	}

	void AppModule::Update(float deltaTime)
	{
		m_App->Update(deltaTime);
	}

	void AppModule::Shutdown()
	{
		m_App->Shutdown();
	}

	bool AppModule::WantsExit() const
	{
		return m_App->WantsExit();
	}
}