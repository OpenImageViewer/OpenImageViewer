#pragma once
#include <functional>
#include <vector>
#include <algorithm>
namespace LLUtils
{

    template<typename T, typename... U>
    void* getAddress(std::function<T(U...)> f) {
        typedef T(fnType)(U...);
        fnType ** fnPointer = f.template target<fnType*>();
        return fnPointer != nullptr ? *fnPointer : nullptr;
    }

	template <class T>
	class Event
	{
	public:
		using Func = std::function<T>;

		template <class ...Args>
		void Raise(Args... args)
		{
			for (const auto& f : fListeners)
				f(args...);
		}
		
		void Add(const Func& func)
		{
			fListeners.push_back(func);
		}

		void Remove(const Func& func)
		{
            fListeners.erase(std::remove_if(fListeners.begin(), fListeners.end(), [&](const Func& elem)
            {
                return getAddress(func) == getAddress(elem);
            }));
		}

    private:
		using Listeners = std::vector<Func>;
		Listeners fListeners;
	};
}