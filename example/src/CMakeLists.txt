FILE(GLOB_RECURSE app_sources "${CMAKE_SOURCE_DIR}/example/src/*.cpp")
FILE(GLOB_RECURSE lib_sources "${CMAKE_SOURCE_DIR}/src/*.cpp")

idf_component_register(
        SRCS ${app_sources} ${lib_sources}
        INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" "."
)
