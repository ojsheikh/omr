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

#ifndef ORDERBLOCKS_INCL
#define ORDERBLOCKS_INCL

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for int32_t
#include "env/TRMemory.hpp"                   // for List::operator new, etc
#include "il/Node.hpp"                        // for Node, vcount_t
#include "infra/List.hpp"                     // for List, ListElement, etc
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_RegionStructure;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class Compilation; }
namespace TR { class TreeTop; }


typedef List<TR::CFGNode> TR_BlockList;
typedef ListIterator<TR::CFGNode> TR_BlockListIterator;
typedef ListElement<TR::CFGNode> TR_BlockListElement;

class TR_BlockOrderingOptimization : public TR::Optimization
   {
   public:

   void dumpBlockOrdering(TR::TreeTop *tt, char *title=NULL); // This is called from other opts!

   protected:
   TR_BlockOrderingOptimization(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}

   void            connectTreesAccordingToOrder(TR_BlockList & newBlockOrder);
   TR::Block *      insertGotoFallThroughBlock(TR::TreeTop *fallThroughTT, TR::Node *node, TR::CFGNode *prevBlock, TR::CFGNode *origSucc, TR_RegionStructure *parent = NULL);
   };

/*
 * Class TR_OrderBlocks
 * ====================
 * 
 * Block ordering moves blocks around with the goal of making the hot path 
 * the fall through. The hot path is determined by profiling and/or static 
 * criteria like loop nesting depth, expected code synergy, etc.
 */

class TR_OrderBlocks : public TR_BlockOrderingOptimization
   {
   typedef TR_BlockOrderingOptimization Super;

   public:
   TR_OrderBlocks(TR::OptimizationManager *manager, bool beforeExtension = false);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_OrderBlocks(manager);
      }

   virtual bool    shouldPerform();
   virtual int32_t perform();

   void            doPeepHoleOptimizations(bool before, bool after)
                                             { if (before)
                                                  _doPeepHoleOptimizationsBefore = true;
                                               if (after)
                                                  _doPeepHoleOptimizationsAfter = true;
                                             }
   void            noPeepHoleOptimizations() { _doPeepHoleOptimizationsBefore = false; _doPeepHoleOptimizationsAfter = false; }
   void            reorderBlocks()           { _reorderBlocks = true; }
   void            dontReorderBlocks()       { _reorderBlocks = false; }
   void            extendBlocks()            { _extendBlocks = true; }
   void            dontExtendBlocks()        { _extendBlocks = false; }
   bool            lookForPeepHoleOpportunities(char *title);
   void            doReordering();

   void            invalidateStructure() { _needInvalidateStructure = true; }
   bool            needInvalidateStructure() {return _needInvalidateStructure;}

   private:
   bool            needBetterChoice( TR::CFG *cfg, TR::CFGNode  *block, TR::CFGNode  *prevBlock);
   bool            cannotFollowBlock(TR::Block *block, TR::Block *prevBlock);
   bool            mustFollowBlock(TR::Block *block, TR::Block *prevBlock);
   bool            hasOtherHotAsmFlowPredeccessors(TR::Block *block, TR::Block *prevBlock);
   bool            asmFlowHasNonFallThroughHotSuccessors(TR::Block *block);
   bool            safeToMoveAway(TR::Block *block, TR::Block *prevBlock);

   TR::CFGNode *    findSuitablePathInList(List<TR::CFGNode> & list, TR::CFGNode *prevBlock);
   TR::CFGNode *    findBestPath(TR::CFGNode *prevBlock);
   bool            endPathAtBlock(TR::CFGNode *block, TR::CFGNode *bestSucc, TR::CFG *cfg);
   bool            analyseForHazards(TR::CFGNode *block);
   bool            candidateIsBetterSuccessorThanBest(TR::CFGEdge *candidateEdge, TR::CFGEdge *currentEdge);
   TR::CFGNode *    chooseBestFallThroughSuccessor(TR::CFG *cfg, TR::CFGNode *block, int32_t &);
   void            addToOrderedBlockList(TR::CFGNode *block, TR_BlockList & list, bool useNumber);
   void            removeFromOrderedBlockLists(TR::CFGNode *block);
   void            addRemainingSuccessorsToList(TR::CFGNode *block, TR::CFGNode *excludeBlock);

   bool            peepHoleGotoToFollowing(TR::CFG *cfg, TR::Block *block, TR::Block *followingBlock, char *title);
   bool            peepHoleGotoToGoto(TR::CFG *cfg, TR::Block *block, TR::Node *gotoNode, TR::Block *destOfGoto, char *title, TR::BitVector &skippedGotoBlocks);
   bool            peepHoleGotoToEmpty(TR::CFG *cfg, TR::Block *block, TR::Node *gotoNode, TR::Block *destOfGoto, char *title);
   void            peepHoleGotoBlock(TR::CFG *cfg, TR::Block *block, char *title);
   void            removeRedundantBranch(TR::CFG *cfg, TR::Block *block, TR::Node *branchNode, TR::Block *takenBlock);
   void            peepHoleBranchBlock(TR::CFG *cfg, TR::Block *block, char *title);
   void            peepHoleBranchAroundSingleGoto(TR::CFG *cfg, TR::Block *block, char *title);
   bool            peepHoleBranchToFollowing(TR::CFG *cfg, TR::Block *block, TR::Block *followingBlock, char *title);
   bool            peepHoleBranchToLoopHeader(TR::CFG *cfg, TR::Block *block, TR::Block *fallThrough, TR::Block *dest, char *title);
   void            removeEmptyBlock(TR::CFG *cfg, TR::Block *block, char *title);
   bool            doPeepHoleBlockCorrections(TR::Block *block, char *title);
   void            addRemainingSuccessorsToListHWProfile(TR::CFGNode *block, TR::CFGNode *excludeBlock);
   void            insertBlocksToList();
   bool            hasValidCandidate(List<TR::CFGNode> & list, TR::CFGNode *prevBlock);
   bool            isCandidateReallyBetter(TR::CFGEdge *candEdge, TR::Compilation *comp);

   void            initialize();
   void            generateNewOrder(TR_BlockList & newBlockOrder);
   bool            doBlockExtension();

   // instance variables
   bool                 _doPeepHoleOptimizationsBefore;
   bool                 _doPeepHoleOptimizationsAfter;
   bool                 _donePeepholeGotoToLoopHeader;
   bool                 _reorderBlocks;
   bool                 _superColdBlockOnly;
   bool                 _extendBlocks;
   int32_t              _numUnschedHotBlocks;

   TR_BlockList         _hotPathList;
   TR_BlockList         _coldPathList;
   bool                 _changeBlockOrderBasedOnHWProfile;

   TR::CFG              *_cfg;
   vcount_t              _visitCount;

   //indicate if we want to invalid the structure at end
   //since we want to defer such invalidation until the
   //end of the blockordering
    bool                  _needInvalidateStructure;
   };

/*
 * Class TR_BlockShuffling
 * =======================
 *
 * Block shuffling changes the textual order in which the blocks/trees appear 
 * in the method without changing the CFG in any way. This is more of a RAS pass.
 */

class TR_BlockShuffling : public TR_BlockOrderingOptimization
   {
   typedef TR_BlockOrderingOptimization Super;

   public:
   TR_BlockShuffling(TR::OptimizationManager *manager)
       : TR_BlockOrderingOptimization(manager)
       {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_BlockShuffling(manager);
      }

   virtual int32_t perform();

   private:

   int32_t          _numBlocks;

   TR::Block **allocateBlockArray();
   void       traceBlocks(TR::Block **blocks);

   void swap(TR::Block **blocks, int32_t first, int32_t second)
      {
      if (first != second)
         {
         TR::Block *temp = blocks[first];
         blocks[first]  = blocks[second];
         blocks[second] = temp;
         }
      }

   void applyBlockOrder(TR::Block **blocks);

   // Shuffling techniques
   //
   void scramble (TR::Block **blocks); // No correlation with previous order
   void riffle   (TR::Block **blocks); // Break into two parts and interleave them randomly.  Blocks generally end up in the same order, but with cold interleaved with hot.
   void reverse  (TR::Block **blocks); // Backward
   };

#endif
