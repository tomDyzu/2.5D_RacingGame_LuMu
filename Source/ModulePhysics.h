#pragma once

#include "Module.h"
#include "Globals.h"

#include "box2d/box2d.h"

// Gravity in m/s^2. Screen Y grows DOWNWARDS and we convert meters <-> pixels without flipping Y,
// so a POSITIVE Y gravity makes bodies fall down on screen. That is why the world receives -GRAVITY_Y.
// TODO 2: No gravity to the bottom of the screen
#define GRAVITY_X 0.0f
#define GRAVITY_Y 0.0f

// Fixed time step of world->Step()
#define PHYSICS_DT (1.0f / 60.0f)

// Units: Box2D works in meters (float), raylib draws in pixels (int)
// 20 px = 1 m: the car is 28 x 80 px -> 1.4 x 4 m
#define PIXELS_PER_METER 20.0f // if touched change METER_PER_PIXEL too
#define METER_PER_PIXEL 0.05f  // this is 1 / PIXELS_PER_METER !

#define METERS_TO_PIXELS(m) ((int) floor(PIXELS_PER_METER * (m)))
#define PIXEL_TO_METERS(p)  ((float) METER_PER_PIXEL * (p))

// Small class to return to other modules to track position and rotation of physics bodies
class PhysBody
{
public:
	PhysBody() : body(NULL), listener(NULL)
	{}

	void GetPosition(int& x, int& y) const;
	float GetRotation() const;
	bool Contains(int x, int y) const;
	int RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const;

public:
	b2Body* body;
	// Module that listens to the collisions of this body
	Module* listener;
};

// Module --------------------------------------
class ModulePhysics : public Module, public b2ContactListener
{
public:
	ModulePhysics(Application* app, bool start_enabled = true);
	~ModulePhysics();

	bool Start();
	update_status PreUpdate();
	update_status PostUpdate();
	bool CleanUp();

	PhysBody* CreateCircle(int x, int y, int radius, float density = 1.0f);
	PhysBody* CreateRectangle(int x, int y, int half_width, int half_height, float density = 1.0f);
	PhysBody* CreateRectangleSensor(int x, int y, int half_width, int half_height);
	PhysBody* CreateChain(int x, int y, const int* points, int size);

	// Joins body_b to body_a. anchor_x, anchor_y: point of body_a (pixels, local) where the center of body_b is pinned
	// Angles in radians, relative to body_a. lower == upper locks the joint at that angle
	b2RevoluteJoint* CreateRevoluteJoint(PhysBody* body_a, PhysBody* body_b, int anchor_x, int anchor_y, float lower_angle, float upper_angle);

	void DestroyBody(PhysBody* pbody);

	void BeginContact(b2Contact* contact) override;

private:

	bool debug;
	b2World* world;

	// Static body with no fixtures, needed to create joints like the mouse joint
	b2Body* ground;
	b2MouseJoint* mouse_joint;
};
