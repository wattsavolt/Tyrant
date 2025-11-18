#pragma once

#include "ModuleManager.h"
#include "IModule.h"

namespace tyr
{
    ModuleManager::ModuleManager()
        : m_Map(c_MaxModules)
        , m_ModulesInitialized(false)
    {

    }

    ModuleManager::~ModuleManager()
    {
        TYR_ASSERT(!m_ModulesInitialized);
        DestroyModules();
    }

    void ModuleManager::RegisterModule(Id64 id, IModule* module)
    {
        TYR_ASSERT(!m_ModulesInitialized);

        m_Map[id] = m_Modules.Size();
        m_Modules.Add(module);
    }

    const IModule* ModuleManager::GetModule(Id64 id) const
    {
        return m_Modules[*m_Map.Find(id)];
    }

    IModule* ModuleManager::GetModule(Id64 id)
    {
        return m_Modules[m_Map[id]];
    }

    void ModuleManager::InitializeModules()
    {
        TYR_ASSERT(!m_ModulesInitialized);

        for (IModule* module : m_Modules)
        {
            module->InitializeModule();
        }

        m_ModulesInitialized = true;
    }

    void ModuleManager::UpdateModules(float deltaTime)
    {
        TYR_ASSERT(m_ModulesInitialized);

        for (IModule* module : m_Modules)
        {
            module->UpdateModule(deltaTime);
        }
    }

    void ModuleManager::ShutdownModules()
    {
        TYR_ASSERT(m_ModulesInitialized);

        for (int i = m_Modules.Size() - 1; i >= 0; --i)
        {
            m_Modules[i]->ShutdownModule();
        }

        m_ModulesInitialized = false;
    }

    void ModuleManager::DestroyModules()
    {
        TYR_ASSERT(!m_ModulesInitialized);
        for (IModule* module : m_Modules)
        {
            delete module;
        }
        m_Modules.Clear();
        m_Map.Clear();
    }
}