#pragma once
#include <memory>
namespace LLUtils
{
    template <class T> class Singleton
    {
    public:
        class Context
        {
        public:
            std::unique_ptr<T> instance;

            void Create()
            {
                if (instance == nullptr)
                    instance = std::unique_ptr<T>(new T());// std::make_unique<T>();
                
            }

            ~Context()
            {
                instance.reset();
            }
        };

        
    public:
        static T& GetSingleton()
        {
            sContext.Create();
            return *sContext.instance.get();
        }

        static void DeleteSingleton()
        {
            sContext = Context();
        }

    protected:
        Singleton() = default;

    private:
        static inline Context sContext;
    };
}