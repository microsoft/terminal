//  boost/detail/lightweight_main.hpp  -------------------------------------------------//

//  Copyright Beman Dawes 2010

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#include <iostream>
#include <exception>

//--------------------------------------------------------------------------------------//
//                                                                                      //
//                exception reporting main() that calls cpp_main()                      //
//                                                                                      //
//--------------------------------------------------------------------------------------//

int cpp_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
  try
  {
    return cpp_main(argc, argv);
  }

  catch (const std::exception& ex)
  {
    std::cout
    << "\nERROR  ERROR  ERROR  ERROR  ERROR  ERROR  ERROR  ERROR  ERROR  ERROR  ERROR\n"
    << "\n****************************** std::exception *****************************\n"
    << ex.what()
    << "\n***************************************************************************\n"
    << std::endl;
  }
  return 1;
}
