#pragma once
#include "KeyCombination.h"
#include <unordered_map>
#include <LLUtils/Exception.h>

namespace OIV
{
    template <class BindingType>
    class KeyBindings
    {
    private:
        using MapCombinationToBinding = std::unordered_map<KeyCombination, BindingType, KeyCombination::Hash>;

    public:
      void AddBinding(KeyCombination combination,const BindingType& binding)
      {
          if (static_cast<KeyCode>(combination.keycode) == KeyCode::UNASSIGNED)
              LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError , "trying to add an 'Unassigned' key binding");


          auto ib = mBindings.emplace(combination, binding);
          if (ib.second == false)
              LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "duplicate entries are not allowed");
      }

      void AddBinding(ListKeyCombinations combination, const BindingType& binding)
      {
          for (const KeyCombination& comb : combination)
              AddBinding(comb, binding);
      }

      const BindingType& GetBinding(KeyCombination combination)
      {
          static BindingType empty;
          typename MapCombinationToBinding::const_iterator it = mBindings.find(combination);
          return it != mBindings.end() ? it->second : empty;
      }

    private:
        MapCombinationToBinding mBindings;

    };
}
