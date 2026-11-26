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

#ifndef INCLUDED_FORMAT
#define INCLUDED_FORMAT

#include <algorithm>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#if __cplusplus >= 202002L || (defined (_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include <version>
#endif

#ifdef __cpp_lib_format
#include <format>
#include <string_view>
#endif

// Opt in to formatting through operator<<. Specialize beside program-defined types.
template<typename T>
struct ostream_formattable : std::false_type {};

template<> struct ostream_formattable<char> : std::true_type {};
template<> struct ostream_formattable<char*> : std::true_type {};
template<> struct ostream_formattable<float> : std::true_type {};
template<> struct ostream_formattable<int> : std::true_type {};
template<> struct ostream_formattable<long> : std::true_type {};
template<> struct ostream_formattable<long long> : std::true_type {};
template<> struct ostream_formattable<unsigned int> : std::true_type {};
template<> struct ostream_formattable<unsigned long> : std::true_type {};
template<> struct ostream_formattable<unsigned long long> : std::true_type {};

#ifdef __cpp_lib_format
template<typename T>
  requires (ostream_formattable<T>::value &&
            (std::is_class_v<T> || std::is_union_v<T> || std::is_enum_v<T>) &&
            requires (std::ostream& out, const T& value) { out << value; })
struct std::formatter<T, char> : std::formatter<std::string_view, char>
{
  auto format (const T& value, auto& context) const
  {
    std::ostringstream out;
    out << value;
    return std::formatter<std::string_view, char>::format (out.view (), context);
  }
};
#endif

std::string format (std::string&);
std::string format (const char*);
std::string formatHex (int);
std::string format (float, int, int);
std::string format (double, int, int);
std::string format (double);

void replace_positional (std::string&, int, const std::string&);

template<typename T>
const std::string format (T value)
{
    if constexpr (ostream_formattable<T>::value)
    {
        std::stringstream s;
        s << value;
        return s.str ();
    }
    else
    {
        static_assert (ostream_formattable<T>::value,
                       "::format<T> requires an ostream_formattable<T> specialization");
        return {};
    }
}

template<typename T>
const std::string format (int fmt_num, const std::string& fmt, T arg)
{
    std::string output = fmt;
    replace_positional (output, fmt_num, format (arg));
    return output;
}

template<typename T, typename... Args>
const std::string format (int fmt_num, const std::string& fmt, T arg, Args... args)
{
    const std::string fmt_replaced (format (fmt_num, fmt, arg));
    return format (fmt_num+1, fmt_replaced, args...);
}

template<typename... Args>
const std::string format0 (const std::string& fmt, Args... args)
{
    return format (0, fmt, args...);
}

std::string leftJustify (const int, const int);
std::string leftJustify (const std::string&, const int);
std::string rightJustifyZero (const int, const int);
std::string rightJustify (const int, const int);
std::string rightJustify (const std::string&, const int);

std::string commify (const std::string&);
std::string formatBytes (size_t);
std::string formatTime (time_t);
std::string printable (const std::string&);
std::string printable (char);

std::string obfuscateText (const std::string&);

#endif
