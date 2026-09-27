#include "AppModule.h"
#include "Config/CommandLine.h"

#if TYR_EDITOR
#include "Editor.h"
#include "GUI/GUIModule.h"
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
		if (CommandLine::Instance().HasFlag("editor"))
		{
			GUIModule* guiModule;
			TYR_GET_MODULE(GUIModule, guiModule);
			m_App = new tyr::Editor(*guiModule);
			return;
		}
#endif
		const char* libName = TYR_TO_LITERAL(TYR_APP_LIB_NAME);
		TYR_ASSERT(m_Loader.Load(libName));
		m_App = LoadApp(m_Loader);
	}

	AppModule::~AppModule()
	{
		delete m_App;
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