add_library(net STATIC
    net_tcp.c net_tcp.h
)

target_include_directories(net PUBLIC "." "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(net mreapi)
