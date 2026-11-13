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
	// Start position: centered just below the "Meta" layer
	void FindStartPosition();
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

	// Where the car starts (pixels), looking up
	int start_x;
	int start_y;

	// The race waits until the player presses SPACE
	bool started;
};
