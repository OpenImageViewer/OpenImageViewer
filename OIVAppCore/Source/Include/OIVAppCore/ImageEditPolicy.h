#pragma once

#include <OIVAppCore/ViewerPresentationPolicy.h>

namespace OIV
{
    class ImageEditPolicy
    {
      public:
        static OperationResult ValidateSelectionOperation(bool imageOpen, bool selectionEmpty);
    };
}  // namespace OIV
