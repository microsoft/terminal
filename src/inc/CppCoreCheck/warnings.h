//
// This is a GENERATED file. It should *not* be edited directly.
// Changes should be made to the defectdefs.xml file
// This file contains symbolic constants for warning numbers.
//

// clang-format off


#pragma once
enum ECppCoreCheckWarningCodes
{
    WARNING_NO_RAW_POINTER_ASSIGNMENT = 26400,                          // Do not assign the result of an allocation or a function call with an owner<T> return value to a raw pointer, use owner<T> instead (i.11: http://go.microsoft.com/fwlink/?linkid=845474).
    WARNING_DONT_DELETE_NON_OWNER = 26401,                              // Do not delete a raw pointer that is not an owner<T> (i.11: http://go.microsoft.com/fwlink/?linkid=845474).
    WARNING_DONT_HEAP_ALLOCATE_MOVABLE_RESULT = 26402,                  // Return a scoped object instead of a heap-allocated if it has a move constructor (r.3: http://go.microsoft.com/fwlink/?linkid=845471).
    WARNING_RESET_OR_DELETE_OWNER = 26403,                              // Reset or explicitly delete an owner<T> pointer '%1$s' (r.3: http://go.microsoft.com/fwlink/?linkid=845471).
    WARNING_DONT_DELETE_INVALID = 26404,                                // Do not delete an owner<T> which may be in invalid state (r.3: http://go.microsoft.com/fwlink/?linkid=845471).
    WARNING_DONT_ASSIGN_TO_VALID = 26405,                               // Do not assign to an owner<T> which may be in valid state (r.3: http://go.microsoft.com/fwlink/?linkid=845471).
    WARNING_DONT_ASSIGN_RAW_TO_OWNER = 26406,                           // Do not assign a raw pointer to an owner<T> (r.3: http://go.microsoft.com/fwlink/?linkid=845471).
    WARNING_DONT_HEAP_ALLOCATE_UNNECESSARILY = 26407,                   // Prefer scoped objects, don't heap-allocate unnecessarily (r.5: http://go.microsoft.com/fwlink/?linkid=845473).
    WARNING_NO_MALLOC_FREE = 26408,                                     // Avoid malloc() and free(), prefer the nothrow version of new with delete (r.10: http://go.microsoft.com/fwlink/?linkid=845483).
    WARNING_NO_NEW_DELETE = 26409,                                      // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11: http://go.microsoft.com/fwlink/?linkid=845485).
    WARNING_NO_REF_TO_CONST_UNIQUE_PTR = 26410,                         // The parameter '%1$s' is a reference to const unique pointer, use const T* or const T& instead (r.32: http://go.microsoft.com/fwlink/?linkid=845478).
    WARNING_NO_REF_TO_UNIQUE_PTR = 26411,                               // The parameter '%1$s' is a reference to unique pointer and it is never reassigned or reset, use T* or T& instead (r.33: http://go.microsoft.com/fwlink/?linkid=845479).
    WARNING_RESET_LOCAL_SMART_PTR = 26414,                              // Move, copy, reassign or reset a local smart pointer '%1$s' (r.5: http://go.microsoft.com/fwlink/?linkid=845473).
    WARNING_SMART_PTR_NOT_NEEDED = 26415,                               // Smart pointer parameter '%1$s' is used only to access contained pointer. Use T* or T& instead (r.30: http://go.microsoft.com/fwlink/?linkid=845475).
    WARNING_NO_RVALUE_REF_SHARED_PTR = 26416,                           // Shared pointer parameter '%1$s' is passed by rvalue reference. Pass by value instead (r.34: http://go.microsoft.com/fwlink/?linkid=845486).
    WARNING_NO_LVALUE_REF_SHARED_PTR = 26417,                           // Shared pointer parameter '%1$s' is passed by reference and not reset or reassigned. Use T* or T& instead (r.35: http://go.microsoft.com/fwlink/?linkid=845488).
    WARNING_NO_VALUE_OR_CONST_REF_SHARED_PTR = 26418,                   // Shared pointer parameter '%1$s' is not copied or moved. Use T* or T& instead (r.36: http://go.microsoft.com/fwlink/?linkid=845489).
    WARNING_NO_GLOBAL_INIT_CALLS = 26426,                               // Global initializer calls a non-constexpr function '%1$s' (i.22: http://go.microsoft.com/fwlink/?linkid=853919).
    WARNING_NO_GLOBAL_INIT_EXTERNS = 26427,                             // Global initializer accesses extern object '%1$s' (i.22: http://go.microsoft.com/fwlink/?linkid=853919).
    WARNING_USE_NOTNULL = 26429,                                        // Symbol '%1$s' is never tested for nullness, it can be marked as not_null (f.23: http://go.microsoft.com/fwlink/?linkid=853921).
    WARNING_TEST_ON_ALL_PATHS = 26430,                                  // Symbol '%1$s' is not tested for nullness on all paths (f.23: http://go.microsoft.com/fwlink/?linkid=853921).
    WARNING_DONT_TEST_NOTNULL = 26431,                                  // The type of expression '%1$s' is already gsl::not_null. Do not test it for nullness (f.23: http://go.microsoft.com/fwlink/?linkid=853921).
    WARNING_DEFINE_OR_DELETE_SPECIAL_OPS = 26432,                       // If you define or delete any default operation in the type '%1$s', define or delete them all (c.21: http://go.microsoft.com/fwlink/?linkid=853922).
    WARNING_OVERRIDE_EXPLICITLY = 26433,                                // Function '%1$s' should be marked with 'override' (c.128: http://go.microsoft.com/fwlink/?linkid=853923).
    WARNING_DONT_HIDE_METHODS = 26434,                                  // Function '%1$s' hides a non-virtual function '%2$s' (c.128: http://go.microsoft.com/fwlink/?linkid=853923).
    WARNING_SINGLE_VIRTUAL_SPECIFICATION = 26435,                       // Function '%1$s' should specify exactly one of 'virtual', 'override', or 'final' (c.128: http://go.microsoft.com/fwlink/?linkid=853923).
    WARNING_NEED_VIRTUAL_DTOR = 26436,                                  // The type '%1$s' with a virtual function needs either public virtual or protected non-virtual destructor (c.35: http://go.microsoft.com/fwlink/?linkid=853924).
    WARNING_DONT_SLICE = 26437,                                         // Do not slice (es.63: http://go.microsoft.com/fwlink/?linkid=853925).
    WARNING_NO_GOTO = 26438,                                            // Avoid 'goto' (es.76: http://go.microsoft.com/fwlink/?linkid=853926).
    WARNING_SPECIAL_NOEXCEPT = 26439,                                   // This kind of function may not throw. Declare it 'noexcept' (f.6: http://go.microsoft.com/fwlink/?linkid=853927).
    WARNING_DECLARE_NOEXCEPT = 26440,                                   // Function '%1$s' can be declared 'noexcept' (f.6: http://go.microsoft.com/fwlink/?linkid=853927).
    WARNING_NO_UNNAMED_GUARDS = 26441,                                  // Guard objects must be named (cp.44: http://go.microsoft.com/fwlink/?linkid=853928).
    WARNING_NO_EXPLICIT_DTOR_OVERRIDE = 26443,                          // Overriding destructor should not use explicit 'override' or 'virtual' specifiers (c.128: http://go.microsoft.com/fwlink/?linkid=853923).
    WARNING_NO_UNNAMED_RAII_OBJECTS = 26444,                            // Avoid unnamed objects with custom construction and destruction (es.84: http://go.microsoft.com/fwlink/?linkid=862923).
    WARNING_RESULT_OF_ARITHMETIC_OPERATION_PROVABLY_LOSSY = 26450,      // Arithmetic overflow: '%1$s' operation causes overflow at compile time. Use a wider type to store the operands (io.1: https://go.microsoft.com/fwlink/?linkid=864597).
    WARNING_RESULT_OF_ARITHMETIC_OPERATION_CAST_TO_LARGER_SIZE = 26451, // Arithmetic overflow: Using operator '%1$s' on a %2$d byte value and then casting the result to a %3$d byte value. Cast the value to the wider type before calling operator '%1$s' to avoid overflow (io.2: https://go.microsoft.com/fwlink/?linkid=864598).
    WARNING_SHIFT_COUNT_NEGATIVE_OR_TOO_BIG = 26452,                    // Arithmetic overflow: Left shift count is negative or greater than or equal to the operand size which is undefined behavior (io.3: https://go.microsoft.com/fwlink/?linkid=864599).
    WARNING_LEFTSHIFT_NEGATIVE_SIGNED_NUMBER = 26453,                   // Arithmetic overflow: Left shift of a negative signed number is undefined behavior (io.4: https://go.microsoft.com/fwlink/?linkid=864600).
    WARNING_RESULT_OF_ARITHMETIC_OPERATION_NEGATIVE_UNSIGNED = 26454,   // Arithmetic overflow: '%1$s' operation produces a negative unsigned result at compile time (io.5: https://go.microsoft.com/fwlink/?linkid=864602).
    WARNING_USE_CONST_REFERENCE_ARGUMENTS = 26460,                      // The reference argument '%s' for function '%s' can be marked as const (con.3: https://go.microsoft.com/fwlink/p/?LinkID=786684).
    WARNING_USE_CONST_POINTER_ARGUMENTS = 26461,                        // The pointer argument '%s' for function '%s' can be marked as a pointer to const (con.3: https://go.microsoft.com/fwlink/p/?LinkID=786684).
    WARNING_USE_CONST_POINTER_FOR_VARIABLE = 26462,                     // The value pointed to by '%1$s' is assigned only once, mark it as a pointer to const (con.4: https://go.microsoft.com/fwlink/p/?LinkID=784969).
    WARNING_USE_CONST_FOR_ELEMENTS = 26463,                             // The elements of array '%1$s' are assigned only once, mark elements const (con.4: https://go.microsoft.com/fwlink/p/?LinkID=784969).
    WARNING_USE_CONST_POINTER_FOR_ELEMENTS = 26464,                     // The values pointed to by elements of array '%1$s' are assigned only once, mark elements as pointer to const (con.4: https://go.microsoft.com/fwlink/p/?LinkID=784969).
    WARNING_NO_CONST_CAST_UNNECESSARY = 26465,                          // Don't use const_cast to cast away const. const_cast is not required; constness or volatility is not being removed by this conversion (type.3: http://go.microsoft.com/fwlink/p/?LinkID=620419).
    WARNING_NO_STATIC_DOWNCAST_POLYMORPHIC = 26466,                     // Don't use static_cast downcasts. A cast from a polymorphic type should use dynamic_cast (type.2: http://go.microsoft.com/fwlink/p/?LinkID=620418).
    WARNING_NO_REINTERPRET_CAST_FROM_VOID_PTR = 26471,                  // Don't use reinterpret_cast. A cast from void* can use static_cast (type.1: http://go.microsoft.com/fwlink/p/?LinkID=620417).
    WARNING_NO_CASTS_FOR_ARITHMETIC_CONVERSION = 26472,                 // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narow (type.1: http://go.microsoft.com/fwlink/p/?LinkID=620417).
    WARNING_NO_IDENTITY_CAST = 26473,                                   // Don't cast between pointer types where the source type and the target type are the same (type.1: http://go.microsoft.com/fwlink/p/?LinkID=620417).
    WARNING_NO_IMPLICIT_CAST = 26474,                                   // Don't cast between pointer types when the conversion could be implicit (type.1: http://go.microsoft.com/fwlink/p/?LinkID=620417).
    WARNING_NO_FUNCTION_STYLE_CASTS = 26475,                            // Do not use function style C-casts (es.49: http://go.microsoft.com/fwlink/?linkid=853930).
    WARNING_NO_POINTER_ARITHMETIC = 26481,                              // Don't use pointer arithmetic. Use span instead (bounds.1: http://go.microsoft.com/fwlink/p/?LinkID=620413).
    WARNING_NO_DYNAMIC_ARRAY_INDEXING = 26482,                          // Only index into arrays using constant expressions (bounds.2: http://go.microsoft.com/fwlink/p/?LinkID=620414).
    WARNING_STATIC_INDEX_OUT_OF_RANGE = 26483,                          // Value %1$lld is outside the bounds (0, %2$lld) of variable '%3$s'. Only index into arrays using constant expressions that are within bounds of the array (bounds.2: http://go.microsoft.com/fwlink/p/?LinkID=620414).
    WARNING_NO_ARRAY_TO_POINTER_DECAY = 26485,                          // Expression '%1$s': No array to pointer decay (bounds.3: http://go.microsoft.com/fwlink/p/?LinkID=620415).
    WARNING_LIFETIMES_FUNCTION_PRECONDITION_VIOLATION = 26486,          // Don't pass a pointer that may be invalid to a function. Parameter %1$d '%2$s' in call to '%3$s' may be invalid (lifetime.1: http://go.microsoft.com/fwlink/p/?LinkID=851958).
    WARNING_LIFETIMES_FUNCTION_POSTCONDITION_VIOLATION = 26487,         // Don't return a pointer that may be invalid (lifetime.1: http://go.microsoft.com/fwlink/p/?LinkID=851958).
    WARNING_DEREF_INVALID_POINTER = 26489,                              // Don't dereference a pointer that may be invalid: '%1$s'. '%2$s' may have been invalidated at line %3$u (lifetime.1: http://go.microsoft.com/fwlink/p/?LinkID=851958).
    WARNING_NO_REINTERPRET_CAST = 26490,                                // Don't use reinterpret_cast (type.1: http://go.microsoft.com/fwlink/p/?LinkID=620417).
    WARNING_NO_STATIC_DOWNCAST = 26491,                                 // Don't use static_cast downcasts (type.2: http://go.microsoft.com/fwlink/p/?LinkID=620418).
    WARNING_NO_CONST_CAST = 26492,                                      // Don't use const_cast to cast away const (type.3: http://go.microsoft.com/fwlink/p/?LinkID=620419).
    WARNING_NO_CSTYLE_CAST = 26493,                                     // Don't use C-style casts (type.4: http://go.microsoft.com/fwlink/p/?LinkID=620420).
    WARNING_VAR_USE_BEFORE_INIT = 26494,                                // Variable '%1$s' is uninitialized. Always initialize an object (type.5: http://go.microsoft.com/fwlink/p/?LinkID=620421).
    WARNING_MEMBER_UNINIT = 26495,                                      // Variable '%1$s' is uninitialized. Always initialize a member variable (type.6: http://go.microsoft.com/fwlink/p/?LinkID=620422).
    WARNING_USE_CONST_FOR_VARIABLE = 26496,                             // The variable '%1$s' is assigned only once, mark it as const (con.4: https://go.microsoft.com/fwlink/p/?LinkID=784969).
    WARNING_USE_CONSTEXPR_FOR_FUNCTION = 26497,                         // The function '%s' could be marked constexpr if compile-time evaluation is desired (f.4: https://go.microsoft.com/fwlink/p/?LinkID=784970).
    WARNING_USE_CONSTEXPR_FOR_FUNCTIONCALL = 26498,                     // The function '%1$s' is constexpr, mark variable '%2$s' constexpr if compile-time evaluation is desired (con.5: https://go.microsoft.com/fwlink/p/?LinkID=784974).
};

#define ALL_CPPCORECHECK_WARNINGS 26400 26401 26402 26403 26404 26405 26406 26407 26408 26409 26410 26411 26414 26415 26416 26417 26418 26426 26427 26429 26430 26431 26432 26433 26434 26435 26436 26437 26438 26439 26440 26441 26443 26444 26450 26451 26452 26453 26454 26460 26461 26462 26463 26464 26465 26466 26471 26472 26473 26474 26475 26481 26482 26483 26485 26486 26487 26489 26490 26491 26492 26493 26494 26495 26496 26497 26498

#define CPPCORECHECK_ARITHMETIC_WARNINGS 26450 26451 26452 26453 26454

#define CPPCORECHECK_BOUNDS_WARNINGS 26481 26482 26483 26485

#define CPPCORECHECK_CLASS_WARNINGS 26432 26433 26434 26435 26436 26443

#define CPPCORECHECK_CONCURRENCY_WARNINGS 26441

#define CPPCORECHECK_CONST_WARNINGS 26460 26461 26462 26463 26464 26496 26497 26498

#define CPPCORECHECK_DECLARATION_WARNINGS 26426 26427 26444

#define CPPCORECHECK_FUNCTION_WARNINGS 26439 26440

#define CPPCORECHECK_LIFETIME_WARNINGS 26486 26487 26489

#define CPPCORECHECK_OWNER_POINTER_WARNINGS 26402 26403 26404 26405 26406 26407 26429 26430 26431

#define CPPCORECHECK_RAW_POINTER_WARNINGS 26400 26401 26402 26408 26409 26429 26430 26431 26481 26485

#define CPPCORECHECK_SHARED_POINTER_WARNINGS 26414 26415 26416 26417 26418

#define CPPCORECHECK_STYLE_WARNINGS 26438

#define CPPCORECHECK_TYPE_WARNINGS 26437 26465 26466 26471 26472 26473 26474 26475 26490 26491 26492 26493 26494 26495

#define CPPCORECHECK_UNIQUE_POINTER_WARNINGS 26410 26411 26414 26415

// clang-format on
