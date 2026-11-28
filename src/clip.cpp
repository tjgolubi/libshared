#include "clip.h"

#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace tjg::detail {
namespace {

template<clip_float value_type>
auto render(value_type value, const format_spec& spec,
            std::optional<int> width, std::optional<int> precision,
            const std::locale& locale) -> std::string {
  std::string pattern = "{0:";
  if (width && spec.alignment) {
    pattern += spec.fill();
    pattern += spec.alignment;
  }
  if (spec.sign)
    pattern += spec.sign;
  if (spec.alternate)
    pattern += '#';
  if (width) {
    if (spec.zero_pad)
      pattern += '0';
    pattern += "{1}";
  }
  if (precision)
    pattern += ".{2}";
  if (spec.localized)
    pattern += 'L';
  if (spec.type)
    pattern += spec.type;
  pattern += '}';

  const int field_width = width.value_or(0);
  const int digits = precision.value_or(0);
  return std::vformat(locale, pattern,
                      std::make_format_args(value, field_width, digits));
}

auto pad_wide_fill(std::string_view text, const format_spec& spec, int width)
  -> std::string {
  const auto padding = std::formatted_size("{:>{}}", text, width) - text.size();
  const auto left = spec.alignment == '^' ? padding / 2
                  : spec.alignment == '>' ? padding : 0;
  const auto right = padding - left;

  std::string result;
  const auto pad = [&](std::size_t columns) {
    for (std::size_t i = 0; i < columns / 2; ++i)
      result += spec.fill();
  };
  pad(left);
  result.append(left % 2, ' ');
  result += text;
  result.append(right % 2, ' ');
  pad(right);
  return result;
}

} // namespace

template<clip_float value_type>
auto format_clipped(value_type value, const format_spec& spec,
                    const std::locale& locale) -> std::string {
  if (!spec.width)
    return render(value, spec, {}, spec.precision, locale);
  const int width = *spec.width;
  if (width == 0)
    return render(value, spec, {}, 0, locale);

  const bool wide_fill = spec.fill_size > 1 &&
                         std::format("{:.1}", spec.fill()).empty();
  const auto columns = static_cast<std::size_t>(width);
  const auto fits = [&](const std::string& text) {
    return text.size() <= columns ||
           (spec.localized && std::format("{:.{}}", text, width) == text);
  };
  const auto pad = [&](std::string text, std::optional<int> precision) {
    if (wide_fill)
      return pad_wide_fill(text, spec, width);
    if (!spec.localized && text.size() == columns)
      return text;
    return render(value, spec, width, precision, locale);
  };

  using limits = std::numeric_limits<value_type>;
  const bool general = spec.type == '\0' || spec.type == 'g' ||
                       spec.type == 'G';
  const bool hex = spec.type == 'a' || spec.type == 'A';
  int precision = spec.precision.value_or(6);
  if (!spec.precision && (spec.type == '\0' || hex)) {
    auto text = render(value, spec, {}, {}, locale);
    if (fits(text))
      return pad(std::move(text), {});
    // Start with enough digits to preserve the value after trying shortest.
    precision = hex ? (limits::digits + 2) / 4 : limits::max_digits10;
  }

  // Any fitting result with more significant digits requested than columns
  // must have dropped trailing zeros. Rounding at width yields the same value.
  precision = std::min(precision, width);
  if (general && !spec.alternate) {
    // General presentation removes trailing zeros. This bound exceeds the
    // number of digits in any exact decimal expansion of the binary type.
    const int exact_digits = std::max(
      limits::max_exponent, 2 * limits::digits - limits::min_exponent);
    precision = std::min(precision, exact_digits);
  }

  for (;; --precision) {
    auto text = render(value, spec, {}, precision, locale);
    if (fits(text))
      return pad(std::move(text), precision);
    if (precision == 0)
      return text;
  }
}

template auto format_clipped(float, const format_spec&, const std::locale&)
  -> std::string;
template auto format_clipped(double, const format_spec&, const std::locale&)
  -> std::string;
template auto format_clipped(long double, const format_spec&,
                             const std::locale&) -> std::string;

} // namespace tjg::detail
