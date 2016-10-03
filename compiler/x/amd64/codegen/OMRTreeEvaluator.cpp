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

#include "codegen/OMRTreeEvaluator.hpp"

#include <stddef.h>                                      // for NULL
#include <stdint.h>                                      // for uint8_t, etc
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"                          // for TR_FrontEnd, etc
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                           // for Machine
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"                          // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                       // for Compilation
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"                              // for DOUBLE_NAN, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                  // for ILOpCode
#include "il/Node.hpp"                                   // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                                 // for Symbol
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"                     // for LabelSymbol
#include "infra/Assert.hpp"                              // for TR_ASSERT
#include "infra/List.hpp"                                // for List
#include "ras/Debug.hpp"
#include "x/codegen/ConstantDataSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/X86Evaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                          // for ::LABEL, etc

TR::Register *OMR::X86::AMD64::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = loadConstant(node, node->getLongInt(), TR_RematerializableAddress, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = loadConstant(node, node->getLongInt(), TR_RematerializableLong, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }

// TODO:AMD64: Could this be combined with istoreEvaluator without too much ugliness?
TR::Register *OMR::X86::AMD64::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   TR::Compilation* comp = cg->comp();

   if (node->getOpCode().isIndirect())
      valueChild = node->getSecondChild();
   else
      valueChild = node->getFirstChild();

   // Handle special cases
   //
   if (valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a double value into long variable
      //
      if (valueChild->getOpCodeValue() == TR::dbits2l &&
          !valueChild->normalizeNanValues())
         {
         if (node->getOpCode().isIndirect())
            {
            node->setChild(1, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstorei);
            floatingPointStoreEvaluator(node, cg);
            node->setChild(1, valueChild);
            TR::Node::recreate(node, TR::lstorei);
            }
         else
            {
            node->setChild(0, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::dstore);
            floatingPointStoreEvaluator(node, cg);
            node->setChild(0, valueChild);
            TR::Node::recreate(node, TR::lstore);
            }
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   return integerStoreEvaluator(node, cg);
   }

// also handles ilload
TR::Register *OMR::X86::AMD64::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register *reg = loadMemory(node, sourceMR, TR_RematerializableLong, node->getOpCode().isIndirect(), cg);

   reg->setMemRef(sourceMR);
   node->setRegister(reg);
   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::landEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return logicalEvaluator(node, _logicalOpPackage[landOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return logicalEvaluator(node, _logicalOpPackage[lorOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return logicalEvaluator(node, _logicalOpPackage[lxorOpPackage], cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::Register *targetRegister = cg->allocateRegister();

      generateRegImmInstruction(MOV8RegImm4, node, targetRegister, node->getFirstChild()->getInt(), cg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(node->getFirstChild());

      return targetRegister;
      }
   else
      {
      // In theory, because iRegStore has chosen to disregard needsSignExtension,
      // we must disregard skipSignExtension here for correctness.
      //
      // However, in fact, it is actually safe to obey skipSignExtension so
      // long as the optimizer only uses it on nodes known to be non-negative
      // when the i2l occurs.  We do already have isNonNegative for that
      // purpose, but it may not always be set by the optimizer if a node known
      // to be non-negative at one point in a block is commoned up above the
      // BNDCHK or branch that determines the node's non-negativity.  The
      // codegen does set the flag during tree evaluation, but the
      // skipSignExtension flag is set by the optimizer with more global
      // knowledge than the tree evaluator, so we will trust it.
      //
      TR_X86OpCodes regMemOpCode,regRegOpCode;
      if(   node->isNonNegative()
         || (node->skipSignExtension() && performTransformation(comp, "TREE EVALUATION: skipping sign extension on node %s despite lack of isNonNegative\n", comp->getDebug()->getName(node))))
         {
         // We prefer these plain (zero-extending) opcodes because the analyser can often eliminate them
         //
         regMemOpCode = L4RegMem;
         regRegOpCode = MOVZXReg8Reg4;
         }
      else
         {
         regMemOpCode = MOVSXReg8Mem4;
         regRegOpCode = MOVSXReg8Reg4;
         }
      return conversionAnalyser(node, regMemOpCode, regRegOpCode, cg);
      }
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *reg   = cg->evaluate(child);
   if (child->getReferenceCount() > 1)
      {
      // This catches two scenarios:
      //
      // 1) A longClobberEvaluate (or any other register-clobbering logic) on
      // the l2i node could see a refcount of 1, and hence won't make a copy.
      // If child's refcount is more than 1, we do in fact need a copy, so we'd
      // better do it here.
      //
      // 2) If the child is commoned, and the l2i node is also commoned, then
      // we may end up with a situation where the last evaluation of the child
      // is a clobberEvaluate.  By that time, the child's refcount would be 1,
      // so no copy is made, and the register would be clobbered.  Therefore,
      // the l2i node can't return that same register, or else the other uses
      // of the node will end up getting the clobbered value.
      //
      // Note that case 2 is conservative, in that it presumes that the child's
      // register will be clobbered by another node.  If this does not occur,
      // then the copy we're about to make is unnecessary.
      //
      TR::Register *childReg = reg;
      reg = cg->allocateRegister();
      // to support signExtension in GRA, need to preserve upper word
      // in this move
      generateRegRegInstruction(MOV8RegReg, node, reg, childReg, cg);
      }

   node->setRegister(reg);
   cg->decReferenceCount(child);

   if (cg->enableRegisterInterferences() && node->getOpCode().getSize() == 1)
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(node->getRegister());

   return reg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (node->getFirstChild()->getOpCode().isLoadConst())
      {
      TR::Register *targetRegister = cg->allocateRegister();

      generateRegImmInstruction(MOV4RegImm4, node, targetRegister, node->getFirstChild()->getInt(), cg); // implicitly zero extended

      node->setRegister(targetRegister);
      cg->decReferenceCount(node->getFirstChild());

      return targetRegister;
      }
   else
      return conversionAnalyser(node, L4RegMem, MOVZXReg8Reg4, cg); // This zero-extends on AMD64
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, MOVSXReg8Mem1, MOVSXReg8Reg1, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, MOVZXReg8Mem1, MOVZXReg8Reg1, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, MOVSXReg8Mem2, MOVSXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, MOVZXReg8Mem2, MOVZXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, MOVZXReg8Mem2, MOVZXReg8Reg2, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *leftRegister  = cg->evaluate(firstChild);
   TR::Register *rightRegister = cg->evaluate(secondChild);

   // Compare left and right operands, all finished with the operands after this.
   generateRegRegInstruction(CMP8RegReg, node, leftRegister, rightRegister, cg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   TR::Register *isLessThanReg = cg->allocateRegister();
   TR::Register *isNotEqualReg = cg->allocateRegister();
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(isLessThanReg);
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(isNotEqualReg);

   // The state of things in each possible case after each instruction:
   //                       left < right    left = right    left > right
   // Processor flags:      NE=1 LT=1       NE=0  LT=0      NE=1 LT=0
   generateRegInstruction(SETL1Reg, node, isLessThanReg, cg);
   // isLessThanReg:        00000001        00000000        00000000
   generateRegInstruction(SETNE1Reg, node, isNotEqualReg, cg);
   // isNotEqualReg:        00000001        00000000        00000001
   generateRegInstruction(NEG1Reg, node, isLessThanReg, cg);
   // isLessThanReg:        11111111        00000000        00000000
   generateRegRegInstruction(OR1RegReg, node, isNotEqualReg, isLessThanReg, cg);
   // isNotEqualReg:        11111111        00000000        00000001

   generateRegRegInstruction(MOVSXReg4Reg1, node, isNotEqualReg, isNotEqualReg, cg);

   node->setRegister(isNotEqualReg);
   cg->stopUsingRegister(isLessThanReg);

   return isNotEqualReg;
   }

static TR::Register *l2fd(TR::Node *node, TR::RealRegister *target, TR_X86OpCodes opRegMem8, TR_X86OpCodes opRegReg8, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::MemoryReference  *tempMR;

   TR_ASSERT(cg->useSSEForSinglePrecision(), "assertion failure");

   if (child->getRegister() == NULL &&
       child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      generateRegMemInstruction(opRegMem8, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      generateRegRegInstruction(opRegReg8, node, target, intReg, cg);
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return l2fd(node, toRealRegister(cg->allocateSinglePrecisionRegister(TR_FPR)), CVTSI2SSRegMem8, CVTSI2SSRegReg8, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return l2fd(node, toRealRegister(cg->allocateRegister(TR_FPR)), CVTSI2SDRegMem8, CVTSI2SDRegReg8, cg);
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: Peepholing
   TR::Node      *child  = node->getFirstChild();
   TR::Register  *sreg   = cg->evaluate(child);
   TR::Register  *treg   = cg->allocateRegister(TR_FPR);
   generateRegRegInstruction(MOVQRegReg8, node, treg, sreg, cg);
   node->setRegister(treg);
   cg->decReferenceCount(child);
   return treg;
   }

TR::Register *OMR::X86::AMD64::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: Peepholing
   TR::Node      *child  = node->getFirstChild();
   TR::Register  *sreg   = cg->evaluate(child);
   TR::Register  *treg   = cg->allocateRegister(TR_GPR);
   generateRegRegInstruction(MOVQReg8Reg, node, treg, sreg, cg);
   if (node->normalizeNanValues())
      {
      static char *disableFastNormalizeNaNs = feGetEnv("TR_disableFastNormalizeNaNs");
      if (disableFastNormalizeNaNs)
         {
         // This one is not clever, but it is simple, and it's based directly
         // on the IA32 version which is known to work, so is safer.
         //
         TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         deps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR_IA32ConstantDataSnippet *nan1Snippet = cg->findOrCreate8ByteConstant(node, DOUBLE_NAN_1_LOW);
         TR_IA32ConstantDataSnippet *nan2Snippet = cg->findOrCreate8ByteConstant(node, DOUBLE_NAN_2_LOW);
         TR::MemoryReference      *nan1MR      = generateX86MemoryReference(nan1Snippet, cg);
         TR::MemoryReference      *nan2MR      = generateX86MemoryReference(nan2Snippet, cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         generateLabelInstruction(   LABEL,       node, startLabel,               cg);
         generateRegMemInstruction(  CMP8RegMem,  node, treg, nan1MR,             cg);
         generateLabelInstruction(   JGE4,        node, normalizeLabel,           cg);
         generateRegMemInstruction(  CMP8RegMem,  node, treg, nan2MR,             cg);
         generateLabelInstruction(   JB4,         node, endLabel,                 cg);
         generateLabelInstruction(   LABEL,       node, normalizeLabel,           cg);
         generateRegImm64Instruction( MOV8RegImm64, node, treg, DOUBLE_NAN,         cg);
         generateLabelInstruction(   LABEL,       node, endLabel,           deps, cg);
         }
      else
         {
         // A bunch of bookkeeping
         //
         uint64_t nanDetector = DOUBLE_NAN_2_LOW;

         TR::RegisterDependencyConditions  *internalControlFlowDeps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         internalControlFlowDeps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR::RegisterDependencyConditions  *helperDeps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         helperDeps->addPreCondition( treg, TR::RealRegister::eax, cg);
         helperDeps->addPostCondition(treg, TR::RealRegister::eax, cg);

         TR_IA32ConstantDataSnippet *nanDetectorSnippet  = cg->findOrCreate8ByteConstant(node, nanDetector);
         TR::MemoryReference      *nanDetectorMR       = generateX86MemoryReference(nanDetectorSnippet,  cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *slowPathLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         // Fast path: if subtracting nanDetector leaves CF=0 or OF=1, then it
         // must be a NaN.
         //
         generateLabelInstruction(  LABEL,       node, startLabel,           cg);
         generateRegMemInstruction( CMP8RegMem,  node, treg, nanDetectorMR,  cg);
         generateLabelInstruction(  JAE4,        node, slowPathLabel,        cg);
         generateLabelInstruction(  JO4,         node, slowPathLabel,        cg);

         // Slow path
         //
         TR_OutlinedInstructions *slowPath = new (cg->trHeapMemory()) TR_OutlinedInstructions(slowPathLabel, cg);
         cg->getOutlinedInstructionsList().push_front(slowPath);
         slowPath->swapInstructionListsWithCompilation();
         generateLabelInstruction(NULL, LABEL,       slowPathLabel,          cg)->setNode(node);
         generateRegImm64Instruction(MOV8RegImm64, node, treg, DOUBLE_NAN, cg);
         generateLabelInstruction(      JMP4,        node, endLabel,         cg);
         slowPath->swapInstructionListsWithCompilation();

         // Merge point
         //
         generateLabelInstruction(LABEL, node, endLabel, internalControlFlowDeps, cg);
         }
      }
   node->setRegister(treg);
   cg->decReferenceCount(child);
   return treg;
   }


// Comparing instanceof to a constant
//
TR::Register *OMR::X86::AMD64::TreeEvaluator::ifInstanceOfHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "TR::TreeEvaluator::ifInstanceOfHelper not implemented");
   return NULL;
   // TODO:AMD64: The ifInstanceOfHelper mechanism could use some cleaning up.
   // Rather than changing the opcode, and then having VMisInstanceOfEvaluator
   // check that opcode, it seems cleaner just to pass in any necessary
   // information to VMisInstanceOfEvaluator via arguments.  It's not a real
   // evaluator anyway, so we can add arguments to it (and we should also
   // probably rename it.)
   }
