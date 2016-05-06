#define BOOST_TEST_MODULE test_c2h5oh
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "c2h5oh.h"

using namespace boost::posix_time;
using boost::posix_time::ptime;

//-----------------------------------------------------------------------------
namespace { const char * kConnString = 
  "host=127.0.0.1 port=5432 user=c2h5oh_test__ password=c2h5oh " 
  "dbname=c2h5oh_test__ connect_timeout=1";
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_query )
{
  // init module
  BOOST_REQUIRE(c2h5oh_module_init(kConnString, strlen(kConnString), 1) == 0);

  // create connection
  auto c = c2h5oh_create();
  BOOST_REQUIRE(c != NULL);
  BOOST_CHECK(c2h5oh_create() == NULL); // check for pool size 1

  // send query
  BOOST_REQUIRE(
    c2h5oh_query(c, "select pq_test.pq_test('{\"a\" : 1, \"b\" : 2}');") == 0);

  // wait result and check it
  ptime time_end = microsec_clock::local_time() + seconds(1);
  while(c2h5oh_poll(c) == 0 && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(c2h5oh_is_error(c) == 0 && 
              std::string(c2h5oh_result(c)) == "{\"sum\" : 3}");

  // reuse connection
  c2h5oh_free(c);
  BOOST_CHECK((c = c2h5oh_create()) != NULL); 

  // send another query and check result
  BOOST_REQUIRE(c2h5oh_query(c, "select pq_test.pq_test('{\"a\" : 5, \"b\" : 6}');") == 0);
  time_end = microsec_clock::local_time() + seconds(1);
  while(c2h5oh_poll(c) == 0 && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(c2h5oh_is_error(c) == 0 && 
              std::string(c2h5oh_result(c)) == "{\"sum\" : 11}");

  // check function not found
  c2h5oh_query(c, "select pq_test.pq_test2('a', 'b');");
  time_end = microsec_clock::local_time() + seconds(1);
  while(c2h5oh_poll(c) == 0 && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(strncmp(c2h5oh_result(c), "42883", 5) == 0);

  // cleanup
  c2h5oh_free(c);
  BOOST_CHECK(c2h5oh_create() != NULL); // check for object reusable
  c2h5oh_module_cleanup();
}

