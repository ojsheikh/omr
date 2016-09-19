/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

#include "il/OMRDataTypes.hpp"

#include <ctype.h>                    // for isdigit
#include <stddef.h>                   // for NULL
#include <stdint.h>                   // for int32_t, intptrj_t
#include <string.h>                   // for strlen
#include "env/jittypes.h"             // for intptrj_t
#include "infra/Assert.hpp"           // for TR_ASSERT

// When adding new types also update pDataTypeNames[] in ras/Tree.cpp

namespace OMR {

#define TR_Bad TR::BadILOp

static TR::ILOpCodes conversionMap[TR::NumOMRTypes][TR::NumOMRTypes] =
//                    No      Int8     Int16    Int32    Int64    Float    Double   Addr     VectorInt8 VectorInt16 VectorInt32 VectorInt64 VectorFloat VectorDouble Aggregate
   {
/* NoType */        { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // NoType
/* Int8 */          { TR_Bad, TR_Bad,  TR::b2s, TR::b2i, TR::b2l, TR::b2f, TR::b2d, TR::b2a, TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Int8
/* Int16 */         { TR_Bad, TR::s2b, TR_Bad,  TR::s2i, TR::s2l, TR::s2f, TR::s2d, TR::s2a, TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Int16
/* Int32 */         { TR_Bad, TR::i2b, TR::i2s, TR_Bad,  TR::i2l, TR::i2f, TR::i2d, TR::i2a, TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Int32
/* Int64 */         { TR_Bad, TR::l2b, TR::l2s, TR::l2i, TR_Bad,  TR::l2f, TR::l2d, TR::l2a, TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Int64
/* Float */         { TR_Bad, TR::f2b, TR::f2s, TR::f2i, TR::f2l, TR_Bad,  TR::f2d, TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Float
/* Double */        { TR_Bad, TR::d2b, TR::d2s, TR::d2i, TR::d2l, TR::d2f, TR_Bad,  TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Double
/* Address */       { TR_Bad, TR::a2b, TR::a2s, TR::a2i, TR::a2l, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Address
/* VectorInt8 */    { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,    TR::v2v,    TR::v2v,    TR::v2v,    TR::v2v,    TR::v2v,     TR_Bad },  // VectorInt8
/* VectorInt16 */   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR::v2v,   TR_Bad,     TR::v2v,    TR::v2v,    TR::v2v,    TR::v2v,     TR_Bad },  // VectorInt16
/* VectorInt32 */   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR::v2v,   TR::v2v,    TR_Bad,     TR::v2v,    TR::v2v,    TR::v2v,     TR_Bad },  // VectorInt32
/* VectorInt64 */   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR::v2v,   TR::v2v,    TR::v2v,    TR_Bad,     TR::v2v,    TR::v2v,     TR_Bad },  // VectorInt64
/* VectorFloat */   { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR::v2v,   TR::v2v,    TR::v2v,    TR::v2v,    TR_Bad,     TR::v2v,     TR_Bad },  // VectorFloat
/* VectorDouble */  { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR::v2v,   TR::v2v,    TR::v2v,    TR::v2v,    TR::v2v,    TR_Bad,      TR_Bad },  // VectorDouble
/* Aggregate */     { TR_Bad, TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,  TR_Bad,    TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,     TR_Bad,      TR_Bad },  // Aggregate
   };

#undef TR_Bad

} // namespace OMR

TR::ILOpCodes
OMR::DataType::getDataTypeConversion(TR::DataTypes t1, TR::DataTypes t2)
   {
   TR_ASSERT(t1 < TR::NumOMRTypes, "conversion opcode from unexpected datatype %d requested", t1);
   TR_ASSERT(t2 < TR::NumOMRTypes, "conversion opcode to unexpected datatype %d requested", t2);
   return OMR::conversionMap[t1][t2];
   }

TR::DataTypes
OMR::DataType::getFloatTypeFromSize(int32_t size)
   {
   TR::DataTypes type = TR::NoType;
   switch (size)
      {
      case 4:  type = TR::Float; break;
      case 8:  type = TR::Double; break;
      default: TR_ASSERT(false,"unexpected size %d for float type\n",size);
      }
   return type;
   }

bool
OMR::DataType::canGetMaxPrecisionFromType(TR::DataTypes type)
   {
   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
         return true;
      default:
         return false;
      }
   }

int32_t
OMR::DataType::getMaxPrecisionFromType(TR::DataTypes type)
   {
   switch (type)
      {
      case TR::Int8: return TR::getMaxSignedPrecision<TR::Int8>();
      case TR::Int16: return TR::getMaxSignedPrecision<TR::Int16>();
      case TR::Int32: return TR::getMaxSignedPrecision<TR::Int32>();
      case TR::Int64: return TR::getMaxSignedPrecision<TR::Int64>();
      default:
         TR_ASSERT(false, "Unsupported data type in getMaxPrecisionFromType\n");
         return 0;
      }
   }

static int32_t OMRDataTypeSizes[] =
   {
   0,                 // TR::NoType
   1,                 // TR::Int8
   2,                 // TR::Int16
   4,                 // TR::Int32
   8,                 // TR::Int64
   4,                 // TR::Float
   8,                 // TR::Double
   sizeof(intptrj_t), // TR::Address
   16,                // TR::VectorInt8
   16,                // TR::VectorInt16
   16,                // TR::VectorInt32
   16,                // TR::VectorInt64
   16,                // TR::VectorFloat
   16,                // TR::VectorDouble
   0,                 // TR::Aggregate
   };

static_assert(TR::NumOMRTypes == (sizeof(OMRDataTypeSizes) / sizeof(OMRDataTypeSizes[0])), "OMRDataTypeSizes is not the correct size");

const int32_t
OMR::DataType::getSize(TR::DataTypes dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "dataTypeSizeMap called on unrecognized data type");
   return OMRDataTypeSizes[dt];
   }

void
OMR::DataType::setSize(TR::DataTypes dt, int32_t newSize)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "setDataTypeSizeInMap called on unrecognized data type");
   OMRDataTypeSizes[dt] = newSize;
   }


static const char * OMRDataTypeNames[] =
   {
   "NoType",
   "Int8",
   "Int16",
   "Int32",
   "Int64",
   "Float",
   "Double",
   "Address",
   "VectorInt8",
   "VectorInt16",
   "VectorInt32",
   "VectorInt64",
   "VectorFloat",
   "VectorDouble",
   "Aggregate",
   };

static_assert(TR::NumOMRTypes == (sizeof(OMRDataTypeNames) / sizeof(OMRDataTypeNames[0])), "OMRDataTypeNames is not the correct size");

const char *
OMR::DataType::getName(TR::DataTypes dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "Name requested for unknown datatype");
   return OMRDataTypeNames[dt];
   }


static const char *OMRDataTypePrefixes[] =
   {
   NULL,    // TR::NoType
   "B",     // TR::Int8
   "S",     // TR::Int16
   "I",     // TR::Int32
   "L",     // TR::Int64
   "F",     // TR::Float
   "D",     // TR::Double
   "A",     // TR::Address
   "VI1",   // TR::VectorInt8
   "VI2",   // TR::VectorInt16
   "VI4",   // TR::VectorInt32
   "VI8",   // TR::VectorInt64
   "VF",    // TR::VectorFloat
   "VD",    // TR::VectorDouble
   "AG",    // TR::Aggregate
   };

static_assert(TR::NumOMRTypes == (sizeof(OMRDataTypePrefixes) / sizeof(OMRDataTypePrefixes[0])), "OMRDataTypePrefixes is not the correct size");

const char *
OMR::DataType::getPrefix(TR::DataTypes dt)
   {
   TR_ASSERT(dt < TR::NumOMRTypes, "Prefix requested for unknown datatype");
   return OMRDataTypePrefixes[dt];
   }

