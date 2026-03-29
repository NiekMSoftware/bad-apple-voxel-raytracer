set(PROJECTION_SOURCES
    src/main.cpp
    src/projection_renderer.cpp
)

set(PROJECTION_HEADERS
    include/projection_renderer.h
)

set(APP_NAME "projections")

create_app(
        NAME ${APP_NAME}
        FOLDER "Applications/Playground"
        SOURCES ${PROJECTION_SOURCES}
        HEADERS ${PROJECTION_HEADERS}
)