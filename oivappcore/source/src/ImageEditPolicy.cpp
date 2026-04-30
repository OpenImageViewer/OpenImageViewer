#include <oivappcore/ImageEditPolicy.h>

namespace OIV
{
    OperationResult ImageEditPolicy::ValidateSelectionOperation(bool imageOpen, bool selectionEmpty)
    {
        if (imageOpen == false)
            return OperationResult::NoDataFound;
        if (selectionEmpty)
            return OperationResult::NoSelection;

        return OperationResult::Success;
    }
}  // namespace OIV
