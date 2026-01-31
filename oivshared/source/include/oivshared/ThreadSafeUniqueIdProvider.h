#pragma once
#include <set>
#include <mutex>

template <typename T>
class ThreadSafeUniqueIdProvider
{
public:
    explicit ThreadSafeUniqueIdProvider(T start = 0)
        : fStartId(start), fNextId(start)
    {}

    T Acquire()
    {
        std::lock_guard lock(fMutex);

        if (!fFreeIds.empty())
        {
            auto it = fFreeIds.begin();
            T id = *it;
            fFreeIds.erase(it);
            return id;
        }
        return fNextId++;
    }

    void Release(T id)
    {
        std::lock_guard lock(fMutex);
        fFreeIds.insert(id);
    }

private:
    T fStartId{};
    T fNextId{};
    std::set<T> fFreeIds;
    std::mutex fMutex;
};