#pragma once

#include <algorithm>
#include <concepts>
#include <format>
#include <locale>
#include <string>

#include <tjg/format_specs.hpp>

namespace tjg {

namespace detail {

template<typename value_type>
concept clip_float = std::same_as<value_type, float> ||
                     std::same_as<value_type, double> ||
                     std::same_as<value_type, long double>;

template<clip_float value_type>
auto format_clipped(value_type value, const format_spec& spec,
                    const std::locale& locale) -> std::string;

} // namespace detail

template<detail::clip_float value_type>
struct clipped_arg {
  value_type value;
};

// With a width, reduce precision until the value fits or precision is zero.
template<detail::clip_float value_type>
[[nodiscard]] constexpr auto clipped(value_type value)
  -> clipped_arg<value_type> {
  return {value};
}

} // namespace tjg

template<tjg::detail::clip_float value_type>
struct std::formatter<tjg::clipped_arg<value_type>> : tjg::format_spec_parser {
  template<typename format_context>
  auto format(tjg::clipped_arg<value_type> arg, format_context& ctx) const
    -> decltype(ctx.out()) {
    const auto spec = specs.resolve(ctx);
    const auto text = tjg::detail::format_clipped(arg.value, spec,
                                                 ctx.locale());
    return std::ranges::copy(text, ctx.out()).out;
  }
};
