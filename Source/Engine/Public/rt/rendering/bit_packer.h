// Header only - Pack voxel data efficiently

#pragma once

#include "tmpl8/template.h"

namespace rt::rendering {

    inline uint packMaterial(const uint materialID)
    {
        return (materialID << 24) | 0x010101;
    }

    inline uint packVoxel(const uint materialID, const uint r, const uint g, const uint b)
    {
        uint voxel = (materialID & 0xFF) << 24;     // upper 8 bits
        voxel |= ((r & 0xFF) << 16);                // red channel
        voxel |= ((g & 0xFF) << 8);                 // green channel
        voxel |= (b & 0xFF);                        // blue channel
        return voxel;
    }

    // Extract material ID from a voxel
    inline uint getMaterialID(const uint voxel) { return voxel >> 24; }

    // Extract color from voxel (keeps lower 24 bits)
    inline float3 getVoxelColor(const uint voxel)
    {
        const float r = ((voxel >> 16) & 0xFF) / 255.0f;
        const float g = ((voxel >> 8) & 0xFF) / 255.0f;
        const float b = (voxel & 0xFF) / 255.0f;
        return float3{r, g, b};
    }

}  //  rt::rendering