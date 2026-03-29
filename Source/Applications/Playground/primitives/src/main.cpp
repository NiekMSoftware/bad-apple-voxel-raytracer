#include "tmpl8/app.h"
#include "primitives_renderer.h"

namespace Tmpl8 {
    Application* CreateApplication()
    {
        return new primitives::PrimitivesRenderer();
    }
}