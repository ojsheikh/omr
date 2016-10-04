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

#ifndef OMR_GCSTACKATLAS_INCL
#define OMR_GCSTACKATLAS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_GCSTACKATLAS_CONNECTOR
#define OMR_GCSTACKATLAS_CONNECTOR
namespace OMR { class GCStackAtlas; }
namespace OMR { typedef OMR::GCStackAtlas GCStackAtlasConnector; }
#endif

#include "codegen/GCStackMap.hpp"

#include <stddef.h>               // for NULL
#include <stdint.h>               // for uint32_t, int32_t, uint8_t
#include "env/TRMemory.hpp"       // for TR_Memory, etc
#include "infra/List.hpp"         // for List
#include "infra/Annotations.hpp"  // for OMR_EXTENSIBLE

class TR_GCStackAllocMap;
class TR_GCStackMap;
class TR_InternalPointerMap;
namespace TR { class GCStackAtlas; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }

namespace OMR
{

class OMR_EXTENSIBLE GCStackAtlas
   {

   public:

   TR_ALLOC(TR_Memory::GCStackAtlas)

   GCStackAtlas(uint32_t numParms, uint32_t numSlots, TR_Memory * m)
      : _atlasBits(NULL),
        _numberOfParmSlotsMapped(numParms),
        _numberOfSlotsMapped(numSlots),
#ifdef J9_PROJECT_SPECIFIC
        _numberOfPaddingSlots(0),
#endif
        _numberOfSlotsToBeInitialized(0),
        _numberOfPendingPushSlots(0),
        _parmBaseOffset(0),
        _trMemory(m),
        _localBaseOffset(0),
        _numberOfMaps(0),
        _parameterMap(NULL),
        _localMap(NULL),
        _indexOfFirstInternalPtr(0),
        _offsetOfFirstInternalPtr(0),
        _internalPointerMap(NULL),
        _pinningArrayPtrsForInternalPtrRegs(m),
        _stackAllocMap(NULL),
        _mapList(m),
        _hasUninitializedPinningArrayPointer(false) {}

   TR::GCStackAtlas * self();

   TR_Memory *trMemory() { return _trMemory; }

   uint8_t *getAtlasBits() { return _atlasBits; }
   uint8_t *setAtlasBits(uint8_t *p) { return (_atlasBits = p); }

   uint32_t getNumberOfParmSlotsMapped() { return _numberOfParmSlotsMapped; }

   uint32_t getNumberOfLocalSlotsMapped() { return _numberOfSlotsMapped-_numberOfParmSlotsMapped; }

   uint32_t getNumberOfSlotsMapped() { return _numberOfSlotsMapped; }
   uint32_t setNumberOfSlotsMapped(uint32_t n) { return (_numberOfSlotsMapped = n); }

#ifdef J9_PROJECT_SPECIFIC
   uint32_t getNumberOfPaddingSlots()          {return _numberOfPaddingSlots;}
   uint32_t setNumberOfPaddingSlots(uint32_t n) {return (_numberOfPaddingSlots = n);}
#endif

   uint32_t getNumberOfSlotsToBeInitialized() { return _numberOfSlotsToBeInitialized; }
   uint32_t setNumberOfSlotsToBeInitialized(uint32_t n) { return (_numberOfSlotsToBeInitialized = n); }

   uint32_t getNumberOfPendingPushSlots() { return _numberOfPendingPushSlots; }
   uint32_t setNumberOfPendingPushSlots(uint32_t n) { return (_numberOfPendingPushSlots = n); }

   uint32_t getIndexOfFirstSpillTemp() { return _indexOfFirstSpillTemp; }
   uint32_t setIndexOfFirstSpillTemp(uint32_t n) { return (_indexOfFirstSpillTemp = n); }
   uint32_t assignGCIndex() { return _numberOfSlotsMapped++; }

   int32_t getParmBaseOffset() { return _parmBaseOffset; }
   int32_t setParmBaseOffset(int32_t n) { return (_parmBaseOffset = n); }

   int32_t getLocalBaseOffset() { return _localBaseOffset; }
   int32_t setLocalBaseOffset(int32_t n) { return (_localBaseOffset = n); }

   uint32_t getNumberOfMaps() { return _numberOfMaps; }
   uint32_t setNumberOfMaps(uint32_t n) { return (_numberOfMaps = n); }
   void decNumberOfMaps() { _numberOfMaps--; }

   TR_GCStackMap *getParameterMap() { return _parameterMap; }
   TR_GCStackMap *setParameterMap(TR_GCStackMap *map) { return (_parameterMap = map); }

   TR_GCStackMap *getLocalMap() { return _localMap; }
   TR_GCStackMap *setLocalMap(TR_GCStackMap *map) { return (_localMap = map); }

   int32_t getIndexOfFirstInternalPointer() { return _indexOfFirstInternalPtr; }
   int32_t setIndexOfFirstInternalPointer(int32_t n) { return (_indexOfFirstInternalPtr = n); }

   int32_t getOffsetOfFirstInternalPointer() { return _offsetOfFirstInternalPtr; }
   int32_t setOffsetOfFirstInternalPointer(int32_t n) { return (_offsetOfFirstInternalPtr = n); }

   TR_InternalPointerMap *getInternalPointerMap() { return _internalPointerMap; }
   TR_InternalPointerMap *setInternalPointerMap(TR_InternalPointerMap *map) { return (_internalPointerMap = map); }

   TR_GCStackAllocMap *getStackAllocMap() { return _stackAllocMap; }
   TR_GCStackAllocMap *setStackAllocMap(TR_GCStackAllocMap *map) { return (_stackAllocMap = map); }

   uint32_t getNumberOfDistinctPinningArrays();

   List<TR::AutomaticSymbol>& getPinningArrayPtrsForInternalPtrRegs() { return _pinningArrayPtrsForInternalPtrRegs; }
   void addPinningArrayPtrForInternalPtrReg(TR::AutomaticSymbol *symbol)
      {
      if (!_pinningArrayPtrsForInternalPtrRegs.find(symbol))
         _pinningArrayPtrsForInternalPtrRegs.add(symbol);
      }

   void removePinningArrayPtrForInternalPtrReg(TR::AutomaticSymbol *symbol)
      {
      _pinningArrayPtrsForInternalPtrRegs.remove(symbol);
      }

   bool hasUninitializedPinningArrayPointer() { return _hasUninitializedPinningArrayPointer; }
   void setHasUninitializedPinningArrayPointer(bool b) { _hasUninitializedPinningArrayPointer = b; }

   List<TR_GCStackMap>& getStackMapList() { return _mapList; }

   void close(TR::CodeGenerator *);

   void addStackMap(TR_GCStackMap *m);

   private:

   uint8_t *_atlasBits;
   TR_GCStackMap *_parameterMap;
   TR_GCStackMap *_localMap;
   TR_InternalPointerMap *_internalPointerMap;
   TR_GCStackAllocMap *_stackAllocMap;
   TR_Memory *_trMemory;

   List<TR::AutomaticSymbol> _pinningArrayPtrsForInternalPtrRegs;
   List<TR_GCStackMap> _mapList;

   uint32_t _numberOfParmSlotsMapped;// span of the parameter map
   uint32_t _numberOfSlotsMapped;    // total span of the map
#ifdef J9_PROJECT_SPECIFIC
   uint32_t _numberOfPaddingSlots;   // number of padding slots used to align local objects
#endif
   uint32_t _numberOfSlotsToBeInitialized; // number of locals that
   uint32_t _numberOfPendingPushSlots; // number of locals that
                                                // need to be initialized to null
   uint32_t _indexOfFirstSpillTemp;
   int32_t _parmBaseOffset;     // measured from the frame pointer for the method
   int32_t _localBaseOffset;    // measured from the frame pointer for the method
   int32_t _indexOfFirstInternalPtr;
   int32_t _offsetOfFirstInternalPtr;
   uint32_t _numberOfMaps;

   bool _hasUninitializedPinningArrayPointer;

   };

}

#endif
