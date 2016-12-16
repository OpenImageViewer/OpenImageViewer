#pragma once
#include "Vector2.h"
namespace OIV
{
    class ViewParameters
    {
    public:
        Vector2 uvscale;
        Vector2 uvOffset;
        Vector2 uViewportSize;
        bool showGrid;
    };
}
