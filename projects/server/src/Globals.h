#pragma once

#include <functional>
#include <optional>
#include <unordered_map>

class Globals
{
public:
    template <class T>
    static T* Get()
    {
        return GetOrSet<T>();
    }

    template <class T>
    static T* Set(T* val, std::optional<std::function<void(void* ptr)>> destructor = std::nullopt)
    {
        return GetOrSet(val, std::move(destructor));
    }

    static void Destroy()
    {
        for(const auto& [ptr, destructor] : s_destructors)
        {
            destructor(ptr);
        }
    }

private:
    static std::unordered_map<void*, std::function<void(void* ptr)>> s_destructors;
    
    template <class T>
    static T* GetOrSet(T* val = nullptr, std::optional<std::function<void(void* ptr)>> destructor = std::nullopt)
    {
        static T* ptr = nullptr;
        if (val)
        {
            ptr = val;
            
            if (destructor.has_value())
            {
                // use provided destructor
                s_destructors.insert(std::make_pair(val, std::move(destructor.value())));
            }
            else
            {
                // default destructor
                s_destructors.insert(std::make_pair(val, [](void* p)
                {
                    delete static_cast<T*>(p);
                }));
            }
        }

        return ptr;
    }
};
