#define BOOST_TEST_MODULE Namecoin Test Suite

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

struct TestingSetup {
  TestingSetup() { }
  ~TestingSetup() { }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);
