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
 *******************************************************************************/

#ifndef OMR_I386_TREE_EVALUATOR_INCL
#define OMR_I386_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace X86 { namespace i386 { class TreeEvaluator; } } }
namespace OMR { typedef OMR::X86::i386::TreeEvaluator TreeEvaluatorConnector; }
#else
#error OMR::X86::i386::TreeEvaluator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/x/codegen/OMRTreeEvaluator.hpp"

#include "x/codegen/X86Ops.hpp"  // for TR_X86OpCodes

namespace TR { class Register; }



namespace OMR
{

namespace X86
{

namespace i386
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::X86::TreeEvaluator
   {

   public:
   static TR::Register *aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *landEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairDualMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairAddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairSubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairDivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairRemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairNegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairShlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairRolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairShrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairUshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerPairByteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lternaryEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpsetEvaluator(TR::Node *n, TR::CodeGenerator *cg);

   private:

   static TR::Register *performLload(TR::Node *node, TR::MemoryReference *sourceMR, TR::CodeGenerator *cg);

   static TR::Register *longArithmeticCompareRegisterWithImmediate(
         TR::Node *node,
         TR::Register *cmpRegister,
         TR::Node *immedChild,
         TR_X86OpCodes firstBranchOpCode,
         TR_X86OpCodes secondBranchOpCode,
         TR::CodeGenerator *cg);

   static TR::Register *compareLongAndSetOrderedBoolean(
         TR::Node *node,
         TR_X86OpCodes highSetOpCode,
         TR_X86OpCodes lowSetOpCode,
         TR::CodeGenerator *cg);

   static void compareLongsForOrder(
         TR::Node *node,
         TR_X86OpCodes highOrderBranchOp,
         TR_X86OpCodes highOrderReversedBranchOp,
         TR_X86OpCodes lowOrderBranchOp,
         TR::CodeGenerator *cg);

   };

} // namespace i386

} // namespace X86

} // namespace OMR

#endif
