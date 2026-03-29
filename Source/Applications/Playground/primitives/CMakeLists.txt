set(PROJECTION_SOURCES
        src/main.cpp
        src/primitives_renderer.cpp
)

set(PROJECTION_HEADERS
        include/primitives_renderer.h
)

set(APP_NAME "primitives")

create_app(
        NAME ${APP_NAME}
        FOLDER "Applications/Playground"
        SOURCES ${PROJECTION_SOURCES}
        HEADERS ${PROJECTION_HEADERS}
)