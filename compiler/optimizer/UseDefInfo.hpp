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

#ifndef USEDEFINFO_INCL
#define USEDEFINFO_INCL

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for int32_t, uint32_t, intptr_t
#include "compile/Compilation.hpp"  // for Compilation
#include "cs2/arrayof.h"            // for ArrayOf
#include "cs2/bitvectr.h"           // for ABitVector
#include "cs2/cs2.h"                // for Pair
#include "cs2/llistof.h"            // for LinkedListOf
#include "cs2/sparsrbit.h"          // for ASparseBitVector
#include "env/TRMemory.hpp"         // for Allocator, SparseBitVector, etc
#include "il/Node.hpp"              // for Node, scount_t
#include "il/Symbol.hpp"            // for Symbol
#include "il/SymbolReference.hpp"   // for SymbolReference
#include "infra/Assert.hpp"         // for TR_ASSERT
#include "infra/TRlist.hpp"         // for TR::list

class TR_ReachingDefinitions;
class TR_ValueNumberInfo;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class ILOpCode; }
namespace TR { class Optimizer; }
namespace TR { class TreeTop; }

/**
 * Use/def information.
 *
 * Each tree node that can use or define a value is assigned an index value.
 * The initial index numbers are reserved for the initial definitions of
 * parameters and fields.
 *
 * The index values are partitioned into 3 parts:
 *    0-X     = nodes that can define values
 *    (X+1)-Y = nodes that can both define and use values
 *    (Y+1)-Z = nodes that can use value
 *
 * This means that there are a total of Z index values.
 * The bit vectors that hold use information are of size (Z-X).
 */
class TR_UseDefInfo : public TR::Allocatable<TR_UseDefInfo, TR::Allocator>
   {
   public:

   // Construct use def info for the current method's trees. This also assigns
   // use/def index values to the relevant nodes.
   //
#if defined(NEW_USEDEF_INFO_SPARSE_INTERFACE)
   typedef TR::SparseBitVector BitVector;
#else
   typedef TR::BitVector BitVector;
#endif

   /**
    * Data that can be freed when TR_UseDefInfo is no longer needed
    * Data that used to be stack allocated
    */
   class AuxiliaryData
      {
      private:
         AuxiliaryData(TR::Compilation *c) :
             _onceReadSymbols(c->allocator("UseDefAux"), BitVector(c->allocator("UseDefAux"))),
             _onceWrittenSymbols(c->allocator("UseDefAux"), BitVector(c->allocator("UseDefAux"))),
             _defsForSymbol(c->allocator("UseDefAux"), BitVector(c->allocator("UseDefAux"))),
             _symsKilledByMustKills(c->allocator("UseDefAux"), TR::SparseBitVector(c->allocator("UseDefAux"))),
             _neverReadSymbols(c->allocator("UseDefAux")),
             _neverReferencedSymbols(c->allocator("UseDefAux")),
             _neverWrittenSymbols(c->allocator("UseDefAux")),
             _volatileOrAliasedToVolatileSymbols(c->allocator("UseDefAux")),
             _onceWrittenSymbolsIndices(c->allocator("UseDefAux"), TR::SparseBitVector(c->allocator("UseDefAux"))),
             _onceReadSymbolsIndices(c->allocator("UseDefAux"), TR::SparseBitVector(c->allocator("UseDefAux"))),
             _nodeSideTableToSymRefNumMap(c->allocator("UseDefAux")),
             _symRefToSideTableIndexMap(c->allocator("UseDefAux")),
             _expandedAtoms(c->allocator("UseDefAux"), CS2::Pair<TR::Node *, TR::TreeTop *>(NULL, NULL)),
             _sideTableToUseDefMap(c->allocator("UseDefAux")),
             _numAliases(c->allocator("UseDefAux")),
             _nodesByGlobalIndex(c->allocator("UseDefAux")),
             _loadsBySymRefNum(c->allocator("UseDefAux")),
             _defsForOSR(c->allocator("UseDefAux"), TR_UseDefInfo::BitVector(c->allocator("UseDefAux")))
            {}

      CS2::ArrayOf<BitVector,TR::Allocator> _onceReadSymbols;
      CS2::ArrayOf<BitVector,TR::Allocator> _onceWrittenSymbols;
      // defsForSymbol are known definitions of the symbol
      CS2::ArrayOf<BitVector, TR::Allocator> _defsForSymbol;
      CS2::ArrayOf<TR::SparseBitVector, TR::Allocator> _symsKilledByMustKills;    // symbol sideTableIndex killed by function call due to mustDef
      TR::BitVector _neverReadSymbols;
      TR::BitVector _neverReferencedSymbols;
      TR::BitVector _neverWrittenSymbols;
      TR::BitVector _volatileOrAliasedToVolatileSymbols;
      CS2::ArrayOf<TR::SparseBitVector, TR::Allocator> _onceWrittenSymbolsIndices;
      CS2::ArrayOf<TR::SparseBitVector, TR::Allocator> _onceReadSymbolsIndices;

      CS2::ArrayOf<int32_t, TR::Allocator>             _nodeSideTableToSymRefNumMap;
      CS2::ArrayOf<uint32_t, TR::Allocator>            _symRefToSideTableIndexMap;
      CS2::ArrayOf<CS2::Pair<TR::Node *, TR::TreeTop *>, TR::Allocator> _expandedAtoms;    //TR::Node            **_expandedNodes;


      protected:
      CS2::ArrayOf<uint32_t, TR::Allocator> _sideTableToUseDefMap;
      private:
      CS2::ArrayOf<uint32_t, TR::Allocator> _numAliases;
      CS2::ArrayOf<TR::Node *, TR::Allocator> _nodesByGlobalIndex;
      CS2::ArrayOf<TR::Node *, TR::Allocator> _loadsBySymRefNum;

      protected:
      // used only in TR_OSRDefInfo - should extend AuxiliaryData really:
      CS2::ArrayOf<BitVector, TR::Allocator> _defsForOSR;

      friend class TR_UseDefInfo;
      friend class TR_ReachingDefinitions;
      friend class TR_OSRDefInfo;
      };


   TR_UseDefInfo(TR::Compilation *, TR::CFG * cfg, TR::Optimizer *,
         bool requiresGlobals = true, bool prefersGlobals = true, bool loadsShouldBeDefs = true, bool cannotOmitTrivialDefs = false,
         bool conversionRegsOnly = false, bool doCompletion = true);

   protected:
   void prepareUseDefInfo(bool requiresGlobals, bool prefersGlobals, bool cannotOmitTrivialDefs, bool conversionRegsOnly);
   void invalidateUseDefInfo();
   virtual bool performAnalysis(AuxiliaryData &aux);
   virtual void processReachingDefinition(void* vblockInfo, AuxiliaryData &aux);
   private:
   bool _runReachingDefinitions(TR_ReachingDefinitions &reachingDefinitions, AuxiliaryData &aux);

   protected:
   TR::Compilation *comp() { return _compilation; }
   TR::Optimizer *optimizer() { return _optimizer; }

   TR_Memory *    trMemory()      { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }
   TR_StackMemory trStackMemory() { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }
   TR_HeapMemory  trHeapMemory()  { TR_ASSERT(false, "trMemory is prohibited in TR_UseDefInfo. Use comp->allocator()"); return NULL; }

   bool trace()       {return _trace;}

   public:
   TR::Allocator allocator() { return comp()->allocator(); }
   bool infoIsValid() {return _isUseDefInfoValid;}
   TR::Node *getNode(int32_t index);
   TR::TreeTop *getTreeTop(int32_t index);
   void clearNode(int32_t index) { _useDefs[index] = TR_UseDef(); }
   bool getUsesFromDefIsZero(int32_t defIndex, bool loadAsDef = false);
   bool getUsesFromDef(BitVector &usesFromDef, int32_t defIndex, bool loadAsDef = false);

   private:
   const BitVector &getUsesFromDef_ref(int32_t defIndex, bool loadAsDef = false);
   public:

   bool getUseDefIsZero(int32_t useIndex);
   bool getUseDef(BitVector &useDef, int32_t useIndex);
   bool getUseDef_noExpansion(BitVector &useDef, int32_t useIndex);
   private:
   const BitVector &getUseDef_ref(int32_t useIndex, BitVector *defs = NULL);
   const BitVector &getUseDef_ref_body(int32_t useIndex, TR_UseDefInfo::BitVector &visitedDefs, TR_UseDefInfo::BitVector *defs = NULL);
   public:

   void          setUseDef(int32_t useIndex, int32_t defIndex);
   void          resetUseDef(int32_t useIndex, int32_t defIndex);
   void          clearUseDef(int32_t useIndex);
   private:
   bool          isLoadAddrUse(TR::Node * node);

   public:
   bool getDefiningLoads(BitVector &definingLoads, TR::Node *node);
   private:
   const BitVector &getDefiningLoads_ref(TR::Node *node);
   public:

   TR::Node      *getSingleDefiningLoad(TR::Node *node);
   void          resetDefUseInfo() {_defUseInfo.MakeEmpty();}

   bool          skipAnalyzingForCompileTime(TR::Node *node, TR::Block *block, TR::Compilation *comp, AuxiliaryData &aux);

   private:
   static const char* const allocatorName;

   void    findTrivialSymbolsToExclude(TR::Node *node, TR::TreeTop *treeTop, AuxiliaryData &aux);
   bool    isTrivialUseDefNode(TR::Node *node, AuxiliaryData &aux);
   bool    isTrivialUseDefSymRef(TR::SymbolReference *symRef, AuxiliaryData &aux);

   // For Languages where an auto can alias a volatile, extra care needs to be taken when setting up use-def
   // The conservative answer is to not index autos that have volatile aliases.

   void setVolatileSybolsIndexAndRecurse(TR::BitVector &volatileSymbols, int32_t symRefNum);
   void findAndPopulateVolatileSymbolsIndex(TR::BitVector &volatileSymbols);

   bool shouldIndexVolatileSym(TR::SymbolReference *ref, AuxiliaryData &aux);



   public:
   int32_t getNumUseNodes() {return _numUseOnlyNodes+_numDefUseNodes;}
   int32_t getNumDefNodes() {return _numDefOnlyNodes+_numDefUseNodes;}
   int32_t getNumDefOnlyNodes() {return _numDefOnlyNodes;}
   int32_t getNumDefsOnEntry() { return (_uniqueIndexForDefsOnEntry ? _numDefsOnEntry : 1); }
   int32_t getTotalNodes()  {return _numDefOnlyNodes+_numDefUseNodes+_numUseOnlyNodes;}
   int32_t getFirstUseIndex() {return _numDefOnlyNodes;}
   int32_t getLastUseIndex() {return getTotalNodes()-1;}
   int32_t getFirstDefIndex() {return 0;}
   int32_t getFirstRealDefIndex() { return getFirstDefIndex()+getNumDefsOnEntry();}
   int32_t getLastDefIndex() {return getNumDefNodes()-1;}

   bool    isDefIndex(uint32_t index) { return index && index <= getLastDefIndex(); }
   bool    isUseIndex(uint32_t index)  { return index >= getFirstUseIndex() && index <= getLastUseIndex(); }

   bool    hasGlobalsUseDefs() {return _indexFields && _indexStatics;}
   bool    canComputeReachingDefs();
   // Information used by the reaching def analysis
   //
   int32_t getNumExpandedUseNodes()
               {return _numExpandedUseOnlyNodes+_numExpandedDefUseNodes;}
   int32_t getNumExpandedDefNodes()
               {return _numExpandedDefOnlyNodes+_numExpandedDefUseNodes;}
   int32_t getNumExpandedDefsOnEntry() {return _numDefsOnEntry;}
   int32_t getExpandedTotalNodes()
               {return _numExpandedDefOnlyNodes+_numExpandedDefUseNodes+_numExpandedUseOnlyNodes;}
   uint32_t getNumAliases (TR::SymbolReference * symRef, AuxiliaryData &aux)
               {return aux._numAliases[symRef->getReferenceNumber()];}

   protected:
   uint32_t getBitVectorSize() {return getNumExpandedDefNodes(); }

   private:

   bool    excludedGlobals(TR::Symbol *);
   bool    isValidAutoOrParm(TR::SymbolReference *);

   public:
   bool    isExpandedDefIndex(uint32_t index)
               { return index && index < getNumExpandedDefNodes(); }
   bool    isExpandedUseIndex(uint32_t index)
               { return index >= _numExpandedDefOnlyNodes && index < getExpandedTotalNodes(); }
   bool    isExpandedUseDefIndex(uint32_t index)
               { return index >= _numExpandedDefOnlyNodes && index < getNumExpandedDefNodes(); }

   bool getDefsForSymbol(BitVector &defs, int32_t symIndex, AuxiliaryData &aux)
      {
      defs.Or(aux._defsForSymbol[symIndex]);
      return !defs.IsZero();
      }

   bool getDefsForSymbolIsZero(int32_t symIndex, AuxiliaryData &aux)
      {
      return aux._defsForSymbol[symIndex].IsZero();
      }

   bool hasLoadsAsDefs() { return _hasLoadsAsDefs; }
   private:

   void dereferenceDefs(int32_t useIndex, BitVector &nodesLookedAt, BitVector &loadDefs);

   public:
   void dereferenceDef(BitVector &useDefInfo, int32_t defIndex, BitVector &nodesLookedAt);
   void buildDefUseInfo(bool loadAsDef = false);
   int32_t getSymRefIndexFromUseDefIndex(int32_t udIndex);

   public:
   int32_t getNumSymbols() { return _numSymbols; }
   int32_t getMemorySymbolIndex (TR::Node *);
   bool isPreciseDef(TR::Node *def, TR::Node *use);
   bool isChildUse(TR::Node *node, int32_t childIndex);

   public:
   bool     _useDefForRegs;
   private:
   bool     _useDefForMemorySymbols;

   void buildValueNumbersToMemorySymbolsMap();
   void findMemorySymbols(TR::Node *node);
   void fillInDataStructures(AuxiliaryData &aux);

   bool indexSymbolsAndNodes(AuxiliaryData &aux);
   bool findUseDefNodes(TR::Block *block, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, bool considerImplicitStores = false);
   bool assignAdjustedNodeIndex(TR::Block *, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, bool considerImplicitStores = false);
   bool childIndexIndicatesImplicitStore(TR::Node * node, int32_t childIndex);
   void insertData(TR::Block *, TR::Node *node, TR::Node *parent, TR::TreeTop *treeTop, AuxiliaryData &aux, TR::SparseBitVector &, bool considerImplicitStores = false);
   void buildUseDefs(void *vblockInfo, AuxiliaryData &aux);
   void buildUseDefs(TR::Node *node, void *vanalysisInfo, TR::BitVector &nodesToBeDereferenced, TR::Node *parent, AuxiliaryData &aux);
   int32_t setSingleDefiningLoad(int32_t useIndex, BitVector &nodesLookedAt, BitVector &loadDefs);

   protected:

   TR::Compilation *      _compilation;
   TR::Optimizer *      _optimizer;

   private:

   CS2::ArrayOf<CS2::Pair<TR::Node *, TR::TreeTop *>, TR::Allocator> _atoms;                          //TR::Node            **_nodes;

   private:
   CS2::ArrayOf<BitVector,TR::Allocator> _useDefInfo;
   bool _isUseDefInfoValid;

   TR::list<BitVector> _infoCache;                                 ///< initially empty bit vectors that are used for caching
   const BitVector _EMPTY;                                         ///< the empty bit vector
   CS2::ArrayOf<const BitVector *,TR::Allocator> _useDerefDefInfo; ///< all load defs are dereferenced
   CS2::ArrayOf<BitVector,TR::Allocator> _defUseInfo;
   CS2::ArrayOf<BitVector,TR::Allocator> _loadDefUseInfo;
   CS2::ArrayOf<int32_t, TR::Allocator> _sideTableToSymRefNumMap;

   int32_t             _numDefOnlyNodes;
   int32_t             _numDefUseNodes;
   int32_t             _numUseOnlyNodes;
   int32_t             _numExpandedDefOnlyNodes;
   int32_t             _numExpandedDefUseNodes;
   int32_t             _numExpandedUseOnlyNodes;
   int32_t             _numDefsOnEntry;
   int32_t             _numSymbols;
   int32_t             _numNonTrivialSymbols;
   int32_t             _numStaticsAndFields;

   bool                _indexFields;
   bool                _indexStatics;
   bool                _tempsOnly;
   bool                _trace;
   bool                _hasLoadsAsDefs;
   bool                _hasCallsAsUses;
   bool                _uniqueIndexForDefsOnEntry;

   class TR_UseDef
      {
      public:
      TR_UseDef() :_useDef(NULL) {}
      TR_UseDef(TR::TreeTop *tt) : _def((TR::TreeTop *)((intptr_t)tt|1)) {}
      TR_UseDef(TR::Node  *node) : _useDef(node) {}
      bool        isDef()     { return ((intptr_t)_def & 1) != 0; }
      TR::TreeTop *getDef()    { TR_ASSERT( isDef(), "assertion failure"); return (TR::TreeTop*)((intptr_t)_def & ~1); }
      TR::Node    *getUseDef() { TR_ASSERT(!isDef(), "assertion failure"); return _useDef;   }

      private:
      /** Tagged bottom bit means def */
      union
         {
         TR::Node    *_useDef;
         TR::TreeTop *_def;
         };
      };


   CS2::ArrayOf<TR_UseDef,TR::Allocator> _useDefs;

   class MemorySymbol
      {
      uint32_t         _size;
      uint32_t         _offset;
      int32_t          _sideTableIndex;

      MemorySymbol(uint32_t size, uint32_t offset, int32_t sideTableIndex) : _size(size), _offset(offset), _sideTableIndex(sideTableIndex) {}

      friend class TR_UseDefInfo;
      };
   typedef CS2::LinkedListOf<MemorySymbol,TR::Allocator> MemorySymbolList;

   int32_t                 _numMemorySymbols;
   CS2::ArrayOf<MemorySymbolList, TR::Allocator> _valueNumbersToMemorySymbolsMap;
   TR_ValueNumberInfo *_valueNumberInfo;
   TR::CFG                   *_cfg;
   };

#endif
