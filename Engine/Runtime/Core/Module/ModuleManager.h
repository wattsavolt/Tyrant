#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Identifiers/Identifiers.h"
#include "Containers/LocalArray.h"
#include "Containers/HashMap.h"
#include "Reflection/ReflectionUtil.h"

namespace tyr
{
	class IModule;
	class TYR_CORE_API ModuleManager final : public INonCopyable
	{
	public:
		static constexpr uint8 c_MaxModules = 12;

		static ModuleManager& Instance()
		{
			static ModuleManager manager;
			return manager;
		}

		ModuleManager();
		~ModuleManager();

		// Modules are initialized in the order they're registered, and shut down the other way round.
		void RegisterModule(Id64 id, IModule* module);

		// Adds a registered module to the frame's update order. BeginFrame, Update and EndFrame each
		// run over every module in the order they're added, and every module is added once.
		void AddModuleForUpdate(Id64 id);

		const IModule* GetModule(Id64 id) const;

		IModule* GetModule(Id64 id);

		void InitializeModules();

		void BeginFrame();

		void Update(float deltaTime);

		void EndFrame();

		void ShutdownModules();

		void DestroyModules();

	private:
		LocalArray<IModule*, c_MaxModules> m_Modules;
		LocalArray<IModule*, c_MaxModules> m_UpdateOrder;
		HashMap<Id64, uint> m_Map;
		bool m_ModulesInitialized;
	};

#define TYR_REGISTER_MODULE(type)	{ \
										const Id64 typeID = GetTypeID<type>(); \
										ModuleManager::Instance().RegisterModule(typeID, new type()); \
									}

#define TYR_ADD_MODULE_FOR_UPDATE(type)	{ \
										const Id64 typeID = GetTypeID<type>(); \
										ModuleManager::Instance().AddModuleForUpdate(typeID); \
									}

#define TYR_GET_MODULE(type, var)	{ \
										const Id64 typeID = GetTypeID<type>(); \
										var = static_cast<type*>(ModuleManager::Instance().GetModule(typeID)); \
									} 

}