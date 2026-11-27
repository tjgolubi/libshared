////////////////////////////////////////////////////////////////////////////////

#include <Lexer.h>
#include <iostream>
#include <print>

int main (int argc, char** argv)
{
  for (auto i = 1; i < argc; i++)
  {
    std::println (std::cout, "argument '{}'", argv[i]);

    Lexer l (argv[i]);
    std::string token;
    Lexer::Type type;
    while (l.token (token, type))
      std::println (std::cout, "  token '{}' {}", token, Lexer::typeToString (type));
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
