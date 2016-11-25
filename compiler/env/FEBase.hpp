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

#ifndef FEBASE_HPP_JniLXw
#define FEBASE_HPP_JniLXw

#include "codegen/FrontEnd.hpp"

#include "compile/CompilationTypes.hpp"
#include "env/JitConfig.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "env/CompilerEnv.hpp"

class TR_ResolvedMethod;

namespace TR
{

// ::TR_FrontEnd: defines the API that can be used across the codebase for both TR and OMR
// FECommon: is a repository of code and data that is common to all OMR frontends but not all TR frontends
// FEBase<D>: is a repository of code and data that is specific to a single frontend, but described as an abstract templat
//            based on the type D
// D: concrete frontend. Stuff that is not common with other frontends.

class FECommon : public ::TR_FrontEnd
   {
   protected:
   FECommon();


   public:
   virtual TR_PersistentMemory       * persistentMemory() = 0;

   virtual TR_Debug *createDebug(TR::Compilation *comp = NULL);

   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT=false) { return NULL; }
   virtual TR_OpaqueClassBlock * getClassFromSignature(const char * sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT=false) { return NULL; }
   virtual const char *       sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *memory) { return NULL; }

   // need this so z codegen can create a sym ref to compare to another sym ref it cannot possibly be equal to
   virtual uintptrj_t getOffsetOfIndexableSizeField() { return -1; }
   };

template <class T> struct FETraits {};

template <class Derived>
class FEBase : public FECommon
   {
   public:
   // Define our types in terms of the Traits
   typedef typename TR::FETraits<Derived>::JitConfig        JitConfig;
   typedef typename TR::FETraits<Derived>::CodeCacheManager CodeCacheManager;
   typedef typename TR::FETraits<Derived>::CodeCache        CodeCache;

   private:
   uint64_t             _start_time;
   JitConfig            _config;
   TR::CodeCacheManager _codeCacheManager;

   // these two are deprecated in favour of TR::GlobalAllocator and TR::Allocator
   TR_PersistentMemory       _persistentMemory; // global memory

   public:
   FEBase()
      : FECommon(),
      _config(),
      _codeCacheManager(this),
      _start_time(TR::Compiler->vm.getUSecClock()),
      _persistentMemory(jitConfig(), TR::Compiler->persistentAllocator())
      {
      ::trPersistentMemory = &_persistentMemory;
      }

   static Derived &singleton() { return *(Derived::instance()); }

   JitConfig *jitConfig() { return &_config; }
   TR::CodeCacheManager &codeCacheManager() { return _codeCacheManager; }

   virtual void     unreserveCodeCache(TR::CodeCache *codeCache);
   virtual TR::CodeCache *getDesignatedCodeCache(TR::Compilation *);

   virtual uint8_t *allocateCodeMemory(TR::Compilation *comp, uint32_t warmCodeSize, uint32_t coldCodeSize,
                                       uint8_t **coldCode, bool isMethodHeaderNeeded);
   virtual void resizeCodeMemory(TR::Compilation * comp, uint8_t *bufferStart, uint32_t numBytes);
   virtual uint8_t * allocateRelocationData(TR::Compilation* comp, uint32_t size);

   virtual intptrj_t indexedTrampolineLookup(int32_t helperIndex, void * callSite);

   virtual TR_PersistentMemory       * persistentMemory() { return &_persistentMemory; }
   virtual TR::PersistentInfo * getPersistentInfo() { return _persistentMemory.getPersistentInfo(); }

   uint64_t getStartTime() { return _start_time; }

   private:
   virtual void switchCodeCache(TR::CodeCache *newCache);
   };

} /* namespace TR */

#endif /* FEBASE_HPP_JniLXw */
