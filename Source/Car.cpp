#include "Car.h"
#include "ModulePhysics.h"

#include <math.h>

// Helpers -----------------------------------------------------------------

// Local point of a body (pixels) -> world point (pixels, float)
static Vector2 LocalToWorld(const b2Body* body, float local_x, float local_y)
{
	b2Vec2 p = body->GetWorldPoint(b2Vec2(PIXEL_TO_METERS(local_x), PIXEL_TO_METERS(local_y)));
	return Vector2{ p.x * PIXELS_PER_METER, p.y * PIXELS_PER_METER };
}

// Rectangle centered at a local point of a body, rotated with the body
static void DrawBodyRectangle(const b2Body* body, float local_x, float local_y, float width, float height, Color color)
{
	Vector2 p = LocalToWorld(body, local_x, local_y);
	Rectangle rec = { p.x, p.y, width, height };
	Vector2 origin = { width / 2.0f, height / 2.0f };
	DrawRectanglePro(rec, origin, body->GetAngle() * RAD2DEG, color);
}

// Tire --------------------------------------------------------------------

Tire::Tire() : body(NULL), skidding(false)
{
}

void Tire::Create(ModulePhysics* physics, int x, int y)
{
	body = physics->CreateRectangle(x, y, Car::tire_half_width, Car::tire_half_height, Car::tire_density);
}

// The car looks to its local -Y
b2Vec2 Tire::GetForwardNormal() const
{
	return body->body->GetWorldVector(b2Vec2(0.0f, -1.0f));
}

b2Vec2 Tire::GetRightNormal() const
{
	return body->body->GetWorldVector(b2Vec2(1.0f, 0.0f));
}

// Part of the velocity that goes sideways
b2Vec2 Tire::GetLateralVelocity() const
{
	b2Vec2 right = GetRightNormal();
	return b2Dot(right, body->body->GetLinearVelocity()) * right;
}

// Part of the velocity that goes where the wheel looks
b2Vec2 Tire::GetForwardVelocity() const
{
	b2Vec2 forward = GetForwardNormal();
	return b2Dot(forward, body->body->GetLinearVelocity()) * forward;
}

void Tire::UpdateFriction(float max_lateral_impulse)
{
	b2Body* b = body->body;

	// A wheel rolls forward but does not want to move sideways:
	// impulse that cancels the lateral velocity (J = m * dv)
	b2Vec2 impulse = b->GetMass() * -GetLateralVelocity();

	// Too much sideways speed: the wheel loses grip and slides
	skidding = false;
	if (impulse.Length() > max_lateral_impulse)
	{
		impulse *= max_lateral_impulse / impulse.Length();
		skidding = true;
	}

	b->ApplyLinearImpulse(impulse, b->GetWorldCenter(), true);

	// Wheels do not spin around themselves on the floor
	b->ApplyAngularImpulse(0.1f * b->GetInertia() * -b->GetAngularVelocity(), true);
}

void Tire::Draw() const
{
	Color color = skidding ? Color{ 90, 90, 90, 255 } : Color{ 25, 25, 25, 255 };
	DrawBodyRectangle(body->body, 0.0f, 0.0f, Car::tire_half_width * 2.0f, Car::tire_half_height * 2.0f, color);
}

// Car ---------------------------------------------------------------------

Car::Car(ModulePhysics* physics, int x, int y)
	: chassis(NULL)
	, front_left_joint(NULL)
	, front_right_joint(NULL)
	, steer_angle(0.0f)
	, braking(false)
	, touching_wall(false)
	, wall_align_left(0.0f)
{
	chassis = physics->CreateRectangle(x, y, chassis_half_width, chassis_half_height, chassis_density);

	// Local position of every wheel, same order as TireId
	const int offsets[TIRE_COUNT][2] = {
		{ -tire_offset_x, -tire_offset_y }, // FRONT_LEFT
		{ tire_offset_x, -tire_offset_y },  // FRONT_RIGHT
		{ -tire_offset_x, tire_offset_y },  // REAR_LEFT
		{ tire_offset_x, tire_offset_y }    // REAR_RIGHT
	};

	for (int i = 0; i < TIRE_COUNT; ++i)
	{
		tires[i].Create(physics, x + offsets[i][0], y + offsets[i][1]);

		// Every joint starts locked at 0: the rear ones stay like this forever
		b2RevoluteJoint* joint = physics->CreateRevoluteJoint(chassis, tires[i].body, offsets[i][0], offsets[i][1], 0.0f, 0.0f);

		if (i == FRONT_LEFT)
			front_left_joint = joint;
		else if (i == FRONT_RIGHT)
			front_right_joint = joint;
	}
}

void Car::Update()
{
	b2Body* body = chassis->body;
	float mass = GetTotalMass();

	// Crash against a wall: lose speed --------------------------------------
	b2Vec2 wall_normal;
	bool touching = IsTouchingWall(wall_normal);
	if (touching && !touching_wall)
	{
		// Direction along the wall, the side the car is looking to
		b2Vec2 forward = body->GetWorldVector(b2Vec2(0.0f, -1.0f));
		b2Vec2 tangent(-wall_normal.y, wall_normal.x);
		if (b2Dot(tangent, forward) < 0.0f)
			tangent = -tangent;

		// Half the speed, only along the wall, and a small push out
		float speed_along = wall_speed_factor * b2Dot(body->GetLinearVelocity(), tangent);
		if (speed_along < 0.0f)
			speed_along = 0.0f;
		b2Vec2 v = speed_along * tangent + wall_bounce_speed * wall_normal;

		body->SetLinearVelocity(v);
		body->SetAngularVelocity(0.0f);
		for (int i = 0; i < TIRE_COUNT; ++i)
			tires[i].body->body->SetLinearVelocity(v);

		// Turn the car until it looks along the wall. The car looks to its local -Y:
		// forward = (sin(angle), -cos(angle)) -> angle = atan2(x, -y)
		float target = atan2f(tangent.x, -tangent.y);
		float diff = target - body->GetAngle();
		while (diff > b2_pi)
			diff -= 2.0f * b2_pi;
		while (diff < -b2_pi)
			diff += 2.0f * b2_pi;
		wall_align_left = diff * RAD2DEG;
	}
	touching_wall = touching;

	// A bit of the turn every frame
	if (wall_align_left != 0.0f)
	{
		float max_step = wall_align_speed * PHYSICS_DT;
		float step = wall_align_left;
		if (step > max_step)
			step = max_step;
		else if (step < -max_step)
			step = -max_step;

		Rotate(step * DEGTORAD);
		wall_align_left -= step;
	}

	float speed = GetSpeed();

	// Steering ---------------------------------------------------------------
	float steer_input = 0.0f;
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
		steer_input -= 1.0f;
	if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
		steer_input += 1.0f;

	// Fast car: smaller steering angle, if not it would spin
	float speed_factor = fabsf(speed) / top_speed;
	if (speed_factor > 1.0f)
		speed_factor = 1.0f;
	float steer_lock = max_steer_angle * (1.0f - (1.0f - min_steer_factor) * speed_factor);

	// Turn the wheels little by little towards the target angle
	float target_angle = steer_input * steer_lock;
	float max_step = steer_speed * PHYSICS_DT;
	float diff = target_angle - steer_angle;
	if (diff > max_step)
		diff = max_step;
	else if (diff < -max_step)
		diff = -max_step;
	steer_angle += diff;

	// Lower limit == upper limit: the joint holds the wheel at exactly that angle
	float angle = steer_angle * DEGTORAD;
	front_left_joint->SetLimits(angle, angle);
	front_right_joint->SetLimits(angle, angle);

	// Grip of the wheels ------------------------------------------------------
	// Each wheel holds a quarter of the car: max sideways force = (m / 4) * grip
	// Impulse per step = force * dt
	float quarter_mass = mass / (float)TIRE_COUNT;
	for (int i = 0; i < TIRE_COUNT; ++i)
	{
		float grip = (i == FRONT_LEFT || i == FRONT_RIGHT) ? front_grip : rear_grip;
		tires[i].UpdateFriction(quarter_mass * grip * PHYSICS_DT);
	}

	// Engine and brake ----------------------------------------------------------
	braking = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);

	b2Vec2 forward = body->GetWorldVector(b2Vec2(0.0f, -1.0f));

	if (braking)
	{
		// Brake against the motion. Never enough to go backwards
		if (speed > 0.0f)
		{
			float force = mass * brake_deceleration;
			float force_to_stop = mass * speed / PHYSICS_DT;
			if (force > force_to_stop)
				force = force_to_stop;

			body->ApplyForceToCenter(-force * forward, true);
		}
	}
	else
	{
		// The engine always pushes: lots of acceleration at low speed, none at top_speed
		float acceleration = engine_acceleration * (1.0f - speed / top_speed);
		if (acceleration > 0.0f)
		{
			// Rear wheel drive: half of the force on each rear wheel
			float force = mass * acceleration / 2.0f;
			for (int i = REAR_LEFT; i <= REAR_RIGHT; ++i)
			{
				b2Body* tire = tires[i].body->body;
				tire->ApplyForce(force * tires[i].GetForwardNormal(), tire->GetWorldCenter(), true);
			}
		}
	}
}

void Car::Draw() const
{
	for (int i = 0; i < TIRE_COUNT; ++i)
		tires[i].Draw();

	const b2Body* body = chassis->body;
	float half_w = (float)chassis_half_width;
	float half_h = (float)chassis_half_height;

	// Front wing, wider than the body
	float wing_width = (tire_offset_x + tire_half_width) * 2.0f;
	DrawBodyRectangle(body, 0.0f, -half_h + 3.0f, wing_width, 6.0f, Color{ 40, 40, 40, 255 });

	// Rear wing
	DrawBodyRectangle(body, 0.0f, half_h - 4.0f, wing_width - 8.0f, 8.0f, Color{ 40, 40, 40, 255 });

	// Body
	DrawBodyRectangle(body, 0.0f, 0.0f, half_w * 2.0f, half_h * 2.0f - 10.0f, Color{ 200, 30, 30, 255 });

	// White stripe on the nose: shows where the front is
	DrawBodyRectangle(body, 0.0f, -half_h * 0.5f, 4.0f, half_h * 0.8f, RAYWHITE);

	// Cockpit and helmet
	DrawBodyRectangle(body, 0.0f, 6.0f, half_w * 1.2f, 18.0f, Color{ 30, 30, 30, 255 });
	Vector2 helmet = LocalToWorld(body, 0.0f, 6.0f);
	DrawCircleV(helmet, 5.0f, YELLOW);

	// Brake lights
	Color light = braking ? Color{ 255, 40, 40, 255 } : Color{ 90, 0, 0, 255 };
	DrawBodyRectangle(body, -half_w + 3.0f, half_h - 1.0f, 5.0f, 3.0f, light);
	DrawBodyRectangle(body, half_w - 3.0f, half_h - 1.0f, 5.0f, 3.0f, light);
}

void Car::Destroy(ModulePhysics* physics)
{
	// Destroying a body also destroys its joints
	for (int i = 0; i < TIRE_COUNT; ++i)
	{
		physics->DestroyBody(tires[i].body);
		tires[i].body = NULL;
	}

	physics->DestroyBody(chassis);
	chassis = NULL;
	front_left_joint = NULL;
	front_right_joint = NULL;
}

void Car::Rotate(float radians)
{
	b2Body* body = chassis->body;
	b2Vec2 center = body->GetPosition();
	b2Rot rotation(radians);

	b2Body* parts[TIRE_COUNT + 1] = { body };
	for (int i = 0; i < TIRE_COUNT; ++i)
		parts[i + 1] = tires[i].body->body;

	// Every part moves around the center and turns the same angle: the joints stay happy
	for (b2Body* part : parts)
	{
		b2Vec2 position = center + b2Mul(rotation, part->GetPosition() - center);
		part->SetTransform(position, part->GetAngle() + radians);
		part->SetLinearVelocity(b2Mul(rotation, part->GetLinearVelocity()));
	}
}

bool Car::Owns(const PhysBody* body) const
{
	if (body == NULL)
		return false;

	if (body == chassis)
		return true;

	for (int i = 0; i < TIRE_COUNT; ++i)
	{
		if (body == tires[i].body)
			return true;
	}
	return false;
}

bool Car::IsTouchingWall(b2Vec2& normal) const
{
	b2Body* parts[TIRE_COUNT + 1] = { chassis->body };
	for (int i = 0; i < TIRE_COUNT; ++i)
		parts[i + 1] = tires[i].body->body;

	normal.SetZero();
	bool touching = false;

	for (b2Body* part : parts)
	{
		for (b2ContactEdge* edge = part->GetContactList(); edge != NULL; edge = edge->next)
		{
			b2Contact* contact = edge->contact;
			if (!contact->IsTouching() || edge->other->GetType() != b2_staticBody)
				continue;

			// Box2D normal goes from fixture A to fixture B: we want it from the wall to the car
			b2WorldManifold manifold;
			contact->GetWorldManifold(&manifold);
			b2Vec2 n = manifold.normal;
			if (contact->GetFixtureA()->GetBody() == part)
				n = -n;

			normal += n;
			touching = true;
		}
	}

	if (touching)
		normal.Normalize();

	return touching;
}

void Car::GetPosition(float& x, float& y) const
{
	b2Vec2 pos = chassis->body->GetPosition();
	x = pos.x * PIXELS_PER_METER;
	y = pos.y * PIXELS_PER_METER;
}

// Speed along the direction the car looks. Negative: going backwards
float Car::GetSpeed() const
{
	b2Body* body = chassis->body;
	b2Vec2 forward = body->GetWorldVector(b2Vec2(0.0f, -1.0f));
	return b2Dot(forward, body->GetLinearVelocity());
}

float Car::GetTotalMass() const
{
	float mass = chassis->body->GetMass();
	for (int i = 0; i < TIRE_COUNT; ++i)
		mass += tires[i].body->body->GetMass();
	return mass;
}
