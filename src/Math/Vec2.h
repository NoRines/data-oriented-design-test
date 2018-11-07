#ifndef VEC2_H
#define VEC2_H

#include <cmath>
#include "Constants.h"

template<typename T>
class Vec2
{
public:
	Vec2();
	Vec2(T v);
	Vec2(T x, T y);

	T length() const;
	T dot(const Vec2& vec) const;

	Vec2& rotate(T amount);

	void normalize();
	Vec2 getUnit() const;

	Vec2 operator-() const;
	Vec2 operator+(const Vec2& vec) const;
	Vec2 operator-(const Vec2& vec) const;
	Vec2 operator*(float s) const;
	Vec2 operator/(float s) const;

	Vec2& operator+=(const Vec2& vec);
	Vec2& operator-=(const Vec2& vec);
	Vec2& operator*=(float s);
	Vec2& operator/=(float s);

	T getX() const;
	T getY() const;

	void setX(T x);
	void setY(T y);
private:
	T arr[2];
};

template<typename T>
Vec2<T>::Vec2() : arr{0.0, 0.0} {}
template<typename T>
Vec2<T>::Vec2(T v) : arr{v, v} {}
template<typename T>
Vec2<T>::Vec2(T x, T y) : arr{x, y} {}

template<typename T>
T Vec2<T>::length() const
{
	return std::sqrt(arr[0] * arr[0] + arr[1] * arr[1]);
}

template<typename T>
T Vec2<T>::dot(const Vec2 &vec) const
{
	return (arr[0] * vec.arr[0]) + (arr[1] * vec.arr[1]);
}

template<typename T>
Vec2<T>& Vec2<T>::rotate(T amount)
{
	T x = std::cos(amount) * arr[0] - std::sin(amount) * arr[1];
	T y = std::sin(amount) * arr[0] + std::cos(amount) * arr[1];
	arr[0] = x;
	arr[1] = y;
	return *this;
}

template<typename T>
void Vec2<T>::normalize()
{
	*this /= length();
}

template<typename T>
Vec2<T> Vec2<T>::getUnit() const
{
	Vec2 tmp = *this;
	tmp.normalize();
	return tmp;
}

template<typename T>
Vec2<T> Vec2<T>::operator-() const
{
	return *this * -1.0;
}

template<typename T>
Vec2<T> Vec2<T>::operator+(const Vec2& vec) const
{
	return Vec2(arr[0] + vec.arr[0], arr[1] + vec.arr[1]);
}

template<typename T>
Vec2<T> Vec2<T>::operator-(const Vec2& vec) const
{
	return Vec2(arr[0] - vec.arr[0], arr[1] - vec.arr[1]);
}

template<typename T>
Vec2<T> Vec2<T>::operator*(float s) const
{
	return Vec2(arr[0] * s, arr[1] * s);
}

template<typename T>
Vec2<T> Vec2<T>::operator/(float s) const
{
	return Vec2(arr[0] / s, arr[1] / s);
}

template<typename T>
Vec2<T>& Vec2<T>::operator+=(const Vec2& vec)
{
	arr[0] += vec.arr[0];
	arr[1] += vec.arr[1];
	return *this;
}

template<typename T>
Vec2<T>& Vec2<T>::operator-=(const Vec2& vec)
{
	arr[0] -= vec.arr[0];
	arr[1] -= vec.arr[1];
	return *this;
}

template<typename T>
Vec2<T>& Vec2<T>::operator*=(float s)
{
	arr[0] *= s;
	arr[1] *= s;
	return *this;
}

template<typename T>
Vec2<T>& Vec2<T>::operator/=(float s)
{
	arr[0] /= s;
	arr[1] /= s;
	return *this;
}

template<typename T>
T Vec2<T>::getX() const
{
	return arr[0];
}

template<typename T>
T Vec2<T>::getY() const
{
	return arr[1];
}

template<typename T>
void Vec2<T>::setX(T x)
{
	arr[0] = x;
}

template<typename T>
void Vec2<T>::setY(T y)
{
	arr[1] = y;
}

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;

#endif
