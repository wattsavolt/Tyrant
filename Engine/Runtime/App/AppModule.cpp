#include "AppModule.h"
#include "Config/CommandLine.h"

#if TYR_EDITOR
#include "Editor.h"
#include "GUI/GUIModule.h"
#endif

namespace tyr
{
	typedef TYR_IMPORT tyr::AppBase* (TYR_STDCALL* CreateApp)(bool);

	tyr::AppBase* LoadApp(const tyr::LibraryLoader& loader, bool embedded)
	{
		CreateApp createApp = static_cast<CreateApp>(loader.GetProcessAddress("CreateApp"));
		return createApp(embedded);
	}

	AppModule::AppModule()
	{
		const char* libName = TYR_TO_LITERAL(TYR_APP_LIB_NAME);
		const bool loaded = m_Loader.Load(libName);
		TYR_ASSERT(loaded);

#if TYR_EDITOR
		if (CommandLine::Instance().HasFlag("editor"))
		{
			GUIModule* guiModule;
			TYR_GET_MODULE(GUIModule, guiModule);
			m_EmbeddedApp = LoadApp(m_Loader, true);
			m_App = new tyr::Editor(*guiModule, *m_EmbeddedApp);
			return;
		}
#endif
		m_App = LoadApp(m_Loader, false);
	}

	AppModule::~AppModule()
	{
		delete m_App;
		delete m_EmbeddedApp;
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
