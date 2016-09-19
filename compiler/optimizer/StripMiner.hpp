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

#include "optimizer/LoopCanonicalizer.hpp"

#include <stdint.h>                           // for int32_t
#include "env/jittypes.h"                     // for intptrj_t
#include "il/ILOpCodes.hpp"                   // for ILOpCodes
#include "il/Node.hpp"                        // for Node, vcount_t
#include "infra/List.hpp"                     // for TR_ScratchList
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_ParentOfChildNode;
class TR_PrimaryInductionVariable;
class TR_RegionStructure;
class TR_Structure;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class Optimization; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

#define DEFAULT_STRIP_LENGTH 1024

class TR_StripMiner : public TR_LoopTransformer
   {
   public:
   TR_StripMiner(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_StripMiner(manager);
      }

   virtual bool    shouldPerform();
   virtual int32_t perform();

   private:
   /* types */
   typedef enum
      {
      unknownLoop,
      preLoop,
      mainLoop,
      postLoop,
      residualLoop,
      offsetLoop
      } TR_ClonedLoopType;

   /* data */
   struct LoopInfo
      {
      TR_RegionStructure *_region;
      intptrj_t _regionNum;
      intptrj_t _arrayDataSize;
      bool _increasing;
      bool _branchToExit;
      bool _canMoveAsyncCheck;
      bool _needOffsetLoop;
      intptrj_t _preOffset;
      intptrj_t _postOffset;
      intptrj_t _offset;
      intptrj_t _stripLen;

      TR::Block *_preHeader;
      TR::Block *_loopTest;
      TR_PrimaryInductionVariable *_piv;
      TR::TreeTop *_asyncTree;

      TR_ScratchList<TR_ParentOfChildNode> _mainParentsOfLoads;
      TR_ScratchList<TR_ParentOfChildNode> _mainParentsOfStores;
      TR_ScratchList<TR_ParentOfChildNode> _residualParentsOfLoads;
      TR_ScratchList<TR_ParentOfChildNode> _residualParentsOfStores;

      TR_ScratchList<TR::CFGEdge> _edgesToRemove;
      };

   intptrj_t _nodesInCFG;
   TR::TreeTop *_endTree;

   TR_ScratchList<LoopInfo> _loopInfos;
   TR::Block **_origBlockMapper;
   TR::Block **_mainBlockMapper;
   TR::Block **_preBlockMapper;
   TR::Block **_postBlockMapper;
   TR::Block **_residualBlockMapper;
   TR::Block **_offsetBlockMapper;

   /* methods */
   void collectLoops(TR_Structure *str);
   void transformLoops();

   void duplicateLoop(LoopInfo *li, TR_ClonedLoopType type);
   void transformLoop(LoopInfo *li);
   TR::Block *stripMineLoop(LoopInfo *li, TR::Block *outerHeader);

   TR::Block *createGotoBlock(TR::Block *source, TR::Block *dest);
   void redirect(TR::Block *source, TR::Block *oldDest, TR::Block *newDest);
   TR::Block *createLoopTest(LoopInfo *li, TR_ClonedLoopType type);
   TR::Block *createStartOffsetLoop(LoopInfo *li, TR::Block *outerHeader);

   void examineLoop(LoopInfo *li, TR_ClonedLoopType type, bool checkClone = true);
   void examineNode(LoopInfo *li, TR::Node *parent, TR::Node *node, TR::SymbolReference *oldSymRef,
                    vcount_t visitCount, TR_ClonedLoopType type, bool checkClone, int32_t childNum);
   void replaceLoopPivs(LoopInfo *li, TR::ILOpCodes newOpCode, TR::Node *newConst,
                        TR::SymbolReference *newSymRef, TR_ClonedLoopType type);

   TR::Block *getLoopPreHeader(TR_Structure *str);
   TR::Block *getLoopTest(TR_Structure *str, TR::Block *preHeader);
   bool checkIfIncrementalIncreasesOfPIV(LoopInfo *li);
   void findLeavesInList();
   bool findPivInSimpleForm(TR::Node *node,  TR::SymbolReference *pivSym);
   };
