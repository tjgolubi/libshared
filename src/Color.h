#pragma once

#include <fmt/color.h>
#include <string>
#include <string_view>

using Color = fmt::text_style;

namespace Style {

// Taskwarrior theme syntax, independent of the style representation.
[[nodiscard]] auto Parse(const std::string&) -> fmt::text_style;
[[nodiscard]] auto Spec(const fmt::text_style&) -> std::string;
[[nodiscard]] auto strip_sgr(std::string_view) -> std::string;

inline std::string colorize(std::string_view input, const std::string& spec)
{ return fmt::wrap_sgr(Parse(spec), input); }

} // Style
