#ifndef MAP_H
#define MAP_H

#include "Math/Vec2.h"
#include <string>
#include <memory>
#include <fstream>
#include <iostream>

namespace map
{
	struct wall
	{
		Vec2f p0;
		Vec2f p1;
		int frontId;
		int backId;
	};

	struct tex
	{
		static constexpr int BYTES_PER_PIXEL = 4;
		std::string name;
		std::unique_ptr<uint8_t[]> data;
		int width;
		int height;
		int pitch;
	};

	struct texcoord
	{
		float left;
		float right;
		float bottom;
		float top;
	};

	struct side
	{
		std::shared_ptr<tex> topTex;
		std::shared_ptr<tex> midTex;
		std::shared_ptr<tex> botTex;
		texcoord texCoord;
		int sectorId;
	};

	struct sector
	{
		float floorHeight;
		float ceilingHeight;
		std::shared_ptr<tex> floorTex;
		std::shared_ptr<tex> ceilingTex;
	};

	std::shared_ptr<tex> load_texture_from_bmp(const std::string& fileName)
	{
		constexpr char START_ERROR_MSG[] = "Error loadTextureFromBmp: ";

		auto loadedTexture = std::make_shared<tex>();
		loadedTexture->name = fileName;

		std::ifstream file(fileName, std::ios::binary | std::ios::in | std::ios::ate);

		if(!file.is_open())
		{
			std::cerr << START_ERROR_MSG << "The file " << fileName << " did not open." << std::endl;
			return std::shared_ptr<tex>(nullptr);
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
			return std::shared_ptr<tex>(nullptr);
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
			return std::shared_ptr<tex>(nullptr);
		}

		int rowSize = ((bitsPerPixel * width + 31) / 32) * 4;

		loadedTexture->width = width;
		loadedTexture->height = height;
		loadedTexture->pitch = width * tex::BYTES_PER_PIXEL;

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

#endif
