#pragma once

#include "Globals.h"
#include "box2d/box2d.h"

class ModulePhysics;
class PhysBody;

// Top view car: Y grows DOWNWARDS on screen, the car looks to its local -Y (up when angle = 0)
// Positive angles turn clockwise on screen: steering right = positive angle

// One wheel: its own Box2D body joined to the chassis
// The wheel kills its sideways velocity (lateral grip): that is what makes the car turn
class Tire
{
public:
	Tire();

	void Create(ModulePhysics* physics, int x, int y);

	b2Vec2 GetForwardNormal() const;
	b2Vec2 GetRightNormal() const;
	b2Vec2 GetLateralVelocity() const;
	b2Vec2 GetForwardVelocity() const;

	// max_lateral_impulse: past it the wheel slides (skids)
	void UpdateFriction(float max_lateral_impulse);

	void Draw() const;

public:
	PhysBody* body;
	bool skidding;
};

class Car
{
public:
	// Wheels, in this order
	enum TireId
	{
		FRONT_LEFT = 0,
		FRONT_RIGHT,
		REAR_LEFT,
		REAR_RIGHT,
		TIRE_COUNT
	};

	// Tuning ---------------------------------------------------------------
	static constexpr float top_speed = 12.0f;          
	static constexpr float engine_acceleration = 0.025f; // m/s^2 at speed 0, less and less near top_speed
	static constexpr float brake_deceleration = 0.1f;  // m/s^2: the brake slows the car little by little
	static constexpr float brake_min_speed = 0.0f;     // m/s: braking never goes below this speed (0 = it can stop)
	static constexpr float max_steer_angle = 12.5f;    // degrees, at speed 0
	static constexpr float min_steer_factor = 0.5f;    // at top_speed only 50% of max_steer_angle
	static constexpr float steer_speed = 30.0f;        // degrees per second the front wheels turn
	static constexpr float front_grip = 2.5f;         // max sideways acceleration the front wheels can hold (m/s^2)
	static constexpr float rear_grip = 45.0f;          // more grip at the back: the car drifts a bit but does not spin
	static constexpr float chassis_density = 0.4f;     // light chassis, heavy wheels: the wheels lead the car
	static constexpr float tire_density = 1.0f;
	static constexpr float wall_speed_factor = 0.7f;   // hitting a wall keeps only 70% of the speed
	static constexpr float wall_bounce_speed = 1.5f;   // m/s the car is pushed away from the wall when it hits it

	// Size in pixels (the drawing and the physics use the same sizes) ------------
	// Body: 2 * chassis_half_width x 2 * chassis_half_height
	// Wheels: 2 * tire_half_width x 2 * tire_half_height, placed at +-tire_offset_x, +-tire_offset_y from the center
	static constexpr int chassis_half_width = 14;
	static constexpr int chassis_half_height = 40;
	static constexpr int tire_half_width = 5;
	static constexpr int tire_half_height = 10;
	static constexpr int tire_offset_x = 20; // from the center of the chassis
	static constexpr int tire_offset_y = 26;

	Car(ModulePhysics* physics, int x, int y);

	// Read the keyboard and apply the forces
	void Update();
	void Draw() const;
	void Destroy(ModulePhysics* physics);

	void GetPosition(float& x, float& y) const; // pixels
	float GetSpeed() const;                     // m/s, forward
	float GetTotalMass() const;
	bool IsBraking() const { return braking; }

	// True if the body is the chassis or one of the wheels
	bool Owns(const PhysBody* body) const;

	// True if any part of the car touches a static body (the walls)
	// normal: direction from the wall to the car
	bool IsTouchingWall(b2Vec2& normal) const;



public:
	PhysBody* chassis;
	Tire tires[TIRE_COUNT];

	// Only the front wheels can turn. The rear ones are joints locked at angle 0
	b2RevoluteJoint* front_left_joint;
	b2RevoluteJoint* front_right_joint;

	float steer_angle; // degrees, current angle of the front wheels
	bool braking;

	// Touching a wall in the last Update(). Only a NEW contact cuts the speed:
	// sliding along a wall (or along the steps of a diagonal wall) does not
	bool touching_wall;
};
