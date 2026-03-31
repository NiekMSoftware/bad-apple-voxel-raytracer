#include "physics_manager.h"
#include "collision_detection.h"

namespace demo
{
	PhysicsManager::PhysicsManager() :
		m_aObjects(),
		m_objCount(0)
	{
	}

	void PhysicsManager::addPhysicsObject(const PhysicsObject& object)
	{
		if (m_objCount >= MAX_OBJECTS) return;
		m_aObjects[m_objCount] = object;
		m_objCount++;
	}

    void PhysicsManager::relaunchObject(const uint index, const float3& position, const float3& velocity) {
	    if (index >= m_objCount) return;
	    m_aObjects[index].body.position = position;
	    m_aObjects[index].body.velocity = velocity;
	    m_aObjects[index].isGrounded = false;
	}

	bool PhysicsManager::update(const rt::scene::Scene& scene, const float dt)
	{
		if (m_objCount == 0) return false;

		bool hasMovement = false;
		for (uint i = 0; i < m_objCount; i++)
		{
			if (!isMoving(m_aObjects[i])) continue;

			m_aObjects[i].isGrounded = false;

			applyGravity(m_aObjects[i], dt);
			integrate(m_aObjects[i], dt);

			float3 N{};
			float depth;
			if (sphereCast(scene, m_aObjects[i].body.position, m_aObjects[i].radius, m_aObjects[i].body.velocity, N, depth))
			{
				resolveCollision(m_aObjects[i], N);
				m_aObjects[i].body.position += N * depth;
			}

			hasMovement = true;
		}

		return hasMovement;
	}

	void PhysicsManager::integrate(PhysicsObject& object, const float dt)
	{
		object.body.position += object.body.velocity * dt;
		object.body.velocity *= powf(DAMPING, dt);
	}

	void PhysicsManager::applyGravity(PhysicsObject& object, const float dt) const
    {
		object.body.velocity += GRAVITY * dt;
	}

	bool PhysicsManager::isMoving(const PhysicsObject& object)
	{
		return length(object.body.velocity) > SLEEP_THRESHOLD || !object.isGrounded;
	}

	void PhysicsManager::resolveCollision(PhysicsObject& object, const float3& normal)
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