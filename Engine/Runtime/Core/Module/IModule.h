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
        virtual void Initialize() = 0;

        // Called before engine shutdown
        virtual void Shutdown() = 0;

        virtual void BeginFrame() {};

        // Called every frame by the engine. Should schedule async tasks for this module
        virtual void Update(float deltaTime) = 0;

        virtual void EndFrame() {};
    };
	
}