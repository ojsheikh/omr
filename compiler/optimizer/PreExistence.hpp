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

#ifndef OMR_PREEXISTENCE_INCL
#define OMR_PREEXISTENCE_INCL

#include <stdint.h>                      // for int32_t, uint32_t
#include <string.h>                      // for memset, NULL
#include "env/KnownObjectTable.hpp"  // for KnownObjectTable, etc
#include "env/TRMemory.hpp"              // for TR_Memory, etc

class TR_CallSite;
class TR_InlinerTracer;
class TR_OpaqueClassBlock;
class TR_VirtualGuard;
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }

enum PrexKnowledgeLevel { NONE, PREEXISTENT, FIXED_CLASS, KNOWN_OBJECT };

class TR_PrexArgument
   {
   friend class TR_DebugExt;

   public:

   TR_ALLOC(TR_Memory::LocalOpts);

   enum ClassKind { ClassIsUnknown, ClassIsFixed, ClassIsPreexistent };

   TR_PrexArgument(
         ClassKind classKind,
         TR_OpaqueClassBlock *clazz = 0,
         TR_OpaqueClassBlock *profiledClazz = 0,
         bool p = false) :
      _classKind(classKind),
      _fixedClass(clazz),
      _profiledClazz(profiledClazz),
      _knownObjectIndex(TR::KnownObjectTable::UNKNOWN)
      { }

   static const char *priorKnowledgeStrings[];
   static PrexKnowledgeLevel knowledgeLevel(TR_PrexArgument *pa);

   TR_PrexArgument(TR::KnownObjectTable::Index knownObjectIndex, TR::Compilation *comp);

   bool classIsFixed() { return _classKind == ClassIsFixed; }

   void setClassIsFixed(TR_OpaqueClassBlock *fixedClass=0, TR_OpaqueClassBlock *profiledClazz=0)
      {
      _classKind     = ClassIsFixed;
      _fixedClass    = fixedClass;
      _profiledClazz = profiledClazz;
      }

   bool classIsPreexistent() { return _classKind == ClassIsPreexistent; }

   bool usedProfiledInfo() { return _profiledClazz != NULL; }

   TR_OpaqueClassBlock *getFixedClass() { return _fixedClass;    }
   TR_OpaqueClassBlock *getFixedProfiledClass() { return _profiledClazz; }

   TR::KnownObjectTable::Index getKnownObjectIndex() { return _knownObjectIndex; }
   bool hasKnownObjectIndex(){ return getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN; }

   private:

   ClassKind _classKind;

   // optionally provided - when ClassIsFixed and the type is known to be
   // more specialized and different from the declared type
   //
   TR_OpaqueClassBlock  *_fixedClass;
   TR_OpaqueClassBlock  *_profiledClazz;

   // optionally provided - when ObjectIsKnown
   //
   TR::KnownObjectTable::Index _knownObjectIndex;
   };

class TR_PrexArgInfo
   {
   public:

#ifdef J9_PROJECT_SPECIFIC
   static TR_PrexArgInfo* enhance(TR_PrexArgInfo *dest, TR_PrexArgInfo *source, TR::Compilation *comp);

   static void propagateReceiverInfoIfAvailable (TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
                                              TR_PrexArgInfo * argInfo, TR_InlinerTracer *tracer);

   static void propagateArgsFromCaller(TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
      TR_PrexArgInfo * argInfo, TR_InlinerTracer *tracer);

   static bool validateAndPropagateArgsFromCalleeSymbol(TR_PrexArgInfo* argsFromSymbol, TR_PrexArgInfo* argsFromTarget, TR_InlinerTracer *tracer);

   static TR_PrexArgInfo* buildPrexArgInfoForMethodSymbol(TR::ResolvedMethodSymbol* methodSymbol, TR_InlinerTracer* tracer);
   void clearArgInfoForNonInvariantArguments(TR::ResolvedMethodSymbol* methodSymbol, TR_InlinerTracer* tracer);
#endif

   TR_ALLOC(TR_Memory::LocalOpts);
   TR_PrexArgInfo(int32_t numArgs, TR_Memory * m) : _numArgs(numArgs)
      {
      _args = (TR_PrexArgument **) m->allocateHeapMemory(sizeof(TR_PrexArgument*) * numArgs);
      memset(_args, 0, sizeof(TR_PrexArgument*) * numArgs);
      }

   void set(int32_t index, TR_PrexArgument *info) { _args[index] = info; }
   TR_PrexArgument *get(int32_t index) { return _args[index]; }

   int32_t getNumArgs() { return _numArgs; }

   private:

   int32_t _numArgs;
   TR_PrexArgument **_args;
   //
#ifdef J9_PROJECT_SPECIFIC
   static TR::Node* getCallNode (TR::ResolvedMethodSymbol* methodSymbol, class TR_CallSite* callsite, class TR_InlinerTracer* tracer);
   static bool hasArgInfoForChild (TR::Node *child, TR_PrexArgInfo * argInfo);
   static TR_PrexArgument* getArgForChild(TR::Node *child, TR_PrexArgInfo* argInfo);
#endif
   };


class TR_InnerAssumption
   {
   public:

   TR_ALLOC(TR_Memory::VirtualGuard)
      ;

   TR_InnerAssumption(int32_t ordinal, TR_VirtualGuard *guard) :
      _ordinal(ordinal),
      _guard(guard) {}

   int32_t _ordinal;
   TR_VirtualGuard *_guard;
   };

#endif
