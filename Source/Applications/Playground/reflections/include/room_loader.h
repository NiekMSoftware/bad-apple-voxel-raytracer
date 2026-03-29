#pragma once

#include "rt/scene/scene_loader.h"

namespace Tmpl8 {
    class RoomLoader : public SceneLoader {
    public:
        void Load(Scene& scene) override;
    };
}