#pragma once

#include "rt/core/ray.h"
#include "tmpl8/tmpl8math.h"
#include "tmpl8/common.h"

namespace rt::core {

    struct Plane
    {
        float3 m_normal;
        float  m_d;     // distance of plane to origin
    };

    struct Frustum
    {
        Plane m_aPlane[4];  // 0=left, 1=right, 2=top, 3=bottom
    };

    inline float planeDistance(const Plane& plane, const float3& pos)
    {
        return dot(plane.m_normal, pos) - plane.m_d;
    }

    class Camera
    {
    public:
        Camera();
        ~Camera();

        Ray  getPrimaryRay(float x, float y) const;
        bool handleInput(float t);

        // Called by Renderer::setRenderResolution() whenever the internal
        // render buffer is resized. Updates aspect ratio.
        void setRenderResolution(int w, int h);

        // Keep public so app.cpp can call it after manually writing
        // camPos / camTarget (e.g. spline updates).
        void updateCameraToWorld();

        // Builds a view frustum from the current camera state.
        // Used by Renderer to store m_prevFrustum for TAA reprojection.
        [[nodiscard]] Frustum buildFrustum() const;

        [[nodiscard]] const mat4& getCameraToWorld() const { return m_camToWorld; }

        // Render resolution this camera generates rays for.
        // Always kept in sync with Renderer's internal buffer dimensions.
        int m_renderWidth  = SCRWIDTH;
        int m_renderHeight = SCRHEIGHT;

        float  m_aspect = static_cast<float>(SCRWIDTH) / static_cast<float>(SCRHEIGHT);
        float3 m_camPos, m_camTarget;
        float3 m_topLeft, m_topRight, m_bottomLeft;

        float m_lensRadius    = 0.0f;
        float m_focalDistance = 2.0f;
        float m_fovDegrees    = 67.4f;

        mat4 m_camToWorld;
    };

}  // namespace rt::core
