#pragma once

#include <algorithm>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace OIV
{
    enum class ResidencyState
    {
        NotResident,
        Pending,
        Resident,
        Failed
    };

    template <typename KeyType, typename ValueType, typename KeyCompare = std::less<KeyType>>
    class ResidentCache
    {
        static_assert(std::tuple_size_v<KeyType> >= 1, "ResidentCache expects a tuple-like key.");

        struct ResidencyEntry
        {
            ResidencyState fState = ResidencyState::NotResident;
            ValueType fValue{};
        };

        template <typename Iterator>
        struct ResidencyRange
        {
            Iterator fBegin;
            Iterator fEnd;

            Iterator begin() const { return fBegin; }
            Iterator end() const { return fEnd; }
            bool empty() const { return fBegin == fEnd; }
        };

      public:

        using Key              = KeyType;
        using Value            = ValueType;
        using ResidencyStorage = std::map<Key, ResidencyEntry, KeyCompare>;

        class IRequestProcessor
        {
          public:

            virtual ~IRequestProcessor()                                          = default;
            virtual bool ProcessResidencyRequest(const Key& key, Value& outValue) = 0;
        };
        ResidentCache(IRequestProcessor* processor) { fProcessor = processor; }

        void RequestResidency(const Key& key)
        {
            ResidencyEntry& entry = fStorage[key];
            if (entry.fState == ResidencyState::Resident || entry.fState == ResidencyState::Pending)
                return;

            SetPending(key);

            ValueType value{};
            if (fProcessor->ProcessResidencyRequest(key, value))
            {
                SetResident(key, std::move(value));
            }
            else
            {
                SetFailure(key);
            }
        }

        void SetPending(const Key& key)
        {
            auto& entry  = fStorage[key];
            entry.fState = ResidencyState::Pending;
            entry.fValue = ValueType{};
        }

        bool HasResident(const Key& key) const
        {
            const ResidencyEntry* entry = FindEntry(key);
            return entry != nullptr && entry->fState == ResidencyState::Resident && HasValue(entry->fValue);
        }

        ResidencyState GetResidencyState(const Key& key) const
        {
            const ResidencyEntry* entry = FindEntry(key);
            return entry != nullptr ? entry->fState : ResidencyState::NotResident;
        }

        ValueType GetResident(const Key& key) const
        {
            const ResidencyEntry* entry = FindEntry(key);
            return (entry != nullptr && entry->fState == ResidencyState::Resident) ? entry->fValue : ValueType{};
        }

        template <typename... PrefixTypes>
        auto GetView(const PrefixTypes&... prefix) -> ResidencyRange<typename ResidencyStorage::iterator>
        {
            return GetViewImpl(fStorage, prefix...);
        }

        template <typename... PrefixTypes>
        auto GetView(const PrefixTypes&... prefix) const -> ResidencyRange<typename ResidencyStorage::const_iterator>
        {
            return GetViewImpl(fStorage, prefix...);
        }

        template <typename... PrefixTypes>
        auto GetUniqueValues(const PrefixTypes&... prefix) const
            -> std::vector<std::tuple_element_t<sizeof...(PrefixTypes), Key>>
        {
            constexpr std::size_t prefixLength = sizeof...(PrefixTypes);
            static_assert(prefixLength < std::tuple_size_v<Key>,
                          "Unique values request must leave at least one remaining key element.");

            auto range       = GetView(prefix...);
            using ResultType = std::tuple_element_t<prefixLength, Key>;
            std::vector<ResultType> result;
            for (auto it = range.begin(); it != range.end(); ++it)
            {
                const auto& value = std::get<prefixLength>(it->first);
                if (std::find(result.begin(), result.end(), value) == result.end())
                    result.push_back(value);
            }
            return result;
        }

        void Clear(const Key& key) { fStorage.erase(key); }

        void SetResident(const Key& key, ValueType value)
        {
            SetResidentEntry(fStorage[key], std::move(value));
        }

        void SetFailure(const Key& key)
        {
            SetFailureEntry(fStorage[key]);
        }

        template <typename... PrefixTypes>
        void ClearPrefix(const PrefixTypes&... prefix)
        {
            auto range = GetView(prefix...);
            fStorage.erase(range.begin(), range.end());
        }

        void Clear() { fStorage.clear(); }

      private:

        ResidencyEntry* FindEntry(const Key& key)
        {
            auto storageIt = fStorage.find(key);
            return storageIt == fStorage.end() ? nullptr : &storageIt->second;
        }

        const ResidencyEntry* FindEntry(const Key& key) const
        {
            auto storageIt = fStorage.find(key);
            return storageIt == fStorage.end() ? nullptr : &storageIt->second;
        }

        static bool HasValue(const ValueType& value)
        {
            if constexpr (std::is_convertible_v<ValueType, bool>)
                return static_cast<bool>(value);
            else
                return true;
        }

        template <typename StorageType, typename... PrefixTypes>
        static auto GetViewImpl(StorageType& storage, const PrefixTypes&... prefix)
        {
            constexpr std::size_t prefixLength = sizeof...(PrefixTypes);
            static_assert(prefixLength <= std::tuple_size_v<Key>, "Prefix length exceeds the number of key elements.");

            using Indices = std::make_index_sequence<prefixLength>;
            static_assert(PrefixTypesMatch<PrefixTypes...>(Indices{}),
                          "Prefix types must match the leading key element types.");

            auto prefixTuple = std::tuple<std::decay_t<PrefixTypes>...>(prefix...);
            return BuildRange(storage, prefixTuple, Indices{});
        }

        template <typename... PrefixTypes, std::size_t... Indices>
        static constexpr bool PrefixTypesMatch(std::index_sequence<Indices...>)
        {
            return ((std::is_same_v<std::tuple_element_t<Indices, Key>, std::decay_t<PrefixTypes>>) && ...);
        }

        template <typename StorageType, typename PrefixTuple, std::size_t... Indices>
        static auto BuildRange(StorageType& storage, const PrefixTuple& prefix, std::index_sequence<Indices...>)
            -> ResidencyRange<decltype(storage.begin())>
        {
            using Iterator = decltype(storage.begin());

            if constexpr (sizeof...(Indices) == 0)
            {
                return ResidencyRange<Iterator>{storage.begin(), storage.end()};
            }
            else
            {
                auto matches = [&](const Key& key)
                { return ((std::get<Indices>(key) == std::get<Indices>(prefix)) && ...); };

                Iterator first = std::find_if(storage.begin(), storage.end(),
                                              [&](const auto& pair) { return matches(pair.first); });

                if (first == storage.end())
                    return ResidencyRange<Iterator>{first, first};

                Iterator last = first;
                ++last;
                while (last != storage.end() && matches(last->first))
                    ++last;

                return ResidencyRange<Iterator>{first, last};
            }
        }

      private:

        ResidencyStorage fStorage;
        IRequestProcessor* fProcessor = nullptr;

        void SetResidentEntry(ResidencyEntry& entry, ValueType value)
        {
            entry.fValue = std::move(value);
            entry.fState = HasValue(entry.fValue) ? ResidencyState::Resident : ResidencyState::NotResident;
        }

        void SetFailureEntry(ResidencyEntry& entry)
        {
            entry.fValue = ValueType{};
            entry.fState = ResidencyState::Failed;
        }
    };
}  // namespace OIV
