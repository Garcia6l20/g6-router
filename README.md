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
  
### Extending

As a router is almost always used in HTTP applications it should be easily
expandable, to be usable with any backend.

- boost beast example:

    ```c++
    namespace http = beast::http;
  
    namespace route {
      template <auto pattern, http::verb method, typename HandlerT>
      struct handler : g6::router::detail::handler<pattern, HandlerT> {
        using base = g6::router::detail::handler<pattern, HandlerT>;
    
        handler(HandlerT &&handler) noexcept
            : base{std::forward<HandlerT>(handler)} {}
    
        using base::matches;
    
        template <typename ContextT, typename ArgsT>
        std::optional<typename base::result_t> operator()(ContextT &context, std::string_view path, ArgsT &&args) {
          if (std::get<http::verb>(args) == method) {
            return base::operator()(context, path, std::forward<ArgsT>(args));
          } else {
            return {};
          }
        }
      };
    
      template <ctll::fixed_string pattern, typename HandlerT>
      constexpr auto get(HandlerT &&handler) noexcept {
        return route::handler<pattern, http::verb::get, HandlerT>{std::forward<HandlerT>(handler)};
      }
      template <ctll::fixed_string pattern, typename HandlerT>
      constexpr auto post(HandlerT &&handler) noexcept {
        return route::handler<pattern, http::verb::post, HandlerT>{std::forward<HandlerT>(handler)};
      }
    }// namespace route
  
    struct route_result {
      http::status status;
      std::string  data;
    };
    
    static auto router = g6::router::router{
      std::make_tuple(),// global context
      route::get<R"(/hello/(\w+))">([](const std::string &who) -> route_result {
        return {http::status::ok, fmt::format("Hello {} !", who)};
      }),
      g6::router::on<R"(.*)">([]() -> route_result {
        return {http::status::not_found, "Not found"};
      }),
    };
  
    // ...
    awaitable<bool> handle_request(http::request<http::string_body> request, responder const &responder) {
    
      auto [status, data] = router(request.target(), request.method());
      auto response = http::response<http::string_body>{std::piecewise_construct, std::make_tuple(std::move(data)),
                                                        std::make_tuple(status, request.version())};
      bool stop     = response.need_eof();
      co_await responder(std::move(response));
      co_return stop;
    }
    // ...
    ```

A working example using `boost::beast` is available [here](examples/http_router.cpp).

## Devel

It uses [cpppm](https://github.com/Garcia6l20/cpppm) internally (for testing and building examples).

Build example:

1. Get cpppm
```bash
pip3 install --user --upgrade cpppm
```

2. Build & Test
```bash
./project.py test g6-router
```
