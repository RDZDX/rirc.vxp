cmake_minimum_required (VERSION 3.10)

project ("rirc.vxp")

# Host-side unit tests for the portable IRC core (no MRE SDK needed):
#   cmake -S . -B build-host -DRIRC_HOST_TESTS=ON && cmake --build build-host && ctest --test-dir build-host
option(RIRC_HOST_TESTS "Build only the IRC core and its unit tests on the host" OFF)
if(RIRC_HOST_TESTS)
    enable_testing()
    add_subdirectory(irc)
    add_subdirectory(tests)
    return()
endif()

set(APP_NAME "rirc.vxp")
set(DEVELOPER_NAME "RDZDX")
set(RAM "1500") #Ram requirement (in kb)
set(IMSI "1234567890" CACHE STRING "Imsi of first SIM-card")
set(API "TCP") #Api for app, plain TCP only (no BT, no OBEX)
set(APPID "-1")
set(CERTID "1" CACHE STRING "")
set(CERT "none" CACHE STRING "")

option(BACKGROUND "App can work background" OFF)

set(MRE_SDK "$ENV{MRE_SDK}" CACHE STRING "MRE SDK dir")
set(TinyMRESDK "$ENV{TinyMRESDK}" CACHE STRING "TinyMRESDK dir")

if(BACKGROUND)
    add_definitions(-DSUPPORT_BG)
endif()

include(cmake/MreUtils.cmake)

add_subdirectory(resources)
add_subdirectory(mreapi)
add_subdirectory(run)
add_subdirectory(irc)
add_subdirectory(net)

add_subdirectory(main)

#Full api list: Audio Camera Call TCP File HTTP Sensor SIM_card Record SMS_person SMS_SP BitStream Contact LBS MMS ProMng SMSMng Video XML Sec SysStorage Payment BT PUSH UDP SysFile
