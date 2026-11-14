#include "TileMap.h"

#include <stdlib.h>
#include <math.h>
#include <algorithm>

// Tiled keeps the flip flags in the 3 highest bits of the gid
#define TILED_GID_MASK 0x1FFFFFFF

// Value of an attribute inside an XML tag: <map width="250"> -> "250"
// The space before the name avoids matching "tilewidth" when we look for "width"
static std::string GetAttribute(const std::string& tag, const char* name)
{
	std::string key = std::string(" ") + name + "=\"";
	size_t start = tag.find(key);
	if (start == std::string::npos)
		return "";

	start += key.size();
	size_t end = tag.find('"', start);
	if (end == std::string::npos)
		return "";

	return tag.substr(start, end - start);
}

static int GetIntAttribute(const std::string& tag, const char* name, int default_value)
{
	std::string value = GetAttribute(tag, name);
	return value.empty() ? default_value : atoi(value.c_str());
}

// Whole tag that starts at "<name", from position "from"
static std::string FindTag(const std::string& text, const char* name, size_t from, size_t& tag_start)
{
	tag_start = text.find(std::string("<") + name, from);
	if (tag_start == std::string::npos)
		return "";

	size_t end = text.find('>', tag_start);
	if (end == std::string::npos)
		return "";

	return text.substr(tag_start, end - tag_start + 1);
}

TileMap::TileMap()
	: width(0), height(0), tile_width(0), tile_height(0)
	, tileset{ 0 }, first_gid(1), columns(1), spacing(0), margin(0)
{
}

bool TileMap::Load(const char* path)
{
	char* file = LoadFileText(path);
	if (file == NULL)
	{
		LOG("Could not load map %s", path);
		return false;
	}

	std::string text = file;
	UnloadFileText(file);

	size_t pos = 0;

	// Map ------------------------------------------------------------------
	std::string map_tag = FindTag(text, "map", 0, pos);
	width = GetIntAttribute(map_tag, "width", 0);
	height = GetIntAttribute(map_tag, "height", 0);
	tile_width = GetIntAttribute(map_tag, "tilewidth", 32);
	tile_height = GetIntAttribute(map_tag, "tileheight", 32);

	// Tileset (only the first one) -------------------------------------------
	std::string tileset_tag = FindTag(text, "tileset", 0, pos);
	if (!tileset_tag.empty())
	{
		first_gid = GetIntAttribute(tileset_tag, "firstgid", 1);
		columns = GetIntAttribute(tileset_tag, "columns", 1);
		spacing = GetIntAttribute(tileset_tag, "spacing", 0);
		margin = GetIntAttribute(tileset_tag, "margin", 0);

		std::string image_tag = FindTag(text, "image", pos, pos);
		std::string source = GetAttribute(image_tag, "source");

		// The image path is relative to the .tmx file
		std::string image_path = std::string(GetDirectoryPath(path)) + "/" + source;
		tileset = LoadTexture(image_path.c_str());
	}

	// Layers ---------------------------------------------------------------
	pos = 0;
	while (true)
	{
		size_t layer_start;
		std::string layer_tag = FindTag(text, "layer", pos, layer_start);
		if (layer_tag.empty())
			break;

		Layer layer;
		layer.name = GetAttribute(layer_tag, "name");
		layer.tiles.reserve(width * height);

		// CSV between <data encoding="csv"> and </data>
		size_t data_start;
		std::string data_tag = FindTag(text, "data", layer_start, data_start);
		size_t csv_start = data_start + data_tag.size();
		size_t csv_end = text.find("</data>", csv_start);

		if (GetAttribute(data_tag, "encoding") != "csv" || csv_end == std::string::npos)
		{
			LOG("Layer %s is not saved as CSV: skipped", layer.name.c_str());
			pos = layer_start + 1;
			continue;
		}

		const char* c = text.c_str() + csv_start;
		const char* end = text.c_str() + csv_end;
		while (c < end)
		{
			char* next;
			unsigned long gid = strtoul(c, &next, 10);
			if (next == c) // not a number: comma, new line...
			{
				++c;
				continue;
			}
			layer.tiles.push_back((int)(gid & TILED_GID_MASK));
			c = next;
		}

		if ((int)layer.tiles.size() != width * height)
			LOG("Layer %s has %d tiles, expected %d", layer.name.c_str(), (int)layer.tiles.size(), width * height);

		layer.tiles.resize(width * height, 0);
		layers.push_back(layer);

		pos = csv_end;
	}

	LOG("Loaded map %s: %d x %d tiles, %d layers", path, width, height, (int)layers.size());
	return true;
}

void TileMap::Unload()
{
	if (tileset.id != 0)
		UnloadTexture(tileset);
	tileset = Texture2D{ 0 };

	layers.clear();
}

const TileMap::Layer* TileMap::FindLayer(const char* layer_name) const
{
	for (const Layer& layer : layers)
	{
		if (layer.name == layer_name)
			return &layer;
	}
	return NULL;
}

void TileMap::DrawLayer(const char* layer_name, const Camera2D& camera) const
{
	const Layer* layer = FindLayer(layer_name);
	if (layer == NULL || tileset.id == 0)
		return;

	// Tiles inside the screen
	Vector2 top_left = GetScreenToWorld2D(Vector2{ 0.0f, 0.0f }, camera);
	Vector2 bottom_right = GetScreenToWorld2D(Vector2{ (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT }, camera);

	int x0 = MAX(0, (int)(top_left.x / tile_width) - 1);
	int y0 = MAX(0, (int)(top_left.y / tile_height) - 1);
	int x1 = MIN(width - 1, (int)(bottom_right.x / tile_width) + 1);
	int y1 = MIN(height - 1, (int)(bottom_right.y / tile_height) + 1);

	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			int gid = layer->tiles[y * width + x];
			if (gid == 0)
				continue;

			int id = gid - first_gid;
			Rectangle source = {
				(float)(margin + (id % columns) * (tile_width + spacing)),
				(float)(margin + (id / columns) * (tile_height + spacing)),
				(float)tile_width,
				(float)tile_height
			};

			DrawTextureRec(tileset, source, Vector2{ (float)(x * tile_width), (float)(y * tile_height) }, WHITE);
		}
	}
}

std::vector<Rectangle> TileMap::GetLayerRectangles(const char* layer_name) const
{
	std::vector<Rectangle> rects;

	const Layer* layer = FindLayer(layer_name);
	if (layer == NULL)
		return rects;

	std::vector<bool> used(width * height, false);

	auto solid = [&](int x, int y)
	{
		int i = y * width + x;
		return layer->tiles[i] != 0 && !used[i];
	};

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (!solid(x, y))
				continue;

			// Grow to the right
			int x2 = x;
			while (x2 + 1 < width && solid(x2 + 1, y))
				++x2;

			// Grow down while the whole row below is solid
			int y2 = y;
			while (y2 + 1 < height)
			{
				bool full_row = true;
				for (int xx = x; xx <= x2 && full_row; ++xx)
					full_row = solid(xx, y2 + 1);

				if (!full_row)
					break;
				++y2;
			}

			for (int yy = y; yy <= y2; ++yy)
				for (int xx = x; xx <= x2; ++xx)
					used[yy * width + xx] = true;

			rects.push_back(Rectangle{
				(float)(x * tile_width),
				(float)(y * tile_height),
				(float)((x2 - x + 1) * tile_width),
				(float)((y2 - y + 1) * tile_height)
			});
		}
	}

	return rects;
}

std::vector<Vector2> TileMap::GetLayerTileCenters(const char* layer_name) const
{
	std::vector<Vector2> centers;

	const Layer* layer = FindLayer(layer_name);
	if (layer == NULL)
		return centers;

	for (int i = 0; i < width * height; ++i)
	{
		if (layer->tiles[i] != 0)
			centers.push_back(Vector2{ (i % width + 0.5f) * tile_width, (i / width + 0.5f) * tile_height });
	}

	return centers;
}

bool TileMap::GetLayerBounds(const char* layer_name, Rectangle& bounds) const
{
	const Layer* layer = FindLayer(layer_name);
	if (layer == NULL)
		return false;

	int min_x = width, min_y = height, max_x = -1, max_y = -1;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (layer->tiles[y * width + x] == 0)
				continue;

			min_x = MIN(min_x, x);
			min_y = MIN(min_y, y);
			max_x = MAX(max_x, x);
			max_y = MAX(max_y, y);
		}
	}

	if (max_x < 0)
		return false;

	bounds = Rectangle{
		(float)(min_x * tile_width),
		(float)(min_y * tile_height),
		(float)((max_x - min_x + 1) * tile_width),
		(float)((max_y - min_y + 1) * tile_height)
	};
	return true;
}

// Lines ---------------------------------------------------------------------

static float Distance(Vector2 a, Vector2 b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	return sqrtf(dx * dx + dy * dy);
}

// Removes the points in the middle of a straight part (same direction before and after)
static void RemoveStraightPoints(std::vector<Vector2>& points, bool closed)
{
	if (points.size() < 3)
		return;

	std::vector<Vector2> result;
	int count = (int)points.size();

	for (int i = 0; i < count; ++i)
	{
		bool is_end = !closed && (i == 0 || i == count - 1);
		if (!is_end)
		{
			Vector2 prev = points[(i - 1 + count) % count];
			Vector2 cur = points[i];
			Vector2 next = points[(i + 1) % count];

			float cross = (cur.x - prev.x) * (next.y - cur.y) - (cur.y - prev.y) * (next.x - cur.x);
			float dot = (cur.x - prev.x) * (next.x - cur.x) + (cur.y - prev.y) * (next.y - cur.y);
			if (fabsf(cross) < 0.001f && dot > 0.0f)
				continue; // straight: the point is not needed
		}
		result.push_back(points[i]);
	}

	points = result;
}

// Replaces every corner with a curve (quadratic Bezier between the two sides)
static std::vector<Vector2> RoundCorners(const std::vector<Vector2>& points, bool closed, float radius)
{
	int count = (int)points.size();
	if (count < 3)
		return points;

	std::vector<Vector2> result;

	for (int i = 0; i < count; ++i)
	{
		bool is_end = !closed && (i == 0 || i == count - 1);
		if (is_end)
		{
			result.push_back(points[i]);
			continue;
		}

		Vector2 prev = points[(i - 1 + count) % count];
		Vector2 cur = points[i];
		Vector2 next = points[(i + 1) % count];

		float len_prev = Distance(cur, prev);
		float len_next = Distance(cur, next);

		// Half of each side at most: two curves never cross each other
		// Two short sides in a row (the steps of a diagonal): their curves join into one big curve
		float d = MIN(radius, MIN(len_prev, len_next) / 2.0f);

		Vector2 start = { cur.x + (prev.x - cur.x) / len_prev * d, cur.y + (prev.y - cur.y) / len_prev * d };
		Vector2 end = { cur.x + (next.x - cur.x) / len_next * d, cur.y + (next.y - cur.y) / len_next * d };

		// More points for bigger curves
		int segments = MAX(2, (int)(d / 8.0f));
		for (int s = 0; s <= segments; ++s)
		{
			float t = (float)s / (float)segments;
			float a = (1.0f - t) * (1.0f - t);
			float b = 2.0f * (1.0f - t) * t;
			float c = t * t;
			Vector2 p = { a * start.x + b * cur.x + c * end.x, a * start.y + b * cur.y + c * end.y };

			// Box2D does not like points too close to each other
			if (result.empty() || Distance(result.back(), p) > 1.0f)
				result.push_back(p);
		}
	}

	if (closed && !result.empty())
		result.push_back(result.front());

	return result;
}

std::vector<std::vector<Vector2>> TileMap::GetLayerLines(const char* layer_name, float corner_radius, float join_distance) const
{
	std::vector<std::vector<Vector2>> lines;

	const Layer* layer = FindLayer(layer_name);
	if (layer == NULL)
		return lines;

	auto solid = [&](int x, int y)
	{
		return x >= 0 && y >= 0 && x < width && y < height && layer->tiles[y * width + x] != 0;
	};

	// Neighbours of a tile. A diagonal neighbour only counts if the two tiles only touch by the corner
	// (if not, the path through the side tile is the right one)
	auto neighbours = [&](int index, int* out)
	{
		int x = index % width;
		int y = index / width;
		int n = 0;
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if ((dx == 0 && dy == 0) || !solid(x + dx, y + dy))
					continue;
				if (dx != 0 && dy != 0 && (solid(x + dx, y) || solid(x, y + dy)))
					continue;
				out[n++] = (y + dy) * width + (x + dx);
			}
		}
		return n;
	};

	struct TileLine
	{
		std::vector<int> tiles;
		bool closed;
	};
	std::vector<TileLine> tile_lines;
	std::vector<bool> visited(width * height, false);

	// Follows the tiles from start until the end of the line (or back to start: closed line)
	auto trace = [&](int start)
	{
		TileLine line;
		line.closed = false;

		int current = start;
		while (true)
		{
			visited[current] = true;
			line.tiles.push_back(current);

			int next_tiles[8];
			int n = neighbours(current, next_tiles);
			int next = -1;
			for (int i = 0; i < n && next == -1; ++i)
			{
				if (!visited[next_tiles[i]])
					next = next_tiles[i];
			}

			if (next == -1)
			{
				// Back next to the first tile: it is a loop
				for (int i = 0; i < n; ++i)
				{
					if (next_tiles[i] == start && line.tiles.size() > 2)
						line.closed = true;
				}
				break;
			}
			current = next;
		}

		tile_lines.push_back(line);
	};

	// First the lines with ends, then the loops
	for (int i = 0; i < width * height; ++i)
	{
		int dummy[8];
		if (layer->tiles[i] != 0 && !visited[i] && neighbours(i, dummy) <= 1)
			trace(i);
	}
	for (int i = 0; i < width * height; ++i)
	{
		if (layer->tiles[i] != 0 && !visited[i])
			trace(i);
	}

	// Tile index -> center of the tile in pixels
	std::vector<std::vector<Vector2>> points;
	std::vector<bool> closed;
	for (const TileLine& line : tile_lines)
	{
		std::vector<Vector2> p;
		for (int index : line.tiles)
			p.push_back(Vector2{ (index % width + 0.5f) * tile_width, (index / width + 0.5f) * tile_height });
		points.push_back(p);
		closed.push_back(line.closed);
	}

	// Join the ends that are very close (holes of 1 or 2 tiles in the map)
	bool joined = true;
	while (joined)
	{
		joined = false;
		for (int a = 0; a < (int)points.size() && !joined; ++a)
		{
			if (closed[a] || points[a].size() < 2)
				continue;

			// The end of a with its own start: close it
			if (Distance(points[a].back(), points[a].front()) <= join_distance && points[a].size() > 2)
			{
				closed[a] = true;
				joined = true;
				break;
			}

			for (int b = 0; b < (int)points.size() && !joined; ++b)
			{
				if (a == b || closed[b] || points[b].empty())
					continue;

				std::vector<Vector2>& pa = points[a];
				std::vector<Vector2>& pb = points[b];

				// Try the 4 combinations of ends: turn the lines so that pa.back() meets pb.front()
				for (int combo = 0; combo < 4 && !joined; ++combo)
				{
					std::vector<Vector2> ta = pa;
					std::vector<Vector2> tb = pb;
					if (combo & 1) std::reverse(ta.begin(), ta.end());
					if (combo & 2) std::reverse(tb.begin(), tb.end());

					if (Distance(ta.back(), tb.front()) <= join_distance)
					{
						ta.insert(ta.end(), tb.begin(), tb.end());
						pa = ta;
						pb.clear();
						joined = true;
					}
				}
			}
		}
	}

	for (int i = 0; i < (int)points.size(); ++i)
	{
		if (points[i].size() < 2)
			continue;

		RemoveStraightPoints(points[i], closed[i]);
		lines.push_back(RoundCorners(points[i], closed[i], corner_radius));
	}

	return lines;
}
