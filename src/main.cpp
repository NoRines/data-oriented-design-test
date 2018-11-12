#include "render.h"
#include "map.h"
#include <iostream>
#include <SDL2/SDL.h>
#include <memory>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <string>
#include <fstream>

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

	std::unique_ptr<uint8_t[]> screenBuf = std::make_unique<uint8_t[]>(RES_W * RES_H * 4);

	render::buffer_width = RES_W;
	render::buffer_height = RES_H;

	bool done = false;

	std::unordered_map<uint32_t, bool> keyMap = {
		{SDLK_UP, false},
		{SDLK_DOWN, false},
		{SDLK_LEFT, false},
		{SDLK_RIGHT, false},
		{SDLK_SPACE, false}
	};

	std::array<map::wall, 7> walls = {
		map::wall{{-4.0f, -3.0f}, {1.0f, -1.0f}, 0, -1},
		map::wall{{1.0f, -1.0f}, {4.0f, 0.0f}, 1, -1},
		map::wall{{4.0f, 0.0f}, {4.0f, 4.0f}, 2, -1},
		map::wall{{4.0f, 4.0f}, {0.0f, 4.0f}, 3, -1},
		map::wall{{0.0f, 4.0f}, {-1.0f, 1.0f}, 4, 5},
		map::wall{{0.0f, 4.0f}, {-4.0f, 2.0f}, 6, -1},
		map::wall{{-4.0f, 2.0f}, {-4.0f, -3.0f}, 7, -1}
	};
	std::array<map::wall, 7> translatedWalls;
	std::array<render::clippedwall, 7> clippedWalls;
	std::array<render::screencoord, 7> screenCoords;

	auto brownBrick = map::load_texture_from_bmp("res/bmp/brown_brick.bmp");
	auto grass = map::load_texture_from_bmp("res/bmp/grass.bmp");
	auto planks = map::load_texture_from_bmp("res/bmp/planks.bmp");
	auto redCarpet = map::load_texture_from_bmp("res/bmp/red_carpet.bmp");
	auto sky = map::load_texture_from_bmp("res/bmp/sky.bmp");
	auto stone_brick = map::load_texture_from_bmp("res/bmp/stone_brick.bmp");

	map::texcoord defaultTexCoord = {0.0f, 1.0f, 0.0f, 1.0f};
	std::array<map::side, 8> sides = {
		map::side{nullptr, brownBrick, nullptr, defaultTexCoord, 0},
		map::side{nullptr, stone_brick, nullptr, defaultTexCoord, 0},
		map::side{nullptr, planks, nullptr, defaultTexCoord, 0},
		map::side{nullptr, sky, nullptr, defaultTexCoord, 0},
		map::side{nullptr, planks, nullptr, defaultTexCoord, 0},
		map::side{nullptr, planks, nullptr, defaultTexCoord, 0},
		map::side{nullptr, stone_brick, nullptr, defaultTexCoord, 0},
		map::side{nullptr, brownBrick, nullptr, defaultTexCoord, 0},
	};

	std::array<map::sector, 1> sectors = {
		map::sector{-1.0f, 1.0f, redCarpet, sky}
	};

	Vec2f playerPos(0.0f);
	float angle = 0.0f;

	auto startTime = clock_type::now();
	auto endTime = clock_type::now();

	int frames = 0;
	int ticks = 0;

	while(!done)
	{
		int frameTicks = limitFps<std::chrono::microseconds, 500>(startTime, endTime);
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

		std::fill(screenBuf.get(), screenBuf.get() + RES_W * RES_H * 4, 0x00);

		auto translatedWallsEndIt = translatedWalls.begin();
		render::translate_walls(playerPos, angle, walls.begin(), walls.end(), translatedWalls.begin(), translatedWallsEndIt);

		auto clippedWallsEndIt = clippedWalls.begin();
		render::clip_walls(translatedWalls.begin(), translatedWallsEndIt, clippedWalls.begin(), clippedWallsEndIt);

		auto screenCoordsEndIt = screenCoords.begin();
		render::gen_screen_coords(clippedWalls.begin(), clippedWallsEndIt, screenCoords.begin(), screenCoordsEndIt, sides.begin(), sectors.begin());

		render::output_to_screen_buffer(screenCoords.begin(), screenCoordsEndIt, sides.begin(), RES_W, RES_H, RES_W * 4, screenBuf.get());

		SDL_UpdateTexture(texture.get(), NULL, screenBuf.get(), RES_W * 4);

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
