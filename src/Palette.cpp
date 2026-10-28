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

#include <Palette.h>
#include <Color.h>

////////////////////////////////////////////////////////////////////////////////
// Use a default palette, which is overwritten in ::initialize.
Palette::Palette ()
{
  using term = fmt::term_color;
  _colors = {
    Color{term::white, term::red},
    Color{term::white, term::blue},
    Color{term::black, term::green},
    Color{term::black, term::magenta},
    Color{term::black, term::cyan},
    Color{term::black, term::yellow},
    Color{term::black, term::white},
    Color{term::white, term::bright_red},
    Color{term::white, term::bright_blue},
    Color{term::black, term::bright_green},
    Color{term::black, term::bright_magenta},
    Color{term::black, term::bright_cyan},
    Color{term::black, term::bright_yellow}
  };

  _current = 0;
}

////////////////////////////////////////////////////////////////////////////////
void Palette::add (Color c)
{
  _colors.push_back (c);
}

////////////////////////////////////////////////////////////////////////////////
// Return the next color in the list.  Cycle to the beginning if necessary.
Color Palette::next ()
{
  if (enabled)
    return _colors[_current++ % _colors.size ()];

  return Color ();
}

////////////////////////////////////////////////////////////////////////////////
int Palette::size () const
{
  return static_cast <int> (_colors.size ());
}

////////////////////////////////////////////////////////////////////////////////
void Palette::clear ()
{
  _colors.clear ();
  _current = 0;
}

////////////////////////////////////////////////////////////////////////////////
