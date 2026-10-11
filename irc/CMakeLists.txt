add_library(irc STATIC
    irc_parse.c irc_parse.h
    irc_log.c irc_log.h
    irc_client.c irc_client.h
)

target_include_directories(irc PUBLIC "." "${PROJECT_SOURCE_DIR}/include")
