#include <Color.h>
#include <shared.h>
#include <format.h>

#include <array>
#include <cstdlib>

namespace {

constexpr auto color_names = std::array<std::string_view, 9>{
  "none", "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"
};

int find_color_name(std::string_view input) {
  for (std::size_t i=0; i != color_names.size(); ++i) {
    if (color_names[i] == input)
      return static_cast<int>(i);
  }
  return -1;
} // find_color_name

[[nodiscard]] constexpr fmt::color_t to_bright(fmt::color_t clr) noexcept {
  if (!fmt::holds_alternative<fmt::term_color>(clr))
    return clr;
  auto val = fmt::to_underlying(fmt::get<fmt::term_color>(clr)) | 0x08;
  return fmt::enum_cast<fmt::term_color>(val);
}

[[nodiscard]] constexpr
fmt::color_t indexed_color(int index) noexcept {
  if (index >= 256)
    return fmt::no_color;
  if (index < 16)
    return fmt::enum_cast<fmt::term_color>(index);
  if (index < 232)
    return fmt::cube_color{static_cast<unsigned>(index - 16)};
  return fmt::gray_color{index - 232};
} // indexed_color

std::string to_string(fmt::color_t clr) {
  if (clr == fmt::no_color) return "none";

  if (fmt::holds_alternative<fmt::rgb>(clr)) {
    auto val = fmt::to_underlying(fmt::get<fmt::rgb>(clr));
    return fmt::format("{:#08x}", val);
  }

  if (fmt::holds_alternative<fmt::term_color>(clr)) {
    auto val = to_underlying(fmt::get<fmt::term_color>(clr));
    auto result = std::string{};
    result.reserve(16);
    constexpr auto bright_black_value =
                              fmt::to_underlying(fmt::term_color::bright_black);
    if (val >= bright_black_value) {
      result = "bright ";
      val -= bright_black_value;
    }
    result += color_names[val + 1];
    return result;
  }

  auto val = unsigned{0};
  if (fmt::holds_alternative<fmt::cube_color>(clr))
    val = to_underlying(fmt::get<fmt::cube_color>(clr)) +  16;
  else if (fmt::holds_alternative<fmt::gray_color>(clr))
    val = to_underlying(fmt::get<fmt::gray_color>(clr)) + 232;
  return fmt::format("color{}", val);
} // to_string

} // local

namespace Style {

////////////////////////////////////////////////////////////////////////////////
// Supports the following constructs:
//   [bright] [color] [on color] [bright] [underline]
//
// Where [color] is one of:
//   black
//   red
//   ...
//   grayN    0 <= N <= 23     fg 38;5;232 + N              bg 48;5;232 + N
//   greyN    0 <= N <= 23     fg 38;5;232 + N              bg 48;5;232 + N
//   colorN   0 <= N <= 255    fg 38;5;N                    bg 48;5;N
//   rgbRGB   0 <= R,G,B <= 5  fg 38;5;16 + R*36 + G*6 + B  bg 48;5;16 + R*36 + G*6 + B
//   0xRRGGBB 0 <= R,G,B <= FF fg 38;2;R;G;B                bg 48;2;R;G;B
auto Parse(const std::string& spec) -> fmt::text_style {
  // Split spec into words.
  auto words = split(spec, ' ');

  // Parse foreground and background independently.
  fmt::color_t fg_value = fmt::no_color;
  fmt::color_t bg_value = fmt::no_color;

  auto emph = fmt::emphasis{};

  fmt::color_t* valptr = &fg_value;
  bool bright = false;
  int index   = -1;
  for (auto& word : words) {
    word = lowerCase(trim(word));

         if (word == "bold")      emph |= fmt::emphasis::bold;
    else if (word == "bright")    bright = true;
    else if (word == "underline") emph |= fmt::emphasis::underline;
    else if (word == "inverse" || word == "reverse")
                                  emph |= fmt::emphasis::reverse;
    else if (word == "strike")    emph |= fmt::emphasis::strike;
    else if (word == "italic")    emph |= fmt::emphasis::italic;
    else if (word == "faint" || word == "dim")
                                  emph |= fmt::emphasis::faint;
    else if (word == "on") {
      if (bright) *valptr = to_bright(*valptr);
      valptr = &bg_value;
      bright = false;
    }

    // X where X is one of black, red, blue ...
    else if ((index = find_color_name(word)) != -1) {
      if (index > 0)
        *valptr = fmt::enum_cast<fmt::term_color>(index - 1);
    }

    // greyN/grayN, where 0 <= N <= 23.
    else if (!word.compare(0, 4, "grey", 4)
          || !word.compare(0, 4, "gray", 4))
    {
      index = strtol(word.substr(4).c_str(), nullptr, 10);
      if (index < 0 || index > 23)
        throw fmt::format("The color '{}' is not recognized.", word);
      *valptr = fmt::gray_color{index};
    }

    // rgbRGB, where 0 <= R,G,B <= 5.
    else if (!word.compare(0, 3, "rgb", 3)) {
      index = strtol(word.substr(3).c_str(), nullptr, 10);
      if (word.length() != 6 || index < 0 || index > 555)
        throw fmt::format("The color '{}' is not recognized.", word);

      int r = strtol(word.substr(3, 1).c_str(), nullptr, 10);
      int g = strtol(word.substr(4, 1).c_str(), nullptr, 10);
      int b = strtol(word.substr(5, 1).c_str(), nullptr, 10);
      if (   r < 0 || r > 5
          || g < 0 || g > 5
          || b < 0 || b > 5)
        throw fmt::format("The color '{}' is not recognized.", word);
      *valptr = fmt::cube_color{r, g, b};
    }

    // colorN, where 0 <= N <= 255.
    else if (!word.compare(0, 5, "color", 5)) {
      index = strtol(word.substr(5).c_str(), nullptr, 10);
      if (index < 0 || index > 255)
        throw fmt::format("The color '{}' is not recognized.", word);
      *valptr = indexed_color(index);
    }

    // 0xX where 0 <= X <= 0xFFFFFF.
    else if (!word.compare(0, 2, "0x", 2)) {
      auto color = strtoul(word.c_str (), nullptr, 16);
      if (color > 0x00FFFFFF)
        throw fmt::format("The color '{}' is not recognized.", word);
      *valptr = fmt::rgb{static_cast<fmt::rgb::value_type>(color)};
    }
    else if (!word.empty())
      throw fmt::format("The color '{}' is not recognized.", word);
  }

  if (bright) *valptr = to_bright(*valptr);

  return fmt::text_style{fg_value, bg_value, emph};

} // Parse

auto Spec(const fmt::text_style& ts) -> std::string {
  if (ts.empty()) return "";

  auto desc = std::string{};
  desc.reserve(64);

  if (ts.is_strike())    desc += " strike";
  if (ts.is_underline()) desc += " underline";
  if (ts.is_faint())     desc += " faint";
  if (ts.is_bold())      desc += " bold";
  if (ts.is_italic())    desc += " italic";
  if (ts.is_reverse())   desc += " inverse";

  if (ts.has_fg()) {
    desc += ' ';
    desc += to_string(ts.fg());
  }

  if (ts.has_bg()) {
    desc += " on ";
    desc += to_string(ts.bg());
  }

  if (!desc.empty()) desc.erase(0, 1);
  return desc;
} // Spec

// Remove color codes from a string.
std::string strip_sgr(std::string_view input) {
  bool inside = false;
  auto output = std::string{};
  output.reserve(input.size());
  for (auto c: input) {
    if (inside) inside = (c != 'm');
    else if (c == '\x1b') inside = true;
    else output += c;
  }
  return output;
} // strip_sgr

} // Style
