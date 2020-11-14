#pragma once

#include <ctre.hpp>
#include <tuple>
#include <variant>

#include <charconv>
#include <filesystem>
#include <functional>
#include <optional>

namespace g6::router {

  namespace detail {
    template <class T, template <class...> class Template>
    struct is_specialization : std::false_type {};

    template <template <class...> class Template, class... Args>
    struct is_specialization<Template<Args...>, Template> : std::true_type {};

    template <class T, template <class...> class Template>
    concept specialization_of = is_specialization<std::decay_t<T>, Template>::value;

    template <typename T>
    concept is_tuple = specialization_of<T, std::tuple>;

    template <typename T, typename Tuple>
    struct tuple_contains : std::false_type {};

    template <typename T, typename... Us>
    struct tuple_contains<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...> {};

    template <typename Tuple, typename T>
    constexpr bool tuple_contains_v = tuple_contains<T, Tuple>::value;

    template <typename TupleT>
    struct tuple_remove_const_refs;

    template <typename... TypesT>
    struct tuple_remove_const_refs<std::tuple<TypesT...>> {
      template <typename T>
      using cleanup = std::conditional_t<std::is_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>,
                                         std::remove_const_t<std::remove_reference_t<T>>, T>;

      using type = std::tuple<cleanup<TypesT>...>;
    };

    template <typename TupleT>
    using tuple_remove_const_refs_t = typename tuple_remove_const_refs<TupleT>::type;

    namespace impl {
      template <typename T, typename Tuple>
      struct tuple_type_count_impl;
      template <typename T, typename... Types>
      struct tuple_type_count_impl<T, std::tuple<Types...>> {
        template <typename ItemT>
        static constexpr int  match = std::is_same<T, ItemT>::value;
        static constexpr auto value = (match<Types> + ...);
      };
    }// namespace impl

    template <typename TupleT, typename T>
    constexpr auto tuple_type_count_v = impl::tuple_type_count_impl<T, TupleT>::value;

    namespace impl {
      template <typename TupleT>
      struct tuple_make_unique_impl;

      template <typename... Types>
      struct tuple_make_unique_impl<std::tuple<Types...>> {

        template <typename T, typename... RestT>
        using cond_type =
          typename std::conditional < tuple_type_count_v<std::tuple<RestT...>, T><1, std::tuple<T>, std::tuple<>>::type;

        template <typename... Args>
        struct builder;

        template <typename FirstT, typename... RestT>
        struct builder<FirstT, RestT...> {
          constexpr auto operator()() { return std::tuple_cat(cond_type<FirstT, RestT...>{}, builder<RestT...>{}()); }
        };

        template <typename LastT>
        struct builder<LastT> {
          constexpr auto operator()() { return std::tuple<LastT>(); }
        };
        using type = decltype(builder<Types...>{}());
      };
    }// namespace impl

    /** @brief Remove duplicate types
     *
     */
    template <class TupleT>
    using tuple_make_unique_t = typename impl::tuple_make_unique_impl<TupleT>::type;

    template <typename TupleT>
    struct tuple_to_variant;

    template <typename... TypesT>
    struct tuple_to_variant<std::tuple<TypesT...>> {
      using type = std::variant<TypesT...>;
    };

    template <typename TupleT>
    using tuple_to_variant_t = typename tuple_to_variant<TupleT>::type;

    struct break_t {
      bool               break_ = false;
      constexpr explicit operator bool() const { return break_; }
    };

    constexpr break_t break_{.break_ = true};
    constexpr break_t continue_{.break_ = false};

    namespace impl {
      template <typename T, typename... Args>
      concept is_index_invocable = requires(T v, Args... args) {
        {v.template operator()<std::size_t(42)>(args...)};
      };
      template <typename T, typename... Args>
      concept is_index_invocable_with_params = requires(T v, Args... args) {
        {v.template operator()<std::size_t(42), Args...>()};
      };

      template <size_t index = 0, typename TupleT, typename LambdaT>
      constexpr auto for_each(TupleT &tuple, LambdaT &&lambda) {
        if constexpr (std::tuple_size_v<TupleT> == 0) {
          return;
        } else {
          using ValueT = std::decay_t<decltype(std::get<index>(tuple))>;
          auto invoker = [&]() mutable {
            if constexpr (is_index_invocable<decltype(lambda), ValueT>) {
              return lambda.template operator()<index>(std::get<index>(tuple));
            } else {
              return lambda(std::get<index>(tuple));
            }
          };
          using ReturnT = decltype(invoker());
          if constexpr (std::is_void_v<ReturnT>) {
            invoker();
            if constexpr (index + 1 < std::tuple_size_v<TupleT>) {
              return for_each<index + 1>(tuple, std::forward<LambdaT>(lambda));
            }
          } else if constexpr (std::same_as<ReturnT, break_t>) {
            // non-constexpr break
            if constexpr (index >= std::tuple_size_v<TupleT>) {
              return;// did not break
            } else {
              if (invoker()) {
                return;
              } else {
                if constexpr (index + 1 < std::tuple_size_v<TupleT>) {
                  for_each<index + 1>(tuple, std::forward<LambdaT>(lambda));
                }
              }
            }
          } else {
            return invoker();
          }
        }
      }
    }// namespace impl

    constexpr decltype(auto) for_each(is_tuple auto &tuple, auto &&functor) {
      return impl::for_each(tuple, std::forward<decltype(functor)>(functor));
    }

    namespace impl {
      template <typename Ret, typename Cls, bool IsMutable, bool IsLambda, typename... Args>
      struct function_type : std::true_type {
        static constexpr bool is_mutable = IsMutable;
        static constexpr bool is_lambda  = IsLambda;

        static constexpr bool is_function = not is_lambda;

        enum { arity = sizeof...(Args) };

        using return_type = Ret;

        using tuple_type = std::tuple<Args...>;

        template <size_t i>
        struct arg {
          using type       = typename std::tuple_element<i, tuple_type>::type;
          using clean_type = std::remove_cvref_t<type>;
        };

        using fn_type    = std::function<Ret(Args...)>;
        using args_tuple = std::tuple<Args...>;
      };
    }// namespace impl

    // primary template: not a function
    template <class T, typename = void>
    struct function_traits : std::false_type {};

    // default resolution with operator()
    // forwards to corresponding
    template <class T>
    struct function_traits<T, std::void_t<decltype(&T::operator())>> : function_traits<decltype(&T::operator())> {};

    // callable
    template <class Ret, class Cls, class... Args>
    struct function_traits<Ret (Cls::*)(Args...)> : impl::function_type<Ret, Cls, true, true, Args...> {};

    // noexcept callable
    template <class Ret, class Cls, class... Args>
    struct function_traits<Ret (Cls::*)(Args...) noexcept> : impl::function_type<Ret, Cls, true, true, Args...> {};

    // const callable
    template <class Ret, class Cls, class... Args>
    struct function_traits<Ret (Cls::*)(Args...) const> : impl::function_type<Ret, Cls, false, true, Args...> {};

    // const noexcept callable
    template <class Ret, class Cls, class... Args>
    struct function_traits<Ret (Cls::*)(Args...) const noexcept>
        : impl::function_type<Ret, Cls, false, true, Args...> {};

    // function
    template <class Ret, class... Args>
    struct function_traits<std::function<Ret(Args...)>>
        : impl::function_type<Ret, std::nullptr_t, true, true, Args...> {};

    // c-style function
    template <class Ret, class... Args>
    struct function_traits<Ret (*)(Args...)> : impl::function_type<Ret, std::nullptr_t, true, false, Args...> {};

  }// namespace detail

  template <typename T>
  struct route_parameter;

  template <>
  struct route_parameter<std::string> {
    static constexpr int group_count() { return 0; }
    static std::string   load(const std::string_view &input) { return {input.data(), input.data() + input.size()}; }

    static constexpr auto pattern = ctll::fixed_string{R"(.+(?=/)|.+)"};
  };

  template <>
  struct route_parameter<std::string_view> {
    static constexpr int    group_count() { return 0; }
    static std::string_view load(const std::string_view &input) { return {input.data(), input.size()}; }

    static constexpr auto pattern = ctll::fixed_string{R"(.+(?=/)|.+)"};
  };

  template <>
  struct route_parameter<std::filesystem::path> {
    static constexpr int         group_count() { return 0; }
    static std::filesystem::path load(const std::string_view &input) { return {input.data()}; }
    static constexpr auto        pattern = ctll::fixed_string{R"(.+)"};
  };

  template <>
  struct route_parameter<int> {
    static constexpr int group_count() { return 0; }
    static int           load(const std::string_view &input) {
      int result = 0;
      std::from_chars(input.data(), input.data() + input.size(), result);
      return result;
    }
    static constexpr auto pattern = ctll::fixed_string{R"(\d+)"};
  };

  template <>
  struct route_parameter<double> {
    static constexpr int group_count() { return 0; }
    static double        load(const std::string_view &input) {
      double result = 0;
      std::from_chars(input.data(), input.data() + input.size(), result);
      return result;
    }
    static constexpr auto pattern = ctll::fixed_string{R"(\d+\.?\d*)"};
  };

  template <>
  struct route_parameter<bool> {
    static bool load(const std::string_view &input) {
      static const std::vector<std::string> true_values = {"yes", "on", "true"};
      return std::any_of(true_values.begin(), true_values.end(), [&input](auto &&elem) {
        return std::equal(elem.begin(), elem.end(), input.begin(), input.end(),
                          [](char left, char right) { return tolower(left) == tolower(right); });
      });
    }
    static constexpr auto pattern = ctll::fixed_string{R"(\w+)"};
  };

  template <typename T>
  struct context {
    using type = T;
                   operator bool() { return elem_ != nullptr; }
    decltype(auto) operator*() { return *elem_; }
    decltype(auto) operator->() { return elem_; }
    auto &         operator=(T &elem) {
      elem_ = &elem;
      return *this;
    }
    T *elem_ = nullptr;
  };

  namespace detail {
    template <auto route_, typename FnT>
    class handler {
    public:
      constexpr explicit handler(FnT &&fn) noexcept
          : fn_{std::forward<decltype(fn)>(fn)} {}
      static constexpr auto route = route_;
      using fn_type               = FnT;

    private:
      using fn_trait = function_traits<fn_type>;

      using parameters_tuple_type = detail::tuple_remove_const_refs_t<typename fn_trait::args_tuple>;

      FnT fn_;

      using builder_type                  = ctre::regex_builder<route>;
      static constexpr inline auto match_ = ctre::regex_match_t<typename builder_type::type>();
      std::invoke_result_t<decltype(match_), std::string_view> match_result_;

      template <int type_idx, int match_idx, typename MatcherT, typename ContextT, typename ArgsT>
      void _load_data(ContextT &context, parameters_tuple_type &data, const MatcherT &match, ArgsT &&args) {
        if constexpr (type_idx < fn_trait::arity) {
          using ParamT = typename fn_trait::template arg<type_idx>::clean_type;
          if constexpr (detail::specialization_of<ParamT, g6::router::context>) {
            if constexpr (detail::tuple_contains_v<ArgsT, typename ParamT::type>) {
              std::get<type_idx>(data) = std::get<typename ParamT::type>(args);
            } else if constexpr (detail::tuple_contains_v<ArgsT, typename ParamT::type &>) {
              std::get<type_idx>(data) = std::get<typename ParamT::type &>(args);
            } else if constexpr (detail::tuple_contains_v<ArgsT, std::reference_wrapper<typename ParamT::type>>) {
              std::get<type_idx>(data) = std::get<typename ParamT::type &>(args);
            } else {
              std::get<type_idx>(data) = std::get<typename ParamT::type>(context);
            }
            _load_data<type_idx + 1, match_idx>(context, data, match, std::forward<ArgsT>(args));
          } else {
            std::get<type_idx>(data) = route_parameter<ParamT>::load(match.template get<match_idx>());
            _load_data<type_idx + 1, match_idx + route_parameter<ParamT>::group_count() + 1>(context, data, match,
                                                                                             std::forward<ArgsT>(args));
          }
        }
      }

      template <typename MatcherT, typename ContextT, typename ArgsT>
      bool load_data(ContextT &context, const MatcherT &match, parameters_tuple_type &data, ArgsT &&args) {
        _load_data<0, 1>(context, data, match, std::forward<ArgsT>(args));
        return true;
      }

    public:
      constexpr bool matches(std::string_view url) {
        match_result_ = std::move(match_(url));
        return bool(match_result_);
      }

      using result_t = typename fn_trait::return_type;

      template <typename ContextT, typename ArgsT>
      std::optional<result_t> operator()(ContextT &context, std::string_view path, ArgsT &&args) {
        if (matches(path)) {
          parameters_tuple_type data{};
          load_data(context, match_result_, data, std::forward<ArgsT>(args));
          return std::apply(fn_, data);
        } else {
          return {};
        }
      }

      struct handler_view {
        static constexpr auto route = route_;
        using fn_type               = FnT;
        fn_type fn_;
      };
    };

  }// namespace detail

  template <ctll::fixed_string route, typename FnT>
  constexpr auto on(FnT &&fn) noexcept {
    return detail::handler<route, FnT>{std::forward<FnT>(fn)};
  }

  template <detail::is_tuple ContextT = std::tuple<>, typename... HandlersT>
  class router {
    ContextT context_{};
    using handlers_t = std::tuple<HandlersT...>;

  public:
    constexpr explicit router(HandlersT &&... handlers) noexcept
        : handlers_{std::forward<HandlersT>(handlers)...} {}

    constexpr explicit router(ContextT context, HandlersT &&... handlers) noexcept
        : context_{std::move(context)}
        , handlers_{std::forward<HandlersT>(handlers)...} {}

    using handlers_return_tuple = detail::tuple_make_unique_t<
      std::tuple<typename detail::handler<HandlersT::route, typename HandlersT::fn_type>::result_t...>>;
    using result_t = std::conditional_t<
      detail::tuple_type_count_v<handlers_return_tuple, std::tuple_element_t<0, handlers_return_tuple>> ==
        std::tuple_size_v<handlers_return_tuple>,
      // all handlers returns same type = dont use variant
      std::tuple_element_t<0, handlers_return_tuple>, detail::tuple_to_variant_t<handlers_return_tuple>>;

    template <typename... HandlerArgsT>
    constexpr auto operator()(std::string_view path, HandlerArgsT &&... args) {
      result_t output;
      detail::for_each(handlers_, [&](auto &&handler) {
        if (auto result = handler(context_, path, std::make_tuple(std::forward<HandlerArgsT>(args)...)); result) {
          output = std::move(result.value());
          return detail::break_;
        }
        return detail::continue_;
      });
      return output;
    }

  protected:
    handlers_t handlers_;
  };
  // template <typename TupleT, typename...HandlersT>

}// namespace g6
