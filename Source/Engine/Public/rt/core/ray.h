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
	    float GetReflectivity( const float3& I ) const; // TODO: implement
	    float GetRefractivity( const float3& I ) const; // TODO: implement
	    float3 GetAbsorption( const float3& I ) const; // TODO: implement
	    // ray data
	    float3 m_o;					// ray origin
	    float3 m_rD;					// reciprocal ray direction
	    float3 m_d;					// ray direction
	    float m_t;					// ray length
	    float3 m_dsign;				// inverted ray direction signs, -1 or 1
	    uint m_voxel;					// payload of the intersected voxel
	    uint m_axis = 0;				// axis of last plane passed by the ray
	    bool m_bInside = false;		// if true, ray started in voxel and t is at exit point

        // Analytic primitive hit data (filled by AnalyticScene::Intersect)
        float3 m_primNormal   = { 0, 0, 0 };
        uint   m_primMatIndex = 0;
        float  m_primRadius   = 0.0f;

    private:
	    // min3 is used in normal reconstruction.
	    __inline static float3 min3( const float3& a, const float3& b )
	    {
		    return float3( min( a.x, b.x ), min( a.y, b.y ), min( a.z, b.z ) );
	    }
    };

};