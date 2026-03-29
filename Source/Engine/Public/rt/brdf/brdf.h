#pragma once

// ============================================
// BRDF Library - Physically Based Rendering
// ============================================
//
// A self-contained, reusable BRDF module.
//
// USAGE:
//   #include "brdf/brdf.h"
//
//   // Option 1: Direct evaluation
//   auto result = Tmpl8::BRDF::Evaluate(N, V, L, albedo, metallic, roughness, ior);
//   float3 brdf = result.Total();
//
//   // Option 2: Using ShadingContext (more efficient for multiple lights)
//   auto ctx = Tmpl8::BRDF::ShadingContext::Create(N, V, L);
//   auto result = Tmpl8::BRDF::Evaluate(ctx, albedo, metallic, roughness, ior);
//
// REFERENCES:
//   - Disney BRDF (Burley 2012)
//   - UE4 PBR (Karis 2013)
//   - glTF 2.0 Specification
//

// Types and structures
#include "brdf_types.h"
#include "brdf_aniso.h"

// Individual BRDF components
#include "brdf_fresnel.h"
#include "brdf_ndf.h"
#include "brdf_geometry.h"
#include "brdf_diffuse.h"

// High-level evaluation
#include "brdf_evaluate.h"