#pragma once
#include <functional>
#include <vector>
namespace LLUtils
{
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
			Listeners::const_iterator it = std::find(fListeners.begin(), fListeners.end(), func);
			if (it != fListeners.end())
				fListeners.erase(it);
			
		}
	private:
		using Listeners = std::vector<Func>;
		Listeners fListeners;

	};
}