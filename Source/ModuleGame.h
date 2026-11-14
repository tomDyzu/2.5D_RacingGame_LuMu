#pragma once

#include "Globals.h"
#include "Module.h"
#include "TileMap.h"

#include "raylib.h"

class Car;

class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status PreUpdate();
	update_status Update();
	update_status PostUpdate();
	bool CleanUp();

	void OnCollision(PhysBody* bodyA, PhysBody* bodyB) override;

private:
	// Start positions from the "Inicio" layer, sorted from the closest to the finish line to the farthest
	void FindStartPositions();
	void ResetCar();

	// Floor with lines: helps to see that the car moves
	void DrawGround() const;

	// Collision lines of the walls (wall_lines)
	void DrawWalls() const;
	void DrawHud() const;
	void UpdateCamera();

public:
	TileMap map;

	// Smooth lines built from the tiles of the "Muros" layer (pixels)
	// The same lines are drawn and used for the collisions
	std::vector<std::vector<Vector2>> wall_lines;

	// Static body with all the walls
	PhysBody* walls;

	Car* car;

	// Grid positions (pixels, center of the car), sorted: [0] is the closest to the finish line
	// The player always starts in the last one. The others are for the bots (later)
	std::vector<Vector2> start_positions;

	// The race waits until the player presses SPACE
	bool started;
};
