#include "rt/core/camera.h"
#include "tmpl8/template.h"

namespace rt::core {

    void Camera::updateCameraToWorld()
    {
        const float3 forward = normalize(m_camTarget - m_camPos);
        const float3 tmpUp(0, 1, 0);
        const float3 right   = normalize(cross(tmpUp, forward));
        const float3 up      = normalize(cross(forward, right));

        m_camToWorld.cell[0]  = right.x;
        m_camToWorld.cell[1]  = up.x;
        m_camToWorld.cell[2]  = forward.x;
        m_camToWorld.cell[3]  = m_camPos.x;

        m_camToWorld.cell[4]  = right.y;
        m_camToWorld.cell[5]  = up.y;
        m_camToWorld.cell[6]  = forward.y;
        m_camToWorld.cell[7]  = m_camPos.y;

        m_camToWorld.cell[8]  = right.z;
        m_camToWorld.cell[9]  = up.z;
        m_camToWorld.cell[10] = forward.z;
        m_camToWorld.cell[11] = m_camPos.z;

        m_camToWorld.cell[12] = 0.0f;
        m_camToWorld.cell[13] = 0.0f;
        m_camToWorld.cell[14] = 0.0f;
        m_camToWorld.cell[15] = 1.0f;

        // Keep convenience corner vectors in sync.
        const float scale = tanf(m_fovDegrees * 0.5f * (PI / 180.0f));
        m_topLeft    = TransformVector(float3(-m_aspect * scale,  scale, 1.0f), m_camToWorld);
        m_topRight   = TransformVector(float3( m_aspect * scale,  scale, 1.0f), m_camToWorld);
        m_bottomLeft = TransformVector(float3(-m_aspect * scale, -scale, 1.0f), m_camToWorld);
    }

    // -------------------------------------------------------------------------

    void Camera::setRenderResolution(const int w, const int h)
    {
        m_renderWidth  = w;
        m_renderHeight = h;
        m_aspect       = static_cast<float>(w) / static_cast<float>(h);
    }

    // -------------------------------------------------------------------------
    // buildFrustum
    //
    // Fills a Frustum with the four side planes (left, right, top, bottom).
    // Each Plane stores an inward-facing normal and the plane's signed distance
    // to the origin, matching the Plane struct: { float3 m_normal; float m_d; }
    //
    // Plane equation: dot(m_normal, P) + m_d >= 0 means P is inside.
    // -------------------------------------------------------------------------
    Frustum Camera::buildFrustum() const
    {
        const float scale    = tanf(m_fovDegrees * 0.5f * (PI / 180.0f));
        const float3 right   = float3(m_camToWorld.cell[0], m_camToWorld.cell[4], m_camToWorld.cell[8]);
        const float3 up      = float3(m_camToWorld.cell[1], m_camToWorld.cell[5], m_camToWorld.cell[9]);
        const float3 forward = float3(m_camToWorld.cell[2], m_camToWorld.cell[6], m_camToWorld.cell[10]);

        // Four frustum edge directions from the camera position.
        const float3 tl = normalize(forward + up * scale - right * m_aspect * scale);
        const float3 tr = normalize(forward + up * scale + right * m_aspect * scale);
        const float3 bl = normalize(forward - up * scale - right * m_aspect * scale);
        const float3 br = normalize(forward - up * scale + right * m_aspect * scale);

        Frustum f;

        // Plane 0 — left:   inward normal = cross(up-ish, tl)
        f.m_aPlane[0].m_normal = normalize(cross(tl, bl));
        f.m_aPlane[0].m_d      = -dot(f.m_aPlane[0].m_normal, m_camPos);

        // Plane 1 — right:  inward normal points left from the right edge
        f.m_aPlane[1].m_normal = normalize(cross(br, tr));
        f.m_aPlane[1].m_d      = -dot(f.m_aPlane[1].m_normal, m_camPos);

        // Plane 2 — top:    inward normal points downward from the top edge
        f.m_aPlane[2].m_normal = normalize(cross(tr, tl));
        f.m_aPlane[2].m_d      = -dot(f.m_aPlane[2].m_normal, m_camPos);

        // Plane 3 — bottom: inward normal points upward from the bottom edge
        f.m_aPlane[3].m_normal = normalize(cross(bl, br));
        f.m_aPlane[3].m_d      = -dot(f.m_aPlane[3].m_normal, m_camPos);

        return f;
    }

    // -------------------------------------------------------------------------

    Camera::Camera()
    {
        FILE* f;
        fopen_s(&f, "camera.bin", "rb");
        if (f)
        {
            fread(this, 1, sizeof(Camera), f);
            fclose(f);
            updateCameraToWorld();
        }
        else
        {
            m_camPos    = float3(0, 0, -2);
            m_camTarget = float3(0, 0, -1);
            updateCameraToWorld();
        }
    }

    Camera::~Camera()
    {
        FILE* f;
        fopen_s(&f, "camera.bin", "wb");
        if (f) {
            fwrite(this, 1, sizeof(Camera), f);
            fclose(f);
        } else {
            printf("failed to save camera data\n");
        }
    }

    // -------------------------------------------------------------------------

    Ray Camera::getPrimaryRay(const float x, const float y) const
    {
        const float w     = static_cast<float>(m_renderWidth);
        const float h     = static_cast<float>(m_renderHeight);
        const float scale = tanf(m_fovDegrees * 0.5f * (PI / 180.0f));

        const float u = (2.0f * (x + 0.5f) / w - 1.0f) * m_aspect * scale;
        const float v = (1.0f - 2.0f * (y + 0.5f) / h) * scale;

        const float3 localDir = float3(u, v, 1.0f);
        const float3 worldDir = TransformVector(localDir, m_camToWorld);

        if (m_lensRadius <= 0.0f)
            return Ray(m_camPos, worldDir);

        const float3 focalPoint = m_camPos + normalize(worldDir) * m_focalDistance;

        const float3 right = float3(m_camToWorld.cell[0], m_camToWorld.cell[4], m_camToWorld.cell[8]);
        const float3 up    = float3(m_camToWorld.cell[1], m_camToWorld.cell[5], m_camToWorld.cell[9]);

        const float rx = RandomFloat() * 2.0f - 1.0f;
        const float ry = RandomFloat() * 2.0f - 1.0f;
        float diskX, diskY;

        if (rx == 0.0f && ry == 0.0f) {
            diskX = diskY = 0.0f;
        } else if (fabsf(rx) >= fabsf(ry)) {
            const float r     = rx;
            const float theta = (PI / 4.0f) * (ry / rx);
            diskX = r * cosf(theta);
            diskY = r * sinf(theta);
        } else {
            const float r     = ry;
            const float theta = (PI / 2.0f) - (PI / 4.0f) * (rx / ry);
            diskX = r * cosf(theta);
            diskY = r * sinf(theta);
        }

        const float3 lensOffset = m_lensRadius * (diskX * right + diskY * up);
        const float3 rayOrigin  = m_camPos + lensOffset;
        const float3 rayDir     = focalPoint - rayOrigin;

        return {rayOrigin, rayDir};
    }

    // -------------------------------------------------------------------------

    bool Camera::handleInput(const float t)
    {
        if (!WindowHasFocus()) return false;
        const float moveSpeed = 0.15f * t;
        const float lookSpeed = 1.2f  * t;

        float3 right = float3(m_camToWorld.cell[0], m_camToWorld.cell[4], m_camToWorld.cell[8]);
        float3 up    = float3(m_camToWorld.cell[1], m_camToWorld.cell[5], m_camToWorld.cell[9]);
        float3 ahead = float3(m_camToWorld.cell[2], m_camToWorld.cell[6], m_camToWorld.cell[10]);

        bool changed = false;

        if (IsKeyDown(GLFW_KEY_K)) { m_camTarget -= lookSpeed * up;    changed = true; }
        if (IsKeyDown(GLFW_KEY_I)) { m_camTarget += lookSpeed * up;    changed = true; }
        if (IsKeyDown(GLFW_KEY_J)) { m_camTarget -= lookSpeed * right; changed = true; }
        if (IsKeyDown(GLFW_KEY_L)) { m_camTarget += lookSpeed * right; changed = true; }

        if (changed) {
            ahead = normalize(m_camTarget - m_camPos);
            const float3 tmpUp(0, 1, 0);
            right = normalize(cross(tmpUp, ahead));
            up    = normalize(cross(ahead, right));
        }

        float3 move(0);
        if (IsKeyDown(GLFW_KEY_A))            move -= moveSpeed * right;
        if (IsKeyDown(GLFW_KEY_D))            move += moveSpeed * right;
        if (IsKeyDown(GLFW_KEY_W))            move += moveSpeed * ahead;
        if (IsKeyDown(GLFW_KEY_S))            move -= moveSpeed * ahead;
        if (IsKeyDown(GLFW_KEY_SPACE))        move += moveSpeed * up;
        if (IsKeyDown(GLFW_KEY_LEFT_CONTROL)) move -= moveSpeed * up;

        if (move.x != 0 || move.y != 0 || move.z != 0) {
            m_camPos    += move;
            m_camTarget += move;
            changed = true;
        }

        if (!changed) return false;

        m_camTarget = m_camPos + normalize(m_camTarget - m_camPos);
        updateCameraToWorld();
        return true;
    }

}  // namespace rt::core