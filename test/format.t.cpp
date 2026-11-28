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

#include <clipped.h>
#include <format.h>
#include <test.h>

////////////////////////////////////////////////////////////////////////////////
int main (int, char**)
{
  UnitTest t (96);

  using tjg::clipped;
  t.is(std::format("{:4.2f}", clipped(9.999)), "10.0",
       "clipped retries when rounding carries into the integer part");
  t.is(std::format("{:4.2e}", clipped(125.0)), "1e+02",
       "clipped keeps the exponent when the minimum precision cannot fit");
  t.is(std::format("{:2.5g}", clipped(125.0)), "1e+02",
       "clipped general presentation keeps one significant digit on overflow");
  t.is(std::format("{:{}.2f}", clipped(9.999), 0), "10",
       "clipped returns the minimum-precision value at width zero");
  t.is(std::format("{:5.6g}", clipped(1234567.0)), "1e+06",
       "clipped general presentation can switch to scientific notation");
  t.is(std::format("{:{}.{}f}/{}", clipped(9.999), 4, 2, "end"), "10.0/end",
       "clipped resolves dynamic fields without consuming the next argument");
  t.is(std::format("{:界>7.2f}", clipped(1.5)), "界 1.50",
       "clipped counts wide fill characters as two display columns");

  // std::string formatHex (int);
  t.is (formatHex (0),   "0",  "formatHex (0) -> 0");
  t.is (formatHex (10),  "a",  "formatHex (10) -> a");
  t.is (formatHex (123), "7b", "formatHex (123) -> 7b");

  t.is(std::format("{:8.4g}", clipped(0.12345678)), "  0.1235",
       "clipped displays four significant digits below one");
  t.is(std::format("{:8.1g}", clipped(1.23456789)), "       1",
       "clipped displays one significant digit");
  t.is(std::format("{:8.2g}", clipped(1.23456789)), "     1.2",
       "clipped displays two significant digits");
  t.is(std::format("{:8.3g}", clipped(1.23456789)), "    1.23",
       "clipped displays three significant digits");
  t.is(std::format("{:8.4g}", clipped(1.23456789)), "   1.235",
       "clipped displays four significant digits");
  t.is(std::format("{:8.5g}", clipped(1.23456789)), "  1.2346",
       "clipped displays five significant digits");
  t.is(std::format("{:8.6g}", clipped(1.23456789)), " 1.23457",
       "clipped displays six significant digits");
  t.is(std::format("{:8.7g}", clipped(1.23456789)), "1.234568",
       "clipped displays seven significant digits");
  t.is(std::format("{:8.8g}", clipped(1.23456789)), "1.234568",
       "clipped reduces precision when eight significant digits overflow");
  t.is(std::format("{:12.11g}", clipped(2444238.56789)), "2444238.5679",
       "clipped displays eleven significant digits when they fit");

  // std::string leftJustify (const std::string&, const int);
  t.is (leftJustify (123, 3), "123",   "leftJustify 123,3 -> '123'");
  t.is (leftJustify (123, 4), "123 ",  "leftJustify 123,4 -> '123 '");
  t.is (leftJustify (123, 5), "123  ", "leftJustify 123,5 -> '123  '");

  // std::string leftJustify (const std::string&, const int);
  t.is (leftJustify ("foo", 3), "foo",   "leftJustify foo,3 -> 'foo'");
  t.is (leftJustify ("foo", 4), "foo ",  "leftJustify foo,4 -> 'foo '");
  t.is (leftJustify ("foo", 5), "foo  ", "leftJustify foo,5 -> 'foo  '");
  t.is (leftJustify ("föo", 5), "föo  ", "leftJustify föo,5 -> 'föo  '");

  // std::string rightJustify (const std::string&, const int);
  t.is (rightJustify (123, 3), "123",   "rightJustify 123,3 -> '123'");
  t.is (rightJustify (123, 4), " 123",  "rightJustify 123,4 -> ' 123'");
  t.is (rightJustify (123, 5), "  123", "rightJustify 123,5 -> '  123'");

  // std::string rightJustify (const std::string&, const int);
  t.is (rightJustify ("foo", 3), "foo",   "rightJustify foo,3 -> 'foo'");
  t.is (rightJustify ("foo", 4), " foo",  "rightJustify foo,4 -> ' foo'");
  t.is (rightJustify ("foo", 5), "  foo", "rightJustify foo,5 -> '  foo'");
  t.is (rightJustify ("föo", 5), "  föo", "rightJustify föo,5 -> '  föo'");

  t.is (rightJustifyZero (123, 2), "123",   "rightJustifyZero preserves values wider than the field");
  t.is (rightJustifyZero (123, 3), "123",   "rightJustifyZero preserves values that fit exactly");
  t.is (rightJustifyZero (123, 5), "00123", "rightJustifyZero pads with leading zeroes");
  t.is (rightJustifyZero (0, 3),   "000",   "rightJustifyZero pads zero");
  t.is (rightJustifyZero (-12, 4), "0-12",  "rightJustifyZero pads before the minus sign");

  // std::string commify (const std::string& data)
  t.is (commify (""),           "",              "commify '' -> ''");
  t.is (commify ("1"),          "1",             "commify '1' -> '1'");
  t.is (commify ("12"),         "12",            "commify '12' -> '12'");
  t.is (commify ("123"),        "123",           "commify '123' -> '123'");
  t.is (commify ("1234"),       "1,234",         "commify '1234' -> '1,234'");
  t.is (commify ("12345"),      "12,345",        "commify '12345' -> '12,345'");
  t.is (commify ("123456"),     "123,456",       "commify '123456' -> '123,456'");
  t.is (commify ("1234567"),    "1,234,567",     "commify '1234567' -> '1,234,567'");
  t.is (commify ("12345678"),   "12,345,678",    "commify '12345678' -> '12,345,678'");
  t.is (commify ("123456789"),  "123,456,789",   "commify '123456789' -> '123,456,789'");
  t.is (commify ("1234567890"), "1,234,567,890", "commify '1234567890' -> '1,234,567,890'");

  t.is (commify ("pre"),         "pre",          "commify 'pre' -> 'pre'");
  t.is (commify ("pre1234"),     "pre1,234",     "commify 'pre1234' -> 'pre1,234'");
  t.is (commify ("1234post"),    "1,234post",    "commify '1234post' -> '1,234post'");
  t.is (commify ("pre1234post"), "pre1,234post", "commify 'pre1234post' -> 'pre1,234post'");

  // std::string formatBytes (size_t);
  t.is (formatBytes (0), "0 B", "0 -> 0 B");

  t.is (formatBytes (994),  "994 B",   "994 -> 994 B");
  t.is (formatBytes (995),  "1.0 KiB", "995 -> 1.0 KiB");
  t.is (formatBytes (999),  "1.0 KiB", "999 -> 1.0 KiB");
  t.is (formatBytes (1000), "1.0 KiB", "1000 -> 1.0 KiB");
  t.is (formatBytes (1001), "1.0 KiB", "1001 -> 1.0 KiB");

  t.is (formatBytes (999999),  "1.0 MiB", "999999 -> 1.0 MiB");
  t.is (formatBytes (1000000), "1.0 MiB", "1000000 -> 1.0 MiB");
  t.is (formatBytes (1000001), "1.0 MiB", "1000001 -> 1.0 MiB");

  t.is (formatBytes (999999999),  "1.0 GiB", "999999999 -> 1.0 GiB");
  t.is (formatBytes (1000000000), "1.0 GiB", "1000000000 -> 1.0 GiB");
  t.is (formatBytes (1000000001), "1.0 GiB", "1000000001 -> 1.0 GiB");

  t.is (formatTime (-1),          "-",     "formatTime renders negative durations as a dash");
  t.is (formatTime (0),           "-",     "formatTime renders zero as a dash");
  t.is (formatTime (1),           "1 s",   "formatTime renders one second");
  t.is (formatTime (59),          "59 s",  "formatTime uses seconds below a minute");
  t.is (formatTime (60),          "1 m",   "formatTime switches to minutes");
  t.is (formatTime (3600),        "1 h",   "formatTime switches to hours");
  t.is (formatTime (86400),       "1 d",   "formatTime switches to days");
  t.is (formatTime (13 * 86400),  "1 wk",  "formatTime switches to weeks at 13 days");
  t.is (formatTime (84 * 86400),  "2 mo",  "formatTime switches to months at 84 days");
  t.is (formatTime (365 * 86400), "1.0 y", "formatTime switches to years at 365 days");

  // std::string printable (const std::string&);
  t.is (printable ("f\ro\no\tb\va\vr"), "f\\ro\\no\\tb\\va\\vr", "printable f\\ro\\no\\tb\\va\\vr --> f\\\\ro\\\\no\\\\tb\\\\va\\\\vr");
  t.is (printable ("foobar"),           "foobar",                "printable foobar --> foobar");

  // std::string printable (char);
  t.is (printable ('\r'), "\\r", "printable \\r --> \\\\r");
  t.is (printable ('\n'), "\\n", "printable \\n --> \\\\n");
  t.is (printable ('\f'), "\\f", "printable \\f --> \\\\f");
  t.is (printable ('\t'), "\\t", "printable \\t --> \\\\t");
  t.is (printable ('\v'), "\\v", "printable \\v --> \\\\v");
  t.is (printable ('x'), "x",    "printable x --> x");

  // std::string obfuscateText (const std::string&);
  t.is (obfuscateText ("foo"),                      "xxx",                      "obfuscateText 'foo' --> 'xxx'");
  t.is (obfuscateText ("[33mfoo[0m"),         "[33mxxx[0m",         "obfuscateText '<red>foo</red>' --> '<red>xxx</red>'");
  t.is (obfuscateText ("one[33mtwo[0mthree"), "xxx[33mxxx[0mxxxxx", "obfuscateText 'one<red>two</red>three' --> 'xxx<red>xxx</red>xxxxx'");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
