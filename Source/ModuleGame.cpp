#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModulePhysics.h"
#include "Car.h"

#include <math.h>

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	car = NULL;
}

ModuleGame::~ModuleGame()
{}

// Load assets
bool ModuleGame::Start()
{
	LOG("Loading game scene");

	App->renderer->SetBackgroundColor(Color{ 60, 120, 60, 255 });

	car = new Car(App->physics, 0, 0);
	UpdateCamera();

	return true;
}

// Unload assets
bool ModuleGame::CleanUp()
{
	LOG("Unloading game scene");

	if (car != NULL)
	{
		car->Destroy(App->physics);
		delete car;
		car = NULL;
	}

	return true;
}

update_status ModuleGame::Update()
{
	// R: back to the start
	if (IsKeyPressed(KEY_R))
	{
		car->Destroy(App->physics);
		delete car;
		car = new Car(App->physics, 0, 0);
	}

	car->Update();

	UpdateCamera();

	// Draw (world pixels) ------------------------------------------------------
	DrawGround();
	car->Draw();

	return UPDATE_CONTINUE;
}

// After the physics debug draw: the HUD goes on top of everything
update_status ModuleGame::PostUpdate()
{
	// HUD in screen pixels: leave the camera and come back
	EndMode2D();
	DrawHud();
	BeginMode2D(App->renderer->camera_2d);

	return UPDATE_CONTINUE;
}

void ModuleGame::UpdateCamera()
{
	float x, y;
	car->GetPosition(x, y);
	App->renderer->camera_2d.target = Vector2{ x, y };
}

void ModuleGame::DrawGround() const
{
	const Camera2D& cam = App->renderer->camera_2d;

	// Only the part of the world we see
	Vector2 top_left = GetScreenToWorld2D(Vector2{ 0.0f, 0.0f }, cam);
	Vector2 bottom_right = GetScreenToWorld2D(Vector2{ (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT }, cam);

	const float cell = 100.0f; // 5 m
	float start_x = floorf(top_left.x / cell) * cell;
	float start_y = floorf(top_left.y / cell) * cell;

	Color line = Color{ 50, 105, 50, 255 };

	for (float x = start_x; x <= bottom_right.x; x += cell)
		DrawLineV(Vector2{ x, top_left.y }, Vector2{ x, bottom_right.y }, line);

	for (float y = start_y; y <= bottom_right.y; y += cell)
		DrawLineV(Vector2{ top_left.x, y }, Vector2{ bottom_right.x, y }, line);

	// Start line
	DrawRectangle(-150, 60, 300, 8, RAYWHITE);
}

void ModuleGame::DrawHud() const
{
	float km_h = car->GetSpeed() * 3.6f;

	DrawText(TextFormat("%3d km/h", (int)roundf(km_h)), 20, SCREEN_HEIGHT - 60, 40, RAYWHITE);
	DrawText(TextFormat("Steer: %+.0f deg", car->steer_angle), 220, SCREEN_HEIGHT - 50, 20, RAYWHITE);

	if (car->IsBraking())
		DrawText("BRAKE", 420, SCREEN_HEIGHT - 50, 20, RED);

	DrawText("A/D or LEFT/RIGHT: steer   S/DOWN/SPACE: brake   R: restart   F1: physics debug", 20, 40, 20, RAYWHITE);
}

void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
}
