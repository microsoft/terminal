//  (C) Copyright John Maddock 2003. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  for C++ Builder the following options effect the ABI:
//
//  -b (on or off - effect emum sizes)
//  -Vx  (on or off - empty members)
//  -Ve (on or off - empty base classes)
//  -aX (alignment - 5 options).
//  -pX (Calling convention - 4 options)
//  -VmX (member pointer size and layout - 5 options)
//  -VC (on or off, changes name mangling)
//  -Vl (on or off, changes struct layout).

//  In addition the following warnings are sufficiently annoying (and
//  unfixable) to have them turned off by default:
//
//  8027 - functions containing [for|while] loops are not expanded inline
//  8026 - functions taking class by value arguments are not expanded inline

#pragma nopushoptwarn
#  pragma option push -a8 -Vx- -Ve- -b- -pc -Vmv -VC- -Vl- -w-8027 -w-8026



