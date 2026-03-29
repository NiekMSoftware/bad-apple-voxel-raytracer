#pragma once

#include "tmpl8/common.h"
#include "tmpl8/tmpl8math.h"

namespace rt::brdf {

    /**
     * @brief Divides by PI to ensure energy conservation
     *
     * @param albedo Surface base color
     */
    inline float3 diffuseLambert(const float3 albedo)
    {
        return albedo * INVPI;
    }

} // rt::brdf