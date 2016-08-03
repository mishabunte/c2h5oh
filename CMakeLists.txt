CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
CMAKE_POLICY(SET CMP0003 NEW)

PROJECT(nginx-c2h5oh)

set(MAJOR_VERSION 0)
set(MINOR_VERSION 0)
set(PATCH_VERSION 1)

set(C2H5OH_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

find_package(Boost COMPONENTS unit_test_framework filesystem REQUIRED)
find_package(ZLIB REQUIRED)
find_library(PQ REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CFLAGS "-Wall -DBOOST_DATE_TIME_NO_LIB -DBOOST_SYSTEM_NO_DEPRECATED -DJSMN_PARENT_LINKS")
set(CFLAGS_RELEASE "-O3 -Werror ${CFLAGS}")
set(CFLAGS_DEBUG   "-O0 -g -D_DEBUG  -Wextra ${CFLAGS}")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -std=c11 ${CFLAGS_RELEASE}")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS} -std=c11 ${CFLAGS_DEBUG}")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -std=c++11 ${CFLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS} -std=c++11 ${CFLAGS_DEBUG}")

# dogshit for enable output in testing
if (CMAKE_CONFIGURATION_TYPES)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
        --force-new-ctest-process --output-on-failure
        --build-config "$<CONFIGURATION>")
else()
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND}
        --force-new-ctest-process --output-on-failure)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

link_directories(
  /usr/lib
  ${CMAKE_SOURCE_DIR}/lib
)

include_directories(
  /usr/include/postgresql
  src/c2h5oh
  deps/jsmn/
)

set(COMMON_SRCS
  src/c2h5oh/pqasync.cc
  src/c2h5oh/c2h5oh.cc
)
add_library(c2h5oh ${COMMON_SRCS})
target_link_libraries(c2h5oh pq)

add_definitions(-DBOOST_ALL_DYN_LINK)

add_executable(test-pq tests/test-pq.cc)
target_link_libraries (test-pq c2h5oh ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_executable(test-c2h5oh tests/test-c2h5oh.cc)
target_link_libraries(test-c2h5oh c2h5oh ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_subdirectory(src/nginx)
add_subdirectory(src/deb)

#------------------------------------------------------------------------------
enable_testing()

add_test(NAME init-db WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND ./tests/init-db.sh)

add_test(NAME test-pq COMMAND test-pq)
set_tests_properties(test-pq PROPERTIES DEPENDS init-db)

add_test(NAME test-c2h5oh COMMAND test-c2h5oh)
set_tests_properties(test-c2h5oh PROPERTIES DEPENDS init-db)

add_test(NAME test-c2h5oh-nginx COMMAND ./tests/test-c2h5oh-nginx.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test-c2h5oh-nginx PROPERTIES DEPENDS init-db)

