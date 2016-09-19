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

#ifndef LOOPVERSIONER_INCL
#define LOOPVERSIONER_INCL

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for int32_t
#include "compile/Compilation.hpp"            // for Compilation
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "il/ILOps.hpp"                       // for ILOpCode
#include "il/Node.hpp"                        // for Node, vcount_t
#include "il/Node_inlines.hpp"                // for Node::getFirstChild, etc
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                     // for TreeTop
#include "il/TreeTop_inlines.hpp"             // for TreeTop::getNode
#include "infra/BitVector.hpp"                // for TR_BitVector
#include "infra/Link.hpp"                     // for TR_Link, etc
#include "infra/List.hpp"                     // for List, etc
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager
#include "optimizer/LoopCanonicalizer.hpp"    // for TR_LoopTransformer

class TR_PostDominators;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class Optimization; }

class TR_NodeParentSymRef
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentSymRef(TR::Node *node, TR::Node *parent, TR::SymbolReference *symRef)
      : _node(node), _parent(parent), _symRef(symRef)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   TR::SymbolReference *_symRef;
   };

class TR_NodeParentSymRefWeightTuple
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentSymRefWeightTuple(TR::Node *node, TR::Node *parent, TR::SymbolReference *symRef, int32_t weight)
      : _node(node), _parent(parent), _symRef(symRef), _weight(weight)
      {
      }

   TR::Node *_node;
   TR::Node *_parent;
   TR::SymbolReference *_symRef;
   int32_t _weight;
   };


struct VirtualGuardPair
   {
   TR::Block *_hotGuardBlock;
   TR::Block *_coldGuardBlock;
   bool _isGuarded;
   TR::Block *_coldGuardLoopEntryBlock;
   bool _isInsideInnerLoop;
   };

struct VGLoopInfo
   {
   VGLoopInfo(): _count(0), _coldGuardLoopInvariantBlock(NULL), _loopTempSymRef(NULL), _targetNum(-1) {}
   int32_t _count;
   TR::Block *_coldGuardLoopInvariantBlock;
   TR::SymbolReference *_loopTempSymRef;
   int32_t _targetNum;
   };

struct VirtualGuardInfo : public TR_Link<VirtualGuardInfo>
   {
   VirtualGuardInfo(TR::Compilation *c)
      :_virtualGuardPairs(c->trMemory())
      {

      _virtualGuardPairs.deleteAll();
      _coldLoopEntryBlock = NULL;
      _coldLoopInvariantBlock = NULL;
      _loopEntry = NULL;
      }
   List<VirtualGuardPair> _virtualGuardPairs;
   TR::Block *_coldLoopEntryBlock;
   TR::Block *_coldLoopInvariantBlock;
   TR::Block *_loopEntry;
   };

 class TR_NodeParentBlockTuple
    {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer)

   TR_NodeParentBlockTuple(TR::Node *node, TR::Node *parent,TR::Block *block)
      : _node(node), _parent(parent),_block(block)
      {
      }

    TR::Node  *_node;
    TR::Node  *_parent;
    TR::Block *_block;
   };

struct LoopTemps : public TR_Link<LoopTemps>
   {
   LoopTemps(): _symRef(NULL), _maxValue(0) {}
   TR::SymbolReference *_symRef;
   int32_t _maxValue;
   };


class TR_LoopVersioner : public TR_LoopTransformer
   {
   public:

   TR_LoopVersioner(TR::OptimizationManager *manager, bool onlySpecialize = false, bool refineAliases = false);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopVersioner(manager);
      }

   virtual int32_t perform();
   virtual TR_LoopVersioner *asLoopVersioner() {return this;}

   virtual int32_t detectCanonicalizedPredictableLoops(TR_Structure *, TR_BitVector **, int32_t);
   virtual bool isStoreInRequiredForm(int32_t, TR_Structure *);

   protected:

   bool shouldOnlySpecializeLoops() { return _onlySpecializingLoops; }
   void setOnlySpecializeLoops(bool b) { _onlySpecializingLoops = b; }

   /* currently do not support wcode methods
   * or realtime since aliasing of arraylets are not handled
   */
   bool refineAliases() { return _refineLoopAliases && !comp()->generateArraylets(); }

   virtual bool processArrayAliasCandidates();

   virtual void collectArrayAliasCandidates(TR::Node *node,vcount_t visitCount);
   virtual void buildAliasRefinementComparisonTrees(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, TR_ScratchList<TR::Node> *, TR::Block *);
   virtual void initAdditionalDataStructures();
   virtual void refineArrayAliases(TR_RegionStructure *);

   int32_t performWithDominators();
   int32_t performWithoutDominators();

   bool isStoreInSpecialForm(int32_t, TR_Structure *);
   bool isConditionalTreeCandidateForElimination(TR::TreeTop * curTree) { return (!curTree->getNode()->getFirstChild()->getOpCode().isLoadConst() ||
                                                                                  !curTree->getNode()->getSecondChild()->getOpCode().isLoadConst()); };
   TR::Node *isDependentOnInductionVariable(TR::Node *, bool, bool &, TR::Node* &, TR::Node* &);
   TR::Node *isDependentOnInvariant(TR::Node *);
   bool boundCheckUsesUnchangedValue(TR::TreeTop *, TR::Node *, TR::SymbolReference *, TR_RegionStructure *);
   bool findStore(TR::TreeTop *start, TR::TreeTop *end, TR::Node *node, TR::SymbolReference *symRef, bool ignoreLoads = false, bool lastTimeThrough = false);
   TR::Node *findLoad(TR::Node *node, TR::SymbolReference *symRef, vcount_t origVisitCount);
   void versionNaturalLoop(TR_RegionStructure *, List<TR::Node> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR_NodeParentSymRef> *, List<TR_NodeParentSymRefWeightTuple> *, List<TR_Structure> *whileLoops, List<TR_Structure> *clonedInnerWhileLoops, bool skipVersioningAsynchk, SharedSparseBitVector &reverseBranchInLoops);

   TR::Node *findCallNodeInBlockForGuard(TR::Node *node);
   bool loopIsWorthVersioning(TR_RegionStructure *naturalLoop);

   bool detectInvariantChecks(List<TR::Node> *, List<TR::TreeTop> *);
   bool detectInvariantBoundChecks(List<TR::TreeTop> *);
   bool detectInvariantSpineChecks(List<TR::TreeTop> *);
   bool detectInvariantDivChecks(List<TR::TreeTop> *);
   bool detectInvariantIwrtbars(List<TR::TreeTop> *);
   bool detectInvariantTrees(List<TR::TreeTop> *, bool, bool *, SharedSparseBitVector &reverseBranchInLoops);
   bool detectInvariantNodes(List<TR_NodeParentSymRef> *invariantNodes, List<TR_NodeParentSymRefWeightTuple> *invariantTranslationNodes);
   bool detectInvariantSpecializedExprs(List<TR::Node> *);
   bool detectInvariantArrayStoreChecks(List<TR::TreeTop> *);
   bool isVersionableArrayAccess(TR::Node *);

   bool isExprInvariant(TR::Node *, bool ignoreHeapificationStore = false);          // ignoreHeapificationStore flags for both
   bool isExprInvariantRecursive(TR::Node *, bool ignoreHeapificationStore = false); // methods need to be in sync!

   bool isDependentOnAllocation(TR::Node *, int32_t);
   bool hasWrtbarBeenSeen(List<TR::TreeTop> *, TR::Node *);

   bool checkProfiledGuardSuitability(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *guardNode, TR::SymbolReference *callSymRef, TR::Compilation *comp);
   bool isBranchSuitableToVersion(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp);
   bool isBranchSuitableToDoLoopTransfer(TR_ScratchList<TR::Block> *loopBlocks, TR::Node *node, TR::Compilation *comp);

   bool detectChecksToBeEliminated(TR_RegionStructure *, List<TR::Node> *, List<TR::TreeTop> *, List<int32_t> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<int32_t> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR_NodeParentSymRef> *,List<TR_NodeParentSymRefWeightTuple> *, bool &);

   void buildNullCheckComparisonsTree(List<TR::Node> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   void buildBoundCheckComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *,List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *, bool);
   void buildSpineCheckComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   void buildDivCheckComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   void buildIwrtbarComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   void buildCheckCastComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   void buildConditionalTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *, SharedSparseBitVector &reverseBranchInLoops);
   void buildArrayStoreCheckComparisonsTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, TR::Block *);
   bool buildSpecializationTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR::Node> *, TR::Block *, TR::Block *, TR::SymbolReference **);
   bool buildLoopInvariantTree(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::Node> *, List<TR_NodeParentSymRef> *, List<TR_NodeParentSymRefWeightTuple> *, TR::Block *, TR::Block *);
   void convertSpecializedLongsToInts(TR::Node *, vcount_t, TR::SymbolReference **);
   void collectAllExpressionsToBeChecked(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, TR::Node *, List<TR::Node> *, TR::Block *, vcount_t);

   void updateDefinitionsAndCollectProfiledExprs(TR::Node *,TR::Node *, vcount_t, List<TR::Node> *, List<TR_NodeParentSymRef> *, List<TR_NodeParentSymRefWeightTuple> *, TR::Node *, bool, TR::Block *, int32_t);
   void findAndReplaceContigArrayLen(TR::Node *, TR::Node *, vcount_t);
   void performLoopTransfer();

   bool replaceInductionVariable(TR::Node *, TR::Node *, int, int, TR::Node *, int);

   void fixupVirtualGuardTargets(VirtualGuardInfo *vgInfo);
   TR::Block *createEmptyGoto(TR::Block *source, TR::Block *dest, TR::TreeTop *endTree);
   TR::Block *createClonedHeader(TR::Block *origHeader, TR::TreeTop **endTree);
   TR::Node *createSwitchNode(TR::Block *clonedHeader, TR::SymbolReference *tempSymRef, int32_t numCase);
   bool canPredictIters(TR_RegionStructure* whileLoop, const TR_ScratchList<TR::Block>& blocksInWhileLoop,
      bool& isIncreasing, TR::SymbolReference*& firstChildSymRef);
   bool isInverseConversions(TR::Node* node);

   void recordCurrentBlock(TR::Block *b) { _currentBlock = b; }
   TR::Block * getCurrentBlock(TR::Block *b) { return _currentBlock; }

   TR_BitVector *_seenDefinedSymbolReferences;
   TR_BitVector *_additionInfo;
   TR::Node *_nullCheckReference, *_conditionalTree, *_duplicateConditionalTree;
   TR_RegionStructure *_currentNaturalLoop;

   List<int32_t> _versionableInductionVariables, _specialVersionableInductionVariables, _derivedVersionableInductionVariables;
   ////List<VirtualGuardPair> _virtualGuardPairs;
   TR_LinkHead<VirtualGuardInfo> _virtualGuardInfo;

   bool _containsGuard, _containsUnguardedCall, _nonInlineGuardConditionalsWillNotBeEliminated;

   int32_t _counter, _origNodeCount, _origBlockCount;
   bool _onlySpecializingLoops;
   bool _inNullCheckReference, _containsCall, _neitherLoopCold, _loopConditionInvariant;
   bool _refineLoopAliases;
   bool _addressingTooComplicated;
   List<TR::Node> _guardedCalls;
   List<TR::Node> *_arrayAccesses;
   List<TR_NodeParentBlockTuple> *_arrayLoadCandidates;
   List<TR_NodeParentBlockTuple> *_arrayMemberLoadCandidates;
   List<TR::TreeTop> _checksInDupHeader;
   TR_BitVector *_unchangedValueUsedInBndCheck;
   TR::Block *    _currentBlock;
   TR_BitVector *_disqualifiedRefinementCandidates;
   TR_BitVector               _visitedNodes; //used to track nodes visited by isExprInvariant

   TR_PostDominators *_postDominators;
   bool _loopTransferDone;
   };

class TR_LoopSpecializer : public TR_LoopVersioner
   {
   public:

   TR_LoopSpecializer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopSpecializer(manager);
      }
   };


#endif
