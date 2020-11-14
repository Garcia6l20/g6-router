# General purpose CPP router

This project provides a c++20 router implementation.

It uses [compile-time-regular-expression](https://github.com/hanickadot/compile-time-regular-expressions) for route pattern matching.

## Install

This is a header-only library so, just grab a copy of it.

## Usage

### Basic

```c++
// define a router
g6::router::router my_router{
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value) -> std::string {
    //                       ^ captured value              ^ goes here
        return value;
    }),
    g6::router::on<R"(.*)">([]() -> std::string {
        return "not found";
    })};

// call it
assert(my_router("/echo/42") == "42");
assert(my_router("/this/does/not/exist") == "not found");
```

### Context

`g6::router` can be used with any contextual data.
- it can be set to the router globally:
  ```c++
  struct session {
    auto id = 24;
  };
  g6::router::router my_router{
    std::make_tuple(session{}),
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value,
                                        g6::router::context<session> session) -> std::string {
        return fmt::format("{}:{}", value, session->id);
    })};
  };
  assert(my_router("/echo/42") == "42:24");
  ```

- or per-call:
  ```c++
  struct session {
    auto id = 24;
  };
  g6::router::router my_router{
    g6::router::on<R"(/echo/(\w+))">([](const std::string &value,
                                        g6::router::context<session> session) -> std::string {
        return fmt::format("{}:{}", value, session->id);
    })};
  };
  assert(my_router("/echo/42", session{.id = 55}) == "42:55");
  ```

## Devel

It uses [cpppm](https://github.com/Garcia6l20/cpppm) internally (for testing).

Build example:

1. Get cpppm
```bash
pip3 install --user --upgrade cpppm
```

2. Build & Test
```bash
./project.py test g6-router
```
