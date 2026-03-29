#include "tmpl8/app.h"
#include "projection_renderer.h"

namespace Tmpl8 {

    Application* CreateApplication()
    {
        return new Projection::ProjectionRenderer();
    }

} // namespace Tmpl8
