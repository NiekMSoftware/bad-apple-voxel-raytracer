// =============================================================================
// Unit Tests
//
// Tests two distinct non-trivial algorithms:
//   1. Ray/sphere intersection  (rt::primitives::intersectSphere)
//   2. Fresnel-Schlick approximation (rt::brdf::fresnelSchlick,
//                                     rt::brdf::fresnelSchlickScalar)
//
// No external framework is required. Each test calls ASSERT_TRUE or
// ASSERT_NEAR, which print a clear pass/fail message and set a global
// failure flag. The process exits with code 1 if any assertion fails.
// =============================================================================

#include "tmpl8/template.h"
#include "rt/brdf/brdf_fresnel.h"
#include "rt/rendering/primitives/sphere.h"

// --------------------------------------------------------------------------
// Minimal test harness
// --------------------------------------------------------------------------

static int g_testsFailed = 0;
static int g_testsPassed = 0;

static void assertTrue(const bool condition, const char* expr, const char* file, const int line)
{
    if (condition)
    {
        printf("  [PASS] %s\n", expr);
        g_testsPassed++;
    }
    else
    {
        printf("  [FAIL] %s  (%s:%d)\n", expr, file, line);
        g_testsFailed++;
    }
}

static void assertNear(const float a, const float b, const float eps,
                       const char* expr, const char* file, const int line)
{
    const float diff = fabsf(a - b);
    if (diff <= eps)
    {
        printf("  [PASS] %s  (|%.6f - %.6f| = %.2e)\n", expr, a, b, diff);
        g_testsPassed++;
    }
    else
    {
        printf("  [FAIL] %s  (|%.6f - %.6f| = %.2e > eps %.2e)  (%s:%d)\n",
               expr, a, b, diff, eps, file, line);
        g_testsFailed++;
    }
}

#define ASSERT_TRUE(cond)       assertTrue((cond),  #cond,  __FILE__, __LINE__)
#define ASSERT_NEAR(a, b, eps)  assertNear((a),(b),(eps), #a " ~= " #b, __FILE__, __LINE__)


// =============================================================================
// Test suite 1: Ray/Sphere Intersection
// =============================================================================

static void test_RaySphereIntersection()
{
    printf("\n[Suite 1] Ray/Sphere Intersection\n");

    // --- Test 1: Direct hit along +Z axis ---
    // Ray origin at (0,0,0), direction (0,0,1).
    // Sphere centered at (0,0,5) with radius 1.
    // Expected: hit at t = 4.0 (front surface of sphere).
    {
        const rt::core::Ray         ray({ 0,0,0 }, normalize({ 0,0,1 }));
        const rt::primitives::Sphere sphere{{ 0,0,5 }, 1.0f};
        float t = 0.0f;
        const bool hit = rt::primitives::intersectSphere(ray, sphere, t);
        ASSERT_TRUE(hit);
        ASSERT_NEAR(t, 4.0f, 1e-4f);
    }

    // --- Test 2: Ray misses sphere ---
    // Ray offset by 2 units on X, sphere radius 1.
    // Discriminant < 0, so no intersection.
    {
        const rt::core::Ray          ray({ 2,0,0 }, normalize({ 0,0,1 }));
        const rt::primitives::Sphere sphere{{ 0,0,5 }, 1.0f};
        float t = 0.0f;
        const bool hit = rt::primitives::intersectSphere(ray, sphere, t);
        ASSERT_TRUE(!hit);
    }

    // --- Test 3: Ray origin inside sphere ---
    // Ray starts at sphere center (0,0,5), so t0 < 0 and t1 > 0.
    // Expected: returns t1 = 1.0 (exit point).
    {
        const rt::core::Ray          ray({ 0,0,5 }, normalize({ 0,0,1 }));
        const rt::primitives::Sphere sphere{{ 0,0,5 }, 1.0f};
        float t = 0.0f;
        const bool hit = rt::primitives::intersectSphere(ray, sphere, t);
        ASSERT_TRUE(hit);
        ASSERT_NEAR(t, 1.0f, 1e-4f);
    }

    // --- Test 4: Ray tangent to sphere ---
    // Ray offset exactly by sphere radius on X.
    // Discriminant == 0, grazing hit at t = 5.0.
    {
        const rt::core::Ray          ray({ 1,0,0 }, normalize({ 0,0,1 }));
        const rt::primitives::Sphere sphere{{ 0,0,5 }, 1.0f};
        float t = 0.0f;
        const bool hit = rt::primitives::intersectSphere(ray, sphere, t);
        ASSERT_TRUE(hit);
        ASSERT_NEAR(t, 5.0f, 1e-4f);
    }

    // --- Test 5: Ray pointing away from sphere ---
    // Direction (0,0,-1), sphere at (0,0,5). Both roots negative.
    {
        const rt::core::Ray          ray({ 0,0,0 }, normalize({ 0,0,-1 }));
        const rt::primitives::Sphere sphere{{ 0,0,5 }, 1.0f};
        float t = 0.0f;
        const bool hit = rt::primitives::intersectSphere(ray, sphere, t);
        ASSERT_TRUE(!hit);
    }
}


// =============================================================================
// Test suite 2: Fresnel-Schlick Approximation
//
// The Fresnel-Schlick approximation states:
//   F(theta) = F0 + (1 - F0)(1 - cos(theta))^5
// =============================================================================

static void test_FresnelSchlick()
{
    printf("\n[Suite 2] Fresnel-Schlick Approximation\n");

    constexpr float kEps = 1e-5f;

    // --- Test 1: Normal incidence (cosTheta = 1.0) ---
    // At normal incidence, (1 - cosTheta)^5 = 0, so F = F0 exactly.
    {
        const float3 f0{ 0.04f, 0.04f, 0.04f };
        const float3 result = rt::brdf::fresnelSchlick(1.0f, f0);
        ASSERT_NEAR(result.x, f0.x, kEps);
        ASSERT_NEAR(result.y, f0.y, kEps);
        ASSERT_NEAR(result.z, f0.z, kEps);
    }

    // --- Test 2: Grazing incidence (cosTheta = 0.0) ---
    // At grazing incidence, (1 - cosTheta)^5 = 1, so F = 1.0 exactly.
    {
        const float3 f0{ 0.04f, 0.04f, 0.04f };
        const float3 result = rt::brdf::fresnelSchlick(0.0f, f0);
        ASSERT_NEAR(result.x, 1.0f, kEps);
        ASSERT_NEAR(result.y, 1.0f, kEps);
        ASSERT_NEAR(result.z, 1.0f, kEps);
    }

    // --- Test 3: F0 = 1.0 (perfect mirror) ---
    // F = F0 regardless of angle.
    {
        const float3 f0{ 1.0f, 1.0f, 1.0f };
        const float3 r0 = rt::brdf::fresnelSchlick(1.0f, f0);
        const float3 r1 = rt::brdf::fresnelSchlick(0.0f, f0);
        const float3 r2 = rt::brdf::fresnelSchlick(0.5f, f0);
        ASSERT_NEAR(r0.x, 1.0f, kEps);
        ASSERT_NEAR(r1.x, 1.0f, kEps);
        ASSERT_NEAR(r2.x, 1.0f, kEps);
    }

    // --- Test 4: Scalar overload at normal incidence ---
    // fresnelSchlickScalar(1.0, n1, n2) must equal F0 = ((n1-n2)/(n1+n2))^2
    // Using air (n1=1.0) to glass (n2=1.5): F0 = ((1-1.5)/(1+1.5))^2 = 0.04
    {
        constexpr float expected = pow2f((1.0f - 1.5f) / (1.0f + 1.5f));
        const float result   = rt::brdf::fresnelSchlickScalar(1.0f, 1.0f, 1.5f);
        ASSERT_NEAR(result, expected, kEps);
    }

    // --- Test 5: Scalar overload at grazing incidence ---
    // fresnelSchlickScalar(0.0, n1, n2) must equal 1.0
    {
        const float result = rt::brdf::fresnelSchlickScalar(0.0f, 1.0f, 1.5f);
        ASSERT_NEAR(result, 1.0f, kEps);
    }

    // --- Test 6: Result is monotonically increasing as angle increases ---
    // Fresnel reflectance increases from F0 at normal incidence
    // to 1.0 at grazing incidence.
    {
        const float3 f0{ 0.04f, 0.04f, 0.04f };
        const float3 r0 = rt::brdf::fresnelSchlick(1.0f, f0);  // normal
        const float3 r1 = rt::brdf::fresnelSchlick(0.5f, f0);  // 60 degrees
        const float3 r2 = rt::brdf::fresnelSchlick(0.0f, f0);  // grazing
        ASSERT_TRUE(r0.x <= r1.x);
        ASSERT_TRUE(r1.x <= r2.x);
    }
}

int main() {
    printf("=== Unit Tests ===\n");

    test_RaySphereIntersection();
    test_FresnelSchlick();

    printf("\n=== Results: %d passed, %d failed ===\n", g_testsPassed, g_testsFailed);

    return (g_testsFailed > 0) ? 1 : 0;
}