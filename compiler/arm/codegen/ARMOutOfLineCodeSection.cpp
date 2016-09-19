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

#include "codegen/ARMOutOfLineCodeSection.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR_ARMOutOfLineCodeSection::TR_ARMOutOfLineCodeSection(TR::Node  *callNode,
                            TR::ILOpCodes      callOp,
                            TR::Register      *targetReg,
                            TR::LabelSymbol    *entryLabel,
                            TR::LabelSymbol    *restartLabel,
                            TR::CodeGenerator *cg) :
                            TR_OutOfLineCodeSection(callNode,callOp,targetReg,entryLabel,restartLabel,cg)
   {
   generateARMOutOfLineCodeSectionDispatch();
   }

void TR_ARMOutOfLineCodeSection::generateARMOutOfLineCodeSectionDispatch()
   {
   // Switch to cold helper instruction stream.
   //
   swapInstructionListsWithCompilation();

   generateLabelInstruction(_cg, ARMOp_label, _callNode, _entryLabel);

   TR::Register *resultReg = NULL;
   if (_callNode->getOpCode().isCallIndirect())
      resultReg = TR::TreeEvaluator::performCall(_callNode, true, _cg);
   else
      resultReg = TR::TreeEvaluator::performCall(_callNode, false, _cg);

   if (_targetReg)
      {
      TR_ASSERT(resultReg, "assertion failure");
      generateTrg1Src1Instruction(_cg, ARMOp_mov, _callNode, _targetReg, resultReg);
      }
   _cg->decReferenceCount(_callNode);

   if (_restartLabel)
      generateLabelInstruction(_cg, ARMOp_b, _callNode, _restartLabel);

   generateLabelInstruction(_cg, ARMOp_label, _callNode, generateLabelSymbol(_cg));

   // Switch from cold helper instruction stream.
   //
   swapInstructionListsWithCompilation();
   }

void TR_ARMOutOfLineCodeSection::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
   {

   if (hasBeenRegisterAssigned())
     return;

   _cg->setInternalControlFlowSafeNestingDepth(_cg->internalControlFlowNestingDepth());

   // Create a dependency list on the first instruction in this stream that captures all current real register associations.
   // This is necessary to get the register assigner back into its original state before the helper stream was processed.
   _cg->incOutOfLineColdPathNestedDepth();

   // This prevents the OOL entry label from resetting all register's startOfranges during RA
   _cg->toggleIsInOOLSection();
   TR::RegisterDependencyConditions  *liveRealRegDeps = _cg->machine()->createCondForLiveAndSpilledGPRs(true, _cg->getSpilledRegisterList());

   if (liveRealRegDeps)
      _firstInstruction->setDependencyConditions(liveRealRegDeps);
   _cg->toggleIsInOOLSection(); // toggle it back because swapInstructionListsWithCompilation() also calls toggle...

   // Register assign the helper dispatch instructions.
   swapInstructionListsWithCompilation();
   _cg->doRegisterAssignment(kindsToBeAssigned);
   swapInstructionListsWithCompilation();

   // If a real reg is assigned at the end of the hot path and free at the beginning, but unused on the cold path
   // it won't show up in the liveRealRegDeps. If a dead virtual occupies that real reg we need to clean it up.
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastFPR; ++i)
      {
      TR::RealRegister *r = _cg->machine()->getARMRealRegister((TR::RealRegister::RegNum)i);
      TR::Register        *v = r->getAssignedRegister();

      if (v && v != r && v->getFutureUseCount() == 0)
         {
         v->setAssignedRegister(NULL);
         r->setState(TR::RealRegister::Unlatched);
         }
      }

   _cg->decOutOfLineColdPathNestedDepth();

   // Returning to mainline, reset this counter
   _cg->setInternalControlFlowSafeNestingDepth(0);

   // Link in the helper stream into the mainline code.
   // We will end up with the OOL items attached at the bottom of the instruction stream
   TR::Instruction *appendInstruction = _cg->comp()->getAppendInstruction();
   appendInstruction->setNext(_firstInstruction);
   _firstInstruction->setPrev(appendInstruction);
   _cg->comp()->setAppendInstruction(_appendInstruction);

   setHasBeenRegisterAssigned(true);
   }
