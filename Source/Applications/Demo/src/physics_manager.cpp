#include "physics_manager.h"
#include "collision_detection.h"

namespace demo
{
	PhysicsManager::PhysicsManager() :
		objects(),
		objectCount(0)
	{
	}

	void PhysicsManager::AddPhysicsObject(const PhysicsObject& object)
	{
		if (objectCount >= MAX_OBJECTS) return;
		objects[objectCount] = object;
		objectCount++;
	}

    void PhysicsManager::RelaunchObject(const uint index, const float3& position, const float3& velocity) {
	    if (index >= objectCount) return;
	    objects[index].body.position = position;
	    objects[index].body.velocity = velocity;
	    objects[index].isGrounded = false;
	}

	bool PhysicsManager::Update(const rt::scene::Scene& scene, const float dt)
	{
		if (objectCount == 0) return false;

		bool hasMovement = false;
		for (uint i = 0; i < objectCount; i++)
		{
			if (!IsMoving(objects[i])) continue;

			objects[i].isGrounded = false;

			ApplyGravity(objects[i], dt);
			Integrate(objects[i], dt);

			float3 N{};
			float depth;
			if (SphereCast(scene, objects[i].body.position, objects[i].radius, objects[i].body.velocity, N, depth))
			{
				ResolveCollision(objects[i], N);
				objects[i].body.position += N * depth;
			}

			hasMovement = true;
		}

		return hasMovement;
	}

	void PhysicsManager::Integrate(PhysicsObject& object, const float dt)
	{
		object.body.position += object.body.velocity * dt;
		object.body.velocity *= powf(DAMPING, dt);
	}

	void PhysicsManager::ApplyGravity(PhysicsObject& object, const float dt) const
    {
		object.body.velocity += GRAVITY * dt;
	}

	bool PhysicsManager::IsMoving(const PhysicsObject& object)
	{
		return length(object.body.velocity) > SLEEP_THRESHOLD || !object.isGrounded;
	}

	void PhysicsManager::ResolveCollision(PhysicsObject& object, const float3& normal)
	{
		const float vn = dot(object.body.velocity, normal);
		const float3 normalComponent = normal * vn;
		const float3 tangentComponent = object.body.velocity - normalComponent;

		const float3 reflected = -normalComponent * RESTITUTION;

		//If the bounce is tiny kill it so the ball settles
		if (length(reflected) < SLEEP_THRESHOLD)
		{
			object.body.velocity = tangentComponent;
		}
		else
		{
			object.body.velocity = tangentComponent + reflected;
		}

		if (normal.y > 0.7f) object.isGrounded = true;
	}
}