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

#ifndef OMRCHECKER_HPP
#define OMRCHECKER_HPP

#include "llvm/Config/llvm-config.h"

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 4
#define LLVM34
#endif
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 6
#define LLVM36
#endif
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 8
#define LLVM38
#endif

#include "clang/Basic/SourceManager.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Support/raw_ostream.h"

#ifdef LLVM34
#include "llvm/ADT/ValueMap.h"
#else
#include "llvm/IR/ValueMap.h"
#endif

#include <string>
#include <queue>
#include <map>
#include <list>
#include <algorithm>
#include <assert.h>
#include <iostream>

using namespace llvm;
using namespace clang;

/**
 * OMR Checker
 * ===================
 *
 * This tool enforces some design principals with the OMR project's extensible
 * classes.
 *
 * # Definitions:
 *
 * The design makes the most sense with a few defintiions.
 *
 * * *Concrete Class:*          The TR namespace end of an extensible class string.
 *                              A concrete class is the one which is consumed by
 *                              most clients.
 *
 * * *Root Class:*              The least-derived class in an extensible class string.
 *                              Currently this is assumed to be in the OMR layer.
 *
 * * *Extensible Class String:* An inheritance path from the Concrete Class to
 *                              the OMR layer root class, and all classes on that
 *                              path.
 *
 * # Requirements:
 * ## Structural Analysis Requirements:
 *
 * * Extensible classes must have extensible subclasses, within the bounds of
 *   an Extensible Class String.
 * * We must be able to ensure that an extensible class string starts in the TR
 *   layer, and terminates in the OMR layer (Root layer).
 * * An extensible class must have a self call that returns the concrete class
 *   type defined in the TR: Layer. *Not checked*
 *
 *
 * ## Call Analysis:
 *
 * * Implicit this use is disallowed. Correct to `self()->call`.
 * * Static cast to concrete type should be a warning, suggest `self()`.
 * * Regular casts should error, suggest `self()`.
 *
 *
 * ## File Layout Analysis
 *
 * *Not yet implemented.*
 *
 * * Check include guards
 * * Check connectors.
 *
 * # Design
 *
 * The checker operates in three phases. Each phase is a complete traversal of
 * the AST.
 *
 * ## Phase 1: Extensibility Discovery
 *
 * The first phase simply discovers whether or not a class is extensible. This
 * is done by keeping a map:
 *
 *     canonicalDecl -> isExtensible
 *
 * This is computed ahead of time to avoid having to mark forward declarations
 * of a class as extensible or issuing diagnostics too early.
 *
 * ## Phase 2: Class structure analysis
 *
 * A second pass ensures that the class structure is enforced, and builds a map
 *
 *     extensibleDecl -> concreteType
 *
 * Which indicates the concrete type for any extensible decl. Diagnostics are
 * issued here about class hierarchy structure.
 *
 * ## Phase 3: Expression Checking.
 *
 * The third phase checks for implicit this or casts to the wrong types, issuing diagnostics.
 *
 * # Known Weaknesses:
 *
 * 1. String matching is used to name special namespaces. A more robust and
 *    general solution would likely be to proved root and concrete annotations.
 *
 * 2. Self functions are checked only by name, with no typechecking.
 *
 * 3. Use of a `_self` member will not trigger warnings, however, there's no
 *    support in the diagnostics to suggest `_self` should it exist.
 *
 * 4. Complex multiple inheritance can thwart this.
 *
 */
namespace OMRChecker {

   static CXXThisExpr* getThisExpr(Expr*);

#define trace(x) if (getenv("OMR_CHECK_TRACE")) { llvm::errs() << x << "\n"; }

/**
 * Extensible Class discovery visitor.
 *
 * Keeps track of what canonical decls actually refer to extensible classes.
 *
 * When done, this will be used to answer isExtensible queries.
 *
 */
class ExtensibleClassDiscoveryVisitor : public RecursiveASTVisitor<ExtensibleClassDiscoveryVisitor> {
public:
   explicit ExtensibleClassDiscoveryVisitor(ASTContext *Context)  { }

   /**
    * Handle an individual record decl.
    *
    * Builds a most-derived type mapping that can be then analyzed for Composible classes.
    */
   bool VisitCXXRecordDecl(const CXXRecordDecl *decl) {
      isExtensible(decl);
      return true;
   }


   /**
    * Determines if a decl, or any decl with the same canonical is extensible
    */
   bool isExtensible(const CXXRecordDecl * declIn) {
      //Check canonical map.
      std::map<const CXXRecordDecl*, bool>::iterator itr = ExtensibleMap.find(declIn->getCanonicalDecl());
      if (itr != ExtensibleMap.end()) { return itr->second; }

      // Check annotations.
      return isExtensibleDecl(declIn);
   }

private:
   /**
    * Check a CXXRecordDecl for the extensible attribute
    *
    * Checks all declarations in the the decl chain.
    *
    * The canonical decl is marked in an extensible map.
    */
   bool isExtensibleDecl(const CXXRecordDecl *declIn) {
      const CXXRecordDecl* decl = declIn;

      while (decl) {
         for (Decl::attr_iterator A = decl->attr_begin(), E = decl->attr_end(); A != E; ++A) {
            if (isa<AnnotateAttr>(*A)) {
               AnnotateAttr *annotation = dyn_cast<AnnotateAttr>(*A);
               if (annotation->getAnnotation() == "OMR_Extensible") {
                  ExtensibleMap[decl->getCanonicalDecl()] = true;
                  return true;
               }
            }
         }
         decl = decl->getPreviousDecl();
      }
      return false;
   }


   /**
    *  A map indicating the canoncial decl is extensible.
    */
   std::map<const CXXRecordDecl*, bool> ExtensibleMap;

};

/**
 * Extensible class checking visitor. Verifies that all extensible classes in a hierarchy are in a single chain.
 *
 * isExtensibleClassHierarchyCorrect(const CXXRecordDecl*) is a wrapper, used to find the extensible classes in the class hierarchy.
 * isExtensibleClassHierarchyCorrect(const CXXRecordDecl*, bool, int) contains the checking algorithm.
 *
 * Uses a two part algorithm:
 *  Tag every CXXRecordDecl with a pass number:
 *    Recursively propagate the pass number to every parent class,
 *    If the current class has a pass number and it's not extensible,
 *    then the pass number must have been propagated from an extensible class elsewhere in the hierarchy.
 *
 *  Check if the parent classes contain another chain of extensible classes:
 *    Recursively iterate through all parent classes.
 *    While in the extensiblePhase, iterate through extensible classes until you reach a non-extensible class.
 *    After exiting the extensiblePhase, iterate through the rest of the parent classes, checking if they are extensible.
 *       If you find an extensible class, you've found an extensible class that is in a hierarchy like so: (parent -> child)
 *
 *         ... -> [Extensible] -> [Non-extensible] -> ... -> [Non-extensible] -> [Extensible] -> [Extensible] ->...
 *         { - - - - - - - - - - - - - !extensiblePhase - - - - - - - - - - - } { - - - extensiblePhase - - - - }
 */
class ExtensibleClassCheckingVisitor : public RecursiveASTVisitor<ExtensibleClassCheckingVisitor> {
public:
   explicit ExtensibleClassCheckingVisitor(ASTContext *Context, ExtensibleClassDiscoveryVisitor &extensible) :
      Extensible(extensible),
      Context(Context),
      currentPass(1)
   { }

   /**
    * Handle an individual record decl.
    */
   bool VisitCXXRecordDecl(const CXXRecordDecl *decl) {
      if (!isExtensibleClassHierarchyCorrect(decl->getCanonicalDecl())) {
         //return false;
      }
      return true;
   }

private:

   /**
    * Iterates through the class hierarchy and calls isExtensibleClassHierarchyCorrect(const CXXRecordDecl*, bool, int) for all extensible classes found.
    */
   bool isExtensibleClassHierarchyCorrect(const CXXRecordDecl* decl) {
      // If extensible, start checking the heirarchy.
      // Assign a pass number if class doesn't have one.
      if (Extensible.isExtensible(decl)) {
         if (passNumber[decl] != 0) {
            return isExtensibleClassHierarchyCorrect(decl, true, passNumber[decl]);
         } else {
            return isExtensibleClassHierarchyCorrect(decl, true, currentPass++);
         }
      }

      // Iterate through parent classes.
      if (decl->hasDefinition()) {
         for (CXXRecordDecl::base_class_const_iterator BI = decl->bases_begin(), BE = decl->bases_end(); BI != BE; ++BI) {
            CXXRecordDecl * base_class = BI->getType()->getAsCXXRecordDecl();
            if (base_class && !isExtensibleClassHierarchyCorrect(base_class->getCanonicalDecl()))
               return false;
         }
      }
      return true;
   }

   /**
    * Starting point for extensible classes. Verifies that all extensible classes are in one chain.
    */
   bool isExtensibleClassHierarchyCorrect(const CXXRecordDecl* decl, bool extensiblePhase, int pass) {
      // Check and assign pass number.
      if (passNumber[decl] != 0) {
         if (Extensible.isExtensible(decl)) {
            pass = passNumber[decl];
         } else {
            if (pass != passNumber[decl]) {
               std::string diagnostic("Extensible classes heirarchy was incorrect, multiple extensible classes deriving from non-extensible class.");
               DiagnosticsEngine &diagEngine = Context->getDiagnostics();
               unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "%0");
               diagEngine.Report(decl->getLocation(), diagID) << diagnostic;
               return false;
            }
         }
      } else {
         passNumber[decl] = pass;
      }

      // Iterate through parent classes.
      for (CXXRecordDecl::base_class_const_iterator BI = decl->bases_begin(), BE = decl->bases_end(); BI != BE; ++BI) {
         CXXRecordDecl * base_class = BI->getType()->getAsCXXRecordDecl()->getCanonicalDecl();
         if (extensiblePhase) {
           if (!isExtensibleClassHierarchyCorrect(base_class, Extensible.isExtensible(base_class), pass))
              return false;
         } else {
            if(!Extensible.isExtensible(decl)) {
               if (!isExtensibleClassHierarchyCorrect(base_class, false, pass))
                  return false;
            } else {
               std::string diagnostic("Extensible classes heirarchy was incorrect, multiple extensible class chains in heirarchy.");
               DiagnosticsEngine &diagEngine = Context->getDiagnostics();
               unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "%0");
               diagEngine.Report(decl->getLocation(), diagID) << diagnostic;
               return false;
            }
         }
      }
      return true;
   }

   /**
    * Current pass i.e. nth CXXRecordDecl being processed
    */
   int currentPass;

   /**
    * Every visited class is passged with the pass number
    */
   std::map<const CXXRecordDecl*,int> passNumber;

   /**
    * This visitor has determined the map of extensible classes
    */
   ExtensibleClassDiscoveryVisitor &Extensible;

   /**
    * Context, required for diagnostics
    */
   ASTContext *Context;

};


/**
 * Class structure discovery visitor. Also checks class structure
 */
class OMRClassCheckingVisitor : public RecursiveASTVisitor<OMRClassCheckingVisitor> {
public:
   explicit OMRClassCheckingVisitor(ASTContext *Context, ExtensibleClassDiscoveryVisitor &extensible) :
      Extensible(extensible),
      Context(Context)
   { }

   /**
    * Handle an individual record decl.
    *
    * Builds a most-derived type mapping that can be then analyzed for Composible classes.
    */
   bool VisitCXXRecordDecl(CXXRecordDecl *decl) {
      if (decl->isCompleteDefinition()) {
         BuildMostDerivedTypeMap(decl);
         Types[ decl->getCanonicalDecl() ] = isExtensible(decl);
      }
      return true;
   }


   void printRelations() {
      llvm::errs() << "Most Derived Type Map: \n";
      for (std::map<CXXRecordDecl*, CXXRecordDecl*>::iterator I = MostDerivedType.begin(), E= MostDerivedType.end(); I != E; ++I) {
         llvm::errs() << "\t\t" << I->first->getQualifiedNameAsString() << " -> " << I->second->getQualifiedNameAsString();
         CXXRecordDecl * concrete = getAssociatedConcreteType(I->first);
         llvm::errs() << " => " << (concrete ? concrete->getQualifiedNameAsString() : "<no concrete found>" );
         llvm::errs() << "\n";
      }

   }

   /**
    * Returns the most derived type for a decl.
    */
   CXXRecordDecl *mostDerivedType(CXXRecordDecl *decl) {
      std::map<CXXRecordDecl*, CXXRecordDecl*>::iterator itr = MostDerivedType.find(decl->getCanonicalDecl());
      if (itr != MostDerivedType.end()) { return itr->second; }
      return NULL;
   }

   bool emptyMap() {
      return MostDerivedType.empty();
   }

   /**
    * Once the recursive visitor has completed, this routine analyzes the
    * MostDerivedTypeMap to find errors in the structure of the OMR classes
    */
   void VerifyTypeStructure() {
      trace("Starting Structure Verification");

      for (std::map<CXXRecordDecl*, bool>::iterator I = Types.begin(), E=Types.end(); I != E; ++I) {
         CXXRecordDecl * Type        = I->first;
         bool extensible             = I->second;
         trace(Type << " " << extensible << " " << getAssociatedConcreteType(Type));
         if (extensible && !getAssociatedConcreteType(Type)) {
            trace("xxxx Issue diagnostic because there's no associated concrete type");
            continue;
         }
         if (extensible) {  //Extnsible type .
            trace("xxxx Verifying " << Type->getQualifiedNameAsString() << " has no non-extensible base classes." );
            for (CXXRecordDecl::base_class_iterator BI = Type->bases_begin(), BE = Type->bases_end(); BI != BE; ++BI) {
               CXXRecordDecl * base_class = BI->getType()->getAsCXXRecordDecl();
               if (base_class
                   && !isExtensible(base_class) // Ensure extensible parent.
                   && !isOMRRootType(Type)) {   // OMR Root type can have non-extensible parents.
                  //Base is not extensible, but an extensible type reaches it, with no concrete class in the middle.
                  //Issue diagnostic.
                  std::string diagnostic("OMR_EXTENSIBLE Type ");
                  diagnostic += Type->getQualifiedNameAsString();
                  diagnostic += " derives from ";
                  diagnostic += base_class->getQualifiedNameAsString();
                  diagnostic += " that is not marked as OMR_EXTENSIBLE.\n";

                  DiagnosticsEngine &diagEngine = Context->getDiagnostics();
                  unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "%0");
                  diagEngine.Report(base_class->getLocation(), diagID) << diagnostic;
               }
            }
         } else {
            trace("xxxx Verifying " << Type->getQualifiedNameAsString() << " has no non-extensible base classes." );
            for (CXXRecordDecl::base_class_iterator BI = Type->bases_begin(), BE = Type->bases_end(); BI != BE; ++BI) {
               CXXRecordDecl * base_class = BI->getType()->getAsCXXRecordDecl();
               if (base_class && isExtensible(base_class) && !isOMRConcreteType(base_class)) {
                  //Base is not extensible, but an extensible type reaches it, with no concrete class in the middle.
                  //Issue diagnostic.
                  std::string diagnostic("Type ");
                  diagnostic += Type->getQualifiedNameAsString();
                  diagnostic += " derives from ";
                  diagnostic += base_class->getQualifiedNameAsString();
                  diagnostic += " that is  marked as OMR_EXTENSIBLE.\n";

                  DiagnosticsEngine &diagEngine = Context->getDiagnostics();
                  unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "%0");
                  diagEngine.Report(Type->getLocation(), diagID) << diagnostic;
               }
            }
         }
      } //each most derived type
   }

   /**
    * Check if a CXX record decl is an OMR Root type
    */
   bool isOMRRootType(const CXXRecordDecl* decl) {
      return checkDeclForNamespace(decl, std::string("OMR"));
   }

   /**
    * Check if a CXX record decl is a concrete type.
    *
    * \FIXME: Right now, we assume any extensible decl in the TR:: namespace is a concrete type.
    */
   bool isOMRConcreteType(const CXXRecordDecl *decl) {
      return checkDeclForNamespace(decl, std::string("TR")) && isExtensible(decl);
   }

   bool checkDeclForNamespace(const CXXRecordDecl *decl, std::string concreteNamespace) {
      const DeclContext * context = decl->getCanonicalDecl()->getDeclContext();
      if (context->isNamespace()) {
         NamespaceDecl * nameDecl = NamespaceDecl::castFromDeclContext(context);
         if (nameDecl->getNameAsString() == concreteNamespace) {
            return true;
         }
      }
      return false;
   }


   /**
    * Return the concrete type in the same extensible class string as me, or
    * NULL if it can't be found.
    *
    * This query only really makes sense for an extensible class.
    */
   CXXRecordDecl* getAssociatedConcreteType(CXXRecordDecl* decl) {

      // a concrete type is always part of its own extensible class string.
      if (isOMRConcreteType(decl))
         return decl;

      // Only extensible decls have a meaningful concrete class string.
      if (isExtensible(decl)) {
         int loopCount = 0;
         CXXRecordDecl* concrete = mostDerivedType(decl);
         trace("==isExtensible: " << decl->getQualifiedNameAsString() << " concrete: " << (concrete ? concrete->getQualifiedNameAsString() : "no concrete found"));
         while (concrete) {
            if (loopCount++ > 50) {
               std::string diagnostic("Found more than 50 layers of classes, likely bug.");
               DiagnosticsEngine &diagEngine = Context->getDiagnostics();
               unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "%0");
               diagEngine.Report(decl->getLocation(), diagID) << diagnostic;
               exit(EXIT_FAILURE);
            }
            if (inSameClassString(decl, concrete)) {
               trace("=== in same string");
               return concrete;  //Found the concrete class in the same string.
            }

            concrete = mostDerivedType(concrete);
            if (concrete) trace("==== new concrete  " << concrete->getQualifiedNameAsString() << " for " << decl->getQualifiedNameAsString());
         }

         trace("Didn't find a concrete class for " << decl->getQualifiedNameAsString() << " ... issue diagnostic?")
      }
      return NULL;
   }

   bool isExtensible(const CXXRecordDecl* decl) {
      return Extensible.isExtensible(decl);
   }


private:

   /**
    * Return true iff the inhertiance from MostDerivedType to Type is not
    * broken by a concrete class.
    *
    *
    * \FIXME: This could be tighter integrated with getAssociatedConcreteType, reducing complexity.
    */
   bool inSameClassString(CXXRecordDecl * queryType, CXXRecordDecl * derivedType, int level = 0) {
      std::string query_name   = queryType->getQualifiedNameAsString();
      std::string derived_name = derivedType->getQualifiedNameAsString();

      std::string prefix("");
      for (int i = 0; i < level; i++) prefix += "\t";
      trace(prefix << "checking " << derived_name << " reaches  " << query_name);

      // If the two decls are the same, then in the same string.
      if (queryType->getCanonicalDecl() == derivedType->getCanonicalDecl())
        return true;


      //Keep searching upwards through all bases.
      // If any base is in the same class string as the query type, then
      // we are as well. This is a reachability problem.
      trace(prefix << query_name << "\t[ - ]  C = " << derivedType->getQualifiedNameAsString() );
      for (CXXRecordDecl::base_class_iterator BI = derivedType->bases_begin(), BE = derivedType->bases_end(); BI != BE; ++BI) {
         CXXRecordDecl* base_class = BI->getType()->getAsCXXRecordDecl();
         if (base_class) {
            if ( isOMRConcreteType(base_class) ) { // Concrete parent terminates search.
               trace(prefix << "not searching " << base_class->getQualifiedNameAsString() << ", is concrete");
            } else {
               if (inSameClassString(queryType, base_class, level + 1)) {
                  trace(prefix << query_name <<"\t[ " << level << " ] reached through " << base_class->getQualifiedNameAsString());
                  return true;
               } else {
                  trace(prefix<< query_name <<"\t[ " << level << " ] not reached reached through " << base_class->getQualifiedNameAsString())
               }
            }
         }
      }

      return false;
   }

   /**
    * For algorithmic verification, ensure that every most-derived-type actually is derived from type.
    */
   void AssertMostDerivedDerivesFromTypes(CXXRecordDecl* type, CXXRecordDecl *mostDerivedType)
      {
      //TODO
      return;
      }

   /**
    * Build the most derived type map.
    */
   void BuildMostDerivedTypeMap(CXXRecordDecl* decl) {
      std::queue<CXXRecordDecl *> toProcess;

      // Only handle complete definitions.
      if (!decl->isCompleteDefinition()) {
         return;
      }
      // Prefer canonical
      decl = decl->getCanonicalDecl();

      // Most derived type map only cares about extensible decls.
      if (!isExtensible(decl))
         return;

      if (MostDerivedType.find(decl) != MostDerivedType.end()
          && mostDerivedType(decl) == decl) {
         trace("\t" << decl->getQualifiedNameAsString() << " has most derived type.");
         return;
      }

      trace("\t\ttoProcess.push " << decl->getQualifiedNameAsString());
      toProcess.push(decl);
      // Add all non-concrete parent classes to processing queue.
      for (CXXRecordDecl::base_class_iterator BI = decl->bases_begin(), BE = decl->bases_end(); BI != BE; ++BI) {
         if (BI->getType()->getAsCXXRecordDecl() && !isOMRConcreteType(BI->getType()->getAsCXXRecordDecl())) {
            toProcess.push(BI->getType()->getAsCXXRecordDecl()->getCanonicalDecl());
         }
      }

      while (!toProcess.empty()) {
         CXXRecordDecl *toUpdate = toProcess.front();
         toProcess.pop();

         if (toUpdate != toUpdate->getCanonicalDecl()) trace("Missed cannonicalization")

         if (MostDerivedType.find(toUpdate) != MostDerivedType.end() // Found a most derived type.
             && MostDerivedType[toUpdate] != decl) {                 // ... and it wasn't this decl

            // Ensure we update everyone who used to think this was most derived
            CXXRecordDecl *oldDerivedType = MostDerivedType[toUpdate];
            for (std::map<CXXRecordDecl*, CXXRecordDecl*>::iterator I = MostDerivedType.begin(), E = MostDerivedType.end(); I != E; ++I) {
               trace("second == old: " << (I->second == oldDerivedType) );
               trace("First " << I->first->getQualifiedNameAsString()   << " Concrete: " << isOMRConcreteType(I->first));
               trace("Second " << I->second->getQualifiedNameAsString() << " Concrete: " << isOMRConcreteType(I->second));
               if (I->second == oldDerivedType
                   && !isOMRConcreteType(I->first) ) {
                  toProcess.push(I->first);
                  trace("\tQueuing previous most derived for processing " << I->first->getQualifiedNameAsString());
               }
            }

            MostDerivedType[toUpdate] = decl;
            trace("\t (X) Updating MDT mapping " << toUpdate->getQualifiedNameAsString() << " -> " << decl->getQualifiedNameAsString());
         } else {
            MostDerivedType[toUpdate] = decl;
            trace("\t (Y) Updating MDT mapping " << toUpdate->getQualifiedNameAsString() << " -> " << decl->getQualifiedNameAsString());
         }
      }
   }

   /**
    * The list of canonical types
    */
   std::map<CXXRecordDecl*, bool> Types;



   /**
    * The most derived type map.
    */
   std::map<CXXRecordDecl*, CXXRecordDecl*> MostDerivedType;

   /**
    * This visitor has determined the map of extensible classes
    */
   ExtensibleClassDiscoveryVisitor &Extensible;

   /**
    * Context, required for diagnostics
    */
   ASTContext *Context;
};


/**
 * Visits all C++ member call expressions, finding extensible class calls
 * that don't comply with the rules, issuing diagnostics.
 */
class OMRThisCheckingVisitor : public RecursiveASTVisitor<OMRThisCheckingVisitor> {
public:
   explicit OMRThisCheckingVisitor(ASTContext *Context, OMRClassCheckingVisitor *ClassChecker) : Context(Context), ClassChecker(ClassChecker) {
   }

   bool VisitCallExpr(CallExpr *call) {
      if (!isStaticExtensibleMemberCall(call)) {
         return true;
      }

      // check that the function has a scope qualifier
      DeclRefExpr *referencedFunc;
      if ((referencedFunc = dyn_cast<DeclRefExpr>(call->getCallee()->IgnoreImplicit())) && !referencedFunc->hasQualifier()) {
         // get the static function's parent class. This cast should be guarenteed to work but let's play it safe.
         CXXMethodDecl * methodDecl;
         if (!(methodDecl = dyn_cast<CXXMethodDecl>(call->getCalleeDecl()))) {
            return false;
         }
         DiagnosticsEngine &diagEngine = Context->getDiagnostics();
         unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "Static member function must be called with scope qualifier. Suggested fix:");
         DiagnosticBuilder builder = diagEngine.Report(call->getExprLoc(), diagID);
         std::string qualifierHint = "TR::" + methodDecl->getParent()->getNameAsString() + "::";
         builder.AddFixItHint(FixItHint::CreateInsertion(call->getExprLoc(), qualifierHint));
      }
      return true;
   }

   bool VisitCXXMemberCallExpr(CXXMemberCallExpr *call) {
      //TODO: Check that this member call expression is inside of an extensible class!
      if (!isExtensibleMemberCall(call)) {
         return true;
      }

      Expr *receiver = call->getImplicitObjectArgument()->IgnoreParenImpCasts();

      if (receiver->isImplicitCXXThis()) {
         // Don't diagnose implicit this recievers that are calling self.
         if (!isSelfCall(call)) {
            // Ignore member function calls that specifically call a base class member function
            MemberExpr * memberFunc;
            // hasQualifier checks for a nested name specifier, e.g. the 'IBM::Foo::' part of 'IBM::Foo::baz()'
            if ((memberFunc = dyn_cast<MemberExpr>(call->getCallee())) && memberFunc->hasQualifier()) {
               return true;
            }
            DiagnosticsEngine &diagEngine = Context->getDiagnostics();
            unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "Implicit this receivers are prohibited in extensible classes");
            DiagnosticBuilder builder = diagEngine.Report(receiver->getExprLoc(), diagID);
            std::string staticCastHint = "self()->";
            builder.AddFixItHint(FixItHint::CreateInsertion(receiver->getExprLoc(), staticCastHint));
         }
      } else if (isa<CXXStaticCastExpr>(receiver)) {
         CXXStaticCastExpr *cast = dyn_cast<CXXStaticCastExpr>(receiver);
         CXXRecordDecl *targetClass = cast->getType()->getAs<PointerType>() ? cast->getType()->getAs<PointerType>()->getPointeeType()->getAsCXXRecordDecl()->getCanonicalDecl() : NULL;
         trace("targetClass of static cast" << (targetClass ? targetClass->getQualifiedNameAsString() : "NULL" ) );
         CXXThisExpr *thisExpr = getThisExpr(cast->getSubExpr());
         if (thisExpr) {
            CXXRecordDecl *thisConcrete = ClassChecker->getAssociatedConcreteType(thisExpr->getType()->getAs<PointerType>()->getPointeeType()->getAsCXXRecordDecl());
            if (thisConcrete) {
               trace("Associated concrete class: " << thisConcrete->getQualifiedNameAsString() );
               if (targetClass == NULL || targetClass != thisConcrete) {
                  DiagnosticsEngine &diagEngine = Context->getDiagnostics();
                  unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "Static casts of this in extensible classes need to be the most derived type. The preferred way of handling this is actually a call to self()");
                  DiagnosticBuilder builder = diagEngine.Report(cast->getExprLoc(), diagID);
                  std::string staticCastHint = "self()->";
                  builder.AddFixItHint(FixItHint::CreateReplacement(cast->getExprLoc(), staticCastHint));
               } else {  // Static cast is correct, but we want a warning.
                  DiagnosticsEngine &diagEngine = Context->getDiagnostics();
                  unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Warning, "extensible class dereference prefers calls to self()");
                  DiagnosticBuilder builder = diagEngine.Report(cast->getExprLoc(), diagID);
                  std::string staticCastHint = "self()->";
                  builder.AddFixItHint(FixItHint::CreateReplacement(cast->getExprLoc(), staticCastHint));
               }
            } else {
               trace("didn't find a most derived for static cast target");
            }
         }
      } else {
         Expr *thisExpr = getThisExpr(receiver->IgnoreParenCasts());
         if (thisExpr) {
            DiagnosticsEngine &diagEngine = Context->getDiagnostics();
            unsigned diagID = diagEngine.getCustomDiagID(DiagnosticsEngine::Error, "Use of this in an extensible class can lead to unexpected behaviour");
            DiagnosticBuilder builder = diagEngine.Report(receiver->getExprLoc(), diagID);
            std::string staticCastHint = "self()->";
            builder.AddFixItHint(FixItHint::CreateReplacement(receiver->getExprLoc(), staticCastHint));
         }
      }
      return true;
   }

   /**
    * Return true iff the call is one to a extenxible downcasting self.
    *
    * \FIXME: This is currently done with a straightup strcmp.
    *         Definitely should be made more robust.
    */
   bool isSelfCall(CXXMemberCallExpr *call) {
      CXXMethodDecl* calleeDecl = call->getMethodDecl();
      DeclarationNameInfo nameInfo = calleeDecl->getNameInfo();
      std::string name = nameInfo.getName().getAsString();
      std::string prefix("self");
      return !name.compare(0, prefix.size(), prefix);
   }

   /**
    * Return true iff a CXXMemberCallExpr is
    * 1) contained inside an extensible class member function definition
    * 2) A call to a member function of an extensible class member.
    * 3) The callee is in the same class as the caller.
    */
   bool isExtensibleMemberCall(CXXMemberCallExpr* call) {
      // > Retrieves the implicit object argument for the member call.
      // > For example, in "x.f(5)", this returns the sub-expression "x".
      Expr*             reciever = call->getImplicitObjectArgument()->IgnoreParenImpCasts();

      if (getenv("OMR_CHECK_TRACE")) {
         llvm::errs() << "BestDynamicClassType => ";
         reciever->getBestDynamicClassType()->printQualifiedName(llvm::errs());
         llvm::errs() << "\n";
      }

      return isExtensible(reciever->getBestDynamicClassType());
   }

   /**
    * Return true iff
    * 1) the declaration corresponding to a CallExpr can be found (not type-dependend)
    * 2) a CallExpr is a call to a static member function of an extensible class.
    */
   bool isStaticExtensibleMemberCall(CallExpr* call) {
      // get the function's declaration
      CXXMethodDecl* methodDecl;
      auto calleeDecl = call->getCalleeDecl();

      // Checking that the pointer being casted is not null is necessary
      // because otherwise, `dyn_cast` will cause a segfault when compiled
      // using g++.

      if (calleeDecl && (methodDecl = dyn_cast<CXXMethodDecl>(calleeDecl))) {

         if (getenv("OMR_CHECK_TRACE")) {
            llvm::errs() << "Parent => ";
            llvm::errs() << methodDecl->getParent()->getNameAsString();
            llvm::errs() << "\n";
         }

         return methodDecl->isStatic() && isExtensible(methodDecl->getParent());
      }

      return false;
   }

private:
   bool isExtensible(const CXXRecordDecl* decl) {
      return ClassChecker->isExtensible(decl);
   }

   //const NamedDecl * getNamedDecl(

   ASTContext *Context;
   OMRClassCheckingVisitor *ClassChecker;
};

class OMRCheckingConsumer : public ASTConsumer {
public:
explicit OMRCheckingConsumer(llvm::StringRef filename) { }

virtual void HandleTranslationUnit(ASTContext &Context) {
   // Visit the classes, gathering information.
   ExtensibleClassDiscoveryVisitor extVisitor(&Context);
   if (extVisitor.TraverseDecl(Context.getTranslationUnitDecl())) {
      ExtensibleClassCheckingVisitor extchkVisitor(&Context, extVisitor);
      if (extchkVisitor.TraverseDecl(Context.getTranslationUnitDecl())) {
         OMRClassCheckingVisitor ClassVisitor(&Context, extVisitor);
         if (ClassVisitor.TraverseDecl(Context.getTranslationUnitDecl())) {
            // Visit this expressions, printing diagnostics.
            if (getenv("OMR_CHECK_TRACE_RELATIONS") || getenv("OMR_CHECK_TRACE"))
               ClassVisitor.printRelations();

            ClassVisitor.VerifyTypeStructure();

            if (ClassVisitor.emptyMap() && !getenv("OMR_CHECK_FORCE_THIS_VISIT"))
               return;

            OMRThisCheckingVisitor ThisVisitor(&Context, &ClassVisitor);
            if (!ThisVisitor.TraverseDecl(Context.getTranslationUnitDecl()))
               {
               llvm::errs() << "This visitor ended early?\n";
               }
         }
      }
   }
}
};

class CheckingAction : public PluginASTAction {
protected:

#if defined(LLVM34)
  ASTConsumer * CreateASTConsumer(CompilerInstance &CI, llvm::StringRef filename) {
    return new OMRCheckingConsumer(filename);
  }
#elif defined(LLVM36) || defined(LLVM38)
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef filename) {
    return std::unique_ptr<ASTConsumer>(new OMRCheckingConsumer(filename));
  }
#else
#error unknown version of LLVM
#endif

  /**
   * Required function -- pure virtual in parent
   */
   bool ParseArgs(const CompilerInstance &CI,
                  const std::vector<std::string>& args) {
     /*for (unsigned i = 0, e = args.size(); i != e; ++i) {
       llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

       // Example error handling.
       if (args[i] == "-an-error") {
         DiagnosticsEngine &D = CI.getDiagnostics();
         unsigned DiagID = D.getCustomDiagID(
           DiagnosticsEngine::Error, "invalid argument '" + args[i] + "'");
         D.Report(DiagID);
         return false;
       }
     }
     if (args.size() && args[0] == "help")
       PrintHelp(llvm::errs());
     */
     return true;

   }

  /**
   * Required function -- pure virtual in parent
   */
   void PrintHelp(llvm::raw_ostream& ros) {
     //ros << "Help for PrintAtoms plugin goes here\n";
   }
};


/**
 * Strip away parentheses and casts we don't care about.
 */
static CXXThisExpr *getThisExpr(Expr *E) {
   while (true) {
     if (ParenExpr *Paren = dyn_cast<ParenExpr>(E)) {
       E = Paren->getSubExpr();
       continue;
     }

     if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
       if (ICE->getCastKind() == CK_NoOp ||
           ICE->getCastKind() == CK_LValueToRValue ||
           ICE->getCastKind() == CK_DerivedToBase ||
           ICE->getCastKind() == CK_UncheckedDerivedToBase) {
         E = ICE->getSubExpr();
         continue;
       }
     }

     if (UnaryOperator* UnOp = dyn_cast<UnaryOperator>(E)) {
       if (UnOp->getOpcode() == UO_Extension) {
         E = UnOp->getSubExpr();
         continue;
       }
     }

     if (MaterializeTemporaryExpr *M = dyn_cast<MaterializeTemporaryExpr>(E)) {
       E = M->GetTemporaryExpr();
       continue;
     }

     break;
   }

   return dyn_cast_or_null<CXXThisExpr>(E);
}


}
#undef trace
#endif
