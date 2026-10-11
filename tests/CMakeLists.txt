add_executable(test_irc test_irc.c)
target_link_libraries(test_irc irc)
add_test(NAME test_irc COMMAND test_irc)
