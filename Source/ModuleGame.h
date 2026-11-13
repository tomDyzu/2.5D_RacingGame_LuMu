#pragma once

#include "Globals.h"
#include "Module.h"

#include "raylib.h"

class Car;

class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status Update();
	update_status PostUpdate();
	bool CleanUp();

	void OnCollision(PhysBody* bodyA, PhysBody* bodyB) override;

private:
	// Floor with lines: without a map it is the only way to see that the car moves
	void DrawGround() const;
	void DrawHud() const;
	void UpdateCamera();

public:
	Car* car;
};
