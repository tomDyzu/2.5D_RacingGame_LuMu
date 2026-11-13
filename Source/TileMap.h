#pragma once

#include "Globals.h"

#include <string>
#include <vector>

// Small loader for Tiled maps (.tmx)
// Supports: orthogonal maps, tile layers saved as CSV (Tiled default), one tileset with an image
class TileMap
{
public:
	TileMap();

	bool Load(const char* path);
	void Unload();

	// Draws only the tiles the camera can see
	void DrawLayer(const char* layer_name, const Camera2D& camera) const;

	// Solid tiles of a layer joined in big rectangles (pixels): fewer bodies for Box2D
	std::vector<Rectangle> GetLayerRectangles(const char* layer_name) const;

	// Lines of tiles turned into smooth lines (pixels), through the center of the tiles:
	// - follows each line of tiles in order, removes the steps of the diagonals
	// - rounds every corner with a curve of corner_radius pixels (smaller if the sides are short)
	// - joins ends closer than join_distance pixels (small holes in the drawing)
	// A closed line repeats its first point at the end
	std::vector<std::vector<Vector2>> GetLayerLines(const char* layer_name, float corner_radius, float join_distance) const;

	// Rectangle (pixels) that contains all the tiles of a layer. False if the layer is empty or missing
	bool GetLayerBounds(const char* layer_name, Rectangle& bounds) const;

	int GetWidthInPixels() const { return width * tile_width; }
	int GetHeightInPixels() const { return height * tile_height; }

public:
	int width;       // tiles
	int height;      // tiles
	int tile_width;  // pixels
	int tile_height; // pixels

private:
	struct Layer
	{
		std::string name;
		std::vector<int> tiles; // gid of every tile, 0 = empty. Row by row
	};

	const Layer* FindLayer(const char* layer_name) const;

private:
	std::vector<Layer> layers;

	// Tileset
	Texture2D tileset;
	int first_gid;
	int columns;
	int spacing;
	int margin;
};
