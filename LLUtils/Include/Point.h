#pragma once
#include "Utility.h"

namespace LLUtils
{
    typedef uint8_t SignType;
    template <class POINT_TYPE>
    class Point
    {
        
    public:
        static const Point<POINT_TYPE> Zero;
        
        POINT_TYPE x = 0;
        POINT_TYPE y = 0;

        uint32_t abs(const uint32_t val) const
        {
            return static_cast<uint32_t>(std::abs(static_cast<long const>(val)));
        }

        float abs(const float val) const
        {
            return static_cast<float>(std::abs(static_cast<long const>(val)));
        }

        Point() {}
        Point(POINT_TYPE aX, POINT_TYPE aY)
        {
            x = aX;
            y = aY;
        }

        bool operator==( const Point& rhs) const
        {
            return x == rhs.x && y == rhs.y;
        }

        bool operator!=(const Point& rhs) const
        {
            return (*this == rhs) == false;
        }

        Point operator-(const Point& rhs) const
        {
            return{ x - rhs.x , y - rhs.y };
        }
        Point operator+(const Point& rhs) const
        {
            return Point ( x + rhs.x , y  + rhs.y );
        }

        Point Abs() const
        {
            using namespace std;
            return Point(abs(x), abs(y));
        }

        Point Sqrt() const
        {
            return{static_cast<POINT_TYPE>(std::sqrt(x)), static_cast<POINT_TYPE>(std::sqrt(y)) };
        }

        Point operator*(const Point& point ) const
        {
            return Point(x * point.x, y * point.y);
        }

        template <class T>
        Point& operator*=(T scalar)
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        template <class T>
        Point& operator+=(T scalar)
        {
            x += scalar;
            y += scalar;
            return *this;
        }

        template <class T>
        Point operator*(const T& scalar) const
        {
            return Point(x * scalar, y * scalar);
        }

        Point& operator*=(Point rhs)
        {
            x *= rhs.x;
            y *= rhs.y;
            return *this;
        }

        Point& operator+=(Point rhs)
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        Point& operator-=(Point rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }
        
        template <class BASE_TYPE>
        explicit operator Point<BASE_TYPE>() const
        {
            return Point<BASE_TYPE>(static_cast<BASE_TYPE>(x), static_cast<BASE_TYPE>(y));
        }

        Point Sign() const
        {
            return Point(Utility::Sign(x), Utility::Sign(y));
        }


#ifdef _WIN32
        Point(const POINT& rhs)
        {
            x = rhs.x;
            y = rhs.y;
        }
#endif

    };
    template <class POINT_TYPE>
    const Point<POINT_TYPE>  Point<POINT_TYPE>::Zero = Point<POINT_TYPE>(0, 0);

    typedef Point<int32_t> PointI32;
    typedef Point<float> PointF32;
    typedef Point<double> PointF64;
}

/*class TestPoint
{
public:
bool RunTest()
{
PointI32 pointI32;

if (pointI32.IsZero() == false)
return false;

pointI32.x = 3;

pointI32 += 5.0;

if (pointI32.x != 3 || pointI32.x != 8)
return false;

POINTF

}
};*/
