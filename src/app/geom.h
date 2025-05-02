#pragma once

#include <compare>

#include "assets.h"

namespace geom {

struct Vec2D {
    Vec2D() = default;
    Vec2D(app::Dimension x, app::Dimension y) : x(x), y(y) {}

    Vec2D& operator*=(app::Dimension scale) {
        x *= scale;
        y *= scale;
        return *this;
    }

    auto operator<=>(const Vec2D&) const = default;

    app::Dimension x = 0;
    app::Dimension y = 0;
};

inline Vec2D operator*(Vec2D lhs, app::Dimension rhs) { return lhs *= rhs; }

inline Vec2D operator*(app::Dimension lhs, Vec2D rhs) { return rhs *= lhs; }

struct Point2D {
    Point2D() = default;
    Point2D(app::Dimension x, app::Dimension y) : x(x), y(y) {}

    Point2D& operator+=(const Vec2D& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    auto operator<=>(const Point2D&) const = default;

    app::Dimension x = 0;
    app::Dimension y = 0;
};

inline Point2D operator+(Point2D lhs, const Vec2D& rhs) { return lhs += rhs; }

inline Point2D operator+(const Vec2D& lhs, Point2D rhs) { return rhs += lhs; }

}  // namespace geom
