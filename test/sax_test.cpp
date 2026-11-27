////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017, 2019 - 2021, 2023, Gothenburg Bit Factory.
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

#include <JSON.h>
#include <fstream>
#include <print>
#include <sstream>

class EventSink : public json::SAX::Sink
{
public:
  void eventObjectStart () override                         { std::println ("# object start"); }
  void eventObjectEnd (int count) override                  { std::println ("# object end ({} items)", count); }
  void eventArrayStart () override                          { std::println ("# array start"); }
  void eventArrayEnd (int count) override                   { std::println ("# array end ({} items)", count); }
  void eventName (const std::string& value) override        { std::println ("# name '{}'", value); }
  void eventValueNull () override                           { std::println ("# value 'null'"); }
  void eventValueBool (bool value) override                 { std::println ("# value '{}'", (value ? "true" : "false")); }
  void eventValueInt (int64_t value) override               { std::println ("# value '{}'", value); }
  void eventValueUint (uint64_t value) override             { std::println ("# value '{}'", value); }
  void eventValueDouble (double value) override             { std::println ("# value '{:.6g}'", value); }
  void eventValueString (const std::string& value) override { std::println ("# value '{}'", value); }
};

////////////////////////////////////////////////////////////////////////////////
int main (int argc, char** argv)
{
  int status = 0;
  if (argc == 1)
  {
    std::println ("\nUsage: json2_test <file> ...\n\n      <file>    file containing JSON\n");
  }
  else
  {
    for (int i = 1; i < argc; ++i)
    {
      try
      {
        std::ifstream inputFile (argv[i]);
        std::stringstream input;
        input << inputFile.rdbuf ();

        EventSink sink;
        json::SAX sax;
        if (! sax.parse (input.str (), sink))
          status = 1;
      }

      catch (const std::string& e) { std::println ("{}", e);         }
      catch (...)                  { std::println ("Unknown error"); }
    }
  }

  return status;
}

////////////////////////////////////////////////////////////////////////////////
