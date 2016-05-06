#------------------------------------------------------------------------------


INCLUDE_DIRECTORIES(
  /usr/include/postgresql
  lib-c2h5oh
)

add_library(c2h5oh ${COMMON_SRCS})
target_link_libraries(c2h5oh pq)

add_executable(test-pq tests/test-pq.cc)
target_link_libraries (test-pq c2h5oh ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_executable(test-c2h5oh tests/test-c2h5oh.cc)
target_link_libraries(test-c2h5oh c2h5oh ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

