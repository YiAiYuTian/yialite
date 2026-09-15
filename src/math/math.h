// Matrices and vectors use column-major order.

#ifndef YIALITE_MATH_H
#define YIALITE_MATH_H

#ifdef YIALITE_MATH_STANDALONE
    #include <cassert>
    #include <cstdint>
    
    typedef std::uint8_t Uint8;

    #ifdef NDEBUG
        #define YIALITE_ASSERT(expression) assert(expression)
    #else
        #define YIALITE_ASSERT(expression) ((void)0)
    #endif
#else
    #include "../core/core.h"
    #include "../utils/base_types.h"
#endif

#include <cmath>
#include <algorithm>
#include <limits>
#include <type_traits>

#if defined(YIALITE_MATH_OPENGL)
    #define YIALITE_CLIP_RH
    #define YIALITE_CLIP_ZO
#elif defined(YIALITE_MATH_VULKAN) || defined(YIALITE_MATH_DIRECTX)
    #define YIALITE_CLIP_LH
    #define YIALITE_CLIP_ZO
#endif

namespace yialite
{

constexpr int TOLERANCE_SCALE = 10;

//================================================================================
//vector2
template<typename T, typename Derived>
struct Vector2Base
{
    [[nodiscard]] constexpr Derived operator-() const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);
        return Derived(-self[0], -self[1]);
    }
    
    [[nodiscard]] constexpr Derived operator+(const Derived &other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] + other[0],
            self[1] + other[1]
        );
    }

    [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] - other[0],
            self[1] - other[1]
        );
    }

    [[nodiscard]] constexpr Derived operator*(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] * scalar,
            self[1] * scalar
        );
    }

    [[nodiscard]] constexpr Derived operator/(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector2 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector2 division by zero!");
        }

        T inv = static_cast<T>(1) / scalar;

        return Derived(
            self[0] * inv,
            self[1] * inv
        );
    }

    constexpr Derived& operator+=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] += other[0];
        self[1] += other[1];

        return self;
    }

    constexpr Derived& operator-=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] -= other[0];
        self[1] -= other[1];

        return self;
    }

    constexpr Derived& operator*=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] *= scalar;
        self[1] *= scalar;

        return self;
    }

    constexpr Derived& operator/=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector2 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector2 division by zero!");
        }
        
        T inv = static_cast<T>(1) / scalar;
        self[0] *= inv;
        self[1] *= inv;

        return self;
    }

    constexpr T* get_data() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* get_data() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    
    constexpr T* begin() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* begin() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    constexpr T* end() noexcept { return begin() + 2; }
    constexpr const T* end() const noexcept { return begin() + 2; }
};

template <typename T, typename Derived>
[[nodiscard]] constexpr Derived operator*(T scalar, const Vector2Base<T, Derived>& vec) noexcept
{
    return static_cast<const Derived&>(vec) * scalar;
}

struct Vector2f : public Vector2Base<float, Vector2f>
{
    union
    {
        float data[2];
        struct{ float x, y; };
    };

    constexpr Vector2f() : x(0.0f), y(0.0f) {}
    constexpr explicit Vector2f(float v) : x(v), y(v) {}
    constexpr Vector2f(float x_, float y_) : x(x_), y(y_) {}

    constexpr float& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 2 && "Vector2f index out of bounds!");
        return data[i];
    }
    constexpr const float& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 2 && "Vector2f index out of bounds!");
        return data[i];
    }
};

struct Vector2i : public Vector2Base<int, Vector2i>
{
    union
    {
        int data[2];
        struct{ int x, y; };
    };

    constexpr Vector2i() : x(0), y(0) {}
    constexpr explicit Vector2i(int v) : x(v), y(v) {}
    constexpr Vector2i(int x_, int y_) : x(x_), y(y_) {}

    constexpr int& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 2 && "Vector2i index out of bounds!");
        return data[i]; 
    }
    constexpr const int& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 2 && "Vector2i index out of bounds!");
        return data[i];
    }
};

//================================================================================
//vector3
template<typename T, typename Derived>
struct Vector3Base
{
    [[nodiscard]] constexpr Derived operator-() const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);
        return Derived(-self[0], -self[1], -self[2]);
    }
    
    [[nodiscard]] constexpr Derived operator+(const Derived &other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] + other[0],
            self[1] + other[1],
            self[2] + other[2]
        );
    }

    [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] - other[0],
            self[1] - other[1],
            self[2] - other[2]
        );
    }

    [[nodiscard]] constexpr Derived operator*(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] * scalar,
            self[1] * scalar,
            self[2] * scalar
        );
    }

    [[nodiscard]] constexpr Derived operator/(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector3 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector3 division by zero!");
        }

        T inv = static_cast<T>(1) / scalar;

        return Derived(
            self[0] * inv,
            self[1] * inv,
            self[2] * inv
        );
    }

    constexpr Derived& operator+=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] += other[0];
        self[1] += other[1];
        self[2] += other[2];

        return self;
    }

    constexpr Derived& operator-=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] -= other[0];
        self[1] -= other[1];
        self[2] -= other[2];

        return self;
    }

    constexpr Derived& operator*=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] *= scalar;
        self[1] *= scalar;
        self[2] *= scalar;

        return self;
    }

    constexpr Derived& operator/=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector3 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector3 division by zero!");
        }
        
        T inv = static_cast<T>(1) / scalar;
        self[0] *= inv;
        self[1] *= inv;
        self[2] *= inv;

        return self;
    }

    constexpr T* get_data() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* get_data() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    
    constexpr T* begin() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* begin() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    constexpr T* end() noexcept { return begin() + 3; }
    constexpr const T* end() const noexcept { return begin() + 3; }
};

template <typename T, typename Derived>
[[nodiscard]] constexpr Derived operator*(T scalar, const Vector3Base<T, Derived>& vec) noexcept
{
    return static_cast<const Derived&>(vec) * scalar;
}

struct Vector3f : public Vector3Base<float, Vector3f>
{
    union
    {
        float data[3];
        struct{ float x, y, z; };
    };

    constexpr Vector3f() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr explicit Vector3f(float v) : x(v), y(v), z(v) {}
    constexpr Vector3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vector2f toVec2f() const noexcept { return Vector2f(x, y); }

    constexpr float& operator[](int i) noexcept
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Vector3f index out of bounds!");
        return data[i];
    }
    constexpr const float& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Vector3f index out of bounds!");
        return data[i];
    }
};

struct Vector3i : public Vector3Base<int, Vector3i>
{
    union
    {
        int data[3];
        struct{ int x, y, z; };
    };

    constexpr Vector3i() : x(0), y(0), z(0) {}
    constexpr explicit Vector3i(int v) : x(v), y(v), z(v) {}
    constexpr Vector3i(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    constexpr int& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Vector3i index out of bounds!");
        return data[i];
    }
    constexpr const int& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Vector3i index out of bounds!");
        return data[i];
    }
};

//================================================================================
//vector4 tag
struct Vector4Tag {};
struct RectTag {};
struct ColorTag {};

//vector4
template<typename T, typename Derived, typename Tag = Vector4Tag>
struct Vector4Base
{
    [[nodiscard]] constexpr Derived operator-() const noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<const Derived&>(*this);
        return Derived(-self[0], -self[1], -self[2], -self[3]);
    }
    
    [[nodiscard]] constexpr Derived operator+(const Derived &other) const noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] + other[0],
            self[1] + other[1],
            self[2] + other[2],
            self[3] + other[3]
        );
    }

    [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] - other[0],
            self[1] - other[1],
            self[2] - other[2],
            self[3] - other[3]
        );
    }

    [[nodiscard]] constexpr Derived operator*(T scalar) const noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0] * scalar,
            self[1] * scalar,
            self[2] * scalar,
            self[3] * scalar
        );
    }

    [[nodiscard]] constexpr Derived operator/(T scalar) const noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<const Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector4 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector4 division by zero!");
        }

        T inv = static_cast<T>(1) / scalar;

        return Derived(
            self[0] * inv,
            self[1] * inv,
            self[2] * inv,
            self[3] * inv
        );
    }

    constexpr Derived& operator+=(const Derived& other) noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] += other[0];
        self[1] += other[1];
        self[2] += other[2];
        self[3] += other[3];

        return self;
    }

    constexpr Derived& operator-=(const Derived& other) noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] -= other[0];
        self[1] -= other[1];
        self[2] -= other[2];
        self[3] -= other[3];

        return self;
    }

    constexpr Derived& operator*=(T scalar) noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<Derived&>(*this);
        self[0] *= scalar;
        self[1] *= scalar;
        self[2] *= scalar;
        self[3] *= scalar;

        return self;
    }

    constexpr Derived& operator/=(T scalar) noexcept
    requires std::is_same_v<Tag, Vector4Tag>
    {
        auto& self = static_cast<Derived&>(*this);

        if constexpr (std::is_floating_point_v<T>) {
            YIALITE_ASSERT(std::abs(scalar) > std::numeric_limits<T>::epsilon() * TOLERANCE_SCALE && "Vector4 division by zero!");
        } else {
            YIALITE_ASSERT(scalar != 0 && "Vector4 division by zero!");
        }
        
        T inv = static_cast<T>(1) / scalar;
        self[0] *= inv;
        self[1] *= inv;
        self[2] *= inv;
        self[3] *= inv;

        return self;
    }

    constexpr T* get_data() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* get_data() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    
    constexpr T* begin() noexcept { return &static_cast<Derived&>(*this)[0]; }
    constexpr const T* begin() const noexcept { return &static_cast<const Derived&>(*this)[0]; }
    constexpr T* end() noexcept { return begin() + 4; }
    constexpr const T* end() const noexcept { return begin() + 4; }
};

template <typename T, typename Derived, typename Tag = Vector4Tag>
[[nodiscard]] constexpr Derived operator*(T scalar, const Vector4Base<T, Derived, Tag>& vec) noexcept
{
    return static_cast<const Derived&>(vec) * scalar;
}

struct alignas(16) Vector4f : public Vector4Base<float, Vector4f>
{
    union
    {
        float data[4];
        struct{ float x, y, z, w; };
    };

    constexpr Vector4f() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr explicit Vector4f(float v) : x(v), y(v), z(v), w(v) {}
    constexpr Vector4f(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    constexpr Vector3f toVec3f() const noexcept { return Vector3f(x, y, z); }

    constexpr float& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Vector4f index out of bounds!");
        return data[i];
    }
    constexpr const float& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Vector4f index out of bounds!");
        return data[i];
    }
};

struct alignas(16) Vector4i : public Vector4Base<int, Vector4i>
{
    union
    {
        int data[4];
        struct{ int x, y, z, w; };
    };

    constexpr Vector4i() : x(0), y(0), z(0), w(0) {}
    constexpr explicit Vector4i(int v) : x(v), y(v), z(v), w(v) {}
    constexpr Vector4i(int x_, int y_, int z_, int w_) : x(x_), y(y_), z(z_), w(w_) {}

    constexpr int& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Vector4i index out of bounds!");
        return data[i];
    }
    constexpr const int& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Vector4i index out of bounds!");
        return data[i];
    }
};

struct alignas(16) Rect : public Vector4Base<int, Rect, RectTag>
{
    union
    {
        int data[4];
        struct{ int x, y, w, h; };
    };

    constexpr Rect() : x(0), y(0), w(0), h(0) {}
    constexpr explicit Rect(int v) : x(v), y(v), w(v), h(v) {}
    constexpr Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}

    constexpr int& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Rect index out of bounds!");
        return data[i];
    }
    constexpr const int& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Rect index out of bounds!");
        return data[i];
    }
};

struct alignas(16) FRect : public Vector4Base<float, FRect, RectTag>
{
    union
    {
        float data[4];
        struct{ float x, y, w, h; };
    };

    constexpr FRect() : x(0.0f), y(0.0f), w(0.0f), h(0.0f) {}
    constexpr explicit FRect(float v) : x(v), y(v), w(v), h(v) {}
    constexpr FRect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    constexpr float& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "FRect index out of bounds!");
        return data[i];
    }
    constexpr const float& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "FRect index out of bounds!");
        return data[i];
    }
};

struct Color : public Vector4Base<Uint8, Color, ColorTag>
{
    union
    {
        Uint8 data[4];
        struct{ Uint8 r, g, b, a; };
    };

    constexpr Color() : r(0), g(0), b(0), a(0) {}
    constexpr explicit Color(Uint8 v) : r(v), g(v), b(v), a(v) {}
    constexpr Color(Uint8 r_, Uint8 g_, Uint8 b_, Uint8 a_) : r(r_), g(g_), b(b_), a(a_) {}

    constexpr Uint8& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Color index out of bounds!");
        return data[i];
    }
    constexpr const Uint8& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Color index out of bounds!");
        return data[i];
    }
};

struct alignas(16) FColor : public Vector4Base<float, FColor, ColorTag>
{
    union
    {
        float data[4];
        struct{ float r, g, b, a; };
    };

    constexpr FColor() : r(0.0f), g(0.0f), b(0.0f), a(0.0f) {}
    constexpr explicit FColor(float v) : r(v), g(v), b(v), a(v) {}
    constexpr FColor(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {}

    constexpr float& operator[](int i) noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "FColor index out of bounds!");
        return data[i];
    }
    constexpr const float& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "FColor index out of bounds!");
        return data[i];
    }
};

//================================================================================
//mat3
template<typename T, typename Derived>
struct Matrix3Base
{
    [[nodiscard]] constexpr Derived operator-() const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);
        return Derived(
            -self[0][0], -self[0][1], -self[0][2],
            -self[1][0], -self[1][1], -self[1][2],
            -self[2][0], -self[2][1], -self[2][2]
        );
    }

    [[nodiscard]] constexpr Derived operator+(const Derived &other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] + other[0][0], self[0][1] + other[0][1], self[0][2] + other[0][2],
            self[1][0] + other[1][0], self[1][1] + other[1][1], self[1][2] + other[1][2],
            self[2][0] + other[2][0], self[2][1] + other[2][1], self[2][2] + other[2][2]
        );
    }

    [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] - other[0][0], self[0][1] - other[0][1], self[0][2] - other[0][2],
            self[1][0] - other[1][0], self[1][1] - other[1][1], self[1][2] - other[1][2],
            self[2][0] - other[2][0], self[2][1] - other[2][1], self[2][2] - other[2][2]
        );
    }

    [[nodiscard]] constexpr Derived operator*(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] * scalar, self[0][1] * scalar, self[0][2] * scalar,
            self[1][0] * scalar, self[1][1] * scalar, self[1][2] * scalar,
            self[2][0] * scalar, self[2][1] * scalar, self[2][2] * scalar
        );
    }

    [[nodiscard]] constexpr Derived operator*(const Derived& other) const noexcept
    {
        const auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] * other[0][0] + self[1][0] * other[0][1] + self[2][0] * other[0][2],
            self[0][1] * other[0][0] + self[1][1] * other[0][1] + self[2][1] * other[0][2],
            self[0][2] * other[0][0] + self[1][2] * other[0][1] + self[2][2] * other[0][2],

            self[0][0] * other[1][0] + self[1][0] * other[1][1] + self[2][0] * other[1][2],
            self[0][1] * other[1][0] + self[1][1] * other[1][1] + self[2][1] * other[1][2],
            self[0][2] * other[1][0] + self[1][2] * other[1][1] + self[2][2] * other[1][2],

            self[0][0] * other[2][0] + self[1][0] * other[2][1] + self[2][0] * other[2][2],
            self[0][1] * other[2][0] + self[1][1] * other[2][1] + self[2][1] * other[2][2],
            self[0][2] * other[2][0] + self[1][2] * other[2][1] + self[2][2] * other[2][2]
        );
    }

    constexpr Derived& operator+=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] += other[0][0]; self[0][1] += other[0][1]; self[0][2] += other[0][2];
        self[1][0] += other[1][0]; self[1][1] += other[1][1]; self[1][2] += other[1][2];
        self[2][0] += other[2][0]; self[2][1] += other[2][1]; self[2][2] += other[2][2];

        return self;
    }

    constexpr Derived& operator-=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] -= other[0][0]; self[0][1] -= other[0][1]; self[0][2] -= other[0][2];
        self[1][0] -= other[1][0]; self[1][1] -= other[1][1]; self[1][2] -= other[1][2];
        self[2][0] -= other[2][0]; self[2][1] -= other[2][1]; self[2][2] -= other[2][2];

        return self;
    }

    constexpr Derived& operator*=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] *= scalar; self[1][0] *= scalar; self[2][0] *= scalar;
        self[0][1] *= scalar; self[1][1] *= scalar; self[2][1] *= scalar;
        self[0][2] *= scalar; self[1][2] *= scalar; self[2][2] *= scalar;

        return self;
    }

    constexpr Derived& operator*=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self = self * other;

        return self;
    }

    constexpr T* get_data() noexcept { return &static_cast<Derived&>(*this)[0][0]; }
    constexpr const T* get_data() const noexcept { return &static_cast<const Derived&>(*this)[0][0]; }
    
    constexpr T* begin() noexcept { return &static_cast<Derived&>(*this)[0][0]; }
    constexpr const T* begin() const noexcept { return &static_cast<const Derived&>(*this)[0][0]; }
    constexpr T* end() noexcept { return begin() + 9; }
    constexpr const T* end() const noexcept { return begin() + 9; }
};

template<typename T, typename Derived>
[[nodiscard]] constexpr Derived operator*(T scalar, const Matrix3Base<T, Derived>& mat3) noexcept
{
    return static_cast<const Derived&>(mat3) * scalar;
}

struct Matrix3f : public Matrix3Base<float, Matrix3f>
{
    using Matrix3Base::operator*;

    union
    {
        float data[3][3];
        Vector3f vec3[3];
    };

    constexpr Matrix3f() : data{} {}
    constexpr explicit Matrix3f(float v)
        : data{}
    {
        data[0][0] = data[1][1] = data[2][2] = v;
        data[0][1] = data[0][2] = 0.0f;
        data[1][0] = data[1][2] = 0.0f;
        data[2][0] = data[2][1] = 0.0f;
    }
    constexpr Matrix3f(float m00, float m10, float m20,
                       float m01, float m11, float m21,
                       float m02, float m12, float m22)
    : data{{m00, m10, m20},
           {m01, m11, m21},
           {m02, m12, m22}} {}

    [[nodiscard]] constexpr Vector3f operator*(const Vector3f& other) const noexcept
    {
        return Vector3f(
            data[0][0] * other.x + data[1][0] * other.y + data[2][0] * other.z,
            data[0][1] * other.x + data[1][1] * other.y + data[2][1] * other.z,
            data[0][2] * other.x + data[1][2] * other.y + data[2][2] * other.z
        );
    }

    constexpr Vector3f& operator[](int i) noexcept
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Matrix3f index out of bounds!");
        return vec3[i];
    }
    constexpr const Vector3f& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 3 && "Matrix3f index out of bounds!");
        return vec3[i];
    }
};

//================================================================================
//mat4
template<typename T, typename Derived>
struct Matrix4Base
{
    [[nodiscard]] constexpr Derived operator-() const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);
        return Derived(
            -self[0][0], -self[0][1], -self[0][2], -self[0][3],
            -self[1][0], -self[1][1], -self[1][2], -self[1][3],
            -self[2][0], -self[2][1], -self[2][2], -self[2][3],
            -self[3][0], -self[3][1], -self[3][2], -self[3][3]
        );
    }

    [[nodiscard]] constexpr Derived operator+(const Derived &other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] + other[0][0], self[0][1] + other[0][1], self[0][2] + other[0][2], self[0][3] + other[0][3],
            self[1][0] + other[1][0], self[1][1] + other[1][1], self[1][2] + other[1][2], self[1][3] + other[1][3],
            self[2][0] + other[2][0], self[2][1] + other[2][1], self[2][2] + other[2][2], self[2][3] + other[2][3],
            self[3][0] + other[3][0], self[3][1] + other[3][1], self[3][2] + other[3][2], self[3][3] + other[3][3]
        ); 
     }

    [[nodiscard]] constexpr Derived operator-(const Derived& other) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

          return Derived(
            self[0][0] - other[0][0], self[0][1] - other[0][1], self[0][2] - other[0][2], self[0][3] - other[0][3],
            self[1][0] - other[1][0], self[1][1] - other[1][1], self[1][2] - other[1][2], self[1][3] - other[1][3],
            self[2][0] - other[2][0], self[2][1] - other[2][1], self[2][2] - other[2][2], self[2][3] - other[2][3],
            self[3][0] - other[3][0], self[3][1] - other[3][1], self[3][2] - other[3][2], self[3][3] - other[3][3]
        );
    }

    [[nodiscard]] constexpr Derived operator*(T scalar) const noexcept
    {
        auto& self = static_cast<const Derived&>(*this);

        return Derived(
            self[0][0] * scalar, self[0][1] * scalar, self[0][2] * scalar, self[0][3] * scalar,
            self[1][0] * scalar, self[1][1] * scalar, self[1][2] * scalar, self[1][3] * scalar,
            self[2][0] * scalar, self[2][1] * scalar, self[2][2] * scalar, self[2][3] * scalar,
            self[3][0] * scalar, self[3][1] * scalar, self[3][2] * scalar, self[3][3] * scalar
        );
    }

    [[nodiscard]] constexpr Derived operator*(const Derived& other) const noexcept
    {
        const auto& self = static_cast<const Derived&>(*this);
        
        return Derived(
            self[0][0] * other[0][0] + self[1][0] * other[0][1] + self[2][0] * other[0][2] + self[3][0] * other[0][3],
            self[0][1] * other[0][0] + self[1][1] * other[0][1] + self[2][1] * other[0][2] + self[3][1] * other[0][3],
            self[0][2] * other[0][0] + self[1][2] * other[0][1] + self[2][2] * other[0][2] + self[3][2] * other[0][3],
            self[0][3] * other[0][0] + self[1][3] * other[0][1] + self[2][3] * other[0][2] + self[3][3] * other[0][3],
            
            self[0][0] * other[1][0] + self[1][0] * other[1][1] + self[2][0] * other[1][2] + self[3][0] * other[1][3],
            self[0][1] * other[1][0] + self[1][1] * other[1][1] + self[2][1] * other[1][2] + self[3][1] * other[1][3],
            self[0][2] * other[1][0] + self[1][2] * other[1][1] + self[2][2] * other[1][2] + self[3][2] * other[1][3],
            self[0][3] * other[1][0] + self[1][3] * other[1][1] + self[2][3] * other[1][2] + self[3][3] * other[1][3],
            
            self[0][0] * other[2][0] + self[1][0] * other[2][1] + self[2][0] * other[2][2] + self[3][0] * other[2][3],
            self[0][1] * other[2][0] + self[1][1] * other[2][1] + self[2][1] * other[2][2] + self[3][1] * other[2][3],
            self[0][2] * other[2][0] + self[1][2] * other[2][1] + self[2][2] * other[2][2] + self[3][2] * other[2][3],
            self[0][3] * other[2][0] + self[1][3] * other[2][1] + self[2][3] * other[2][2] + self[3][3] * other[2][3],
            
            self[0][0] * other[3][0] + self[1][0] * other[3][1] + self[2][0] * other[3][2] + self[3][0] * other[3][3],
            self[0][1] * other[3][0] + self[1][1] * other[3][1] + self[2][1] * other[3][2] + self[3][1] * other[3][3],
            self[0][2] * other[3][0] + self[1][2] * other[3][1] + self[2][2] * other[3][2] + self[3][2] * other[3][3],
            self[0][3] * other[3][0] + self[1][3] * other[3][1] + self[2][3] * other[3][2] + self[3][3] * other[3][3]
        );
    }

    constexpr Derived& operator+=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] += other[0][0]; self[1][0] += other[1][0]; self[2][0] += other[2][0]; self[3][0] += other[3][0];
        self[0][1] += other[0][1]; self[1][1] += other[1][1]; self[2][1] += other[2][1]; self[3][1] += other[3][1];
        self[0][2] += other[0][2]; self[1][2] += other[1][2]; self[2][2] += other[2][2]; self[3][2] += other[3][2];
        self[0][3] += other[0][3]; self[1][3] += other[1][3]; self[2][3] += other[2][3]; self[3][3] += other[3][3];

        return self;
    }

    constexpr Derived& operator-=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] -= other[0][0]; self[1][0] -= other[1][0]; self[2][0] -= other[2][0]; self[3][0] -= other[3][0];
        self[0][1] -= other[0][1]; self[1][1] -= other[1][1]; self[2][1] -= other[2][1]; self[3][1] -= other[3][1];
        self[0][2] -= other[0][2]; self[1][2] -= other[1][2]; self[2][2] -= other[2][2]; self[3][2] -= other[3][2];
        self[0][3] -= other[0][3]; self[1][3] -= other[1][3]; self[2][3] -= other[2][3]; self[3][3] -= other[3][3];

        return self;
    }

    constexpr Derived& operator*=(T scalar) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self[0][0] *= scalar; self[1][0] *= scalar; self[2][0] *= scalar; self[3][0] *= scalar;
        self[0][1] *= scalar; self[1][1] *= scalar; self[2][1] *= scalar; self[3][1] *= scalar;
        self[0][2] *= scalar; self[1][2] *= scalar; self[2][2] *= scalar; self[3][2] *= scalar;
        self[0][3] *= scalar; self[1][3] *= scalar; self[2][3] *= scalar; self[3][3] *= scalar;

        return self;
    }

    constexpr Derived& operator*=(const Derived& other) noexcept
    {
        auto& self = static_cast<Derived&>(*this);
        self = self * other;

        return self;
    }

    constexpr T* get_data() noexcept { return &static_cast<Derived&>(*this)[0][0]; }
    constexpr const T* get_data() const noexcept { return &static_cast<const Derived&>(*this)[0][0]; }
    
    constexpr T* begin() noexcept { return &static_cast<Derived&>(*this)[0][0]; }
    constexpr const T* begin() const noexcept { return &static_cast<const Derived&>(*this)[0][0]; }
    constexpr T* end() noexcept { return begin() + 16; }
    constexpr const T* end() const noexcept { return begin() + 16; }
};

template<typename T, typename Derived>
[[nodiscard]] constexpr Derived operator*(T scalar, const Matrix4Base<T, Derived>& mat4) noexcept
{
    return static_cast<const Derived&>(mat4) * scalar;
}

struct alignas(16) Matrix4f : public Matrix4Base<float, Matrix4f>
{
    using Matrix4Base::operator*;

    union
    {
        float data[4][4];
        Vector4f vec4[4];
    };

    constexpr Matrix4f() : data{} {}
    constexpr explicit Matrix4f(float v)
        : data{}
    {
        data[0][0] = data[1][1] = data[2][2] = data[3][3] = v;
        data[0][1] = data[0][2] = data[0][3] = 0.0f;
        data[1][0] = data[1][2] = data[1][3] = 0.0f;
        data[2][0] = data[2][1] = data[2][3] = 0.0f;
        data[3][0] = data[3][1] = data[3][2] = 0.0f;
    }
    constexpr Matrix4f(float m00, float m10, float m20, float m30,
                       float m01, float m11, float m21, float m31,
                       float m02, float m12, float m22, float m32,
                       float m03, float m13, float m23, float m33)
    : data{{m00, m10, m20, m30},
           {m01, m11, m21, m31},
           {m02, m12, m22, m32},
           {m03, m13, m23, m33}} {}

    [[nodiscard]] constexpr Vector4f operator*(const Vector4f& other) const noexcept
    {
        return Vector4f(
            data[0][0] * other.x + data[1][0] * other.y + data[2][0] * other.z + data[3][0] * other.w,
            data[0][1] * other.x + data[1][1] * other.y + data[2][1] * other.z + data[3][1] * other.w,
            data[0][2] * other.x + data[1][2] * other.y + data[2][2] * other.z + data[3][2] * other.w,
            data[0][3] * other.x + data[1][3] * other.y + data[2][3] * other.z + data[3][3] * other.w
        );
    }

    constexpr Vector4f& operator[](int i) noexcept
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Matrix4f index out of bounds!");
        return vec4[i];
    }
    constexpr const Vector4f& operator[](int i) const noexcept 
    {
        YIALITE_ASSERT(i >= 0 && i < 4 && "Matrix4f index out of bounds!");
        return vec4[i];
    }
};

static_assert(std::is_standard_layout_v<Vector2f>, "must be C layout");
static_assert(std::is_standard_layout_v<Vector2i>, "must be C layout");
static_assert(std::is_standard_layout_v<Vector3f>, "must be C layout");
static_assert(std::is_standard_layout_v<Vector3i>, "must be C layout");
static_assert(std::is_standard_layout_v<Vector4f>, "must be C layout");
static_assert(std::is_standard_layout_v<Vector4i>, "must be C layout");
static_assert(std::is_standard_layout_v<Rect>,     "must be C layout");
static_assert(std::is_standard_layout_v<FRect>,    "must be C layout");
static_assert(std::is_standard_layout_v<Color>,    "must be C layout");
static_assert(std::is_standard_layout_v<FColor>,   "must be C layout");
static_assert(std::is_standard_layout_v<Matrix3f>, "must be C layout");
static_assert(std::is_standard_layout_v<Matrix4f>, "must be C layout");

//================================================================================
//tools

//vector
[[nodiscard]] constexpr inline float length_squared(const Vector2f& v) noexcept
{
    return v.x * v.x + v.y * v.y;
}

[[nodiscard]] constexpr inline float length_squared(const Vector3f& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] constexpr inline float length_squared(const Vector4f& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

[[nodiscard]] constexpr inline float dot(const Vector2f& a, const Vector2f& b) noexcept
{
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] constexpr inline float dot(const Vector3f& a, const Vector3f& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr inline float dot(const Vector4f& a, const Vector4f& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] constexpr inline float cross(const Vector2f& a, const Vector2f& b) noexcept
{
    return a.x * b.y - a.y * b.x;
}

[[nodiscard]] constexpr inline Vector3f cross(const Vector3f& a, const Vector3f& b) noexcept
{
    return Vector3f(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

template <typename T>
[[nodiscard]] constexpr float length(const T& v) noexcept
{
    return std::sqrt(length_squared(v));
}

template <typename T>
[[nodiscard]] constexpr T normalize(const T& v) noexcept
{
    float len = length(v);
    if (len < std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE) return T();
    return v / len;
}

template<typename T>
[[nodiscard]] constexpr float distance_squared(const T& a, const T& b) noexcept
{
    return length_squared(b - a);
}

template<typename T>
[[nodiscard]] constexpr float distance(const T& a, const T& b) noexcept
{
    return length(b - a);
}

template<typename T>
[[nodiscard]] constexpr T radians(T degrees) noexcept
requires std::numeric_limits<T>::is_iec559
{
    return degrees * static_cast<T>(0.01745329251994329576923690768489);
}

template<typename T>
[[nodiscard]] constexpr inline T lerp(const T& a, const T& b, float t) noexcept
{
    return a + (b - a) * t;
}

template<typename T>
[[nodiscard]] constexpr inline T clamp(T v, T min, T max) noexcept
{
    return std::max(min, std::min(v, max));
}

//matrix
[[nodiscard]] constexpr inline Matrix3f transpose(const Matrix3f& m) noexcept
{
    return Matrix3f(
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2]
    );
}

[[nodiscard]] inline Matrix3f inverse(const Matrix3f& m) noexcept
{
    const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];
    const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
    const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];

    const float c00 = m11 * m22 - m12 * m21;
    const float c01 = m10 * m22 - m12 * m20;
    const float c02 = m10 * m21 - m11 * m20;

    const float det = m00 * c00 - m01 * c01 + m02 * c02;
    if(std::abs(det) < std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE) 
    {
        return Matrix3f(
            NAN, NAN, NAN,
            NAN, NAN, NAN,
            NAN, NAN, NAN
        );
    }

    const float inv_det = 1.0f / det;

    const float c10 = m01 * m22 - m02 * m21;
    const float c11 = m00 * m22 - m02 * m20;
    const float c12 = m00 * m21 - m01 * m20;

    const float c20 = m01 * m12 - m02 * m11;
    const float c21 = m00 * m12 - m02 * m10;
    const float c22 = m00 * m11 - m01 * m10;

    return Matrix3f(
         c00 * inv_det, -c01 * inv_det,  c02 * inv_det,
        -c10 * inv_det,  c11 * inv_det, -c12 * inv_det,
         c20 * inv_det, -c21 * inv_det,  c22 * inv_det
    );
}

[[nodiscard]] constexpr inline Matrix4f transpose(const Matrix4f& m) noexcept
{
    return Matrix4f(
        m[0][0], m[1][0], m[2][0], m[3][0],
        m[0][1], m[1][1], m[2][1], m[3][1],
        m[0][2], m[1][2], m[2][2], m[3][2],
        m[0][3], m[1][3], m[2][3], m[3][3]
    );
}

[[nodiscard]] inline Matrix4f inverse(const Matrix4f& m) noexcept
{
    const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    const float v0 = m20 * m31 - m21 * m30;
    const float v1 = m20 * m32 - m22 * m30;
    const float v2 = m20 * m33 - m23 * m30;
    const float v3 = m21 * m32 - m22 * m31;
    const float v4 = m21 * m33 - m23 * m31;
    const float v5 = m22 * m33 - m23 * m32;

    const float t00 = + (v5 * m11 - v4 * m12 + v3 * m13);
    const float t10 = - (v5 * m10 - v2 * m12 + v1 * m13);
    const float t20 = + (v4 * m10 - v2 * m11 + v0 * m13);
    const float t30 = - (v3 * m10 - v1 * m11 + v0 * m12);

    const float det = m00 * t00 + m01 * t10 + m02 * t20 + m03 * t30;
    if (std::abs(det) < std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE)
    {
        return Matrix4f{
            NAN, NAN, NAN, NAN,
            NAN, NAN, NAN, NAN,
            NAN, NAN, NAN, NAN,
            NAN, NAN, NAN, NAN
        };
    }

    const float inv_det = 1.0f / det;

    const float d00 = t00 * inv_det;
    const float d01 = t10 * inv_det;
    const float d02 = t20 * inv_det;
    const float d03 = t30 * inv_det;

    const float d10 = - (v5 * m01 - v4 * m02 + v3 * m03) * inv_det;
    const float d11 = + (v5 * m00 - v2 * m02 + v1 * m03) * inv_det;
    const float d12 = - (v4 * m00 - v2 * m01 + v0 * m03) * inv_det;
    const float d13 = + (v3 * m00 - v1 * m01 + v0 * m02) * inv_det;

    const float v6 = m10 * m31 - m11 * m30;
    const float v7 = m10 * m32 - m12 * m30;
    const float v8 = m10 * m33 - m13 * m30;
    const float v9 = m11 * m32 - m12 * m31;
    const float v10 = m11 * m33 - m13 * m31;
    const float v11 = m12 * m33 - m13 * m32;

    const float d20 = + (v11 * m01 - v10 * m02 + v9 * m03) * inv_det;
    const float d21 = - (v11 * m00 - v8 * m02 + v7 * m03) * inv_det;
    const float d22 = + (v10 * m00 - v8 * m01 + v6 * m03) * inv_det;
    const float d23 = - (v9 * m00 - v7 * m01 + v6 * m02) * inv_det;

    const float v12 = m10 * m21 - m11 * m20;
    const float v13 = m10 * m22 - m12 * m20;
    const float v14 = m10 * m23 - m13 * m20;
    const float v15 = m11 * m22 - m12 * m21;
    const float v16 = m11 * m23 - m13 * m21;
    const float v17 = m12 * m23 - m13 * m22;

    const float d30 = - (v17 * m01 - v16 * m02 + v15 * m03) * inv_det;
    const float d31 = + (v17 * m00 - v14 * m02 + v13 * m03) * inv_det;
    const float d32 = - (v16 * m00 - v14 * m01 + v12 * m03) * inv_det;
    const float d33 = + (v15 * m00 - v13 * m01 + v12 * m02) * inv_det;

    return Matrix4f(
        d00, d10, d20, d30,
        d01, d11, d21, d31,
        d02, d12, d22, d32,
        d03, d13, d23, d33
    );
}

[[nodiscard]] constexpr inline Matrix4f translate(const Matrix4f& m, const Vector3f& v) noexcept
{
    Matrix4f result(m);
    result[3] = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
    return result;
}

[[nodiscard]] inline Matrix4f rotate(const Matrix4f& m, float angle, const Vector3f& v) noexcept
{
    const float a = angle;
    const float c = std::cos(a);
    const float s = std::sin(a);

    Vector3f axis(normalize(v));
    Vector3f temp((1.0f - c) * axis);

    Matrix4f rotate;
    rotate[0][0] = c + temp[0] * axis[0];
    rotate[0][1] = temp[0] * axis[1] + s * axis[2];
    rotate[0][2] = temp[0] * axis[2] - s * axis[1];

    rotate[1][0] = temp[1] * axis[0] - s * axis[2];
    rotate[1][1] = c + temp[1] * axis[1];
    rotate[1][2] = temp[1] * axis[2] + s * axis[0];

    rotate[2][0] = temp[2] * axis[0] + s * axis[1];
    rotate[2][1] = temp[2] * axis[1] - s * axis[0];
    rotate[2][2] = c + temp[2] * axis[2];

    Matrix4f result;
    result[0] = m[0] * rotate[0][0] + m[1] * rotate[0][1] + m[2] * rotate[0][2];
    result[1] = m[0] * rotate[1][0] + m[1] * rotate[1][1] + m[2] * rotate[1][2];
    result[2] = m[0] * rotate[2][0] + m[1] * rotate[2][1] + m[2] * rotate[2][2];
    result[3] = m[3];

    return result;
}

[[nodiscard]] constexpr inline Matrix4f scale(const Matrix4f& m, const Vector3f& v) noexcept
{
    Matrix4f result;
    result[0] = m[0] * v[0];
    result[1] = m[1] * v[1];
    result[2] = m[2] * v[2];
    result[3] = m[3];
    return result;
}

[[nodiscard]] constexpr inline Matrix4f look_at_rh(const Vector3f& eye, const Vector3f& center, const Vector3f& up) noexcept
{
    const Vector3f f(normalize(center - eye));
    const Vector3f s(normalize(cross(f, up)));
    const Vector3f u(cross(s, f));

    Matrix4f result(1.0f);
    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] =-f.x;
    result[1][2] =-f.y;
    result[2][2] =-f.z;
    result[3][0] =-dot(s, eye);
    result[3][1] =-dot(u, eye);
    result[3][2] = dot(f, eye);
    return result;
}

[[nodiscard]] constexpr inline Matrix4f look_at_lh(const Vector3f& eye, const Vector3f& center, const Vector3f& up) noexcept
{
    const Vector3f f(normalize(center - eye));
    const Vector3f s(normalize(cross(up, f)));
    const Vector3f u(cross(f, s));

    Matrix4f result(1.0f);
    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] = f.x;
    result[1][2] = f.y;
    result[2][2] = f.z;
    result[3][0] = -dot(s, eye);
    result[3][1] = -dot(u, eye);
    result[3][2] = -dot(f, eye);
    return result;
}

[[nodiscard]] constexpr inline Matrix4f look_at(const Vector3f& eye, const Vector3f& center, const Vector3f& up) noexcept
{
    #if defined(YIALITE_CLIP_RH)
        return look_at_rh(eye, center, up);
    #else
        return look_at_lh(eye, center, up);
    #endif
}

[[nodiscard]] constexpr inline Matrix4f ortho_rh_no(float left, float right, float bottom, float top, float z_near, float z_far) noexcept
{
    Matrix4f result(1.0f);
    result[0][0] =  2.0f / (right - left);
    result[1][1] =  2.0f / (top - bottom);
    result[2][2] = -2.0f / (z_far - z_near);
    result[3][0] = -(right + left) / (right - left);
    result[3][1] = -(top + bottom) / (top - bottom);
    result[3][2] = -(z_far + z_near) / (z_far - z_near);
    return result;
}

[[nodiscard]] constexpr inline Matrix4f ortho_rh_zo(float left, float right, float bottom, float top, float z_near, float z_far) noexcept
{
    Matrix4f result(1.0f);
    result[0][0] =  2.0f / (right - left);
    result[1][1] =  2.0f / (top - bottom);
    result[2][2] = -1.0f / (z_far - z_near);
    result[3][0] = -(right + left) / (right - left);
    result[3][1] = -(top + bottom) / (top - bottom);
    result[3][2] = -z_near / (z_far - z_near);
    return result;
}

[[nodiscard]] constexpr inline Matrix4f ortho_lh_zo(float left, float right, float bottom, float top, float z_near, float z_far) noexcept
{    
    Matrix4f result(1.0f);
    result[0][0] =  2.0f / (right - left);
    result[1][1] =  2.0f / (top - bottom);
    result[2][2] =  1.0f / (z_far - z_near);
    result[3][0] = -(right + left) / (right - left);
    result[3][1] = -(top + bottom) / (top - bottom);
    result[3][2] = -z_near / (z_far - z_near);
    return result;
}

[[nodiscard]] constexpr inline Matrix4f ortho(float left, float right, float bottom, float top, float z_near, float z_far) noexcept
{
    #if defined(YIALITE_CLIP_RH) && defined(YIALITE_CLIP_NO)
        return ortho_rh_no(left, right, bottom, top, z_near, z_far);
    #elif defined(YIALITE_CLIP_RH) && defined(YIALITE_CLIP_ZO)
        return ortho_rh_zo(left, right, bottom, top, z_near, z_far);
    #elif defined(YIALITE_CLIP_LH) && defined(YIALITE_CLIP_ZO)
        return ortho_lh_zo(left, right, bottom, top, z_near, z_far);
    #else
        return ortho_lh_zo(left, right, bottom, top, z_near, z_far);
    #endif
}

[[nodiscard]] inline Matrix4f perspective_rh_no(float fovy, float aspect, float z_near, float z_far) noexcept
{
    YIALITE_ASSERT(aspect > std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE && "aspect must be greater than 0");

    const float tan_half_fovy = std::tan(fovy / 2.0f);

    Matrix4f result(0.0f);
    result[0][0] =  1.0f / (aspect * tan_half_fovy);
    result[1][1] =  1.0f / (tan_half_fovy);
    result[2][2] = -(z_far + z_near) / (z_far - z_near);
    result[2][3] = -1.0f;
    result[3][2] = -(2.0f * z_far * z_near) / (z_far - z_near);
    return result;
}

[[nodiscard]] inline Matrix4f perspective_rh_zo(float fovy, float aspect, float z_near, float z_far) noexcept
{
    YIALITE_ASSERT(aspect > std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE && "aspect must be greater than 0");

    const float tan_half_fovy = std::tan(fovy / 2.0f);

    Matrix4f result(0.0f);
    result[0][0] =  1.0f / (aspect * tan_half_fovy);
    result[1][1] =  1.0f / (tan_half_fovy);
    result[2][2] =  z_far / (z_near - z_far);
    result[2][3] = -1.0f;
    result[3][2] = -(z_far * z_near) / (z_far - z_near);
    return result;
}

[[nodiscard]] inline Matrix4f perspective_lh_zo(float fovy, float aspect, float z_near, float z_far) noexcept
{
    YIALITE_ASSERT(aspect > std::numeric_limits<float>::epsilon() * TOLERANCE_SCALE && "aspect must be greater than 0");

    const float tan_half_fovy = std::tan(fovy / 2.0f);

    Matrix4f result(0.0f);
    result[0][0] = 1.0f / (aspect * tan_half_fovy);
    result[1][1] = 1.0f / (tan_half_fovy);
    result[2][2] = z_far / (z_far - z_near);
    result[2][3] = 1.0f;
    result[3][2] = -(z_far * z_near) / (z_far - z_near);
    return result;
}

[[nodiscard]] inline Matrix4f perspective(float fovy, float aspect, float z_near, float z_far) noexcept
{
    #if defined(YIALITE_CLIP_RH) && defined(YIALITE_CLIP_NO)
        return perspective_rh_no(fovy, aspect, z_near, z_far);
    #elif defined(YIALITE_CLIP_RH) && defined(YIALITE_CLIP_ZO)
        return perspective_rh_zo(fovy, aspect, z_near, z_far);
    #elif defined(YIALITE_CLIP_LH) && defined(YIALITE_CLIP_ZO)
        return perspective_lh_zo(fovy, aspect, z_near, z_far);
    #else
        return perspective_lh_zo(fovy, aspect, z_near, z_far);
    #endif
}

}

#endif
