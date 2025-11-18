#pragma once

#include "Base/base.h"
#include "Base/INonCopyable.h"

namespace tyr
{
    class IModule : public INonCopyable
    {
    public:
        virtual ~IModule() = default;

        // Called when the engine is booting up
        virtual void InitializeModule() = 0;

        // Called before engine shutdown
        virtual void ShutdownModule() = 0;

        // Called every frame by the engine
        virtual void UpdateModule(float deltaTime) = 0;
    };
	
}