#pragma once
#include "Exception.h"
#include <cassert>
namespace LLUtils
{
    template <typename T, typename Container = std::set<T>>
    class UniqueIdProvider
    {
    private:
        //Note: Do not change the order of the members deceleration
        // if its inevitable keep 'fFreeIds' prior to 'fFreeIdsEnd'

        Container fFreeIds;
        const typename Container::const_iterator fFreeIdsEnd;
        const T fStartID;
        T fNextId;

    public:
        UniqueIdProvider(const T startID = 0) : fNextId(startID), fStartID(startID), fFreeIdsEnd(fFreeIds.end())
        {

        }

        const T Acquire()
        {
            T result;
            if (fFreeIds.empty())
            {
                result = fNextId++;
            }
            else
            {
                typename Container::iterator it = fFreeIds.begin();
                result = *it;
                fFreeIds.erase(it);
            }
            return result;
        }

        static constexpr bool IsVector = std::is_same_v<typename Container, typename std::vector<T>>;
        
        /*
        template <typename = typename std::enable_if_t<IsVector>>
        void Release(const T id)
        {
            ThrowException("Trying to release an id that has never been acquired", id < fNextId && id >= fStartID);
            assert("id already released" &&  (std::find(fFreeIds.begin(), fFreeIds.end(),id) == fFreeIds.end())    );
            fFreeIds.push_back(id);
        }
        */

        template <typename = typename std::enable_if_t<!IsVector>>
        void Release(const T id)
        {
            ThrowException("Trying to release an id that has never been acquired", id < fNextId && id >= fStartID);
            auto result = fFreeIds.insert(id);
            ThrowException("id already released", result.second);
        }
        
        void Normalize()
        {
            Container::const_iterator it;
            while ((it = fFreeIds.find(fNextId - 1)) != mFreeIdsEnd)
            {
                fFreeIds.erase(it);
                fNextId--;
            }
        }

        void ThrowException(const std::string& message, const bool condition) const
        {
            if (condition == false)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, message);

        }
    };
}