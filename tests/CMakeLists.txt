add_executable(uo_tests
    main.cpp
    test_core.cpp
    test_observatory.cpp
    test_property.cpp
    test_adversarial.cpp
    test_concurrency.cpp
    test_persistence.cpp
)
target_link_libraries(uo_tests PRIVATE utilization_observatory)
target_include_directories(uo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
if(MSVC)
    target_compile_options(uo_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(uo_tests PRIVATE -Wall -Wextra -Werror)
endif()
target_link_libraries(uo_tests PRIVATE Threads::Threads)
add_test(NAME uo_tests COMMAND uo_tests)
