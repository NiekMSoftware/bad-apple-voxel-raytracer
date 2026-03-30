#pragma once
#include "tmpl8/template.h"
#include "tmpl8/tmpl8math.h"

namespace rt::core {

    class Ray
    {
    public:
	    Ray( const float3 origin, const float3 direction, const float rayLength = 1e34f, const uint rgb = 0 );
	    float3 intersectionPoint() const { return m_o + m_t * m_d; }
	    float3 getNormal() const;
	    float3 getAlbedo() const;

	    // ray data
	    float3 m_o;					// ray origin
	    float3 m_rD;					// reciprocal ray direction
	    float3 m_d;					// ray direction
	    float m_t;					// ray length
	    float3 m_dsign;				// inverted ray direction signs, -1 or 1
	    uint m_voxel;					// payload of the intersected voxel
	    uint m_axis = 0;				// axis of last plane passed by the ray
	    bool m_bInside = false;		// if true, ray started in voxel and t is at exit point

    private:
	    // min3 is used in normal reconstruction.
	    __inline static float3 min3( const float3& a, const float3& b )
	    {
		    return float3( min( a.x, b.x ), min( a.y, b.y ), min( a.z, b.z ) );
	    }
    };

    inline bool isRayValid(const core::Ray& ray) {
        return std::isfinite(ray.m_o.x) && std::isfinite(ray.m_o.y) && std::isfinite(ray.m_o.z)
            && std::isfinite(ray.m_d.x) && std::isfinite(ray.m_d.y) && std::isfinite(ray.m_d.z)
            && std::isfinite(ray.m_t);
    }

};