#pragma once

#include "tmpl8/template.h"

namespace rt::bsdf {

    // =========================================================================
    // Lobe classification
    //
    // PBRT uses BxDFFlags for this (Section 9.1.2). We use a simpler enum
    // since we have a fixed set of lobes: Lambert diffuse, Cook-Torrance
    // specular, and specular transmission (Snell refraction).
    // =========================================================================
    enum class LobeType : uint8_t
    {
        None,
        DiffuseReflection,      // Lambert
        SpecularReflection,     // mirror / Cook-Torrance
        SpecularTransmission    // Snell refraction
    };

    // =========================================================================
    // BSDFSample
    //
    // Returned by bsdf::sample(). Mirrors PBRT's BSDFSample:
    //   - sampled direction  (wi)
    //   - scattering value   (f)
    //   - probability density (pdf)
    //   - which lobe was chosen
    //
    // For delta distributions (perfect specular/transmission), pdf is set
    // to 1.0 by convention. The implied delta cancels with the implied
    // delta in the BSDF value when the Monte Carlo estimator is evaluated.
    // (PBRT 4th Ed., Section 9.1.2)
    // =========================================================================
    struct BSDFSample
    {
        float3   m_wi{0};                         // sampled incident direction (world space)
        float3   m_value{0};                      // f(wo, wi) — BSDF value for this pair
        float    m_pdf       = 0.0f;                // probability of sampling this direction
        LobeType m_lobe      = LobeType::None;      // which lobe produced this sample
        bool     m_bIsValid  = false;               // false if sampling failed (e.g. TIR fallback)
    };

}  // namespace rt::bsdf