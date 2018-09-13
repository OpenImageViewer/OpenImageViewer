#pragma once
#include <memory>
template <class T> class Singleton
{
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

private:
    struct Context
    {
        std::unique_ptr<T> instance;

        void Create()
        {
            if (instance == nullptr)
                instance = std::make_unique<T>();
        }

        ~Context()
        {
            instance.reset();
        }
    };

    static inline Context sContext;

};         
