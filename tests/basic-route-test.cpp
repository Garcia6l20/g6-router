#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <fmt/format.h>

#include <g6/router.hpp>

TEST_CASE("basic router usage", "[g6][router][basic]") {
  g6::router::router test_router{
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value) -> std::string { return value; }),
    g6::router::on<R"(.*)">([]() -> std::string { return "not found"; })};
  REQUIRE(test_router("/echo/42") == "42");
  REQUIRE(test_router("/this/does/not/exist") == "not found");
}

TEST_CASE("global context router usage", "[g6][router][context]") {
  struct session {
    int id = 24;
  };
  g6::router::router test_router{
    std::make_tuple(session{}),
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value, g6::router::context<session> session) -> std::string {
      return fmt::format("{}:{}", value, session->id);
    }),
    g6::router::on<R"(.*)">([]() -> std::string { return "not found"; })};
  REQUIRE(test_router("/echo/42") == "42:24");
  REQUIRE(test_router("/this/does/not/exist") == "not found");
}

SCENARIO("local context router usage", "[g6][router][context]") {
  struct session {
    int id = 24;
  };

  GIVEN("a router using context session") {
    g6::router::router test_router{g6::router::on<R"(/echo/(\w+))">(
      [](const std::string &value, g6::router::context<session> session) -> std::string {
        session->id += 1;
        return fmt::format("{}:{}", value, session->id);
      })};
    WHEN("i pass a session as std::reference") {
      session s{.id = 41};
      REQUIRE(test_router("/echo/42", std::ref(s)) == "42:42");
      REQUIRE(s.id == 42);
    }
    WHEN("i pass a session as reference") {
      session s{.id = 41};
      REQUIRE(test_router("/echo/42", std::ref(s)) == "42:42");
      REQUIRE(s.id == 42);
    }
    WHEN("i pass a session as value") { REQUIRE(test_router("/echo/42", session{.id = 41}) == "42:42"); }
  }
  //  REQUIRE(test_router("/this/does/not/exist", session{.id = 51}) == "not found");
}
