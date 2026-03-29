#pragma once
#include "tiny_bvh.h"
#include <vector>

namespace rt::rendering
{
    struct Mesh
    {
        std::vector<tinybvh::bvhvec4> m_vVertices;     // flat, 3 verts per triangle
        tinybvh::BVH m_bvh;

        void build()
        {
            const int triCount = static_cast<int>(m_vVertices.size() / 3);
            m_bvh.Build(m_vVertices.data(), triCount);
        }
    };
}  // rt::rendering