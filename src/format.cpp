////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 - 2021, 2023, Gothenburg Bit Factory.
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

#include <algorithm>
#include <cmake.h>
#include <cstring>
#include <format>
#include <format.h>
#include <iomanip>
#include <locale>
#include <sstream>
#ifndef _WIN32
#include <strings.h>
#endif
#include <utf8.h>

////////////////////////////////////////////////////////////////////////////////
std::string leftJustify (const int input, const int width)
{
  std::stringstream s;
  s << input;
  std::string output = s.str ();
  return output + std::string (std::max<int> (width - output.length (), 0), ' ');
}

////////////////////////////////////////////////////////////////////////////////
std::string leftJustify (const std::string& input, const int width)
{
  return input + std::string (std::max<int> (width - utf8_text_width (input), 0), ' ');
}

////////////////////////////////////////////////////////////////////////////////
std::string rightJustifyZero (const int input, const int width)
{
  std::stringstream s;
  s << std::setw (width) << std::setfill ('0') << input;
  return s.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string rightJustify (const int input, const int width)
{
  std::stringstream s;
  s << std::setw (width) << std::setfill (' ') << input;
  return s.str ();
}

////////////////////////////////////////////////////////////////////////////////
std::string rightJustify (const std::string& input, const int width)
{
  return std::string (std::max<int> (width - utf8_text_width (input), 0), ' ') + input;
}

////////////////////////////////////////////////////////////////////////////////
namespace
{
class byte_grouping : public std::numpunct<char>
{
  char do_decimal_point () const override { return '.'; }
  char do_thousands_sep () const override { return ','; }
  std::string do_grouping () const override { return "\3"; }
};
} // namespace

////////////////////////////////////////////////////////////////////////////////
// Convert a quantity in bytes to a more readable format.
std::string formatBytes (size_t bytes)
{
  if (bytes < 995)
    return std::format ("{} B", bytes);

  static const std::locale grouped {std::locale::classic (), new byte_grouping};
  if (bytes >= 995000000)
    return std::format (grouped, "{:.1Lf} GiB", bytes / 1000000000.0);
  if (bytes >= 995000)
    return std::format (grouped, "{:.1Lf} MiB", bytes / 1000000.0);
  return std::format (grouped, "{:.1Lf} KiB", bytes / 1000.0);
}

////////////////////////////////////////////////////////////////////////////////
// Convert a quantity in seconds to a more readable format.
std::string formatTime (time_t seconds)
{
  char formatted[24];
  float days = (float) seconds / 86400.0;

       if (seconds >= 86400 * 365) snprintf (formatted, sizeof(formatted), "%.1f y", (days / 365.0));
  else if (seconds >= 86400 * 84)  snprintf (formatted, sizeof(formatted), "%1d mo", (int) (days / 30));
  else if (seconds >= 86400 * 13)  snprintf (formatted, sizeof(formatted), "%d wk",  (int) (float) (days / 7.0));
  else if (seconds >= 86400)       snprintf (formatted, sizeof(formatted), "%d d",   (int) days);
  else if (seconds >= 3600)        snprintf (formatted, sizeof(formatted), "%d h",   (int) (seconds / 3600));
  else if (seconds >= 60)          snprintf (formatted, sizeof(formatted), "%d m",   (int) (seconds / 60));
  else if (seconds >= 1)           snprintf (formatted, sizeof(formatted), "%d s",   (int) seconds);
#ifdef HAVE_STRLCPY
  else                             strlcpy (formatted, "-", sizeof(formatted));
#else
  else                             strcpy (formatted, "-");
#endif /* HAVE_STRLCPY */

  return std::string (formatted);
}

////////////////////////////////////////////////////////////////////////////////
std::string printable (const std::string& input)
{
  // Sanitize 'message'.
  std::string sanitized = input;
  std::string::size_type bad;
  while ((bad = sanitized.find ('\r')) != std::string::npos)
    sanitized.replace (bad, 1, "\\r");

  while ((bad = sanitized.find ('\n')) != std::string::npos)
    sanitized.replace (bad, 1, "\\n");

  while ((bad = sanitized.find ('\f')) != std::string::npos)
    sanitized.replace (bad, 1, "\\f");

  while ((bad = sanitized.find ('\t')) != std::string::npos)
    sanitized.replace (bad, 1, "\\t");

  while ((bad = sanitized.find ('\v')) != std::string::npos)
    sanitized.replace (bad, 1, "\\v");

  return sanitized;
}

////////////////////////////////////////////////////////////////////////////////
std::string printable (char input)
{
  // Sanitize 'message'.
  char stringized[2] = {0};
  stringized[0] = input;

  std::string sanitized = stringized;
  switch (input)
  {
  case '\r': sanitized = "\\r"; break;
  case '\n': sanitized = "\\n"; break;
  case '\f': sanitized = "\\f"; break;
  case '\t': sanitized = "\\t"; break;
  case '\v': sanitized = "\\v"; break;
  default:   sanitized = input; break;
  }

  return sanitized;
}

////////////////////////////////////////////////////////////////////////////////
// Iterate over the input, converting text to 'x'.
// Does not modify color codes.
std::string obfuscateText (const std::string& input)
{
  std::stringstream output;
  std::string::size_type i = 0;
  int character;
  bool inside = false;

  while ((character = utf8_next_char (input, i)))
  {
    if (inside)
    {
      output << (char) character;

      if (character == 'm')
        inside = false;
    }
    else
    {
      if (character == 033)
        inside = true;

      if (inside || character == ' ')
        output << (char) character;
      else
        output << 'x';
    }
  }

  return output.str ();
}

////////////////////////////////////////////////////////////////////////////////
