#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModulePhysics.h"
#include "Car.h"

#include <math.h>

// Space between the finish line and the front of the car (pixels)
#define START_GAP 20

// Camera zoom: 1.0 = normal, bigger = closer (everything looks bigger), smaller = see more of the map
#define CAMERA_ZOOM 0.75f

// Walls ----------------------------------------------------------------------
// Radius of the curves at the corners of the walls (pixels). Bigger: rounder corners
#define WALL_CORNER_RADIUS 128.0f
// Holes in the walls of the map smaller than this are closed (pixels)
#define WALL_JOIN_DISTANCE 80.0f
// Collision lines of the walls (the real look will come from the map)
#define WALL_LINE_THICKNESS 3.0f
#define WALL_LINE_COLOR YELLOW

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	walls = NULL;
	car = NULL;
	start_x = 0;
	start_y = 0;
	started = false;
}

ModuleGame::~ModuleGame()
{}

// Load assets
bool ModuleGame::Start()
{
	LOG("Loading game scene");

	App->renderer->SetBackgroundColor(Color{ 60, 120, 60, 255 });
	App->renderer->camera_2d.zoom = CAMERA_ZOOM;

	if (!map.Load("Assets/Mapa_Tiled/racing_circuit_F1_Shanghai.tmx"))
		return false;

	// Walls: the tiles of "Muros" are only a guide. We follow them and build smooth lines:
	// diagonal steps become straight lines and the corners become curves
	wall_lines = map.GetLayerLines("Muros", WALL_CORNER_RADIUS, WALL_JOIN_DISTANCE);
	walls = App->physics->CreateStaticLines(wall_lines);
	LOG("Walls: %d lines", (int)wall_lines.size());

	FindStartPosition();
	ResetCar();

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

	App->physics->DestroyBody(walls);
	walls = NULL;

	map.Unload();
	wall_lines.clear();

	return true;
}

void ModuleGame::FindStartPosition()
{
	Rectangle meta;
	if (map.GetLayerBounds("Meta", meta))
	{
		// Centered on the line, the front of the car just below it
		start_x = (int)(meta.x + meta.width / 2.0f);
		start_y = (int)(meta.y + meta.height) + START_GAP + Car::chassis_half_height;
	}
	else
	{
		LOG("No Meta layer: the car starts at the center of the map");
		start_x = map.GetWidthInPixels() / 2;
		start_y = map.GetHeightInPixels() / 2;
	}
}

void ModuleGame::ResetCar()
{
	if (car != NULL)
	{
		car->Destroy(App->physics);
		delete car;
	}

	car = new Car(App->physics, start_x, start_y);
	started = false;
	UpdateCamera();
}

// Runs after the physics step and BEFORE the renderer starts the camera:
// the camera looks at the car where it is drawn this frame
update_status ModuleGame::PreUpdate()
{
	UpdateCamera();

	return UPDATE_CONTINUE;
}

update_status ModuleGame::Update()
{
	// R: back to the start
	if (IsKeyPressed(KEY_R))
		ResetCar();

	// Paused at the start line: the car gets no forces, so it does not move
	if (!started)
	{
		if (IsKeyPressed(KEY_SPACE))
			started = true;
	}
	else
	{
		car->Update();
	}

	// Draw (world pixels) ------------------------------------------------------
	const Camera2D& camera = App->renderer->camera_2d;

	DrawGround();
	map.DrawLayer("Meta", camera);
	DrawWalls();
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
	float start_line_x = floorf(top_left.x / cell) * cell;
	float start_line_y = floorf(top_left.y / cell) * cell;

	Color line = Color{ 50, 105, 50, 255 };

	for (float x = start_line_x; x <= bottom_right.x; x += cell)
		DrawLineV(Vector2{ x, top_left.y }, Vector2{ x, bottom_right.y }, line);

	for (float y = start_line_y; y <= bottom_right.y; y += cell)
		DrawLineV(Vector2{ top_left.x, y }, Vector2{ bottom_right.x, y }, line);
}

void ModuleGame::DrawWalls() const
{
	const Camera2D& cam = App->renderer->camera_2d;

	// Part of the world we see, a bit bigger
	Vector2 top_left = GetScreenToWorld2D(Vector2{ 0.0f, 0.0f }, cam);
	Vector2 bottom_right = GetScreenToWorld2D(Vector2{ (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT }, cam);
	Rectangle view = { top_left.x - 50.0f, top_left.y - 50.0f, bottom_right.x - top_left.x + 100.0f, bottom_right.y - top_left.y + 100.0f };

	for (const std::vector<Vector2>& line : wall_lines)
	{
		for (int i = 0; i + 1 < (int)line.size(); ++i)
		{
			Vector2 a = line[i];
			Vector2 b = line[i + 1];

			// The box around the segment touches the view. Checking only the two ends is not enough:
			// a long straight crosses the screen with both ends outside
			Rectangle box = { MIN(a.x, b.x), MIN(a.y, b.y), fabsf(b.x - a.x) + 1.0f, fabsf(b.y - a.y) + 1.0f };
			if (CheckCollisionRecs(box, view))
				DrawLineEx(a, b, WALL_LINE_THICKNESS, WALL_LINE_COLOR);
		}
	}
}

void ModuleGame::DrawHud() const
{
	float km_h = car->GetSpeed() * 3.6f;

	DrawText(TextFormat("%3d km/h", (int)roundf(km_h)), 20, SCREEN_HEIGHT - 60, 40, RAYWHITE);
	DrawText(TextFormat("Steer: %+.0f deg", car->steer_angle), 220, SCREEN_HEIGHT - 50, 20, RAYWHITE);

	if (car->IsBraking())
		DrawText("BRAKE", 420, SCREEN_HEIGHT - 50, 20, RED);

	DrawText("A/D or LEFT/RIGHT: steer   S/DOWN: brake   R: restart   F1: physics debug", 20, 40, 20, RAYWHITE);

	if (!started)
	{
		const char* text = "PRESS SPACE TO START";
		int font_size = 50;
		int y = SCREEN_HEIGHT / 2 - 150;
		DrawRectangle(0, y - 20, SCREEN_WIDTH, font_size + 40, Color{ 0, 0, 0, 150 });
		DrawText(text, (SCREEN_WIDTH - MeasureText(text, font_size)) / 2, y, font_size, YELLOW);
	}
}

// Called from inside the physics step: only remember things, never create or destroy bodies here
// The car checks its wall contacts by itself (Car::IsTouchingWall)
void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
}
