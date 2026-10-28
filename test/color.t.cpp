////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 - 2017, 2019 - 2021, 2023, Gothenburg Bit Factory.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://opensource.org/license/mit
//
////////////////////////////////////////////////////////////////////////////////

#include <Color.h>
#include <cstdio>
#include <test.h>

////////////////////////////////////////////////////////////////////////////////
int main (int, char**)
{
  UnitTest t (40 + 256 + 256 + 6*6*6 + 6*6*6 + 1 + 24 + 24 + 7 + 6 + 12 + 13*3);

  // Names matched to values.
  t.ok (Style::Parse("")        == Color{}, "''        == empty style");
  t.ok (Style::Parse("black")   == Color{fmt::term_color::black}, "'black'   == fmt::term_color::black");
  t.ok (Style::Parse("red")     == Color{fmt::term_color::red}, "'red'     == fmt::term_color::red");
  t.ok (Style::Parse("green")   == Color{fmt::term_color::green}, "'green'   == fmt::term_color::green");
  t.ok (Style::Parse("yellow")  == Color{fmt::term_color::yellow}, "'yellow'  == fmt::term_color::yellow");
  t.ok (Style::Parse("blue")    == Color{fmt::term_color::blue}, "'blue'    == fmt::term_color::blue");
  t.ok (Style::Parse("magenta") == Color{fmt::term_color::magenta}, "'magenta' == fmt::term_color::magenta");
  t.ok (Style::Parse("cyan")    == Color{fmt::term_color::cyan}, "'cyan'    == fmt::term_color::cyan");
  t.ok (Style::Parse("white")   == Color{fmt::term_color::white}, "'white'   == fmt::term_color::white");

  // Canonical names for terminal palette entries.
  Color c = Style::Parse("red on color0");
  t.is (Style::Spec(c), "red on black", "red on color0 -> red on black");

  c = Style::Parse("color1 on black");
  t.is (Style::Spec(c), "red on black", "color1 on black -> red on black");

  c = Style::Parse("bold red on color0");
  t.is (Style::Spec(c), "bold red on black", "bold red on color0 -> bold red on black");

  c = Style::Parse("color1 on bright black");
  t.is (Style::Spec(c), "red on bright black", "color1 on bright black -> red on bright black");

  // Simple blending.
  c = Style::Parse("red");
  c.overlay (Style::Parse("on white"));
  t.is (Style::Spec(c), "red on white", "red + on white -> red on white");

  c = Style::Parse("bold underline red");
  c.overlay (Style::Parse("on bright white"));
  t.is (Style::Spec(c), "underline bold red on bright white", "bold underline red + on bright white -> underline bold red on bright white");

  // Blending with conflicts.
  c = Style::Parse("red on white");
  c.overlay (Style::Parse("on blue"));
  t.is (Style::Spec(c), "red on blue", "red on white + on blue -> red on blue");

  c = Style::Parse("red on white");
  c.overlay (Style::Parse("blue on magenta"));
  t.is (Style::Spec(c), "blue on magenta", "red on white + blue on magenta -> blue on magenta");

  // Blending preserves independent channel representations.
  c = Style::Parse("red on black");
  c.overlay (Style::Parse("blue"));
  t.is (Style::Spec(c), "blue on black", "color1 on color0 + blue -> blue on black");

  // Now the dumb show of every color and its code.
  t.is (Style::colorize ("foo", "red"),                std::string ("\033[31mfoo\033[0m"),       "red                -> ^[[31m");
  t.is (Style::colorize ("foo", "bold red"),           std::string ("\033[1;31mfoo\033[0m"),     "bold red           -> ^[[1;31m");
  t.is (Style::colorize ("foo", "underline red"),      std::string ("\033[4;31mfoo\033[0m"),     "underline red      -> ^[[4;31m");
  t.is (Style::colorize ("foo", "underline bold red"), std::string ("\033[1;4;31mfoo\033[0m"),   "underline bold red -> ^[[1;4;31m");

  // 16-color foregrounds.
  t.is (Style::colorize ("foo", ""),                   std::string ("foo"),                      "''                 -> ''");

  t.is (Style::colorize ("foo", "black"),              std::string ("\033[30mfoo\033[0m"),       "black              -> ^[[30m");
  t.is (Style::colorize ("foo", "red"),                std::string ("\033[31mfoo\033[0m"),       "red                -> ^[[31m");
  t.is (Style::colorize ("foo", "green"),              std::string ("\033[32mfoo\033[0m"),       "green              -> ^[[32m");
  t.is (Style::colorize ("foo", "yellow"),             std::string ("\033[33mfoo\033[0m"),       "yellow             -> ^[[33m");
  t.is (Style::colorize ("foo", "blue"),               std::string ("\033[34mfoo\033[0m"),       "blue               -> ^[[34m");
  t.is (Style::colorize ("foo", "magenta"),            std::string ("\033[35mfoo\033[0m"),       "magenta            -> ^[[35m");
  t.is (Style::colorize ("foo", "cyan"),               std::string ("\033[36mfoo\033[0m"),       "cyan               -> ^[[36m");
  t.is (Style::colorize ("foo", "white"),              std::string ("\033[37mfoo\033[0m"),       "white              -> ^[[37m");

  // 16-color backgrounds.
  t.is (Style::colorize ("foo", "on bright black"),    std::string ("\033[100mfoo\033[0m"),      "on bright black    -> ^[[100m");

  t.is (Style::colorize ("foo", "on black"),           std::string ("\033[40mfoo\033[0m"),       "on black           -> ^[[40m");
  t.is (Style::colorize ("foo", "on red"),             std::string ("\033[41mfoo\033[0m"),       "on red             -> ^[[41m");
  t.is (Style::colorize ("foo", "on green"),           std::string ("\033[42mfoo\033[0m"),       "on green           -> ^[[42m");
  t.is (Style::colorize ("foo", "on yellow"),          std::string ("\033[43mfoo\033[0m"),       "on yellow          -> ^[[43m");
  t.is (Style::colorize ("foo", "on blue"),            std::string ("\033[44mfoo\033[0m"),       "on blue            -> ^[[44m");
  t.is (Style::colorize ("foo", "on magenta"),         std::string ("\033[45mfoo\033[0m"),       "on magenta         -> ^[[45m");
  t.is (Style::colorize ("foo", "on cyan"),            std::string ("\033[46mfoo\033[0m"),       "on cyan            -> ^[[46m");
  t.is (Style::colorize ("foo", "on white"),           std::string ("\033[47mfoo\033[0m"),       "on white           -> ^[[47m");

  // 256-color, basic colors.
  char color [24];
  char codes [64];
  char description [64];
  for (int i = 0; i < 256; ++i)
  {
    snprintf (color, 24, "color%d", i);
    if (i < 16)
      snprintf (codes, 64, "\033[%dmfoo\033[0m", i < 8 ? 30 + i : 90 + i - 8);
    else
      snprintf (codes, 64, "\033[38;5;%dmfoo\033[0m", i);
    snprintf (description, 64, "color%d selects foreground palette entry %d", i, i);

    t.is (Style::colorize ("foo", color), std::string (codes), description);
  }

  for (int i = 0; i < 256; ++i)
  {
    snprintf (color, 24, "on color%d", i);
    if (i < 16)
      snprintf (codes, 64, "\033[%dmfoo\033[0m", i < 8 ? 40 + i : 100 + i - 8);
    else
      snprintf (codes, 64, "\033[48;5;%dmfoo\033[0m", i);
    snprintf (description, 64, "on color%d selects background palette entry %d", i, i);

    t.is (Style::colorize ("foo", color), std::string (codes), description);
  }

  // RGB Color Cube.
  for (int r = 0; r < 6; ++r)
    for (int g = 0; g < 6; ++g)
      for (int b = 0; b < 6; ++b)
      {
        int code = 16 + (r*36 + g*6 + b);
        snprintf (color, 24, "rgb%d%d%d", r, g, b);
        snprintf (codes, 64, "\033[38;5;%dmfoo\033[0m", code);
        snprintf (description, 64, "\033[38;5;%dmfoo\033[0m: rgb%d%d%d -> ^[[38;5;%dm", code, r, g, b, code);

        t.is (Style::colorize ("foo", color), std::string (codes), description);
      }

  for (int r = 0; r < 6; ++r)
    for (int g = 0; g < 6; ++g)
      for (int b = 0; b < 6; ++b)
      {
        int code = 16 + (r*36 + g*6 + b);
        snprintf (color, 24, "on rgb%d%d%d", r, g, b);
        snprintf (codes, 64, "\033[48;5;%dmfoo\033[0m", code);
        snprintf (description, 64, "\033[48;5;%dmfoo\033[0m: on rgb%d%d%d -> ^[[48;5;%dm", code, r, g, b, code);

        t.is (Style::colorize ("foo", color), std::string (codes), description);
      }

  // 256-color, grays.
  // grey == gray.
  t.is (Style::colorize ("foo", "grey0"), std::string ("\033[38;5;232mfoo\033[0m"), "\033[38;5;232mfoo\033[0m: grey0 -> ^[[38;5;232m");

  for (int i = 0; i < 24; ++i)
  {
    snprintf (color, 24, "gray%d", i);
    snprintf (codes, 64, "\033[38;5;%dmfoo\033[0m", i + 232);
    snprintf (description, 64, "\033[38;5;%dmfoo\033[0m: gray%d -> ^[[38;5;%dm", i + 232, i, i + 232);

    t.is (Style::colorize ("foo", color), std::string (codes), description);
  }

  for (int i = 0; i < 24; ++i)
  {
    snprintf (color, 24, "on gray%d", i);
    snprintf (codes, 64, "\033[48;5;%dmfoo\033[0m", i + 232);
    snprintf (description, 64, "\033[48;5;%dmfoo\033[0m: on gray%d -> ^[[48;5;%dm", i + 232, i, i + 232);

    t.is (Style::colorize ("foo", color), std::string (codes), description);
  }

  // 24-bit color
  t.is (Style::colorize ("foo", "0x0a1b2c"),               std::string ("\033[38;2;10;27;44mfoo\033[0m"),                  std::string("\033[38;2;10;27;44mfoo\033[0m: ")                  + "0x0a1b2c               -> ^[[38;2;10;27;44m");
  t.is (Style::colorize ("foo", "0x0a1b2c on color127"), std::string ("\033[38;2;10;27;44;48;5;127mfoo\033[0m"), "0x0a1b2c on color127 preserves channel representations");
  t.is (Style::colorize ("foo", "red on 0x0a1b2c"), std::string ("\033[31;48;2;10;27;44mfoo\033[0m"), "red on 0x0a1b2c preserves channel representations");
  t.is (Style::colorize ("foo", "bold red on 0x0a1b2c"), std::string ("\033[1;31;48;2;10;27;44mfoo\033[0m"), "bold red on 0x0a1b2c preserves channel representations");
  t.is (Style::colorize ("foo", "0x0a1b2c on bright red"), std::string ("\033[38;2;10;27;44;101mfoo\033[0m"), "0x0a1b2c on bright red preserves channel representations");
  t.is (Style::colorize ("foo", "0x010101 on grey0"), std::string ("\033[38;2;1;1;1;48;5;232mfoo\033[0m"), "0x010101 on grey0 preserves channel representations");
  t.is (Style::colorize ("foo", "0x101010 on grey23"), std::string ("\033[38;2;16;16;16;48;5;255mfoo\033[0m"), "0x101010 on grey23 preserves channel representations");

  // std::string Style::strip_sgr (const std::string&);
  t.is (Style::strip_sgr (""),                  "",    "Style::strip_sgr '' -> ''");
  t.is (Style::strip_sgr ("foo"),               "foo", "Style::strip_sgr 'foo' -> 'foo'");
  t.is (Style::strip_sgr ("f\033[1mo\033[0mo"), "foo", "Style::strip_sgr 'f<b>o</b>o' -> 'foo'");

  // Native SGR generation.
  t.is (fmt::to_sgr(Style::Parse("rgb012 on bright yellow")), "\033[38;5;24;103m", "fmt::to_sgr combines indexed foreground and bright terminal background");
  t.is (fmt::to_sgr(Style::Parse("0x0F1E2D")), "[38;2;15;30;45m", "fmt::to_sgr '0x0F1E2D' --> ^[[38;2;15;30;45m");

  // Native terminal reset.
  t.is (std::string (fmt::sgr_reset().data(), fmt::sgr_reset().size()), "[0m", "fmt::sgr_reset --> ^[[0m");

  // Specification round trips preserve independent brightness and color kinds.
  for (const auto* spec : {"bright red on cyan", "red on bright cyan",
                           "bright red on bright cyan", "0x010203 on gray4"})
  {
    const auto style = Style::Parse(spec);
    const auto canonical = Style::Spec(style);
    t.ok (Style::Parse(canonical) == style, "style specification round trip");
    t.ok (Style::strip_sgr(fmt::wrap_sgr(style, "sample")) == "sample",
          "styled text retains its visible content");
  }

  auto style = Style::Parse("bold bright red on bright blue");
  style.overlay(Style::Parse("green"));
  t.is (Style::Spec(style), "bold green on bright blue",
        "foreground overlay replaces brightness and preserves background");
  style.overlay(Style::Parse("on cyan"));
  t.is (Style::Spec(style), "bold green on cyan",
        "background overlay replaces brightness and preserves foreground");
  const auto unchanged = style;
  style.overlay(Color{});
  t.ok (style == unchanged, "empty overlay leaves style unchanged");
  t.is (fmt::to_sgr(Color{}), "", "empty style emits no opening SGR");

  // Emphasis keywords, aliases, and canonical specification order.
  using Term = fmt::term_color;
  using Emphasis = fmt::emphasis;
  const auto all_emphasis = Emphasis::strike | Emphasis::underline | Emphasis::faint |
                            Emphasis::bold   | Emphasis::italic    | Emphasis::reverse;
  const struct
  {
    const char* input;
    Color expected;
    const char* canonical;
  } emphasis_cases[] = {
    {"strike", fmt::strike(), "strike"},
    {"STRIKE UNDERLINE",
     fmt::strike().underline(), "strike underline"},
    {"strike strike", fmt::strike(), "strike"},
    {"red on strike color173",
     Color{Term::red, fmt::cube_color{157}, Emphasis::strike},
     "strike red on color173"},
    {"italic",  fmt::italic(),  "italic"},
    {"faint",   fmt::faint(),   "faint"},
    {"dim",     fmt::faint(),   "faint"},
    {"reverse", fmt::reverse(), "inverse"},
    {"inverse", fmt::reverse(), "inverse"},
    {"ITALIC DIM REVERSE",
     fmt::italic().faint().reverse(),
     "faint italic inverse"},
    {"reverse underline italic bold strike dim bright red on bright blue",
     Color{Term::bright_red, Term::bright_blue, all_emphasis},
     "strike underline faint bold italic inverse bright red on bright blue"},
    {"red on italic faint reverse color173",
     fmt::italic().faint().reverse().fg(Term::red).bg(fmt::cube_color{157}),
     "faint italic inverse red on color173"},
    {"italic italic dim faint reverse inverse",
     Color{Emphasis::italic | Emphasis::faint | Emphasis::reverse},
     "faint italic inverse"},
  };
  for (const auto& test_case : emphasis_cases)
  {
    const auto description = std::string(test_case.input);
    t.ok (Style::Parse(test_case.input) == test_case.expected,
          "Parse emphasis: " + description);
    t.is (Style::Spec(test_case.expected), std::string(test_case.canonical),
          "Spec emphasis: " + description);
    t.ok (Style::Parse(Style::Spec(test_case.expected)) == test_case.expected,
          "emphasis specification round trip: " + description);
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
