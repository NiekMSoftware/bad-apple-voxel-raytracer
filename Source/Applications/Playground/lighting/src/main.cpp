#include "tmpl8/app.h"
#include "lighting.h"

namespace Tmpl8 {

    Application* CreateApplication()
    {
        return reinterpret_cast<Renderer*>(new LightRenderer());
    }

} // namespace Tmpl8
