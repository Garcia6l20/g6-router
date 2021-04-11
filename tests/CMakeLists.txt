find_package(fmt REQUIRED)

link_libraries(g6::router fmt::fmt)

g6_add_unit_test(basic-route-test.cpp)
