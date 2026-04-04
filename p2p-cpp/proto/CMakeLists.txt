cmake_minimum_required(VERSION 3.20)

find_package(Protobuf REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ABSL QUIET absl_log_internal_check_op)
endif()

set(PROTO_FILES
    dcutr.proto
    relay_v2.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(p2p-proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})

# Enable position-independent code for linking into shared libraries
set_target_properties(p2p-proto PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(p2p-proto PRIVATE
        -Wno-nullability-extension
        -Wno-nullability-completeness
    )
endif()

target_link_libraries(p2p-proto PUBLIC protobuf::libprotobuf)

if(ABSL_FOUND)
    target_link_libraries(p2p-proto PUBLIC ${ABSL_LIBRARIES})
    target_link_directories(p2p-proto PUBLIC ${ABSL_LIBRARY_DIRS})
endif()

target_include_directories(p2p-proto
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

install(TARGETS p2p-proto ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES ${PROTO_HDRS} DESTINATION include/p2p/proto)
