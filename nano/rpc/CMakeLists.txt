if(NANO_SECURE_RPC OR RAIBLOCKS_SECURE_RPC)
  set(secure_rpc_sources rpc_secure.hpp rpc_secure.cpp
                         rpc_connection_secure.hpp rpc_connection_secure.cpp)
endif()

add_library(
  rpc
  ${secure_rpc_sources}
  rpc.hpp
  rpc.cpp
  rpc_connection.hpp
  rpc_connection.cpp
  rpc_handler.hpp
  rpc_handler.cpp
  rpc_request_processor.hpp
  rpc_request_processor.cpp)

# Kakitu dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(JSONCPP jsoncpp)
  if(JSONCPP_FOUND)
    target_include_directories(rpc PRIVATE ${JSONCPP_INCLUDE_DIRS})
  endif()
endif()

target_link_libraries(rpc Boost::beast nano_lib ${OPENSSL_LIBRARIES})
