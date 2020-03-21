#define BOOST_TEST_MODULE test_pq
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "pqasync.h"

using namespace boost::posix_time;
using boost::posix_time::ptime;
using namespace Pq;

//-----------------------------------------------------------------------------
namespace { const char * kConnStr = 
  "host=127.0.0.1 port=5432 user=c2h5oh_test__ password=c2h5oh " 
  "dbname=c2h5oh_test__ connect_timeout=1";
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_query )
{
  // create database and connect
  PqAsync db;
  bool connected = db.connect(kConnStr);
  if (!connected) {
    BOOST_ERROR(db.get_last_error());
  }

  // simple query test
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select pq_test.pq_test('{\"a\" : 1, \"b\" : 2}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "{\"sum\" : 3}");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_sleep )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr, true)); // non blocking connect

  // perform query sleeping for 100 ms and check its time
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select pq_test.pq_test('{\"a\":1, \"b\":2, \"sleep\":0.1}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "{\"sum\" : 3}");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
  BOOST_CHECK(time_end - microsec_clock::local_time() > millisec(100));
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_insert_update )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // perform insert and check result
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("delete from pq_test.t; insert into pq_test.t values (1, 1); "
              "select count(*) from pq_test.t;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "1");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());

  // update and check result
  db.do_query("update pq_test.t set val=val + 1; select val from pq_test.t;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "2");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_null )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // check for null value
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select pq_test.pq_test('{\"null\":true}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.result_is_null() && db.get_result() == "");
  BOOST_CHECK(db.result_is_null());
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_error )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // check for error value
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select pq_test.pq_test('{\"error\":true}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result());
  BOOST_CHECK(db.result_is_error());
  BOOST_CHECK(db.get_result().starts_with("00001_ERROR:  00001\n"));

  // check for second query is working
  db.do_query("delete from pq_test.t; insert into pq_test.t values(1, 7); "
              "select val from pq_test.t;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "7");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_disconnect )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // do query and disconnect 
  db.do_query("select pq_test.pq_test('{\"a\":5, \"b\":6, \"sleep\":10}');");
  ptime time_end = microsec_clock::local_time() + millisec(10);
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  db.disconnect();

  // perform another query and check result
  time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select pq_test.pq_test('{\"a\" : 1, \"b\" : 2}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "{\"sum\" : 3}");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_in_progress )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // clear data
  ptime time_end = microsec_clock::local_time() + millisec(1000);
  db.do_query("delete from pq_test.t; insert into pq_test.t values(1, 7); "
              "select val from pq_test.t;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_REQUIRE(db.has_result() && db.get_result() == "7");

  // perform query and don't wait it to stop
  time_end = microsec_clock::local_time() + millisec(50);
  db.do_query("select pg_sleep(0.1); update pq_test.t set val = 8;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);

  // perform another query and check that previous query was cancelled
  time_end = microsec_clock::local_time() + millisec(10);
  db.do_query("select val from pq_test.t");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "7");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_abort )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr));

  // clear data
  ptime time_end = microsec_clock::local_time() + millisec(1000);
  db.do_query("delete from pq_test.t; insert into pq_test.t values(1, 7); "
              "select val from pq_test.t;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_REQUIRE(db.has_result() && db.get_result() == "7");

  // perform query and don't wait it to stop
  time_end = microsec_clock::local_time() + millisec(10);
  db.do_query("select pg_sleep(1000.1); update pq_test.t set val = 8;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  db.abort();
  time_end = microsec_clock::local_time() + millisec(150);
  while(time_end > microsec_clock::local_time()) { usleep(10); }

  // perform another query and check that previous query was cancelled
  time_end = microsec_clock::local_time() + millisec(10);
  db.do_query("select val from pq_test.t");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "7");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_error_conn_string )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect("blah blah blah", true)); // non blocking connect

  // perform query sleeping for 100 ms and check its time
  ptime time_end = microsec_clock::local_time() + millisec(1);
  db.do_query("select 1;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.result_is_error() && db.get_result() == 
    "missing \"=\" after \"blah\" in connection info string\n");
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_error_wrong_host )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect("host=192.168.0.1", true)); // non blocking connect

  // perform query sleeping for 100 ms and check its time
  ptime time_end = microsec_clock::local_time() + millisec(1);
  db.do_query("select 1;");
  while(db.poll() && time_end > microsec_clock::local_time()) { usleep(1); };
  BOOST_CHECK(db.result_is_error() && db.get_result().size() > 1);
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_error_wrong_query_client )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr, true)); // non blocking connect

  // perform query with wrong syntax - "client" error
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("blah;");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.result_is_error() && 
    db.get_result().find_first_of("42601_ERROR:  syntax error") == 0);

  // check what all is work
  db.do_query("select pq_test.pq_test('{\"a\" : 1, \"b\" : 2}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "{\"sum\" : 3}");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

//-----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE( test_error_wrong_query_server )
{
  // create database and connect
  PqAsync db;
  BOOST_REQUIRE(db.connect(kConnStr, true)); // non blocking connect

  // perform query with non-existent schema and function - server error
  ptime time_end = microsec_clock::local_time() + seconds(1);
  db.do_query("select nonexistentschema.nonexistent_query();");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.result_is_error() && 
    db.get_result().find_first_of("3F000_ERROR:  schema \"nonexistent") == 0);

  // check what all is work
  db.do_query("select pq_test.pq_test('{\"a\" : 1, \"b\" : 2}');");
  while(db.poll() && time_end > microsec_clock::local_time()) usleep(1);
  BOOST_CHECK(db.has_result() && db.get_result() == "{\"sum\" : 3}");
  if (db.result_is_error()) BOOST_ERROR(db.get_result());
}

