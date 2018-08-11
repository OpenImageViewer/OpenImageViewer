#pragma once
#include "Point.h"
namespace LLUtils
{
    enum Corner
    {
        None,
        TopLeft,
        BottomLeft,
        TopRight,
        BottomRight
    };

    template <class T>  
    class Rect
    {
    public:
        using Point_Type = Point<T>;
        
        Rect()
        {
            
        }


        Rect(Point_Type point1, Point_Type point2)
        {
            if (point1.x > point2.x)
                std::swap(point1.x, point2.x);

            if (point1.y > point2.y)
                std::swap(point1.y, point2.y);

            p0 = point1;
            p1 = point2;
        }


        Rect Intersection(const Rect& rect)
        {
            T x0 = std::max(p0.x, rect.p0.x);
            T x1 = std::min(p1.x, rect.p1.x);
            T y0 = std::max(p0.y, rect.p0.y);
            T y1 = std::min(p1.y, rect.p1.y);

            return{ {x0,y0},{x1,y1}};
            
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

        Rect& operator +=(Point_Type translation)
        {
            p0 += translation;
            p1 += translation;
            return *this;
        }

        T GetWidth() const { return p1.x - p0.x; }
        T GetHeight() const { return p1.y - p0.y; }

        Point_Type GetCorner(const Corner corner) const
        {
            switch (corner)
            {
            case Corner::TopLeft:
                return p0;
            case Corner::BottomRight:
                return p1;
            case Corner::BottomLeft:
                return Point_Type(p0.x, p1.y);
            case Corner::TopRight:
                return Point_Type(p1.x, p0.y);
            default:
                throw std::logic_error("Unexpected or corrupted value");
                
            }
        }

        // Casting operator
        template <class BASE_TYPE>
        explicit operator Rect<BASE_TYPE>() const
        {
            using BASE_POINT_TYPE = Point<BASE_TYPE>;
            return  Rect<BASE_TYPE>(static_cast<BASE_POINT_TYPE>(p0), static_cast<BASE_POINT_TYPE>(p1));
        }
    private:

        Point_Type p0;
        Point_Type p1;
    };

    typedef Rect<int32_t> RectI32;
    typedef Rect<float>   RectF32;
    typedef Rect<double>  RectF64;
}