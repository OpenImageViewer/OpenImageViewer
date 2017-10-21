#pragma once
#include <windows.h>
#include "KeyCombination.h"
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
          if (combination.keycode == KC_UNASSIGNED)
              throw std::logic_error("trying to add an 'Unassigned' key binding");


          auto ib = mBindings.insert(MapCombinationToBinding::value_type(combination, binding));
          if (ib.second == false)
              throw std::logic_error("Duplicate entries are not allowed");

      }

      void AddBinding(ListKeyCombinations combination, const BindingType& binding)
      {
          for (const KeyCombination& comb : combination)
              AddBinding(comb, binding);
      }

      const BindingType& GetBinding(KeyCombination combination)
      {
          static BindingType empty;
          MapCombinationToBinding::const_iterator it = mBindings.find(combination);
          return it != mBindings.end() ? it->second : empty;
      }

    private:
        MapCombinationToBinding mBindings;

    };
}
