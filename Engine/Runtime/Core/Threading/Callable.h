#pragma once

#include "Base/Base.h"

namespace tyr
{
    struct Callable
    {
        // Task function to be executed
        void (*Execute)(void*) = nullptr;
        // Argument / data to be used in the task function
        void* Context = nullptr;
        // Optional function to delete the task function data after the task function is executed.
        void (*Destroy)(void*) = nullptr;

        Callable() = default;

        // Move constructor
        Callable(Callable&& other) noexcept
            : Execute(other.Execute),
            Context(other.Context),
            Destroy(other.Destroy)
        {
            other.Execute = nullptr;
            other.Context = nullptr;
            other.Destroy = nullptr;
        }

        // Move assignment
        Callable& operator=(Callable&& other) noexcept
        {
            if (this != &other)
            {
                // Clean up existing context if needed
                if (Destroy && Context)
                    Destroy(Context);

                Execute = other.Execute;
                Context = other.Context;
                Destroy = other.Destroy;

                other.Execute = nullptr;
                other.Context = nullptr;
                other.Destroy = nullptr;
            }
            return *this;
        }

        // No copy support
        Callable(const Callable&) = delete;
        Callable& operator=(const Callable&) = delete;

        ~Callable()
        {
            if (Destroy && Context)
                Destroy(Context);
        }

        void Invoke() const
        {
            if (Execute)
                Execute(Context);
        }

        explicit operator bool() const
        {
            return Execute != nullptr;
        }
    };
}