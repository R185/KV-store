cmake_minimum_required(VERSION 3.20)
project(ValkeyDB CXX)


include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2 
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(valkey
    main.cpp
    lib/REPL.cpp
    lib/Engine.cpp
    lib/Command_Proc.cpp
    lib/String_commands.cpp
    lib/List_commands.cpp
    lib/Set_commands.cpp
    lib/Geo_commands.cpp
    lib/Gen_commands.cpp
)

add_executable(valkey_tests
    tests.cpp
    lib/REPL.cpp
    lib/Engine.cpp
    lib/Command_Proc.cpp
    lib/String_commands.cpp
    lib/List_commands.cpp
    lib/Set_commands.cpp
    lib/Geo_commands.cpp
    lib/Gen_commands.cpp
)

target_include_directories(valkey_tests PRIVATE .)

target_link_libraries(valkey_tests PRIVATE 
    gtest_main 
    pthread
)

enable_testing()
add_test(NAME ValkeyDBTests COMMAND valkey_tests)

include(coverage.cmake)