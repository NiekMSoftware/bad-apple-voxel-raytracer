set(DEMO_SOURCES
        src/entry_point.cpp
        src/app.cpp
)

set(DEMO_HEADERS
        include/audio_player.h
        include/app.h
        include/bad_apple_player.h
        include/scene_builder.h
        include/spline.h
        include/spline_editor.h
)

set(APP_NAME "raytracing-demo")

create_app(
        NAME ${APP_NAME}
        FOLDER "Applications/Main"
        SOURCES ${PROJECTION_SOURCES}
        HEADERS ${PROJECTION_HEADERS}
)