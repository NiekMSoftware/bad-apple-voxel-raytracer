set(UNIT_TESTS_SOURCES
        test_algorithm.cpp
)

set(APP_NAME "unit_tests")

create_app(
        NAME ${APP_NAME}
        FOLDER "Tests"
        SOURCES ${PROJECTION_SOURCES}
)