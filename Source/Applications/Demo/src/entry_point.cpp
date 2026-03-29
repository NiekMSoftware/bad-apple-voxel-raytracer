#include "app.h"

namespace Tmpl8 {

    Application* CreateApplication()
    {
        return new demo::DemoApplication();
    }

}  // namespace Tmpl8