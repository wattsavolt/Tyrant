#pragma once

#include "Base/Base.h"

namespace tyr
{
    struct Callable
    {
        // Task function to be executed
        void (*Execute)(void*) = nullptr;
        // Argument / data to be used in the task function
        void* context = nullptr;
        // Optional function to delete the context after the task function is executed.
        void (*Destroy)(void*) = nullptr;

        Callable() = default;

        // Move constructor
        Callable(Callable&& other) noexcept
            : Execute(other.Execute),
            context(other.context),
            Destroy(other.Destroy)
        {
            other.Execute = nullptr;
            other.context = nullptr;
            other.Destroy = nullptr;
        }

        // Move assignment
        Callable& operator=(Callable&& other) noexcept
        {
            if (this != &other)
            {
                // Clean up existing context if needed
                if (Destroy && context)
                    Destroy(context);

                Execute = other.Execute;
                context = other.context;
                Destroy = other.Destroy;

                other.Execute = nullptr;
                other.context = nullptr;
                other.Destroy = nullptr;
            }
            return *this;
        }

        // No copy support
        Callable(const Callable&) = delete;
        Callable& operator=(const Callable&) = delete;

        ~Callable()
        {
            if (Destroy && context)
                Destroy(context);
        }

        void Invoke() const
        {
            if (Execute)
                Execute(context);
        }

        explicit operator bool() const
        {
            return Execute != nullptr;
        }
    };
}