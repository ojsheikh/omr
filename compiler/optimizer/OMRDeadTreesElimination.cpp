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

#include "optimizer/DeadTreesElimination.hpp"

#include <stddef.h>                             // for NULL
#include <stdint.h>                             // for int32_t, uint32_t
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd, etc
#include "compile/Compilation.hpp"              // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"          // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                           // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"            // for TR::StackMemoryRegion
#include "env/jittypes.h"                       // for intptrj_t
#include "il/Block.hpp"                         // for Block
#include "il/DataTypes.hpp"                     // for DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                         // for ILOpCode, etc
#include "il/Node.hpp"                          // for Node
#include "il/NodePool.hpp"                      // for TR::NodePool
#include "il/Node_inlines.hpp"                  // for Node::getDataType, etc
#include "il/Symbol.hpp"                        // for Symbol
#include "il/SymbolReference.hpp"               // for SymbolReference, etc
#include "il/TreeTop.hpp"                       // for TreeTop
#include "il/TreeTop_inlines.hpp"               // for Node::getChild, etc
#include "il/symbol/AutomaticSymbol.hpp"        // for AutomaticSymbol
#include "il/symbol/MethodSymbol.hpp"           // for MethodSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/BitVector.hpp"                  // for TR_BitVector
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"                       // for TR_ScratchList, etc
#include "optimizer/Optimization.hpp"           // for Optimization
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"              // for Optimizer
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"                        // for TR_Debug


// Local helper functions

static OMR::TreeInfo *findOrCreateTreeInfo(TR::TreeTop *treeTop, List<OMR::TreeInfo> *targetTrees, TR::Compilation * comp)
   {
   ListIterator<OMR::TreeInfo> trees(targetTrees);
   OMR::TreeInfo *t;
   for (t = trees.getFirst(); t; t = trees.getNext())
      {
      if (t->getTreeTop() == treeTop)
         return t;
      }

   t = new (comp->trStackMemory()) OMR::TreeInfo(treeTop, 0);
   targetTrees->add(t);
   return t;
   }

// temporarily revert this fix
//static bool fixUpTree(TR::Node *node, TR::TreeTop *treeTop, TR::SparseBitVector &seenNodes, bool &highGlobalIndex, TR::Compilation *comp, vcount_t oldCompVisitCount)
static bool fixUpTree(TR::Node *node, TR::TreeTop *treeTop, TR::SparseBitVector &seenNodes, bool &highGlobalIndex, TR::Compilation *comp)
   {
   bool containsFloatingPoint = false;
   bool anchorLoadaddr = true;
   bool anchorArrayCmp = true;

   // temporarily revert this fix
   /*if (node->getVisitCount() <= oldCompVisitCount)
     {
     // This node was from a treetop that was encountered before the current treetop
     // in which case it should already be anchored and so would not need to be anchored afresh
     //
     return containsFloatingPoint;
     }*/


   // for arraycmp node, don't create its tree top anchor
   // fold it into if statment and save jump instruction
   if (node->getOpCodeValue() == TR::arraycmp &&
      !node->isArrayCmpLen() &&
      TR::Compiler->target.cpu.isX86())
      {
      anchorArrayCmp = false;
      }

   if ((node->getReferenceCount() > 1) &&
       !seenNodes.ValueAt(node->getGlobalIndex()) &&
       !node->getOpCode().isLoadConst() &&
       anchorLoadaddr &&
       anchorArrayCmp)
      {
      if (!comp->getOption(TR_ProcessHugeMethods)&& (comp->getNodeCount() > (3*USHRT_MAX/4)))
         {
         highGlobalIndex = true;
         return containsFloatingPoint;
         }

      seenNodes[node->getGlobalIndex()] = 1;
      if (node->getOpCode().isFloatingPoint())
        containsFloatingPoint = true;
      TR::TreeTop *nextTree = treeTop->getNextTreeTop();
      node->incFutureUseCount();
      TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node));
      anchorTreeTop->getNode()->setFutureUseCount(0);
      treeTop->join(anchorTreeTop);
      anchorTreeTop->join(nextTree);
      }
   else
      {
      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node *child = node->getChild(i);
         // temporarily rever this fix
         //if (fixUpTree(child, treeTop, seenNodes, highGlobalIndex, comp, oldCompVisitCount))
         if (fixUpTree(child, treeTop, seenNodes, highGlobalIndex, comp))
            containsFloatingPoint = true;
         }
      }
   return containsFloatingPoint;
   }

bool collectSymbolReferencesInNode(TR::Node *node,
                                   TR::SparseBitVector &symbolReferencesInNode,
                                   int32_t *numDeadSubNodes, vcount_t visitCount, TR::Compilation *comp,
                                   bool *seenInternalPointer, bool *seenArraylet,
                                   bool *cantMoveUnderBranch)
   {
   // The visit count in the node must be maintained by this method.
   //
   vcount_t oldVisitCount = node->getVisitCount();
   if (oldVisitCount == visitCount || oldVisitCount == comp->getVisitCount())
      return true;
   node->setVisitCount(comp->getVisitCount());

   //diagnostic("Walking node %p, height=%d, oldVisitCount=%d, visitCount=%d, compVisitCount=%d\n", node, *height, oldVisitCount, visitCount,comp->getVisitCount());

   // For all other subtrees collect all symbols that could be killed between
   // here and the next reference.
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      if (child->getFutureUseCount() == 1 &&
          child->getReferenceCount() > 1 &&
          !child->getOpCode().isLoadConst())
         *numDeadSubNodes = (*numDeadSubNodes) + 1;

      collectSymbolReferencesInNode(child, symbolReferencesInNode, numDeadSubNodes, visitCount, comp,
            seenInternalPointer, seenArraylet, cantMoveUnderBranch);
      }

   // detect if this is a direct load that shouldn't be moved under a branch (because an update was moved past
   // this load by treeSimplification)
   if (cantMoveUnderBranch &&
       (node->getOpCode().isLoadVarDirect() || node->getOpCode().isLoadReg()) &&
       node->isDontMoveUnderBranch())
      *cantMoveUnderBranch = true;

   if (seenInternalPointer && node->isInternalPointer() && node->getReferenceCount() > 1)
      *seenInternalPointer = true;

   if (seenArraylet)
      {
      if (node->getOpCode().hasSymbolReference() &&
          node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol() &&
          node->getReferenceCount() > 1)
         {
         *seenArraylet = true;
         }
      }

   // Add this node's symbol reference to the set
   if (node->getOpCode().hasSymbolReference())
      {
      symbolReferencesInNode[node->getSymbolReference()->getReferenceNumber()]=true;
      }

   return true;
   }

// TODO: Path retrieval is not implemented, but is trivial, making this O(N)
static int32_t getLongestPathOfDAG(TR::Node *entry, TR::Compilation *cm)
   {
   using namespace CS2;
   TR::StackMemoryRegion stackMemoryRegion(*cm->trMemory());

   QueueOf<ncount_t, TR::Allocator> queue(cm->allocator());
   HashTable<ncount_t, int32_t, TR::Allocator> longestPathLens(cm->allocator());
   longestPathLens.Add(entry->getNodePoolIndex(), 0);
   queue.Push(entry->getNodePoolIndex());
   int32_t maxLen = 0;
   while(!queue.IsEmpty())
      {
      auto nodePoolIndex = queue.Pop();
      auto node = cm->getNodePool().getNodeAtPoolIndex(nodePoolIndex);
      auto prevLongestPathLen = longestPathLens.Get(nodePoolIndex);
      if (node->getNumChildren() == 0)
         {
         if (prevLongestPathLen > maxLen)
            maxLen = prevLongestPathLen;
         }
      for(int i=0; i<node->getNumChildren(); i++)
         {
         auto childPoolIndex = node->getChild(i)->getNodePoolIndex();
         HashIndex hashIdx;
         if (!longestPathLens.Locate(childPoolIndex, hashIdx))
            longestPathLens.Add(childPoolIndex, 0, hashIdx);

         // if new longest path is found, update and push child
         if (prevLongestPathLen + 1 > longestPathLens[hashIdx])
            {
            longestPathLens[hashIdx] = prevLongestPathLen + 1;
            queue.Push(childPoolIndex);
            }
         }
      }

   return maxLen;
   }

static bool containsNode(TR::Node *containerNode, TR::Node *node, vcount_t visitCount, TR::Compilation *comp, int32_t *height, int32_t *maxHeight, bool &canMoveIfVolatile)
   {
   if (containerNode == node)
      return true;

   vcount_t oldVisitCount = containerNode->getVisitCount();
   if ((oldVisitCount == visitCount) || (oldVisitCount == comp->getVisitCount()))
      return false;
   containerNode->setVisitCount(comp->getVisitCount());

   if (containerNode->getOpCode().hasSymbolReference() &&
       (containerNode->getSymbol()->isShadow() || containerNode->getSymbol()->isStatic()))
      canMoveIfVolatile = false;

   (*height)++;
   if (*height > *maxHeight)
     *maxHeight = *height;
   for (int32_t i = 0; i < containerNode->getNumChildren(); ++i)
      {
      if (containsNode(containerNode->getChild(i), node, visitCount, comp, height, maxHeight, canMoveIfVolatile))
         return true;
      }
   (*height)--;

   return false;
   }

#define MAX_ALLOWED_HEIGHT 50

static bool isSafeToReplaceNode(TR::Node *currentNode, TR::TreeTop *curTreeTop, bool *seenConditionalBranch,
      vcount_t visitCount, TR::Compilation *comp, List<OMR::TreeInfo> *targetTrees, bool &cannotBeEliminated)
   {
   LexicalTimer tx("safeToReplace", comp->phaseTimer());

   TR::SparseBitVector symbolReferencesInNode(comp->allocator());

   // Collect all symbols that could be killed between here and the next reference
   //
   comp->incVisitCount();
   //////vcount_t visitCount = comp->getVisitCount();
   int32_t numDeadSubNodes = 0;
   bool cantMoveUnderBranch = false;
   bool seenInternalPointer = false;
   bool seenArraylet = false;
   int32_t curMaxHeight = getLongestPathOfDAG(currentNode, comp);
   collectSymbolReferencesInNode(currentNode, symbolReferencesInNode, &numDeadSubNodes, visitCount, comp,
         &seenInternalPointer, &seenArraylet, &cantMoveUnderBranch);

   bool registersScarce = comp->cg()->areAssignableGPRsScarce();
#ifdef J9_PROJECT_SPECIFIC
   bool isBCD = currentNode->getType().isBCD();
#endif

   if (numDeadSubNodes > 1 &&
#ifdef J9_PROJECT_SPECIFIC
       !isBCD &&
#endif
       registersScarce)
      {
      return false;
      }

   OMR::TreeInfo *curTreeInfo = findOrCreateTreeInfo(curTreeTop, targetTrees, comp);
   int32_t curHeight = curTreeInfo->getHeight()+curMaxHeight;
   if (curHeight > MAX_ALLOWED_HEIGHT)
      {
      cannotBeEliminated = true;
      return false;
      }

   // TEMPORARY
   // Don't allow removal of a node containing an unresolved reference if
   // the gcOnResolve option is set
   //
   bool isUnresolvedReference = currentNode->hasUnresolvedSymbolReference();
   if (isUnresolvedReference)
      return false;

   bool mayBeVolatileReference = currentNode->mightHaveVolatileSymbolReference();
   //if (mayBeVolatileReference)
   //   return false;

   // Now scan forwards through the trees looking for the next use and checking
   // to see if any symbols in the subtree are getting modified; if so it is not
   // safe to replace the node at its next use.
   //

   comp->incVisitCount();
   for (TR::TreeTop *treeTop = curTreeTop->getNextTreeTop(); treeTop; treeTop = treeTop->getNextTreeTop())

      {
      TR::Node *node = treeTop->getNode();
      if(node->getOpCodeValue() == TR::treetop)
          node = node->getFirstChild();

      if (node->getOpCodeValue() == TR::BBStart &&
          !node->getBlock()->isExtensionOfPreviousBlock())
         return true;

      if (cantMoveUnderBranch && (node->getOpCode().isBranch()
         || node->getOpCode().isJumpWithMultipleTargets()))
         return false;

      if (node->canGCandReturn() &&
          seenInternalPointer)
         return false;

      int32_t tempHeight = 0;
      int32_t maxHeight = 0;
      bool canMoveIfVolatile = true;
      if (containsNode(node, currentNode, visitCount, comp, &tempHeight, &maxHeight, canMoveIfVolatile))
         {
         // TEMPORARY
         // Disable moving an unresolved reference down to the middle of a
         // JNI call, until the resolve helper is fixed properly
         //
         if (isUnresolvedReference && node->getFirstChild()->getOpCode().isCall() &&
             node->getFirstChild()->getSymbol()->castToMethodSymbol()->isJNI())
            return false;

         if (curTreeInfo)
            {
            OMR::TreeInfo *treeInfo = findOrCreateTreeInfo(treeTop, targetTrees, comp);
            int32_t height = treeInfo->getHeight();
            int32_t maxHeightUsed = maxHeight;
            if (maxHeightUsed < curMaxHeight)
               maxHeightUsed = curMaxHeight;

            if (height < curTreeInfo->getHeight())
               height = curTreeInfo->getHeight();
            height++;
            if ((height+maxHeightUsed) > MAX_ALLOWED_HEIGHT)
               {
               cannotBeEliminated = true;
               return false;
               }
            treeInfo->setHeight(height);
            }

         return true;
         }

      if (mayBeVolatileReference && !canMoveIfVolatile)
         return false;

      if ((node->getOpCode().isBranch() &&
           (node->getOpCodeValue() != TR::Goto)) ||
           (node->getOpCode().isJumpWithMultipleTargets() && node->getOpCode().hasBranchChildren()))
        *seenConditionalBranch = true;

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCode().isNullCheck() ||
          node->getOpCode().isResolveCheck() ||
          node->getOpCodeValue() == TR::ArrayStoreCHK ||
          node->getOpCode().isSpineCheck())
         {
         node = node->getFirstChild();
         }

      if (node->getOpCode().isStore())
         {
         // For a store, just the single symbol reference is killed.
         // Resolution of the store symbol is handled by TR::ResolveCHK
         //
         if (symbolReferencesInNode.ValueAt(node->getSymbolReference()->getReferenceNumber()))
            return false;
         }

      // Node Aliasing Changes
      // Check if the definition modifies any symbol in the subtree
      //
      if (node->mayKill(true).containsAny(symbolReferencesInNode, comp))
        return false;
      }
   return true;
   }

static void removeGlRegDep(TR::Node * parent, TR_GlobalRegisterNumber registerNum, TR::Block *containingBlock, TR::Optimization *opt)
   {
   if (parent->getNumChildren() == 0)
      return;

   TR_ASSERT(parent->getNumChildren() > 0, "expected TR::GlRegDeps %p", parent);
   TR::Node * predGlRegDeps = parent->getLastChild();

   if (predGlRegDeps->getOpCodeValue() != TR::GlRegDeps) // could be already removed
      return;

   TR_ASSERT(predGlRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");

   for (int32_t i = predGlRegDeps->getNumChildren() - 1; i >= 0; --i)
      if (predGlRegDeps->getChild(i)->getGlobalRegisterNumber() == registerNum)
         {
         dumpOptDetails(opt->comp(), "%sRemove GlRegDep : %p\n", opt->optDetailString(), predGlRegDeps->getChild(i));
         TR::Node *removedChild = predGlRegDeps->removeChild(i);
         if (removedChild->getReferenceCount() <= 1)
            {
            // The only remaining parent is the RegStore.  Another pass of
            // deadTrees may be able to eliminate that.
            //
            opt->requestOpt(OMR::deadTreesElimination, true, containingBlock);
            }
         break;
         }

   if (predGlRegDeps->getNumChildren() == 0)
      parent->removeLastChild();
   }

// Note: the future use counts are incremented with visit counts but are decremented here without
// using visit counts so they cannot be trusted for any functional purpose but only to guide heuristics.
static scount_t recursivelyDecFutureUseCount(TR::Node *node)
   {
   if (node->getFutureUseCount() > 0)
      node->decFutureUseCount();

   if (node->getReferenceCount() == 0)
      {
      for (int32_t childCount = node->getNumChildren()-1; childCount >= 0; childCount--)
         recursivelyDecFutureUseCount(node->getChild(childCount));
      }

   return node->getFutureUseCount();
   }


// DeadTreesElimination class methods

TR::Optimization *OMR::DeadTreesElimination::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::DeadTreesElimination(manager);
   }


OMR::DeadTreesElimination::DeadTreesElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _targetTrees(manager->trMemory())
   {
   _cannotBeEliminated = false;
   _delayedRegStores = false;
   }

int32_t OMR::DeadTreesElimination::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 1;
   }


int32_t OMR::DeadTreesElimination::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
   return 0;
   }

void OMR::DeadTreesElimination::prePerformOnBlocks()
   {
   _cannotBeEliminated = false;
   _delayedRegStores = false;

   _targetTrees.deleteAll();

   // Walk through all the blocks to remove trivial dead trees of the form
   // treetop
   //   => node
   // The problem with these trees is in the scenario where the earlier use
   // of 'node' is also dead.  However, our analysis won't find that because
   // the reference count is > 1.
   vcount_t visitCount = comp()->incOrResetVisitCount();
   for (TR::TreeTop *tt = comp()->getStartTree();
        tt != 0;
        tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop &&
          node->getFirstChild()->getVisitCount() == visitCount &&
          performTransformation(comp(), "%sRemove trivial dead tree: %p\n", optDetailString(), node))
         comp()->removeTree(tt);
      else
         {
         if (node->getOpCode().isCheck() &&
             node->getFirstChild()->getOpCode().isCall() &&
             node->getFirstChild()->getReferenceCount() == 1 &&
             node->getFirstChild()->getSymbolReference()->getSymbol()->isResolvedMethod() &&
             node->getFirstChild()->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->isSideEffectFree() &&
             performTransformation(comp(), "%sRemove dead check of side-effect free call: %p\n", optDetailString(), node))
            comp()->removeTree(tt);
         }

      if (node->getVisitCount() >= visitCount)
         continue;
      TR::TransformUtil::recursivelySetNodeVisitCount(tt->getNode(), visitCount);
      }

   // If the last use of an iRegLoad has been removed, then remove the node from
   // the BBStart and remove the corresponding dependency node from each of the block's
   // predecessors.
   //
   while (1)
      {
      bool glRegDepRemoved = false;
      for (TR::Block * b = comp()->getStartBlock(); b; b = b->getNextBlock())
         {
         TR::TreeTop * startTT = b->getEntry();
         TR::Node * startNode = startTT->getNode();
         if (startNode->getNumChildren() > 0 && !debug("disableEliminationOfGlRegDeps"))
            {
            TR::Node * glRegDeps = startNode->getFirstChild();
            TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
            for (int32_t i = glRegDeps->getNumChildren() - 1; i >= 0; --i)
               {
               TR::Node * dep = glRegDeps->getChild(i);
               if (dep->getReferenceCount() == 1 &&
                   (!dep->getOpCode().isFloatingPoint() ||
                    cg()->getSupportsJavaFloatSemantics()) &&
                   performTransformation(comp(), "%sRemove GlRegDep : %p\n", optDetailString(), glRegDeps->getChild(i)))

                  {
                  glRegDeps->removeChild(i);
                  glRegDepRemoved = true;
                  TR_GlobalRegisterNumber registerNum = dep->getGlobalRegisterNumber();
                  for (auto e = b->getPredecessors().begin(); e != b->getPredecessors().end(); ++e)
                     {
                     TR::Block * pred = toBlock((*e)->getFrom());
                     if (pred == comp()->getFlowGraph()->getStart())
                        continue;

                     TR::Node * parent = pred->getLastRealTreeTop()->getNode();
                     if ( parent->getOpCode().isJumpWithMultipleTargets() && parent->getOpCode().hasBranchChildren())
                        {
                        for (int32_t j = parent->getCaseIndexUpperBound() - 1; j > 0; --j)
                           {
                           TR::Node * caseNode = parent->getChild(j);
                           TR_ASSERT(caseNode->getOpCode().isCase() || caseNode->getOpCodeValue() == TR::branch,
                                  "having problems navigating a switch");
                           if (caseNode->getBranchDestination() == startTT &&
                               caseNode->getNumChildren() > 0 &&
                               0) // can't do this now that all glRegDeps are hung off the default branch
                              removeGlRegDep(caseNode, registerNum, pred, this);
                           }
                        }
                     else if (!parent->getOpCode().isReturn() &&
                              parent->getOpCodeValue() != TR::igoto &&
                              !( parent->getOpCode().isJumpWithMultipleTargets() && parent->getOpCode().hasBranchChildren()) &&
                              !(parent->getOpCodeValue()==TR::treetop &&
                              parent->getFirstChild()->getOpCode().isCall() &&
                              parent->getFirstChild()->getOpCode().isIndirect()))

                        {
                        if (pred->getNextBlock() == b)
                           parent = pred->getExit()->getNode();
                        removeGlRegDep(parent, registerNum, pred, this);
                        }
                     }
                  }
               }

            if (glRegDeps->getNumChildren() == 0)
               startNode->removeChild(0);
            }
         }

      if (!glRegDepRemoved)
         break;
      }
   }

int32_t OMR::DeadTreesElimination::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::TreeTop *treeTop;
   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextTreeTop())
      treeTop->getNode()->initializeFutureUseCounts(visitCount);

   TR::Block *block = NULL;
   bool delayedRegStoresBeforeThisPass = _delayedRegStores;

   // Update visitCount as they are used in this optimization and need to be
   visitCount = comp()->incOrResetVisitCount();
   //TR_ScratchList<TR::Node> seenNodes(trMemory());
   TR::SparseBitVector seenNodes(comp()->allocator());
   for (TR::TreeTopIterator iter(startTree, this); iter != endTree; ++iter)
      {
      // temporarily revert this fix
      //vcount_t compVisitCount = comp()->getVisitCount();
      TR::Node *node = iter.currentTree()->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         block = node->getBlock();

      if (comp()->getVisitCount() > (MAX_VCOUNT - 3))
         return 0;

      // correct at all intermediate stages
      //
      if ((node->getOpCodeValue() != TR::treetop) &&
          (!node->getOpCode().isAnchor() || (node->getFirstChild()->getReferenceCount() != 1)) &&
          (!node->getOpCode().isStoreReg() || (node->getFirstChild()->getReferenceCount() != 1)) &&
          (delayedRegStoresBeforeThisPass ||
           (iter.currentTree() == block->getLastRealTreeTop()) ||
           !node->getOpCode().isStoreReg() ||
           (node->getVisitCount() == visitCount)))
         {
         TR::TransformUtil::recursivelySetNodeVisitCount(node, visitCount);
         continue;
         }

      if (node->getOpCode().isStoreReg())
         _delayedRegStores = true;

      TR::Node *child = node->getFirstChild();
      if (child->getOpCodeValue() == TR::PassThrough)
         {
         TR::Node *newChild = child->getFirstChild();
         node->setAndIncChild(0, newChild);
         newChild->incFutureUseCount();
         if (child->getReferenceCount() <= 1)
            optimizer()->prepareForNodeRemoval(child);
         child->recursivelyDecReferenceCount();
         recursivelyDecFutureUseCount(child);
         child = newChild;
         }

      bool treeTopCanBeEliminated = false;

      // If the treetop child has been seen before then it must be anchored
      // somewhere above already; so we don't need the treetop to be anchoring
      // this node (as the computation is already done at the first reference to
      // the node).
      //

      if (visitCount == child->getVisitCount())
         {
         treeTopCanBeEliminated = true;
         }
      else
         {
         TR::ILOpCode &childOpCode = child->getOpCode();
         TR::ILOpCodes opCodeValue = childOpCode.getOpCodeValue();
         bool seenConditionalBranch = false;

         bool callWithNoSideEffects = child->getOpCode().isCall() &&
              child->getSymbolReference()->getSymbol()->isResolvedMethod() &&
              child->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->isSideEffectFree();

         if (callWithNoSideEffects)
            {
            treeTopCanBeEliminated = true;
            }
         else if (!((childOpCode.isCall() && !callWithNoSideEffects) ||
               childOpCode.isStore() ||
               ((opCodeValue == TR::New ||
                 opCodeValue == TR::anewarray ||
                 opCodeValue == TR::newarray) &&
                 child->getReferenceCount() > 1) ||
                 opCodeValue == TR::multianewarray ||
                 opCodeValue == TR::MergeNew ||
               opCodeValue == TR::checkcast ||
               opCodeValue == TR::Prefetch ||
               opCodeValue == TR::iu2l ||
               ((childOpCode.isDiv() ||
                 childOpCode.isRem()) &&
                 child->getNumChildren() == 3)))
            {
            // Perform the rather complex check to see whether its safe
            // to disconnect the child node from the treetop
            //
            bool safeToReplaceNode = false;
            if (child->getReferenceCount() == 1)
               {
               safeToReplaceNode = true;
#ifdef J9_PROJECT_SPECIFIC
               if (child->getOpCode().isPackedExponentiation())
                  {
                  // pdexp has a possible message side effect in truncating or no significant digits left cases
                  safeToReplaceNode = false;
                  }
#endif
               if (opCodeValue == TR::loadaddr)
                  treeTopCanBeEliminated = true;
               }
            else if (!_cannotBeEliminated)
               safeToReplaceNode = isSafeToReplaceNode(child, iter.currentTree(), &seenConditionalBranch, visitCount, comp(), &_targetTrees, _cannotBeEliminated);

            if (safeToReplaceNode)
               {
               if (childOpCode.hasSymbolReference())
                  {
                  TR::SymbolReference *symRef = child->getSymbolReference();

                  if (symRef->getSymbol()->isAuto() || symRef->getSymbol()->isParm())
                     treeTopCanBeEliminated = true;
                  else
                     {
                     if (childOpCode.isLoad() ||
                         (opCodeValue == TR::loadaddr) ||
                         (opCodeValue == TR::instanceof) ||
                         (((opCodeValue == TR::New)  ||
                            (opCodeValue == TR::anewarray ||
                              opCodeValue == TR::newarray)) &&
                          ///child->getFirstChild()->isNonNegative()))
                           child->markedAllocationCanBeRemoved()))
                       //        opCodeValue == TR::multianewarray ||
                       //        opCodeValue == TR::MergeNew)
                        treeTopCanBeEliminated = true;
                     }
                  }
               else
                  treeTopCanBeEliminated = true;
               }
            }

         // Fix for the case when a float to non-float conversion node swings
         // down past a branch on IA32; this would cause a FP value to be commoned
         // across a branch where there was none originally; this causes pblms
         // as a value is left on the stack.
         //
         if (treeTopCanBeEliminated &&
             seenConditionalBranch)
            {
            if (!cg()->getSupportsJavaFloatSemantics())
               {
               if (child->getOpCode().isConversion() ||
                   child->getOpCode().isBooleanCompare())
                 {
                 if (child->getFirstChild()->getOpCode().isFloatingPoint() &&
                     !child->getOpCode().isFloatingPoint())
                     treeTopCanBeEliminated = false;
                 }
               }
            }

         if (treeTopCanBeEliminated)
            {
            seenNodes.Clear();
            bool containsFloatingPoint = false;
            for (int32_t i = 0; i < child->getNumChildren(); ++i)
               {
               // Anchor nodes with reference count > 1
               //
               bool highGlobalIndex = false;
               // temporarily revert this fix
               //if (fixUpTree(child->getChild(i), iter.currentTree(), seenNodes, highGlobalIndex, comp(), compVisitCount))
               if (fixUpTree(child->getChild(i), iter.currentTree(), seenNodes, highGlobalIndex, comp()))
                  containsFloatingPoint = true;
               if (highGlobalIndex)
                  return 0;
               }

            if (seenConditionalBranch &&
                containsFloatingPoint)
               {
               if (!cg()->getSupportsJavaFloatSemantics())
                  treeTopCanBeEliminated = false;
               }
            }
         }

      // Update visitCount as they are used in this optimization and need to be
      // correct at all intermediate stages
      //
      if (!treeTopCanBeEliminated)
         TR::TransformUtil::recursivelySetNodeVisitCount(node, visitCount);

      if (treeTopCanBeEliminated)
         {
         TR::TreeTop *prevTree = iter.currentTree()->getPrevTreeTop();
         TR::TreeTop *nextTree = iter.currentTree()->getNextTreeTop();

         if (!node->getOpCode().isStoreReg() || (node->getFirstChild()->getReferenceCount() == 1))
            {
            // Actually going to remove the treetop now
            //
            if (performTransformation(comp(), "%sRemove tree : [" POINTER_PRINTF_FORMAT "] ([" POINTER_PRINTF_FORMAT "] = %s)\n", optDetailString(), node, node->getFirstChild(), node->getFirstChild()->getOpCode().getName()))
               {
               prevTree->join(nextTree);
               optimizer()->prepareForNodeRemoval(node);
               ///child->recursivelyDecReferenceCount();
               node->recursivelyDecReferenceCount();
               recursivelyDecFutureUseCount(child);
               iter.jumpTo(prevTree);
               if (child->getReferenceCount() == 1)
                  requestOpt(OMR::treeSimplification, true, block);
               }
            }
         else
            {
            if (performTransformation(comp(), "%sMove tree : [" POINTER_PRINTF_FORMAT "]([" POINTER_PRINTF_FORMAT "] = %s) to end of block\n", optDetailString(), node, node->getFirstChild(), node->getFirstChild()->getOpCode().getName()))
               {
               prevTree->join(nextTree);
               node->setVisitCount(visitCount);

               TR::TreeTop *lastTree = findLastTreetop(block, prevTree);
               TR::TreeTop *prevLastTree = lastTree->getPrevTreeTop();

               TR::TreeTop *cursorTreeTop = nextTree;
               while (cursorTreeTop != lastTree)
                  {
                  if (cursorTreeTop->getNode()->getOpCode().isStoreReg() &&
                      (cursorTreeTop->getNode()->getGlobalRegisterNumber() == iter.currentTree()->getNode()->getGlobalRegisterNumber()))
                     {
                     lastTree = cursorTreeTop;
                     prevLastTree = lastTree->getPrevTreeTop();
                     break;
                     }

                  cursorTreeTop = cursorTreeTop->getNextTreeTop();
                  }

               if (lastTree->getNode()->getOpCodeValue() == TR::BBStart)
                  {
                  prevLastTree = lastTree;
                  lastTree = block->getExit();
                  }

               TR::Node *lastNode = lastTree->getNode();
               TR::Node *prevLastNode = prevLastTree->getNode();

               if (lastNode->getOpCode().isIf() && !lastNode->getOpCode().isCompBranchOnly() &&
                   prevLastNode->getOpCode().isStoreReg() &&
                   ((prevLastNode->getFirstChild() == lastNode->getFirstChild()) ||
                    (prevLastNode->getFirstChild() == lastNode->getSecondChild())))
                  {
                  lastTree = prevLastTree;
                  prevLastTree = lastTree->getPrevTreeTop();
                  }

               prevLastTree->join(iter.currentTree());
               iter.currentTree()->join(lastTree);

               iter.jumpTo(prevTree);
               requestOpt(OMR::treeSimplification, true, block);
               }
            }
         }
      }

   return 1; // actual cost
   }

TR::TreeTop *OMR::DeadTreesElimination::findLastTreetop(TR::Block *block, TR::TreeTop *prevTree)
   {
   return block->getLastRealTreeTop();
   }


