#include <iostream>
#include <SDL2/SDL.h>
#include <memory>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <thread>
#include <string>
#include <fstream>
#include "Math/Math.h"

// Här är skärm storleken.
#define RES_W 640
#define RES_H 480

// Denna funktor används som en anpassad destruktor i unique_ptrs av typen SDL_Window osv.
struct SDL_Destroyer
{
	void operator()(SDL_Window* window)
	{
		std::cout << "SDL_DestroyWindow(window);" << std::endl;
		SDL_DestroyWindow(window);
	}

	void operator()(SDL_Renderer* renderer)
	{
		std::cout << "SDL_DestroyRenderer(renderer);" << std::endl;
		SDL_DestroyRenderer(renderer);
	}

	void operator()(SDL_Texture* texture)
	{
		std::cout << "SDL_DestroyTexture(texture);" << std::endl;
		SDL_DestroyTexture(texture);
	}
};

namespace map
{
	struct Wall
	{
		Vec2f p0;
		Vec2f p1;
		int frontId;
		int backId;
	};

	struct Tex
	{
		static constexpr int BYTES_PER_PIXEL = 4;
		std::string name;
		std::unique_ptr<uint8_t[]> data;
		int width;
		int height;
		int pitch;
	};

	struct TexCoord
	{
		float left;
		float right;
		float top;
		float bottom;
	};

	struct Side
	{
		std::shared_ptr<Tex> topTex;
		std::shared_ptr<Tex> midTex;
		std::shared_ptr<Tex> botTex;
		TexCoord texCoord;
		int sectorId;
	};

	std::shared_ptr<Tex> loadTextureFromBmp(const std::string& fileName)
	{
		constexpr char START_ERROR_MSG[] = "Error loadTextureFromBmp: ";

		auto loadedTexture = std::make_shared<Tex>();
		loadedTexture->name = fileName;

		std::ifstream file(fileName, std::ios::binary | std::ios::in | std::ios::ate);

		if(!file.is_open())
		{
			std::cerr << START_ERROR_MSG << "The file " << fileName << " did not open." << std::endl;
			return std::shared_ptr<Tex>(nullptr);
		}

		int fileSize = file.tellg();
		file.seekg(0, file.beg);

		auto buffer = std::make_unique<uint8_t[]>(fileSize);
		file.read((char*)buffer.get(), fileSize);
		file.close();

		uint8_t* walker = buffer.get();

		if(walker[0] != 'B' || walker[1] != 'M')
		{
			std::cerr << START_ERROR_MSG << "The file " << fileName << " header is not correct." << '\n';
			return std::shared_ptr<Tex>(nullptr);
		}

		walker += 10;

		int32_t pixelDataOffset = *((int32_t*)&walker[0]);
		walker += 8;

		int32_t width = *((int32_t*)&walker[0]);
		walker += 4;
		int32_t height = *((int32_t*)&walker[0]);
		walker += 6;

		int16_t bitsPerPixel = *((int16_t*)&walker[0]);

		if(bitsPerPixel != 24 && bitsPerPixel != 32)
		{
			std::cerr << START_ERROR_MSG << "The file " << fileName << " uses the worng bits per pixel. 32 bpp and 24 bpp are supported." << '\n';
			return std::shared_ptr<Tex>(nullptr);
		}

		int rowSize = ((bitsPerPixel * width + 31) / 32) * 4;

		loadedTexture->width = width;
		loadedTexture->height = height;
		loadedTexture->pitch = width * Tex::BYTES_PER_PIXEL;

		auto texBuffer = std::make_unique<uint8_t[]>(loadedTexture->pitch * loadedTexture->height);

		uint8_t* rowSrc = buffer.get() + pixelDataOffset;
		rowSrc += rowSize * (height - 1);
		uint8_t* rowDest = texBuffer.get();

		for(int y = 0; y < height; y++)
		{
			uint8_t* pixelSrc = rowSrc;
			uint8_t* pixelDest = rowDest;
			for(int x = 0; x < width; x++)
			{
				if(bitsPerPixel == 24)
				{
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = 0xFF;
				}
				else if(bitsPerPixel = 32)
				{
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = *(pixelSrc++);
					*pixelDest++ = *(pixelSrc++);
				}
			}
			rowSrc -= rowSize;
			rowDest += loadedTexture->pitch;
		}

		loadedTexture->data = std::move(texBuffer);

		return loadedTexture;
	}
}

namespace render
{
	struct ScreenCoords
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

	struct ClippedWall
	{
		Vec2f p0;
		Vec2f p1;
		int visibleSide;
		float texClipLeft;
		float texClipRight;
	};

	template<typename WallIt>
	void translateWalls(Vec2f playerPos, float angle, WallIt beg,
			WallIt end, WallIt out)
	{
		while(beg != end)
		{
			out->frontId = beg->frontId;
			out->backId = beg->backId;
			auto& p0 = out->p0;
			auto& p1 = out->p1;
			p0 = beg->p0 - playerPos;
			p1 = beg->p1 - playerPos;
			p0.rotate(-angle);
			p1.rotate(-angle);

			++beg;
			++out;
		}
	}

	template<typename WallIt, typename ClippedWallIt>
	void clipWalls(WallIt beg, WallIt end, ClippedWallIt out)
	{
		static const float fov = (90.0f * PI) / 180.0f;
		static const Vec2f leftBound(std::cos(-fov / 2.0f), std::sin(-fov / 2.0f));
		static const Vec2f leftNormal = leftBound.getUnit().rotate(HALF_PI);
		static const Vec2f rightBound(std::cos(fov / 2.0f), std::sin(fov / 2.0f));
		static const Vec2f rightNormal = rightBound.getUnit().rotate(-HALF_PI);

		while(beg != end)
		{
			auto& visibleSide = out->visibleSide;
			auto& w0 = out->p0;
			auto& w1 = out->p1;
			auto& texClipLeft = out->texClipLeft;
			auto& texClipRight = out->texClipRight;

			texClipLeft = 0.0f;
			texClipRight = 0.0f;

			w0 = beg->p0;
			w1 = beg->p1;

			visibleSide = beg->frontId;

			Vec2f wallNormal = (w1 - w0).getUnit().rotate(-HALF_PI);
			if(distFromLine({0.0f}, w0, wallNormal) > 0.0f)
			{
				std::swap(w0, w1);
				visibleSide = beg->backId;
			}

			++beg;
			++out;

			float leftDist0 = distFromLine(w0, leftBound, leftNormal);
			float leftDist1 = distFromLine(w1, leftBound, leftNormal);
			float rightDist0 = distFromLine(w0, rightBound, rightNormal);
			float rightDist1 = distFromLine(w1, rightBound, rightNormal);

			if((rightDist0 < 0.0f && rightDist1 < 0.0f) || (leftDist0 < 0.0f && leftDist1 < 0.0f))
			{
				visibleSide = -1;
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
					visibleSide = -1;
		}
	}

	template<typename ClippedWallIt, typename ScreenCoordsIt>
	void genScreenCoords(ClippedWallIt beg, ClippedWallIt end, ScreenCoordsIt out)
	{
		static const float fov = (90.0f * PI) / 180.0f;
		static const float tanHalfFov = std::tan(fov / 2.0f);

		while(beg != end)
		{
			auto& screenCoords = *out;
			const auto& visibleSide = beg->visibleSide;
			const auto& w0 = beg->p0;
			const auto& w1 = beg->p1;
			const auto& texClipLeft = beg->texClipLeft;
			const auto& texClipRight = beg->texClipRight;

			++beg;
			++out;

			screenCoords.sideId = visibleSide;

			if(visibleSide < 0)
			{
				continue;
			}

			float xScaleLeft = w0.getY() / (w0.getX() * tanHalfFov);
			float xScaleRight = w1.getY() / (w1.getX() * tanHalfFov);
			float yScaleLeft = (0.2f * RES_H) / w0.getX();
			float yScaleRight = (0.2f * RES_H) / w1.getX();

			screenCoords.leftX = (int)(xScaleLeft * (RES_W / 2) + (RES_W / 2));
			screenCoords.rightX = (int)(xScaleRight * (RES_W / 2) + (RES_W / 2));
			screenCoords.topLeftY = -(int)(yScaleLeft) + (RES_H / 2);
			screenCoords.bottomLeftY = (int)(yScaleLeft) + (RES_H / 2);
			screenCoords.topRightY = -(int)(yScaleRight) + (RES_H / 2);
			screenCoords.bottomRightY = (int)(yScaleRight) + (RES_H / 2);

			screenCoords.texClipLeft = texClipLeft;
			screenCoords.texClipRight = texClipRight;
			screenCoords.zDistLeft = w0.getX();
			screenCoords.zDistRight = w1.getX();
		}
	}

	template<typename ScreenCoordsIt, typename SideIt>
	void outputToScreenBuffer(ScreenCoordsIt beg, ScreenCoordsIt end, SideIt sideArr,
		int screenWidth, int screenHeight, int bufferPitch, uint8_t* buffer)
	{
		auto drawVerticalLine = [screenWidth, screenHeight, bufferPitch, buffer]
			(int yMin, int yMax, int x, int u, float topTex, float bottomTex,
			const std::shared_ptr<map::Tex>& tex)
		{
			if(x < 0 || x >= screenWidth) return;

			const int yDiff = yMax - yMin;
			const int& texWidth = tex->width;
			const int& texHeight = tex->height;
			float v = topTex * texHeight;
			const float vMax = bottomTex * texHeight;
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
				srcPixStart = tex->data.get() + (u + (((int)v % texHeight) * texWidth)) * 4;
				destPixStart = buffer + (x + y * screenWidth) * 4;
				std::copy(srcPixStart, srcPixStart + 4, destPixStart);
				v += vStep;
			}
		};
		while(beg != end)
		{
			const auto& coords = *beg;
			++beg;

			if(coords.sideId < 0)
				continue;

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
				drawVerticalLine((int)yTop, (int)yBottom, x, (int)(texLeft / oneOverZLeft) % texWidth, texCoord.top, texCoord.bottom, side.midTex);
				yTop += yTopStep;
				yBottom += yBottomStep;

				oneOverZLeft += oneOverZStep;
				texLeft += texStep;
			}
		}
	}
}

using clock_type = std::conditional<
	std::chrono::high_resolution_clock::is_steady,
	std::chrono::high_resolution_clock,
	std::chrono::steady_clock>::type;

template<typename resolution, int targetFps>
int limitFps(clock_type::time_point& startTime, clock_type::time_point& endTime)
{
	startTime = clock_type::now();

	constexpr std::chrono::seconds sec(1);
	constexpr int targetFrameTime = resolution(sec).count() / targetFps;

	int workTime = std::chrono::duration_cast<resolution>(startTime - endTime).count();

	if(workTime < targetFrameTime)
		std::this_thread::sleep_for(resolution(targetFrameTime - workTime));

	endTime = clock_type::now();

	int sleepTime = std::chrono::duration_cast<resolution>(endTime - startTime).count();

	int ticks = sleepTime + workTime;
	return ticks;
}

void program()
{
	std::cout << "SDL_CreateWindow(\"dod test\", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, RES_W, RES_H, 0);" << std::endl;
	std::unique_ptr<SDL_Window, SDL_Destroyer> window(
		SDL_CreateWindow("dod test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
						RES_W, RES_H, 0));

	std::cout << "SDL_CreateRenderer(window.get(), -1, 0);" << std::endl;
	std::unique_ptr<SDL_Renderer, SDL_Destroyer> renderer(
		SDL_CreateRenderer(window.get(), -1, 0));

	std::cout << "SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RES_W, RES_H);" << std::endl;
	std::unique_ptr<SDL_Texture, SDL_Destroyer> texture(
		SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888,
						SDL_TEXTUREACCESS_STREAMING, RES_W, RES_H));

	std::unique_ptr<uint8_t[]> bufData = std::make_unique<uint8_t[]>(RES_W * RES_H * 4);

	bool done = false;

	std::unordered_map<uint32_t, bool> keyMap = {
		{SDLK_UP, false},
		{SDLK_DOWN, false},
		{SDLK_LEFT, false},
		{SDLK_RIGHT, false},
		{SDLK_SPACE, false}
	};

	std::array<map::Wall, 1> walls = {map::Wall{{2.0f, 1.0f}, {1.0f, -1.0f}, 0, 1}};
	std::array<map::Wall, 1> translatedWalls;
	std::array<render::ClippedWall, 1> clippedWalls;
	std::array<render::ScreenCoords, 1> screenCoords;

	auto frontTex = map::loadTextureFromBmp("res/bmp/grass.bmp");
	auto backTex = map::loadTextureFromBmp("res/bmp/brown_brick.bmp");
	map::TexCoord defaultTexCoord = {0.0f, 1.0f, 1.0f, 0.0f};
	std::array<map::Side, 2> sides = {
		map::Side{nullptr, frontTex, nullptr, defaultTexCoord},
		map::Side{nullptr, backTex, nullptr, defaultTexCoord}
	};

	Vec2f playerPos(0.0f);
	float angle = 0.0f;

	auto startTime = clock_type::now();
	auto endTime = clock_type::now();

	int frames = 0;
	int ticks = 0;

	while(!done)
	{
		int frameTicks = limitFps<std::chrono::microseconds, 70>(startTime, endTime);
		ticks += frameTicks;
		double frameTime = frameTicks / 1000000.0;

		if(ticks >= 1000000)
		{
			std::cout << frames << std::endl;
			frames = 0;
			ticks = 0;
		}
		frames++;

		SDL_Event e;
		while(SDL_PollEvent(&e))
		{
			switch(e.type)
			{
				case SDL_QUIT:
					done = true;
					break;
				case SDL_KEYDOWN:
					keyMap[e.key.keysym.sym] = true;
					break;
				case SDL_KEYUP:
					keyMap[e.key.keysym.sym] = false;
					break;
				default:
					break;
			}
		}

		if(keyMap[SDLK_UP])
			playerPos += Vec2f(std::cos(angle), std::sin(angle)) * frameTime * 2.0f;
		if(keyMap[SDLK_DOWN])
			playerPos -= Vec2f(std::cos(angle), std::sin(angle)) * frameTime * 2.0f;

		if(keyMap[SDLK_LEFT])
			angle -= 2.0f * frameTime;
		if(keyMap[SDLK_RIGHT])
			angle += 2.0f * frameTime;

		std::fill(bufData.get(), bufData.get() + RES_W * RES_H * 4, 0x00);

		render::translateWalls(playerPos, angle, walls.begin(), walls.end(), translatedWalls.begin());
		render::clipWalls(translatedWalls.begin(), translatedWalls.end(), clippedWalls.begin());
		render::genScreenCoords(clippedWalls.begin(), clippedWalls.end(), screenCoords.begin());
		render::outputToScreenBuffer(screenCoords.begin(), screenCoords.end(), sides.begin(), RES_W, RES_H, RES_W * 4, bufData.get());

		SDL_UpdateTexture(texture.get(), NULL, bufData.get(), RES_W * 4);

		SDL_RenderClear(renderer.get());
		SDL_RenderCopy(renderer.get(), texture.get(), NULL, NULL);
		SDL_RenderPresent(renderer.get());
	}
}

int main(int argc, char** argv)
{
	std::cout << "SDL_Init(SDL_INIT_VIDEO);" << std::endl;
	SDL_Init(SDL_INIT_VIDEO);

	program();

	std::cout << "SDL_Quit();" << std::endl;
	SDL_Quit();

	return 0;
}
