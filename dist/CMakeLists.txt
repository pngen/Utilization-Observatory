# Utilization Observatory : distributed proof (real OS processes over TCP loopback).
add_executable(uo_coordinator coordinator.cpp protocol.cpp)
add_executable(uo_worker worker.cpp protocol.cpp)
add_executable(uo_distributed_proof proof.cpp protocol.cpp)

foreach(tgt uo_coordinator uo_worker uo_distributed_proof)
    target_link_libraries("${tgt}" PRIVATE utilization_observatory ws2_32)
    target_include_directories("${tgt}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror)
    endif()
endforeach()
