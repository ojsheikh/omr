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
 ******************************************************************************/

#ifndef ALIASSETINTERFACE_INCL
#define ALIASSETINTERFACE_INCL

#include <stddef.h>                            // for NULL
#include <stdint.h>                            // for uint32_t
#include "compile/Compilation.hpp"             // for Compilation, comp, etc
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                    // for SparseBitVector, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitContainer, etc


template <class AliasSetInterface>
class TR_AliasSetInterface : public AliasSetInterface {
public:

  TR_AliasSetInterface(TR::SymbolReference *symRef, bool isDirectCall = false,
                       bool includeGCSafePoint = false)
    : AliasSetInterface(symRef),
      _isDirectCall(isDirectCall),
      _includeGCSafePoint(includeGCSafePoint) {
  }

  TR_AliasSetInterface(TR::Node *node, bool isDirectCall = false, bool includeGCSafePoint = false) :
    AliasSetInterface(node),
    _isDirectCall(isDirectCall),
    _includeGCSafePoint(includeGCSafePoint) {
  }

  TR_BitContainer getTRAliases() {
    return AliasSetInterface::getTRAliases(_isDirectCall, _includeGCSafePoint);
  }

   template <class BitVector>
   bool
   getAliases(BitVector &aliases)
      {
        TR::Compilation *comp = TR::comp();
        LexicalTimer t("getAliases", comp->phaseTimer());
        return getAliasesWithClear(aliases);
      }

   bool
   getAliasesAndUnionWith(TR::SparseBitVector &aliases)
      {
        TR::Compilation *comp = TR::comp();
        LexicalTimer t("getAliasesAndUnionWith", comp->phaseTimer());
        TR::SparseBitVector tmp(comp->allocator());
        getAliasesWithClear(tmp);
        return aliases.Or(tmp);
      }

   bool
   getAliasesAndUnionWith(TR_BitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndUnionWith_TR", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      aliases |= bc_aliases;
      return (!aliases.isEmpty());
      }


   template <class BitVector> bool
   getAliasesAndSubtractFrom(BitVector &v)
      {
      TR::Compilation *comp = TR::comp();
   	LexicalTimer t("getAliasesAndSubtractFrom", comp->phaseTimer());
    	TR_BitContainer bc_aliases = getTRAliases();
    	if (bc_aliases.isSingleValue())
         {
         v[bc_aliases.getSingleValue()]=false;
         }
      else
         {
         if (bc_aliases.getBitVector())
            v.Andc(CS2_TR_BitVector(*bc_aliases.getBitVector()));
         }
      return (!v.IsZero());
      }

   template <class BitVector> bool
   getAliasesAndIntersectWith(BitVector &v)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndIntersectWith", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      if (bc_aliases.isSingleValue())
         {
         bool set = v.ValueAt(bc_aliases.getSingleValue());
         v.Clear();
         if (set) v[bc_aliases.getSingleValue()]=true;
         }
      else
         {
         if (bc_aliases.getBitVector())
            v.And(CS2_TR_BitVector(*bc_aliases.getBitVector()));
         }
      return (!v.IsZero());
      }

   bool
   getAliasesAndSubtractFrom(TR_BitVector &bitvector)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("getAliasesAndSubtractFrom_TR", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      bitvector -= bc_aliases;
      return (!bitvector.isEmpty());
      }

   bool
   contains(TR::SymbolReference *symRef, TR::Compilation *comp)
      {
      return contains(symRef->getReferenceNumber(), comp);
      }

   bool
   contains(uint32_t refNum, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContains", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      return (bc_aliases.get(refNum) != 0);
      }

   template <class BitVector> bool
   containsAny(const BitVector &refs, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContainsAny", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      if (bc_aliases.isSingleValue())
         {
         return refs.ValueAt(bc_aliases.getSingleValue());
         }
      else
         {
         if (!bc_aliases.getBitVector()) return false;
         return refs.Intersects(CS2_TR_BitVector(*bc_aliases.getBitVector()));
         }
      }

   void
   removeAliases(TR::SparseBitVector &aliasesToModify)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAliases", comp->phaseTimer());
      AliasSetInterface::setAliases(aliasesToModify, false,  _isDirectCall, _includeGCSafePoint);
      }

   void
   addAliases(TR::SparseBitVector &aliasesToModify)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("addAliases", comp->phaseTimer());
      AliasSetInterface::setAliases(aliasesToModify, true,  _isDirectCall, _includeGCSafePoint);
      }

   void
   addAlias(TR::SymbolReference *symRef2)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("addAlias", comp->phaseTimer());
      setAlias(symRef2, true);
      }

   void
   removeAlias(TR::SymbolReference *symRef2)
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAlias", comp->phaseTimer());
      setAlias(symRef2, false);
      }

   bool containsAny(TR_BitVector& v2, TR::Compilation *comp)
      {
      LexicalTimer t("aliasesContainsAny_TR", comp->phaseTimer());
      TR_BitContainer bc_aliases = getTRAliases();
      return bc_aliases.intersects(v2);
      }

   bool isZero(TR::Compilation *comp)
      {
      LexicalTimer t("isZero", comp->phaseTimer());
      return getTRAliases().isEmpty();
      }

   bool hasAliases()
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("hasAliases", comp->phaseTimer());
      return !isZero(comp) && getTRAliases().hasMoreThanOneElement();
      }

   void
   setAlias(TR::SymbolReference *symRef2, bool value) {
     AliasSetInterface::setAlias(symRef2, value, _isDirectCall, _includeGCSafePoint);
   }

   template <class BitVector> bool
   getAliasesWithClear(BitVector &aliases)
      {
      TR::Compilation *comp = TR::comp();
      TR_BitContainer bc_aliases = getTRAliases();
      Assign(aliases, bc_aliases, true);
      return (!aliases.IsZero());
      }

   bool _isDirectCall;
   bool _includeGCSafePoint;
   };

////////////////// SYMREF-BASED ALIASING

typedef enum {
   useDefAliasSet,
   UseOnlyAliasSet
} TR_AliasSetType;

template <uint32_t _aliasSetType>
class TR_SymAliasSetInterface {
public:
  TR_SymAliasSetInterface(TR::SymbolReference *symRef) :
    _symbolReference(symRef) {}

   TR_BitContainer getTRAliases(bool isDirectCall, bool includeGCSafePoint);

   void
   setAlias(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint);

   void
   setAliases(TR::SparseBitVector &aliasesToModify, bool value, bool isDirectCall, bool includeGCSafePoint)
      {
      TR::Compilation *comp = TR::comp();
      TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only setAliases for UseOnly or useDef presently");

         {
         TR::SparseBitVector::Cursor aliasCursor(aliasesToModify);
         for (aliasCursor.SetToFirstOne(); aliasCursor.Valid(); aliasCursor.SetToNextOne())
            {
            uint32_t symRefNum = aliasCursor;
            TR::SymbolReference *symRef2 = comp->getSymRefTab()->getSymRef(aliasCursor);
            setAlias(symRef2, value, isDirectCall, includeGCSafePoint);
            }
         }
      }

   void
   removeAllAliases()
      {
      TR::Compilation *comp = TR::comp();
      LexicalTimer t("removeAllAliases", comp->phaseTimer());
      TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only setAliases for UseOnly or useDef presently");

      }

private:
    static void removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);

    static void addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint);

    static void setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value);

  TR::SymbolReference *_symbolReference;
};

struct TR_UseDefAliasSetInterface : public TR_AliasSetInterface<TR_SymAliasSetInterface<useDefAliasSet> > {
  TR_UseDefAliasSetInterface(TR::SymbolReference *symRef,
                             bool isDirectCall = false,
                             bool includeGCSafePoint = false) :
  TR_AliasSetInterface<TR_SymAliasSetInterface<useDefAliasSet> >
    (symRef, isDirectCall, includeGCSafePoint) {}
};

struct TR_UseOnlyAliasSetInterface: public TR_AliasSetInterface<TR_SymAliasSetInterface<UseOnlyAliasSet> > {
  TR_UseOnlyAliasSetInterface(TR::SymbolReference *symRef,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_AliasSetInterface<TR_SymAliasSetInterface<UseOnlyAliasSet> >
    (symRef, isDirectCall, includeGCSafePoint) {}
};

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::setAlias(TR::SymbolReference *symRef2, bool value, bool isDirectCall, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   if (symRef2 == NULL)
      return;

   TR_ASSERT(_aliasSetType == useDefAliasSet || _aliasSetType ==  UseOnlyAliasSet, "failed: can only setAliases for UseOnly or useDef presently");

      {
      if (_aliasSetType == useDefAliasSet)
         {
         // carry out symmetrically
         setSymRef1KillsSymRef2Asymmetrically(_symbolReference, symRef2, includeGCSafePoint, value);

         if (!_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            {
            // symmetric, complete symmetry
            setSymRef1KillsSymRef2Asymmetrically(symRef2, _symbolReference, includeGCSafePoint, value);
            }
         }
      else if (_aliasSetType == UseOnlyAliasSet)
         {
         // carry out asymmetrically
         if (_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            { // foo uses x, same as x kills foo
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else if (!_symbolReference->getSymbol()->isMethod() && symRef2->getSymbol()->isMethod())
            { // x uses foo, same as foo kills x
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else if (!_symbolReference->getSymbol()->isMethod() && !symRef2->getSymbol()->isMethod())
            { // x uses y, same as y kills x. Its symmetric
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         else
            { // foo uses bar, bar kills foo
            symRef2->getUseDefAliases(includeGCSafePoint).setAlias(_symbolReference, value);
            }
         }
      }
   }

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::removeSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
   {
   TR_BitVector *symRef1_killedAliases = symRef1->getUseDefAliases(includeGCSafePoint).getTRAliases().getBitVector();
   TR_BitVector *symRef2_useAliases = symRef2->getUseonlyAliases().getTRAliases().getBitVector();

   if ((symRef1_killedAliases == NULL) || (symRef2_useAliases == NULL))
      return;

   if (symRef1_killedAliases && symRef1_killedAliases->isSet(symRef2->getReferenceNumber()))
      symRef1_killedAliases->reset(symRef2->getReferenceNumber());

   if (symRef2_useAliases && symRef2_useAliases->isSet(symRef1->getReferenceNumber()))
      symRef2_useAliases->reset(symRef1->getReferenceNumber());
   }

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::addSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint)
   {
   TR::Compilation *comp = TR::comp();
   TR_BitVector *symRef1_killedAliases = symRef1->getUseDefAliases(includeGCSafePoint).getTRAliases().getBitVector();

   if (symRef1_killedAliases != NULL)
      {
      if (!symRef1_killedAliases->isSet(symRef2->getReferenceNumber()))
         symRef1_killedAliases->set(symRef2->getReferenceNumber());
      }

   TR_BitVector *symRef2_useAliases = symRef2->getUseonlyAliases().getTRAliases().getBitVector();
   if (symRef2_useAliases != NULL)
      {
      if (!symRef2_useAliases->isSet(symRef1->getReferenceNumber()))
         symRef2_useAliases->set(symRef1->getReferenceNumber());
      }
   }

template <uint32_t _aliasSetType> inline
void TR_SymAliasSetInterface<_aliasSetType>::setSymRef1KillsSymRef2Asymmetrically(TR::SymbolReference *symRef1, TR::SymbolReference *symRef2, bool includeGCSafePoint, bool value)
   {
   TR::Compilation *comp = TR::comp();
   if ((symRef1 == NULL) || (symRef2 == NULL))
      return;

   if (value)
      addSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   else
      removeSymRef1KillsSymRef2Asymmetrically(symRef1, symRef2, includeGCSafePoint);
   }

template <> inline
TR_BitContainer TR_SymAliasSetInterface<useDefAliasSet>::getTRAliases(bool isDirectCall, bool includeGCSafePoint) {
  return _symbolReference->getUseDefAliasesBV(isDirectCall, includeGCSafePoint);
}

template <> inline
TR_BitContainer TR_SymAliasSetInterface<UseOnlyAliasSet>::getTRAliases(bool isDirectCall, bool includeGCSafePoint) {
  return _symbolReference->getUseonlyAliasesBV(TR::comp()->getSymRefTab());
}

template <class T>
void CountUseDefAliases( T& t, const TR::SparseBitVector &syms)
   {
   TR::Compilation *comp = TR::comp();
   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();

      {
      TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
      TR_BitVector *tr_syms = new (comp->trStackMemory()) TR_BitVector(syms.PopulationCount(), comp->trMemory(), stackAlloc, growable);
      *tr_syms = syms;

      typename T::Cursor c(t);
      for (c.SetToFirst(); c.Valid(); c.SetToNext())
         {
         TR::SymbolReference *symRef = symRefTab->getSymRef(t.KeyAt(c));
         if (symRef && symRef->sharesSymbol())
            t[c] = symRef->getUseDefAliases().getTRAliases().getBitVector()->commonElementCount(*tr_syms);
         else
            t[c] = 0;
         }
      }
   }


////NODE ALIASING

typedef enum {
   mayUseAliasSet,
   mayKillAliasSet
} TR_NodeAliasSetType;

template <uint32_t _aliasSetType>
class TR_NodeAliasSetInterface {
public:
   TR_NodeAliasSetInterface(TR::Node *node) :
        _node(node) {}

   TR_BitContainer getTRAliases(bool isDirectCall, bool includeGCSafePoint);

private:
  TR::Node            *_node;
};

template<> inline
TR_BitContainer TR_NodeAliasSetInterface<mayUseAliasSet>::getTRAliases(bool isDirectCall, bool includeGCSafePoint) {
   TR::Compilation *comp = TR::comp();
   TR_BitContainer bc;

   if (_node->getOpCode().isLikeUse() && _node->getOpCode().hasSymbolReference())
      bc = _node->getSymbolReference()->getUseonlyAliasesBV(comp->getSymRefTab());
   // else there is no symbol reference associated with the node, we return an empty bit container

   return bc;
}

template<> inline
TR_BitContainer TR_NodeAliasSetInterface<mayKillAliasSet>::getTRAliases(bool isDirectCall, bool includeGCSafePoint) {
   TR::Compilation *comp = TR::comp();
   TR_BitContainer bc;

   if (_node->getOpCode().hasSymbolReference() && (_node->getOpCode().isLikeDef() || _node->mightHaveVolatileSymbolReference())) //we want the old behavior in these cases
      {
      if (_node->getSymbolReference()->sharesSymbol(includeGCSafePoint))
         bc = _node->getSymbolReference()->getUseDefAliasesBV(isDirectCall, includeGCSafePoint);
      else
         bc = _node->getSymbolReference()->getReferenceNumber();
      }

   // else there is no symbol reference associated with the node, we return an empty bit container
   return bc;
}

struct TR_NodeUseAliasSetInterface: public TR_AliasSetInterface<TR_NodeAliasSetInterface<mayUseAliasSet> >  {
  TR_NodeUseAliasSetInterface(TR::Node *node,
                              bool isDirectCall = false,
                              bool includeGCSafePoint = false) :
  TR_AliasSetInterface<TR_NodeAliasSetInterface<mayUseAliasSet> >
    (node, isDirectCall, includeGCSafePoint) {}
};

struct TR_NodeKillAliasSetInterface: public TR_AliasSetInterface<TR_NodeAliasSetInterface<mayKillAliasSet> >{
  TR_NodeKillAliasSetInterface(TR::Node *node,
                               bool isDirectCall = false,
                               bool includeGCSafePoint = false) :
    TR_AliasSetInterface<TR_NodeAliasSetInterface<mayKillAliasSet> >
     (node, isDirectCall, includeGCSafePoint) {}
};

///////////////////////////////////////


inline TR_UseOnlyAliasSetInterface
OMR::SymbolReference::getUseonlyAliases()
   {
   TR_UseOnlyAliasSetInterface aliasSetInterface(self());
   return aliasSetInterface;
   }

inline TR_UseDefAliasSetInterface
OMR::SymbolReference::getUseDefAliases(bool isDirectCall, bool includeGCSafePoint)
   {
   TR_UseDefAliasSetInterface aliasSetInterface(self(), isDirectCall, includeGCSafePoint);
   return aliasSetInterface;
   }

inline TR_UseDefAliasSetInterface
OMR::SymbolReference::getUseDefAliasesIncludingGCSafePoint(bool isDirectCall)
   {
   TR_UseDefAliasSetInterface aliasSetInterface(self(), isDirectCall, true);
   return aliasSetInterface;
   }

#endif
