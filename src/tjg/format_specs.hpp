#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace tjg {

struct format_spec {
  std::array<char, 4> fill_bytes{' '};
  std::size_t fill_size = 1;
  char alignment = '\0';
  char sign      = '\0';
  bool alternate = false;
  bool zero_pad  = false;
  std::optional<int> width;
  std::optional<int> precision;
  std::optional<std::size_t> width_arg;
  std::optional<std::size_t> precision_arg;
  bool localized = false;
  char type = '\0';

  [[nodiscard]] constexpr auto fill() const -> std::string_view
  { return {fill_bytes.data(), fill_size}; }

  // Resolve dynamic fields against the same argument list as the outer format.
  template<typename format_context>
  [[nodiscard]] auto resolve(format_context& ctx) const -> format_spec {
    auto result = *this;

    auto get = [&ctx](std::size_t id) {
      auto get_int = [](auto value) -> int {
        using value_type = decltype(value);
        if constexpr (std::same_as<value_type, int>
                      || std::same_as<value_type, unsigned int>
                      || std::same_as<value_type, long long>
                      || std::same_as<value_type, unsigned long long>)
        {
          if (   std::cmp_less(value, 0)
              || std::cmp_greater(value, std::numeric_limits<int>::max()))
          {
            throw std::format_error{"width or precision is out of range"};
          }
          return static_cast<int>(value);
        } else {
          throw std::format_error{"width or precision requires an integer"};
        }
      }; // get_int
      return std::visit_format_arg(get_int, ctx.arg(id));
    }; // get

    if (width_arg) {
      result.width = get(*width_arg);
      result.width_arg.reset();
    }
    if (precision_arg) {
      result.precision = get(*precision_arg);
      result.precision_arg.reset();
    }
    return result;
  } // resolve
}; // format_spec

// A base for narrow-character formatters using the floating-point grammar.
// The derived formatter supplies format() and decides how to use specs.
class format_spec_parser {
protected:
  format_spec specs;

private:
  // Only used to recognize a single fill character in a constexpr parse.
  static constexpr auto utf8_fill_size(std::string_view text) -> std::size_t {
    const auto first = static_cast<unsigned char>(text.front());
    if (first < 0x80)
      return 1;

    std::size_t size = 0;
    if      (first >= 0xc2 && first <= 0xdf)
      size = 2;
    else if (first >= 0xe0 && first <= 0xef)
      size = 3;
    else if (first >= 0xf0 && first <= 0xf4)
      size = 4;
    if (size == 0 || text.size() < size)
      return 0;

    const auto second = static_cast<unsigned char>(text[1]);
    if (   (first == 0xe0 && second <  0xa0)
        || (first == 0xed && second >= 0xa0)
        || (first == 0xf0 && second <  0x90)
        || (first == 0xf4 && second >= 0x90))
      return 0;
    for (std::size_t i = 1; i < size; ++i) {
      if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
        return 0;
    }
    return size;
  } // utf8_fill_size

public:
  constexpr auto parse(std::format_parse_context& ctx)
    -> std::format_parse_context::iterator
  {
    auto parsed = format_spec{};
    auto it = ctx.begin();
    const auto end = ctx.end();

    auto peek = [&] { return (it == end) ? '\0' : *it; };

    auto is_alignment = [](char ch) {
      return ch == '<' || ch == '>' || ch == '^';
    };

    auto is_digit = [](char ch) { return ch >= '0' && ch <= '9'; };

    if (it != end && *it != '}') {
      const auto size = utf8_fill_size({it, end});
      const auto next = it + size;
      if (size != 0 && next != end && is_alignment(*next)) {
        if (*it == '{')
          throw std::format_error{"invalid fill character"};
        for (std::size_t i = 0; i < size; ++i)
          parsed.fill_bytes[i] = it[i];
        parsed.fill_size = size;
        parsed.alignment = *next;
        it = next + 1;
      } else if (is_alignment(*it)) {
        parsed.alignment = *it++;
      }
    }

    auto consume = [&](char ch) {
      if (peek() != ch)
        return false;
      ++it;
      return true;
    }; // consume

    auto number = [&] -> int {
      if (!is_digit(peek()))
        throw std::format_error{"expected a decimal integer"};
      int value = 0;
      while (is_digit(peek())) {
        const int digit = *it++ - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10)
          throw std::format_error{"format specifier integer is too large"};
        value = value * 10 + digit;
      }
      return value;
    }; // number

    auto argument_id = [&]() -> std::size_t {
      if (consume('}'))
        return ctx.next_arg_id();
      const auto id = consume('0') ? 0 : number();
      if (!consume('}'))
        throw std::format_error{"invalid dynamic width or precision"};
      ctx.check_arg_id(static_cast<std::size_t>(id));
      return static_cast<std::size_t>(id);
    }; // argument_id

    if (peek() == '+' || peek() == '-' || peek() == ' ')
      parsed.sign = *it++;
    parsed.alternate = consume('#');
    parsed.zero_pad  = consume('0');
    if (consume('{'))
      parsed.width_arg = argument_id();
    else if (peek() >= '1' && peek() <= '9')
      parsed.width = number();
    if (consume('.')) {
      if (consume('{'))
        parsed.precision_arg = argument_id();
      else
        parsed.precision = number();
    }
    parsed.localized = consume('L');
    if (it != end && std::string_view{"aAeEfFgG"}.contains(*it))
      parsed.type = *it++;
    if (it != end && *it != '}')
      throw std::format_error{"invalid floating-point format specifier"};
    specs = parsed;
    return it;
  }
}; // format_spec_parser

} // tjg
