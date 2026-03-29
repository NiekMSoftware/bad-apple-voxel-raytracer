#include "rt/core/ray.h"

namespace rt::core {

    Ray::Ray( const float3 origin, const float3 direction, const float rayLength, const uint rgb )
        : m_o( origin ), m_d( normalize( direction ) ), m_t( rayLength ), m_voxel( rgb )
    {
        // calculate reciprocal ray direction for triangles and AABBs
        // TODO: prevent NaNs - or don't
        m_rD = float3( 1 / m_d.x, 1 / m_d.y, 1 / m_d.z );
        uint xsign = *(uint*)&m_d.x >> 31;
        uint ysign = *(uint*)&m_d.y >> 31;
        uint zsign = *(uint*)&m_d.z >> 31;
        m_dsign = float3( (float)xsign, (float)ysign, (float)zsign ); // tnx Timon
    }

    float3 Ray::getNormal() const
    {
        // return the voxel normal at the nearest intersection
        const float3 sign = m_dsign * 2.0f - 1.0f;
        return float3( m_axis == 0 ? sign.x : 0, m_axis == 1 ? sign.y : 0, m_axis == 2 ? sign.z : 0 );
    }

    float3 Ray::getAlbedo() const
    {
        // return the (floating point) albedo at the nearest intersection
        return RGB8_to_RGBF32( m_voxel );
    }

}