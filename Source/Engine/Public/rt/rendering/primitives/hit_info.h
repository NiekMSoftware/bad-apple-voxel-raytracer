#pragma once

#include "tmpl8/template.h"

namespace rt::primitives
{
    struct HitInfo
    {
        float  m_t        = 1e34f;    // intersection distance
        float  m_radius   = 0.0f;
        uint   m_matIndex = 0;        // index into MaterialManager
        float3 m_normal   = { 0 };  // geometric normal at hit point
    };

}