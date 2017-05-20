#pragma once
#include "Point.h"
namespace LLUtils
{
    template <class T>  
    struct Rect
    {
        Rect Intersection(const Rect& rect)
        {
            T x0 = std::max(p0.x, rect.p0.x);
            T x1 = std::min(p1.x, rect.p1.x);
            T y0 = std::max(p0.y, rect.p0.y);
            T y1 = std::min(p1.y, rect.p1.y);

            return{ {x0,y0},{x1,y1}};
            
        }
        bool IsValid() const
        {
            return GetWidth() >= 0 && GetHeight() >= 0;
        }

        bool IsNonNegative() const
        {
            return p0.x >= 0 && p1.x >= 0 && p0.y >= 0 && p1.y >= 0;
        }
        bool IsInside(const Rect& rect) const
        {
            return
                p0.x >= rect.p0.x
                && p1.x <= rect.p1.x
                && p0.y >= rect.p0.y
                && p1.y <= rect.p1.y;

        }

        int32_t GetWidth() const { return p1.x - p0.x; }
        int32_t GetHeight() const { return p1.y - p0.y; }

        Point<T> p0;
        Point<T> p1;
    };

    typedef Rect<int32_t> RectI32;
    typedef Rect<float>   RectF32;
    typedef Rect<double>  RectF64;
}