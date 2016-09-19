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

#ifndef LCA_INCL
#define LCA_INCL

#include <stdint.h>                           // for int32_t, int64_t, etc
#include <string.h>                           // for NULL, memset
#include "compile/Compilation.hpp"            // for Compilation
#include "compile/SymbolReferenceTable.hpp"   // for SymbolReferenceTable
#include "cs2/bitvectr.h"                     // for ABitVector
#include "env/TRMemory.hpp"                   // for BitVector, TR_Memory, etc
#include "il/Node.hpp"                        // for vcount_t
#include "infra/Assert.hpp"                   // for TR_ASSERT
#include "infra/BitVector.hpp"                // for TR_BitVector, etc
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_LoopCanonicalizer;
class TR_LoopPredictor;
class TR_LoopReducer;
class TR_LoopVersioner;
class TR_RegionStructure;
class TR_Structure;
class TR_StructureSubGraphNode;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class NodeChecklist; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

class TR_NodeParentPair
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentPair(TR::Node *node, TR::Node *parent)
      : _node(node), _parent(parent)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   };

class TR_LoopTransformer : public TR::Optimization
   {
   public:
   TR_LoopTransformer(TR::OptimizationManager *manager)
      : TR::Optimization(manager),
        _invariantBlocks(trMemory()),
        _blocksToBeCleansed(trMemory()),
        _analysisStack(trMemory(), 8, false, stackAlloc),
        _writtenAndNotJustForHeapification(NULL),
        _writtenExactlyOnce(comp()->allocator("LoopTransformer")),
        _readExactlyOnce(comp()->allocator("LoopTransformer")),
        _allKilledSymRefs(comp()->allocator("LoopTransformer")),
        _autosAccessed(NULL)
      {
      _doingVersioning = false;
      _nodesInCycle = NULL;
      _indirectInductionVariable = false;
      }

   void initializeSymbolsWrittenAndReadExactlyOnce(int32_t symRefCount, TR_BitVectorGrowable growableOrNot)
      {
      _storeTrees = (TR::TreeTop **)trMemory()->allocateStackMemory(symRefCount*sizeof(TR::TreeTop *));
      memset(_storeTrees, 0, symRefCount*sizeof(TR::TreeTop *));

      _cannotBeEliminated = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);

      _neverRead = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);
      _neverWritten = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc, growableOrNot);

      TR::BitVector tmp(comp()->allocator("LoopTransformer"));
      comp()->getSymRefTab()->getAllSymRefs(tmp);

      *_neverWritten = tmp;
      *_neverRead = tmp;

      _writtenExactlyOnce.Clear();
      _readExactlyOnce.Clear();
      _allKilledSymRefs.Clear();
      }

   virtual int32_t perform(){return 0;}
   virtual TR_LoopCanonicalizer *asLoopCanonicalizer() {return NULL;}
   virtual TR_LoopReducer *asLoopReducer() { return NULL; }
   virtual TR_LoopVersioner *asLoopVersioner() {return NULL;}
   virtual TR_LoopPredictor *asLoopPredictor() {return NULL;}
   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t){return 0;}
   virtual bool isStoreInRequiredForm(int32_t, TR_Structure *);
   virtual int32_t checkLoopForPredictability(TR_Structure *, TR::Block *, TR::Node **, bool returnIfNotPredictable = true);
   virtual int32_t getInductionSymbolReference(TR::Node *);
   virtual void updateStoreInfo(int32_t i, TR::TreeTop *tree) { _storeTrees[i] = tree; }
   virtual void checkIfIncrementInDifferentExtendedBlock(TR::Block *block, int32_t inductionVariable);
   virtual TR::Node *updateLoadUsedInLoopIncrement(TR::Node *node, int32_t);


   protected:
   void createWhileLoopsList(TR_ScratchList<TR_Structure>* whileLoops);

   void detectWhileLoops(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, bool innerFirst);
   void detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, TR_StructureSubGraphNode *rootNode, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst);
   void detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst);

   bool blockIsAlwaysExecutedInLoop(TR::Block *block, TR_RegionStructure *loopStructure, bool *atEntry = NULL);

   TR::Block * createNewEmptyBlock();
   TR::Node* createNewGotoNode();
   void printTrees();
   void adjustTreesInBlock(TR::Block *);
   TR::Node *duplicateExact(TR::Node *, List<TR::Node> *, List<TR::Node> *);
   bool cleanseTrees(TR::Block *);
   bool makeInvariantBlockFallThroughIfPossible(TR::Block *);

   void collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *, vcount_t);

 private:
   struct updateInfo_tables {
     TR::BitVector seenLoads, seenMultipleLoads, seenStores, seenMultipleStores, currentlyWrittenOnce, currentlyReadOnce;
     updateInfo_tables(TR::Allocator a) : seenLoads(a), seenMultipleLoads(a),
                                               seenStores(a), seenMultipleStores(a),
                                               currentlyWrittenOnce(a), currentlyReadOnce(a) {}
   };
   void updateInfo(TR::Node *, vcount_t, updateInfo_tables &);
   void collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *, vcount_t, updateInfo_tables &);

 public:
   TR::Node *getCorrectNumberOfIterations(TR::Node *, TR::Node *);
   TR::Node *containsOnlyInductionVariableAndAdditiveConstant(TR::Node *, int32_t);
   bool isSymbolReferenceWrittenNumberOfTimesInStructure(TR_Structure *, int32_t, int32_t *, int32_t);
     //bool isSymbolReferenceReadNumberOfTimesInStructure(TR_Structure *, int32_t, int32_t *, int32_t);
   bool detectEmptyLoop(TR_Structure *, int32_t *);
   bool findMatchingIVInRegion(TR::TreeTop*, TR_RegionStructure*);

   virtual bool replaceAllInductionVariableComputations(TR::Block *loopInvariantBlock, TR_Structure *, TR::SymbolReference **, TR::SymbolReference *);
   virtual bool examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *, int32_t, TR::Node *, vcount_t, TR::SymbolReference **)
      {
      TR_ASSERT(0, "Should be overridden in the subclass that makes use of this function\n");
      return true;
      }

   typedef enum
      {
      transformerNoReadOrWrite,
      transformerReadFirst,
      transformerWrittenFirst
      } TR_TransformerDefUseState;

   TR_TransformerDefUseState getSymbolDefUseStateInSubTree(TR::Node *node, TR::RegisterMappedSymbol* indVarSym);
   TR_TransformerDefUseState getSymbolDefUseStateInBlock(TR::Block *block, TR::RegisterMappedSymbol* indVarSym);

   TR::Block *_loopTestBlock;
   TR::TreeTop **_storeTrees;
   TR::TreeTop *_currTree, *_insertionTreeTop, *_loopTestTree, *_asyncCheckTree;
   TR_BitVector *_cannotBeEliminated, *_writtenAndNotJustForHeapification, *_autosAccessed;
   TR_BitVector *_neverRead, *_neverWritten;
   TR::SparseBitVector _writtenExactlyOnce;
   TR::SparseBitVector _readExactlyOnce;
   TR::SparseBitVector _allKilledSymRefs;

   TR::Node *_numberOfIterations, *_constNode, *_loadUsedInLoopIncrement;

   TR::TreeTop    *_startOfHeader;
   TR::CFG        *_cfg;
   TR_Structure  *_rootStructure;

   List<TR::Block> _invariantBlocks;
   List<TR::Block> _blocksToBeCleansed;
   TR_Stack<TR_StructureSubGraphNode*> _analysisStack;
   TR_BitVector *_nodesInCycle;
   TR_BitVector *_hasPredictableExits;

   int32_t        _whileIndex;
   vcount_t       _visitCount;
   int32_t        _topDfNum;
   int32_t        _counter;
   int32_t _loopDrivingInductionVar, _nextExpression, _startExpressionForThisInductionVariable, _numberOfTreesInLoop;

   bool _isAddition, _requiresAdditionalCheckForIncrement, _doingVersioning;
   bool _incrementInDifferentExtendedBlock;
   bool _indirectInductionVariable; // JIT Design 1347

   TR::SymbolReference        **_symRefUsedInLoopIncrement;
   };



class TR_LoopCanonicalizer : public TR_LoopTransformer
   {
   public:

   TR_LoopCanonicalizer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopCanonicalizer(manager);
      }

   virtual int32_t perform();
   virtual TR_LoopCanonicalizer *asLoopCanonicalizer() {return this;}

   protected:
   void eliminateRedundantInductionVariablesFromLoop(TR_RegionStructure *naturalLoop);

   private:
   void canonicalizeNaturalLoop(TR_RegionStructure *whileLoop);
   void canonicalizeDoWhileLoop(TR_RegionStructure *doWhileLoop);

   bool isLegalToSplitEdges(TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop);
   bool modifyBranchesForSplitEdges(TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop, TR::Block *loopInvariantBlock,
         TR::Block *targetBlock, bool addToEnd, int32_t *sumPredFreq, bool isCheckOnly = false);

   bool replaceInductionVariableComputationsInExits(TR_Structure *structure, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar);
   virtual bool examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *, int32_t, TR::Node *, vcount_t, TR::SymbolReference **);
   void placeInitializationTreeInLoopPreHeader(TR::Block *b, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar);
   bool incrementedInLockStep(TR_Structure *, TR::SymbolReference *, TR::SymbolReference *, int64_t derivedInductionVarIncrement, int64_t primaryInductionVarIncrement, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks);
   void findIncrements(TR::Node * currentNode, vcount_t visitCount, TR::SymbolReference *derivedInductionVar, TR::SymbolReference *primaryInductionVar, int64_t &derivedInductionVarIncrement, int64_t &primaryInductionVarIncrement, bool &unknownIncrement);
   bool checkIfOrderOfBlocksIsKnown(TR_RegionStructure *naturalLoop, TR::Block *entryBlock, TR::Block *loopTestBlock, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks, uint8_t &primaryFirst);
   bool checkComplexInductionVariableUse(TR_Structure *structure);
   bool checkComplexInductionVariableUseNode(TR::Node *node, bool inAddr);

   TR::SymbolReference *_symRefBeingReplaced;
   TR::SymbolReference *_primaryInductionVariable;
   TR::Node *_primaryInductionVarStoreInBlock;
   TR::Node *_derivedInductionVarStoreInBlock;
   TR::Node *_primaryInductionVarStoreSomewhereInBlock;
   uint8_t _primaryIncrementedFirst;
   TR::Block *_entryBlock;
   TR::Block *_loopTestBlock;
   TR::Block *_currentBlock;
   TR::Block *_primaryInductionIncrementBlock;
   TR::Block *_derivedInductionIncrementBlock;
   int64_t _primaryIncr;
   int64_t _derivedIncr;
   };


class TR_LoopInverter : public TR_LoopTransformer
   {
   public:

   TR_LoopInverter(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopInverter(manager);
      }

   virtual int32_t perform();

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);
   bool isInvertibleLoop(int32_t, TR_Structure *);
   bool checkIfSymbolIsReadInKnownTree(TR::Node *, int32_t, TR::TreeTop *, TR::NodeChecklist &);
   };

class TR_RedundantInductionVarElimination : public TR_LoopCanonicalizer
   {
   public:

   TR_RedundantInductionVarElimination(TR::OptimizationManager *manager)
       : TR_LoopCanonicalizer(manager)
       {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_RedundantInductionVarElimination(manager);
      }


   virtual int32_t perform();
   };

#endif
