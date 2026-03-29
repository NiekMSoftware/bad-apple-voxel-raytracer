#include "tmpl8/app.h"
#include "reflections_renderer.h"
#include "rt/core/renderer.h"

namespace Tmpl8 {

    Application* CreateApplication()
    {
        return reinterpret_cast<Renderer*>(new ReflectionsRenderer());
    }

} // namespace Tmpl8
