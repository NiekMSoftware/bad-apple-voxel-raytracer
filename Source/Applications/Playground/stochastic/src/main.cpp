#include "tmpl8/app.h"
#include "stochastic_renderer.h"
#include "rt/core/renderer.h"

namespace Tmpl8 {

    Application* CreateApplication()
    {
        return reinterpret_cast<Renderer*>(new StochasticRenderer());
    }

} // namespace Tmpl8
