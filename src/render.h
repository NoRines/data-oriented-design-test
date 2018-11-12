#ifndef RENDER_H
#define RENDER_H

#include "Math/Math.h"
#include "map.h"
#include <cstdint>
#include <memory>

namespace render
{
	static int buffer_width;
	static int buffer_height;

	struct screencoord
	{
		int leftX, rightX;
		int topLeftY, bottomLeftY;
		int topRightY, bottomRightY;
		float texClipLeft;
		float texClipRight;
		float zDistLeft;
		float zDistRight;
		int sideId;
	};

	struct clippedwall
	{
		Vec2f p0;
		Vec2f p1;
		int visibleSide;
		float texClipLeft;
		float texClipRight;
	};

	template<typename WallIt>
	void translate_walls(const Vec2f playerPos, const float angle, WallIt inBeg,
			const WallIt inEnd, WallIt outBeg, WallIt& outEnd)
	{
		while(inBeg != inEnd)
		{
			outBeg->frontId = inBeg->frontId;
			outBeg->backId = inBeg->backId;
			auto& p0 = outBeg->p0;
			auto& p1 = outBeg->p1;
			p0 = inBeg->p0 - playerPos;
			p1 = inBeg->p1 - playerPos;
			p0.rotate(-angle);
			p1.rotate(-angle);

			++inBeg;
			++outBeg;
		}
		outEnd = outBeg;
	}

	template<typename WallIt, typename ClippedWallIt>
	void clip_walls(WallIt inBeg, const WallIt inEnd, ClippedWallIt outBeg, ClippedWallIt& outEnd)
	{
		static const float fov = (90.0f * PI) / 180.0f;
		static const Vec2f leftBound(std::cos(-fov / 2.0f), std::sin(-fov / 2.0f));
		static const Vec2f leftNormal = leftBound.getUnit().rotate(HALF_PI);
		static const Vec2f rightBound(std::cos(fov / 2.0f), std::sin(fov / 2.0f));
		static const Vec2f rightNormal = rightBound.getUnit().rotate(-HALF_PI);

		while(inBeg != inEnd)
		{
			auto& visibleSide = outBeg->visibleSide;
			auto& w0 = outBeg->p0;
			auto& w1 = outBeg->p1;
			auto& texClipLeft = outBeg->texClipLeft;
			auto& texClipRight = outBeg->texClipRight;

			texClipLeft = 0.0f;
			texClipRight = 0.0f;

			w0 = inBeg->p0;
			w1 = inBeg->p1;

			visibleSide = inBeg->frontId;

			Vec2f wallNormal = (w1 - w0).getUnit().rotate(-HALF_PI);
			if(distFromLine({0.0f}, w0, wallNormal) > 0.0f)
			{
				std::swap(w0, w1);
				visibleSide = inBeg->backId;
			}

			++inBeg;
			++outBeg;

			if(visibleSide < 0)
			{
				--outBeg;
				continue;
			}

			float leftDist0 = distFromLine(w0, leftBound, leftNormal);
			float leftDist1 = distFromLine(w1, leftBound, leftNormal);
			float rightDist0 = distFromLine(w0, rightBound, rightNormal);
			float rightDist1 = distFromLine(w1, rightBound, rightNormal);

			if((rightDist0 < 0.0f && rightDist1 < 0.0f) || (leftDist0 < 0.0f && leftDist1 < 0.0f))
			{
				--outBeg;
				continue;
			}

			const float wallLength = (w1 - w0).length();

			Vec2f intersection;
			bool intersectionLeft = lineSegmentIntersection({0.0f},
				leftBound * 100.0f, w0, w1, intersection);
			if(intersectionLeft)
				if(leftDist1 > 0.0f && leftDist0 < 0.0f)
				{
					texClipLeft = (intersection - w0).length() / wallLength;
					w0 = intersection;
				}

			bool intersectionRight = lineSegmentIntersection({0.0f},
				rightBound * 100.0f, w0, w1, intersection);
			if(intersectionRight)
				if(rightDist0 > 0.0f && rightDist1 < 0.0f)
				{
					texClipRight = (w1 - intersection).length() / wallLength;
					w1 = intersection;
				}

			if(!intersectionLeft && !intersectionRight)
				if(w0.getX() < 0.0f || w1.getX() < 0.0f)
					--outBeg;
		}
		outEnd = outBeg;
	}

	template<typename ClippedWallIt, typename ScreenCoordsIt, typename SideIt, typename SectorIt>
	void gen_screen_coords(ClippedWallIt inBeg, const ClippedWallIt inEnd,
		ScreenCoordsIt outBeg, ScreenCoordsIt& outEnd, SideIt sideArr, SectorIt sectorArr)
	{
		static const float fov = (90.0f * PI) / 180.0f;
		static const float tanHalfFov = std::tan(fov / 2.0f);

		while(inBeg != inEnd)
		{
			auto& screenCoords = *outBeg;
			const auto& visibleSide = inBeg->visibleSide;
			const auto& w0 = inBeg->p0;
			const auto& w1 = inBeg->p1;
			const auto& texClipLeft = inBeg->texClipLeft;
			const auto& texClipRight = inBeg->texClipRight;

			++inBeg;
			++outBeg;

			screenCoords.sideId = visibleSide;

			const auto& sector = sectorArr[sideArr[visibleSide].sectorId];

			float xScaleLeft = w0.getY() / (w0.getX() * tanHalfFov);
			float xScaleRight = w1.getY() / (w1.getX() * tanHalfFov);
			float yScaleLeft = (0.2f * buffer_height) / w0.getX();
			float yScaleRight = (0.2f * buffer_height) / w1.getX();

			screenCoords.leftX = (int)(xScaleLeft * (buffer_width / 2) + (buffer_width / 2));
			screenCoords.rightX = (int)(xScaleRight * (buffer_width / 2) + (buffer_width / 2));
			screenCoords.topLeftY = -(int)(yScaleLeft * sector.ceilingHeight) + (buffer_height / 2);
			screenCoords.bottomLeftY = (int)(yScaleLeft * -sector.floorHeight) + (buffer_height / 2);
			screenCoords.topRightY = -(int)(yScaleRight * sector.ceilingHeight) + (buffer_height / 2);
			screenCoords.bottomRightY = (int)(yScaleRight * -sector.floorHeight) + (buffer_height / 2);

			screenCoords.texClipLeft = texClipLeft;
			screenCoords.texClipRight = texClipRight;
			screenCoords.zDistLeft = w0.getX();
			screenCoords.zDistRight = w1.getX();
		}
		outEnd = outBeg;
	}

	template<typename ScreenCoordsIt, typename SideIt>
	void output_to_screen_buffer(ScreenCoordsIt inBeg, const ScreenCoordsIt inEnd, SideIt sideArr,
		int screenWidth, int screenHeight, int bufferPitch, uint8_t* buffer)
	{
		auto drawVerticalWallColumn = [screenWidth, screenHeight, bufferPitch, buffer]
			(int yMin, int yMax, int x, int u, float topTex, float bottomTex,
			const std::shared_ptr<map::tex>& tex)
		{
			if(x < 0 || x >= screenWidth) return;

			const int yDiff = yMax - yMin;
			const int& texWidth = tex->width;
			const int& texHeight = tex->height;
			float v = bottomTex * texHeight;
			const float vMax = topTex * texHeight;
			const float vStep = (vMax - v) / yDiff;

			if(yMin < 0)
			{
				v += -yMin * vStep;
				yMin = 0;
			}
			if(yMax >= screenHeight)
				yMax = screenHeight - 1;

			uint8_t* srcPixStart = nullptr;
			uint8_t* destPixStart = nullptr;
			for(int y = yMin; y <= yMax; y++)
			{
				srcPixStart = tex->data.get() + (u + (((int)v  % texHeight) * texWidth)) * 4;
				//srcPixStart = tex->data.get() + ((int)v + ((u % texWidth) * texHeight)) * 4;
				destPixStart = buffer + (x + y * screenWidth) * 4;
				*destPixStart++ = *srcPixStart++;
				*destPixStart++ = *srcPixStart++;
				*destPixStart++ = *srcPixStart++;
				*destPixStart++ = *srcPixStart++;
				v += vStep;
			}
		};

		while(inBeg != inEnd)
		{
			const auto& coords = *inBeg;
			++inBeg;

			const auto& side = sideArr[coords.sideId];
			auto texCoord = side.texCoord;
			texCoord.left += coords.texClipLeft * (side.texCoord.right - side.texCoord.left);
			texCoord.right -= coords.texClipRight * (side.texCoord.right - side.texCoord.left);

			const int xDiff = (coords.rightX - coords.leftX == 0) ? 1 : coords.rightX - coords.leftX;

			const float yTopStep = (float)(coords.topRightY - coords.topLeftY) / xDiff;
			const float yBottomStep = (float)(coords.bottomRightY - coords.bottomLeftY) / xDiff;

			float yTop = coords.topLeftY;
			float yBottom = coords.bottomLeftY;

			float oneOverZLeft = 1.0f / coords.zDistLeft;
			const float oneOverZRight = 1.0f / coords.zDistRight;
			const float oneOverZStep = (oneOverZRight - oneOverZLeft) / xDiff;

			const int& texWidth = side.midTex->width;
			float texLeft = (texCoord.left * texWidth) / coords.zDistLeft;
			const float texRight = (texCoord.right * texWidth) / coords.zDistRight;
			const float texStep = (texRight - texLeft) / xDiff;

			for(int x = coords.leftX; x <= coords.rightX; x++)
			{
				drawVerticalWallColumn((int)yTop, (int)yBottom, x, (int)(texLeft / oneOverZLeft) % texWidth, texCoord.top, texCoord.bottom, side.midTex);
				yTop += yTopStep;
				yBottom += yBottomStep;

				oneOverZLeft += oneOverZStep;
				texLeft += texStep;
			}
		}
	}
}

#endif
