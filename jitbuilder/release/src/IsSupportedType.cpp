/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/
 
#include <iostream>
#include <cstdint>
#include <cassert>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"

// types for use in test cases
enum Int8Enum : int8_t { Field_8 };
struct StructType { int field; };
union UnionType { StructType s; char field; };

int main()
   {
   /* Note: In an actual project, `static_assert` should be used in
    * favour of `assert`. This is not done here for testability
    * purposes.
    */

   std::cout << "Step 1: test signed integral types\n";
   assert(true == TR::TypeDictionary::is_supported<int8_t>::value);
   assert(true == TR::TypeDictionary::is_supported<int16_t>::value);
   assert(true == TR::TypeDictionary::is_supported<int32_t>::value);
   assert(true == TR::TypeDictionary::is_supported<int64_t>::value);

   std::cout << "Step 2: test unsigned integral types\n";
   assert(true == TR::TypeDictionary::is_supported<uint8_t>::value);
   assert(true == TR::TypeDictionary::is_supported<uint16_t>::value);
   assert(true == TR::TypeDictionary::is_supported<uint32_t>::value);
   assert(true == TR::TypeDictionary::is_supported<uint64_t>::value);

   std::cout << "Step 3: test language primitive signed integral types\n";
   assert(true == TR::TypeDictionary::is_supported<char>::value);
   assert(true == TR::TypeDictionary::is_supported<short>::value);
   assert(true == TR::TypeDictionary::is_supported<int>::value);
   assert(true == TR::TypeDictionary::is_supported<long>::value);
   assert(true == TR::TypeDictionary::is_supported<long long>::value);

   std::cout << "Step 4: test language primitive unsigned integral types\n";
   assert(true == TR::TypeDictionary::is_supported<unsigned char>::value);
   assert(true == TR::TypeDictionary::is_supported<unsigned short>::value);
   assert(true == TR::TypeDictionary::is_supported<unsigned int>::value);
   assert(true == TR::TypeDictionary::is_supported<unsigned long>::value);
   assert(true == TR::TypeDictionary::is_supported<unsigned long long>::value);

   std::cout << "Step 5: test floating point types\n";
   assert(true == TR::TypeDictionary::is_supported<float>::value);
   assert(true == TR::TypeDictionary::is_supported<double>::value);

   std::cout << "Step 6: test cv qualified types\n";
   assert(true == TR::TypeDictionary::is_supported<const int8_t>::value);
   assert(true == TR::TypeDictionary::is_supported<volatile uint16_t>::value);
   assert(true == TR::TypeDictionary::is_supported<const volatile int32_t>::value);

   std::cout << "Step 7: test void type\n";
   assert(true == TR::TypeDictionary::is_supported<void>::value);

   std::cout << "Step 8: test pointer to primitive types\n";
   assert(true == TR::TypeDictionary::is_supported<int32_t*>::value);
   assert(true == TR::TypeDictionary::is_supported<double*>::value);
   assert(true == TR::TypeDictionary::is_supported<void*>::value);

   std::cout << "Step 9: test pointer to pointer to primitive types\n";
   assert(true == TR::TypeDictionary::is_supported<int32_t**>::value);
   assert(true == TR::TypeDictionary::is_supported<double**>::value);
   assert(true == TR::TypeDictionary::is_supported<void**>::value);

   std::cout << "Step 10: test unsupported types\n";
   assert(false == TR::TypeDictionary::is_supported<int[10]>::value);
   assert(false == TR::TypeDictionary::is_supported<Int8Enum>::value);
   assert(false == TR::TypeDictionary::is_supported<StructType>::value);
   assert(false == TR::TypeDictionary::is_supported<UnionType>::value);
   assert(false == TR::TypeDictionary::is_supported<StructType*>::value);
   assert(false == TR::TypeDictionary::is_supported<UnionType*>::value);
   assert(false == TR::TypeDictionary::is_supported<StructType**>::value);
   assert(false == TR::TypeDictionary::is_supported<UnionType**>::value);

   std::cout << "PASS\n";
   }
