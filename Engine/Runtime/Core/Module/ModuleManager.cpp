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

    void ModuleManager::AddModuleForUpdate(Id64 id)
    {
        TYR_ASSERT(!m_ModulesInitialized);

        const uint* index = m_Map.Find(id);
        TYR_ASSERT(index);
        if (!index)
        {
            return;
        }
        IModule* module = m_Modules[*index];
        TYR_ASSERT(!m_UpdateOrder.Contains(module));
        if (!m_UpdateOrder.Contains(module))
        {
            m_UpdateOrder.Add(module);
        }
    }

    void ModuleManager::InitializeModules()
    {
        TYR_ASSERT(!m_ModulesInitialized);
        // A module left out of the update order would never update.
        TYR_ASSERT(m_UpdateOrder.Size() == m_Modules.Size());

        for (IModule* module : m_Modules)
        {
            module->Initialize();
        }

        m_ModulesInitialized = true;
    }

    void ModuleManager::BeginFrame()
    {
        TYR_ASSERT(m_ModulesInitialized);

        for (IModule* module : m_UpdateOrder)
        {
            module->BeginFrame();
        }
    }

    void ModuleManager::Update(float deltaTime)
    {
        TYR_ASSERT(m_ModulesInitialized);

        // Okay to update linearly as each module should declare tasks and have a completion task that is waited on by modules that depend on it
        for (IModule* module : m_UpdateOrder)
        {
            module->Update(deltaTime);
        }
    }

    void ModuleManager::EndFrame()
    {
        TYR_ASSERT(m_ModulesInitialized);

        for (IModule* module : m_UpdateOrder)
        {
            module->EndFrame();
        }
    }

    void ModuleManager::ShutdownModules()
    {
        TYR_ASSERT(m_ModulesInitialized);

        for (int i = m_Modules.Size() - 1; i >= 0; --i)
        {
            m_Modules[i]->Shutdown();
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
        m_UpdateOrder.Clear();
        m_Map.Clear();
    }
}