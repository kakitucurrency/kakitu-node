add_executable(kakitu_node daemon.cpp daemon.hpp entry.cpp)

target_link_libraries(
  kakitu_node
  Boost::process
  node
  rpc
  secure
  argon2
  ${PLATFORM_LIBS})
include_directories(${CMAKE_SOURCE_DIR}/submodules)

target_compile_definitions(
  kakitu_node PRIVATE -DTAG_VERSION_STRING=${TAG_VERSION_STRING}
                    -DGIT_COMMIT_HASH=${GIT_COMMIT_HASH})

set_target_properties(
  kakitu_node PROPERTIES COMPILE_FLAGS
                       "-DQT_NO_KEYWORDS -DBOOST_ASIO_HAS_STD_ARRAY=1")

add_custom_command(
  TARGET kakitu_node
  POST_BUILD
  COMMAND kakitu_node --generate_config node >
          ${PROJECT_BINARY_DIR}/config-node.toml.sample
  COMMAND kakitu_node --generate_config rpc >
          ${PROJECT_BINARY_DIR}/config-rpc.toml.sample)

if((NANO_GUI OR RAIBLOCKS_GUI) AND NOT APPLE)
  if(WIN32)
    install(TARGETS kakitu_node RUNTIME DESTINATION .)
  else()
    install(TARGETS kakitu_node RUNTIME DESTINATION ./bin)
  endif()
endif()
