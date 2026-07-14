module;

// Normal imports
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "stb_image_write.h"

/// <summary>
/// Makes the perlin noise used by the Perlin Noise and Terrain scenes.
///
/// Each grid point gets a seeded random value, that gets smoothed with its neighbors, points
/// in between are cosine interpolated, and a few octaves get summed together.
///
/// The raw values rarely reach the full [-1,1] range so the image looks washed out. To fix
/// that we find the real min and max and stretch everything to fill [0,1], which brings back
/// proper blacks and whites.
///
/// Saved as noise.raw (the terrain reads this as a heightmap) and noise.jpg (visual check).
/// The float heights also stay in memory so the terrain can rebuild without reading the file. :)
/// </summary>
export module TerrainGen;

import DebugUtilities;
import <cstdint>;

// Generation settings
export struct NoiseParams
{
	int width = 512;           // Output size in pixels. Bigger = slower to generate.
	int height = 512;
	int octaves = 6;           // How many noise layers get summed per point.
	float wavelength = 128.0f; // Bigger = broader features.
	float gain = 0.5f;         // Amplitude multiplier per octave.
	float lacunarity = 2.0f;   // Frequency multiplier per octave.
	uint32_t seed = 0;         // 0 = seed from the time. Positive values only!
};

// The result. heights01 is the float version for the terrain, gray8 is the byte version for
// the file exports and the texture.
export struct NoiseMap
{
	int width = 0;
	int height = 0;
	uint32_t seedUsed = 0;
	double generationSeconds = 0.0;
	std::vector<float> heights01;
	std::vector<uint8_t> gray8;
};

// Publically exported functions
export bool Noise_Generate(const NoiseParams& _params, NoiseMap& _out);
export bool Noise_SaveRAW(const NoiseMap& _map, const char* _filePath);
export bool Noise_SaveJPG(const NoiseMap& _map, const char* _filePath);

// ==========================================================================================
// Internal noise functions. They all take the seed so one generation stays consistent.
// ==========================================================================================

// Seeded random value in roughly (-1, 1] for a grid point.
static double RandomValuePoint(int _x, int _y, uint32_t _seed)
{
	uint32_t noise = (uint32_t)_x + (uint32_t)_y * _seed;
	noise = (noise << 13) ^ noise;

	// The & keeps this in [0, 2^31)
	uint32_t t = (noise * (noise * noise * 15731u + 789221u) + 1376312589u) & 0x7fffffffu;

	// Map that to (-1, 1]. The magic number is 2 / 2^31.
	return 1.0 - (double)t * 0.93132257461548515625e-9;
}

// Average of a point with its 8 neighbors. Corners /16, sides /8, center /4 (adds up to 1).
static double Smooth(int _x, int _y, uint32_t _seed)
{
	double corners = (RandomValuePoint(_x - 1, _y - 1, _seed) + RandomValuePoint(_x + 1, _y - 1, _seed) +
		RandomValuePoint(_x - 1, _y + 1, _seed) + RandomValuePoint(_x + 1, _y + 1, _seed)) / 16.0;
	double sides = (RandomValuePoint(_x - 1, _y, _seed) + RandomValuePoint(_x + 1, _y, _seed) +
		RandomValuePoint(_x, _y - 1, _seed) + RandomValuePoint(_x, _y + 1, _seed)) / 8.0;
	double center = RandomValuePoint(_x, _y, _seed) / 4.0;

	return corners + sides + center;
}

// Cosine interpolation, looks smoother than linear for basically free.
static double CosineInterpolate(double _point1, double _point2, double _fract)
{
	constexpr double kPi = 3.14159265358979323846;
	double fract2 = (1.0 - std::cos(_fract * kPi)) / 2.0;
	return (_point1 * (1.0 - fract2) + _point2 * fract2);
}

// Samples the smoothed grid at a spot between grid points.
static double SmoothInterpolate(double _x, double _y, uint32_t _seed)
{
	int truncatedX = (int)_x;
	int truncatedY = (int)_y;

	// Just the bit after the decimal point
	double fractX = _x - (double)truncatedX;
	double fractY = _y - (double)truncatedY;

	// The 4 grid corners around this spot
	double v1 = Smooth(truncatedX, truncatedY, _seed);
	double v2 = Smooth(truncatedX + 1, truncatedY, _seed);
	double v3 = Smooth(truncatedX, truncatedY + 1, _seed);
	double v4 = Smooth(truncatedX + 1, truncatedY + 1, _seed);

	// Blend across X on both rows, then across Y between them
	double interpolate1 = CosineInterpolate(v1, v2, fractX);
	double interpolate2 = CosineInterpolate(v3, v4, fractX);

	return CosineInterpolate(interpolate1, interpolate2, fractY);
}

// Sums the octaves for one point. Dividing by the total amplitude keeps it in range.
static double TotalNoisePerPoint(int _x, int _y, const NoiseParams& _params, uint32_t _seed)
{
	float maxValue = 0.0f;
	double total = 0.0;

	for (int i = 0; i < _params.octaves; i++)
	{
		float frequency = std::pow(_params.lacunarity, (float)i) / _params.wavelength;
		float amplitude = std::pow(_params.gain, (float)i);
		maxValue += amplitude;

		total += SmoothInterpolate(_x * frequency, _y * frequency, _seed) * amplitude;
	}

	return total / maxValue;
}

// ==========================================================================================
// Public implementation
// ==========================================================================================

bool Noise_Generate(const NoiseParams& _params, NoiseMap& _out)
{
	if (_params.width <= 0 || _params.height <= 0 || _params.octaves <= 0 ||
		_params.wavelength <= 0.0f || _params.gain <= 0.0f || _params.lacunarity <= 0.0f)
	{
		LogWarning("Noise_Generate: invalid parameters, generation skipped.");
		return false;
	}

	const auto timeStart = std::chrono::steady_clock::now();

	_out.width = _params.width;
	_out.height = _params.height;

	// Use the given seed, or make one from the time if it's 0
	_out.seedUsed = (_params.seed != 0)
		? _params.seed
		: (uint32_t)std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

	const size_t pixelCount = (size_t)_params.width * (size_t)_params.height;

	// 1. Generate the raw noise (roughly [-1, 1])
	std::vector<float> raw(pixelCount);
	for (int y = 0; y < _params.height; y++)
		for (int x = 0; x < _params.width; x++)
			raw[(size_t)y * _params.width + x] =
			(float)TotalNoisePerPoint(x, y, _params, _out.seedUsed);

	// 2. Find the real min and max (usually much narrower than [-1, 1])
	const auto [minIt, maxIt] = std::minmax_element(raw.begin(), raw.end());
	const float minV = *minIt;
	const float maxV = *maxIt;
	const float invRange = (maxV > minV) ? 1.0f / (maxV - minV) : 0.0f;

	// 3. Stretch to fill [0,1], and make the byte version at the same time
	_out.heights01.resize(pixelCount);
	_out.gray8.resize(pixelCount);
	for (size_t i = 0; i < pixelCount; ++i)
	{
		const float normalized = (raw[i] - minV) * invRange;
		_out.heights01[i] = normalized;
		_out.gray8[i] = (uint8_t)(normalized * 255.0f);
	}

	_out.generationSeconds = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - timeStart).count();

	Log(("Noise generated: " + std::to_string(_params.width) + "x" + std::to_string(_params.height) +
		" seed=" + std::to_string(_out.seedUsed) +
		" in " + std::to_string(_out.generationSeconds) + "s").c_str());
	return true;
}

// Makes sure the map has data and the target folder exists before saving.
static bool PrepareSavePath(const NoiseMap& _map, const char* _filePath)
{
	if (_map.gray8.empty() || _map.width <= 0 || _map.height <= 0)
	{
		LogWarning("Noise save: map is empty, call Noise_Generate first.");
		return false;
	}

	std::error_code ec;
	const std::filesystem::path parent = std::filesystem::path(_filePath).parent_path();
	if (!parent.empty())
		std::filesystem::create_directories(parent, ec); // Does nothing if it's already there

	return true;
}

// Saves the raw bytes, the format the terrain heightmap loader reads.
bool Noise_SaveRAW(const NoiseMap& _map, const char* _filePath)
{
	if (!PrepareSavePath(_map, _filePath)) return false;

	std::ofstream file(_filePath, std::ios::binary);
	if (!file)
	{
		LogWarning((std::string("Noise_SaveRAW: cannot open for writing: ") + _filePath).c_str());
		return false;
	}

	file.write(reinterpret_cast<const char*>(_map.gray8.data()), (std::streamsize)_map.gray8.size());
	return (bool)file;
}

// Saves a JPG so the noise can be viewed in the folder.
bool Noise_SaveJPG(const NoiseMap& _map, const char* _filePath)
{
	if (!PrepareSavePath(_map, _filePath)) return false;

	if (!stbi_write_jpg(_filePath, _map.width, _map.height, 1, _map.gray8.data(), 100))
	{
		LogWarning((std::string("Noise_SaveJPG: stbi_write_jpg failed: ") + _filePath).c_str());
		return false;
	}
	return true;
}
