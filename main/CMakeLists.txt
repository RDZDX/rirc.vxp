cmake_minimum_required (VERSION 3.8)

set(TARGET_NAME "main")

set(SOURCES "main.c" "app.c" "app.h" "ui.c" "ui.h")

add_exec_vxp(${TARGET_NAME}
    SRCS ${SOURCES}
    RESOURSES resources
    APP_NAME "${APP_NAME}"
    APPID "${APPID}"
    BACKGROUND ${BACKGROUND}
    API "${API}"
    RAM "${RAM}"
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${TARGET_NAME} irc net mreapi)
