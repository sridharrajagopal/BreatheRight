set(COMPONENT_SRCDIRS .)
set(COMPONENT_ADD_INCLUDEDIRS .)


file(GLOB_RECURSE childdir LIST_DIRECTORIES true *)
foreach (child ${childdir})
    if(IS_DIRECTORY ${child})
        list(APPEND COMPONENT_SRCDIRS ${child})
        list(APPEND COMPONENT_ADD_INCLUDEDIRS ${child})       
    endif()
endforeach()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -D__ESP32__=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -D__ESP32__=1")
set(CMAKE_STATIC_LINKER_FLAGS "-lm" "-lstdc++")
set(CMAKE_EXE_LINKER_FLAGS "-lm" "-lstdc++")

register_component()

