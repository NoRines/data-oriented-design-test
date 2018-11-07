#ifndef MATH_H
#define MATH_H

#include "Constants.h"

#include "Vec2.h"

template<typename T>
bool lineIntersection(const Vec2<T>& p0, const Vec2<T>& p1,
		const Vec2<T>& p2, const Vec2<T>& p3, Vec2<T>& res)
{
	T A1 = p1.getY() - p0.getY();
	T B1 = p0.getX() - p1.getX();
	T C1 = A1 * p0.getX() + B1 * p0.getY();

	T A2 = p3.getY() - p2.getY();
	T B2 = p2.getX() - p3.getX();
	T C2 = A2 * p2.getX() + B2 * p2.getY();

	T denominator = A1 * B2 - A2 * B1;

	if(denominator < 0.001 && denominator > -0.001)
	{
		return false;
	}

	res = {(B2 * C1 - B1 * C2) / denominator,
		(A1 * C2 - A2 * C1) / denominator};
	return true;
}

template<typename T>
bool lineSegmentIntersection(const Vec2<T>& p0, const Vec2<T>& p1,
		const Vec2<T>& p2, const Vec2<T>& p3, Vec2<T>& res)
{
	Vec2<T> intersectionPoint;

	if(!lineIntersection(p0, p1, p2, p3, intersectionPoint))
		return false;

	T rx0 = (intersectionPoint.getX() - p0.getX()) / (p1.getX() - p0.getX());
	T ry0 = (intersectionPoint.getY() - p0.getY()) / (p1.getY() - p0.getY());
	T rx1 = (intersectionPoint.getX() - p2.getX()) / (p3.getX() - p2.getX());
	T ry1 = (intersectionPoint.getY() - p2.getY()) / (p3.getY() - p2.getY());

	if(((rx0 >= 0.0 && rx0 <= 1.0) || (ry0 >= 0.0 && ry0 <= 1.0)) &&
		((rx1 >= 0.0 && rx1 <= 1.0) || (ry1 >= 0.0 && ry1 <= 1.0)))
	{
		res = intersectionPoint;
		return true;
	}
	else
	{
		return false;
	}
}

template<typename T>
T distFromLine(const Vec2<T>& point, const Vec2<T>& linePoint, const Vec2<T>& lineNormal)
{
	return (point - linePoint).dot(lineNormal.getUnit());
}

#endif
