#pragma once

#include "tmpl8/template.h"

namespace rt::rendering {

    // A PBR-based material structure
    struct Material 
    {
        // Core properties
        float3 m_baseColor;           // RGB albedo color
        float m_metallic;             // [0.0...1.0)
        float m_roughness;            // [0.0...1.0)
        float m_anisotropy;           // [-1.0...1.0)

        float m_indexOfRefraction;    // IOR ranging from [1.0..2.5)
        float m_transparency;         // 0.0 = opaque, 1.0 = fully transparent
        float m_absorptionDensity;    // Controls Beer-Lambert absorption rate [0.0 ... 10.0+]

        uint m_id;                    // unique identifier
    };

}  // rt::rendering