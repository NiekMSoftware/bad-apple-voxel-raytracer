#ifndef VOXPOPULI_LIGHTING_H
#define VOXPOPULI_LIGHTING_H

#include "rt/core/renderer.h"

namespace Tmpl8 {

    class LightRenderer : Renderer {
    public:
        void Init() override;

        float3 Trace(Ray &ray, int, int, int) override;

        void Tick(float deltaTime) override;

        void UI() override;

        void Shutdown() override;

        void MouseUp(int button) override;

        void MouseDown(int button) override;

        void MouseMove(int x, int y) override;

        void MouseWheel(float y) override;

        void KeyUp(int key) override;

        void KeyDown(int key) override;
    };

}

#endif //VOXPOPULI_LIGHTING_H