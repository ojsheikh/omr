/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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
 ******************************************************************************/

#include "JBTestUtil.hpp"

union TestUnionInt8pChar
   {
   uint8_t v_uint8;
   const char* v_pchar;
   };

union TestUnionInt32Int32
   {
   uint32_t f1;
   uint32_t f2;
   };

union TestUnionInt16Double
   {
   uint16_t v_uint16;
   double v_double;
   };

DEFINE_TYPES(UnionTypes)
   {
   DefineUnion("TestUnionInt8pChar");
   UnionField("TestUnionInt8pChar", "v_uint8", toIlType<uint8_t>());
   UnionField("TestUnionInt8pChar", "v_pchar", toIlType<char*>());
   CloseUnion("TestUnionInt8pChar");

   DefineUnion("TestUnionInt32Int32");
   UnionField("TestUnionInt32Int32", "f1", Int32);
   UnionField("TestUnionInt32Int32", "f2", Int32);
   CloseUnion("TestUnionInt32Int32");

   DefineUnion("TestUnionInt16Double");
   UnionField("TestUnionInt16Double", "v_uint16", Int16);
   UnionField("TestUnionInt16Double", "v_double", Double);
   CloseUnion("TestUnionInt16Double");
   }

typedef uint32_t (*TypePunInt32Int32Function)(TestUnionInt32Int32*, uint32_t, uint32_t);
typedef uint16_t (*TypePunInt16DoubleFunction)(TestUnionInt16Double*, uint16_t, double);

DEFINE_BUILDER( TypePunInt32Int32Builder,
                Int32,
                PARAM("u", PointerTo(LookupUnion("TestUnionInt32Int32"))),
                PARAM("v1", Int32),
                PARAM("v2", Int32) )
   {
   StoreIndirect("TestUnionInt32Int32", "f1", Load("u"), Load("v1"));
   StoreIndirect("TestUnionInt32Int32", "f2", Load("u"), Load("v2"));
   Return(LoadIndirect("TestUnionInt32Int32", "f1", Load("u")));

   return  true;
   }

DEFINE_BUILDER( TypePunInt16DoubleBuilder,
                Int16,
                PARAM("u", PointerTo(LookupUnion("TestUnionInt16Double"))),
                PARAM("v1", Int16),
                PARAM("v2", Double) )
   {
   StoreIndirect("TestUnionInt16Double", "v_uint16", Load("u"), Load("v1"));
   StoreIndirect("TestUnionInt16Double", "v_double", Load("u"), Load("v2"));
   Return(LoadIndirect("TestUnionInt16Double", "v_uint16", Load("u")));

   return true;
   }

class UnionTest : public JitBuilderTest {};

TEST_F(UnionTest, UnionTypeDictionaryTest)
   {
   UnionTypes td;
   ASSERT_EQ(sizeof(char*),    td.LookupUnion("TestUnionInt8pChar")->getSize());
   ASSERT_EQ(sizeof(uint32_t), td.LookupUnion("TestUnionInt32Int32")->getSize());
   ASSERT_EQ(sizeof(double),   td.LookupUnion("TestUnionInt16Double")->getSize());
   }

TEST_F(UnionTest, TypePunWithEqualTypes)
   {
   TypePunInt32Int32Function typePunInt32Int32;
   ASSERT_COMPILE(UnionTypes, TypePunInt32Int32Builder, typePunInt32Int32);

   TestUnionInt32Int32 testUnion;
   const uint32_t v1 = 5;
   const uint32_t v2 = 10;
   ASSERT_EQ(v2, typePunInt32Int32(&testUnion, v1, v2))
         << "Value returned by compiled method is wrong.";
   }

TEST_F(UnionTest, TypePunWithDifferentTypes)
   {
   TypePunInt16DoubleFunction typePunInt16Dboule;
   ASSERT_COMPILE(UnionTypes, TypePunInt16DoubleBuilder, typePunInt16Dboule);

   TestUnionInt16Double testUnion;
   const uint16_t v1 = 0xFFFF;
   const double v2 = +0.0;
   ASSERT_EQ(static_cast<uint16_t>(v2) , typePunInt16Dboule(&testUnion, v1, v2))
         << "Value returned by compiled method is wrong.";
   }
