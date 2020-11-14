#include <concepts>
#include <coroutine>

#include <g6/router.hpp>
#include <spdlog/spdlog.h>

#define BOOST_BEAST_USE_STD_STRING_VIEW
//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#define BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/outcome.hpp>

#include <boost/beast.hpp>

namespace outcome = boost::outcome_v2;

using namespace boost;

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;
namespace http      = beast::http;
namespace this_coro = asio::this_coro;

//#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
//#define use_awaitable boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
//#endif

struct responder {
  beast::tcp_stream &stream_;

  responder(beast::tcp_stream &stream)
      : stream_(stream) {}

  template <bool isRequest, class Body, class Fields>
  awaitable<void> operator()(http::message<isRequest, Body, Fields> &&msg) const {
    // We need the serializer here because the serializer requires
    // a non-const file_body, and the message oriented version of
    // http::write only works with const messages.
    http::serializer<isRequest, Body, Fields> sr{std::move(msg)};
    auto                                      res = co_await http::async_write(stream_, sr, use_awaitable);
    co_return;
  }
};

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

awaitable<bool> handle_request(http::request<http::string_body> request, responder const &responder) {

  auto [status, data] = router(request.target(), request.method());
  spdlog::info("target: {} -> {} ({})", request.target(), data, status);
  auto response = http::response<http::string_body>{std::piecewise_construct, std::make_tuple(std::move(data)),
                                                    std::make_tuple(status, request.version())};
  bool stop     = response.need_eof();
  co_await responder(std::move(response));
  co_return stop;
}


awaitable<void> co_session(beast::tcp_stream stream) {
  auto              executor = co_await this_coro::executor;
  bool              close    = false;
  beast::error_code ec;
  auto const &      socket = stream.socket();

  // This buffer is required to persist across reads
  beast::flat_buffer buffer;
  responder          responder{stream};

  for (;;) {
    stream.expires_after(std::chrono::seconds(30));
    // Read a request
    http::request<http::string_body> req{};
    auto                             res = co_await http::async_read(stream, buffer, req, use_awaitable);
    spdlog::info("{}:{}: {} {}", socket.remote_endpoint().address().to_string(), socket.remote_endpoint().port(),
                 req.method_string(), req.target());
    if (co_await handle_request(std::move(req), responder)) break;
  }

  // Send a TCP shutdown
  stream.socket().shutdown(tcp::socket::shutdown_send);
  co_return;
}

awaitable<int> co_serve() try {
  auto          executor = co_await this_coro::executor;
  tcp::acceptor acceptor(executor, {tcp::v4(), 55555});
  acceptor.set_option(asio::socket_base::reuse_address(true));
  acceptor.listen(asio::socket_base::max_listen_connections);
  for (;;) {
    tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
    spdlog::info("new connection: {}:{}", socket.remote_endpoint().address().to_string(),
                 socket.remote_endpoint().port());
    co_spawn(executor, co_session(beast::tcp_stream(std::move(socket))), asio::detached);
  }
  co_return 0;
} catch (std::exception &error) {
  spdlog::error("error: {}", error.what());
  co_return -1;
}

int main() {
  asio::io_context context{10};
  asio::signal_set signals(context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { context.stop(); });
  co_spawn(context, co_serve(), asio::detached);
  co_spawn(
    context,
    [&context]() -> awaitable<void> {
      auto get_hello = co_spawn(
        context,
        [&]() -> awaitable<bool> {
          tcp::resolver     resolver{context};
          beast::tcp_stream stream{context};
          auto const        results =
            co_await resolver.async_resolve({asio::ip::address::from_string("127.0.0.1"), 55555}, use_awaitable);
          stream.expires_after(std::chrono::seconds(30));
          co_await stream.async_connect(results, use_awaitable);
          http::request<http::empty_body>   req{http::verb::get, "/hello/asio", 11};
          beast::flat_buffer                b;
          http::response<http::string_body> res;
          co_await http::async_write(stream, req, use_awaitable);
          co_await http::async_read(stream, b, res, use_awaitable);
          spdlog::info("Got: {}", std::string_view{res.body().data(), res.body().size()});
          stream.socket().shutdown(tcp::socket::shutdown_both);
          co_return res.result() == http::status::ok;
        },
        use_awaitable);
      auto get_not_found = co_spawn(
        context,
        [&]() -> awaitable<bool> {
          tcp::resolver     resolver{context};
          beast::tcp_stream stream{context};
          auto const        results =
            co_await resolver.async_resolve({asio::ip::address::from_string("127.0.0.1"), 55555}, use_awaitable);
          stream.expires_after(std::chrono::seconds(30));
          co_await stream.async_connect(results, use_awaitable);
          http::request<http::empty_body>   req{http::verb::get, "/this/doesnt/exist", 11};
          beast::flat_buffer                b;
          http::response<http::string_body> res;
          co_await http::async_write(stream, req, use_awaitable);
          co_await http::async_read(stream, b, res, use_awaitable);
          spdlog::info("Got: {}", std::string_view{res.body().data(), res.body().size()});
          stream.socket().shutdown(tcp::socket::shutdown_both);
          co_return res.result() == http::status::not_found;
        },
        use_awaitable);
      assert(co_await std::move(get_hello));
      assert(co_await std::move(get_not_found));
      context.stop();
      co_return;
    }(),
    detached);
  context.run();
  return 0;
}
