#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <g6/router.hpp>

TEST_CASE("basic router usage", "[g6][router][basic-usage]") {
  g6::router::router test_router{
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value) -> std::string { return value; }),
    g6::router::on<R"(.*)">([]() -> std::string { return "not found"; })};
  REQUIRE(test_router("/echo/42") == "42");
  REQUIRE(test_router("/this/does/not/exist") == "not found");
}
