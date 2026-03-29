/**
 * @note The physics were created by Thomas M.
 *
 * I further adapted it to my own code-base so it works with my API.
 */

#pragma once
#include "tmpl8/template.h"
#include "rt/scene/scene.h"

namespace demo
{
	struct RigidBody
	{
		float3 position;
		float3 velocity;
		float mass;
	};

	struct PhysicsObject
	{
		RigidBody body;
		float radius; //Just spheres only
		bool isGrounded;
	};

	class PhysicsManager
	{
	public:
		static constexpr uint MAX_OBJECTS = 256;
		const float3 GRAVITY = {0.f, -0.5f, 0.f};
		static constexpr float SLEEP_THRESHOLD = 0.01f;
		static constexpr float DAMPING = 0.5f;
		static constexpr float RESTITUTION = 0.5f;

		PhysicsManager();
		~PhysicsManager() = default;

		void AddPhysicsObject(const PhysicsObject& object);
		const PhysicsObject& GetPhysicsObject(const uint index) const { return objects[index]; }

	    void RelaunchObject(uint index, const float3& position, const float3& velocity);

		bool Update(const rt::scene::Scene& scene, float dt);

	private:
		PhysicsObject objects[MAX_OBJECTS];
		uint objectCount;

		//Update position
        static void Integrate(PhysicsObject& object, float dt);
		void ApplyGravity(PhysicsObject& object, float dt) const;

		//Sleep check
        static bool IsMoving(const PhysicsObject& object);
        static void ResolveCollision(PhysicsObject& object, const float3& normal);
	};
}