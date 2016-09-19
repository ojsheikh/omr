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

#include <stdint.h>                                 // for int32_t
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator
#include "compile/Compilation.hpp"                  // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/Node.hpp"                              // for Node, vcount_t
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "infra/BitVector.hpp"                      // for TR_BitVector
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/TRCfgNode.hpp"                      // for CFGNode
#include "optimizer/Structure.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

namespace TR { class Optimizer; }

#define MAX_REGISTERS 32

// Methods to implement the backward data flow analysis required for
// computing the register anticipatability. The results of the analysis
// will be used to compute optimal save points for callee-preserved registers
//
TR_DataFlowAnalysis::Kind TR_RegisterAnticipatability::getKind()
   {
   return RegisterAnticipatability;
   }

TR_RegisterAnticipatability *TR_RegisterAnticipatability::asRegisterAnticipatability()
   {
   return this;
   }

int32_t TR_RegisterAnticipatability::getNumberOfBits()
   {
   return TR::RealRegister::NumRegisters;
   }


void TR_RegisterAnticipatability::analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, TR_BitVector *)
   {
   }

void TR_RegisterAnticipatability::initializeRegisterUsageInfo()
   {
   // initialize outSets bitvector as well
   //
   TR_BitVector **originalRegUsageInfo = _registerUsageInfo;
   _registerUsageInfo = (TR_BitVector **) trMemory()->allocateStackMemory(_numberOfNodes * sizeof(TR_BitVector *));
   _outSetInfo = (TR_BitVector **) trMemory()->allocateStackMemory(_numberOfNodes * sizeof(TR_BitVector *));

   for (int32_t i = 0; i < _numberOfNodes; i++)
      {
      _registerUsageInfo[i] = new (trStackMemory()) TR_BitVector(_numberOfBits, trMemory(), stackAlloc);
      copyFromInto(originalRegUsageInfo[i], _registerUsageInfo[i]);
      _outSetInfo[i] = new (trStackMemory()) TR_BitVector(_numberOfBits, trMemory(), stackAlloc);
      _outSetInfo[i]->empty();
      }

   }

TR_RegisterAnticipatability::TR_RegisterAnticipatability(TR::Compilation *comp,
                                                         TR::Optimizer *optimizer,
                                                         TR_Structure *rootStructure,
                                                         TR_BitVector **regUsageInfo,
                                                         bool trace)
   : TR_BackwardIntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, trace)
   {
   if (comp->getOption(TR_TraceShrinkWrapping))
      traceMsg(comp, "Starting RegisterAnticipatability\n");

   _registerUsageInfo = regUsageInfo;   // Will be copied in initializeRegisterUsageInfo

   performAnalysis(rootStructure, false);

   // copy the solution from the first block of the CFG
   // into the dummy entry -- this is because the dummy entry
   // dominates every block
   //
   int32_t entryNum = comp->getStartTree()->getEnclosingBlock()->getNumber(); // dummy entry block
   int32_t dummyNum = comp->getFlowGraph()->getStart()->getNumber();
   *_blockAnalysisInfo[dummyNum] |= *_blockAnalysisInfo[entryNum];
   *_outSetInfo[dummyNum] |= *_blockAnalysisInfo[entryNum];

   if (comp->getOption(TR_TraceShrinkWrapping))
      {
      for (int32_t i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp, "Block number : %d has solution : ", i);
         _blockAnalysisInfo[i]->print(comp);
         traceMsg(comp, "\n");
         }

      for (int32_t i = 0; i < _numberOfNodes; i++)
         {
         traceMsg(comp, "Block number : %d has outSet : ", i);
         _outSetInfo[i]->print(comp);
         traceMsg(comp, "\n");
         }

      traceMsg(comp, "Ending RegisterAnticipatability\n");
      }
   }

bool TR_RegisterAnticipatability::postInitializationProcessing()
   {
   // initialize the RUSE vectors for each block
   //
   initializeRegisterUsageInfo();
   return true;
   }

void TR_RegisterAnticipatability::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   int32_t blockNum = blockStructure->getBlock()->getNumber();
   // save the outSetInfo for later use
   //
   copyFromInto(_regularInfo, _outSetInfo[blockNum]);

   // now compose the exception info into the outSet
   // for this block
   //
   compose(_regularInfo, _exceptionInfo);
   compose(_outSetInfo[blockNum], _exceptionInfo);

   // compute the _inSet = _outSet + RUSE
   //
   *_regularInfo |= *_registerUsageInfo[blockNum];
   *_exceptionInfo |= *_registerUsageInfo[blockNum]; //FIXME: is this too conservative?

   if (comp()->getOption(TR_TraceShrinkWrapping))
      {
      traceMsg(comp(), "Normal info of block_%d : ", blockNum);
      _regularInfo->print(comp());
      traceMsg(comp(), "\n");
      }
   }
