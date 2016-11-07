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
#include "optimizer/RematTools.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                   // for ILOpCode, etc
#include "il/Node_inlines.hpp"
#include "il/TreeTop_inlines.hpp"

void RematSafetyInformation::add(TR::TreeTop *argStore, TR::SparseBitVector &symRefDependencies)
   {
   dependentSymRefs.push_back(symRefDependencies);
   argumentTreeTops.push_back(argStore);
   rematTreeTops.push_back(argStore);
   }
   
void RematSafetyInformation::add(TR::TreeTop *argStore, TR::TreeTop *rematStore)
   {
   TR::SparseBitVector symrefDeps(comp->allocator());
   symrefDeps[rematStore->getNode()->getSymbolReference()->getReferenceNumber()] = true;
   dependentSymRefs.push_back(symrefDeps);
   argumentTreeTops.push_back(argStore);
   rematTreeTops.push_back(rematStore);
   }
   
void RematSafetyInformation::dumpInfo(TR::Compilation *comp)
   {
   for (uint32_t i = 0; i < dependentSymRefs.size(); ++i)
      {
      traceMsg(comp,"  Arg Remat Safety Info for priv arg store node %d", argumentTreeTops[i]->getNode()->getGlobalIndex());
      if (rematTreeTops[i])
         {
         if (rematTreeTops[i] != argumentTreeTops[i])
            traceMsg(comp,"     partial remat candidate node %d", rematTreeTops[i]->getNode()->getGlobalIndex());
         else
            traceMsg(comp,"     node candidate for full remat");
         traceMsg(comp,"    dependent symrefs: ");
         (*comp) << dependentSymRefs[i];
         traceMsg(comp,"\n");
         }
      else
         {
         traceMsg(comp,"    candidate is unsafe for remat - no candidates under consideration");
         }
      }
   }
   
/*
 * This function is used to walk the nodes from each of the privatized inliner
 * args down to (a) gather nodes we need to check for safety and (b) prempt
 * further checking if not needed
 * Return TR_yes if the node is a candidate for remat after safety checks
 * (eg it is an expression containing loads and constants), TR_maybe if it is a
 * constant expression (we won't duplicate because constant prop should sort these),
 * TR_no if remat is unsafe
 * Side-effect - adds node to scanTarget if returning true, otherwise none
 */
TR_YesNoMaybe RematTools::gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace, TR::SparseBitVector &visitedNodes)
   {
   visitedNodes[currentNode->getGlobalIndex()] = true;

   TR::ILOpCode &opCode = currentNode->getOpCode();
   if (opCode.hasSymbolReference() && !opCode.isLoad())
      {
      if (trace)
         traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - non-load with a symref", privArg, currentNode);
      return TR_no;
      }
   else if (opCode.isLoadVarDirect())
      {
      if (currentNode->getSymbolReference()->hasKnownObjectIndex() ||
          currentNode->getSymbol()->isConstObjectRef())
         {
         // In these limited cases we are dealing with a known constant object
         // which we can trust so we don't need to scan for safety
         return TR_yes;
         }
      else
         {
         scanTargets[currentNode->getGlobalIndex()] = true;
         symRefsToCheck[currentNode->getSymbolReference()->getReferenceNumber()] = true;
         return TR_yes;
         }
      }
   else if (opCode.isLoadIndirect())
      {
      // cannot rematerialize an array access without also creating a spine check
      // avoiding this case for now but this case can be handled in the future by creating a spine check
      //
      if (comp->requiresSpineChecks() && currentNode->getSymbol()->isArrayShadowSymbol())
         {
         if (trace)
            traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - array access needs spine check", privArg, currentNode);

         return TR_no;
         }

      // only add this node to the checking set if the root of the dereference
      // chain does not prempt remat checks
      if (gatherNodesToCheck(comp, privArg, currentNode->getFirstChild(), scanTargets, symRefsToCheck, trace, visitedNodes) != TR_no)
         {
         scanTargets[currentNode->getGlobalIndex()] = true;
         symRefsToCheck[currentNode->getSymbolReference()->getReferenceNumber()] = true;
         return TR_yes;
         }
      // chained TR_no so no diagnostics here
      return TR_no;
      }
   else if (opCode.isLoadConst())
      {
      // constants are ok too, but we don't need to clone them since
      // constant prop should take care of them
      return TR_maybe;
      }
   else if (opCode.isArithmetic()  || opCode.isConversion() ||
            opCode.isMax()         || opCode.isMin()        ||
            opCode.isArrayLength() || (opCode.isBooleanCompare() && !opCode.isBranch())
           )
      {
      // gather the child nodes of interest separately until we know it is worth checking them
      TR::SparseBitVector childScanTargets(comp->allocator());
      TR::SparseBitVector childSymRefsToCheck(comp->allocator());
      TR_YesNoMaybe candidateForRemat = TR_maybe;
      for (uint16_t i = 0; i < currentNode->getNumChildren(); ++i)
         {
         // avoid exponential traversal
         if (visitedNodes.ValueAt(currentNode->getChild(i)->getGlobalIndex()))
            continue;

         TR_YesNoMaybe childResult = gatherNodesToCheck(comp, privArg, currentNode->getChild(i), childScanTargets,
                                                   childSymRefsToCheck, trace, visitedNodes);
         if (childResult == TR_no)
            {
            if (trace)
               traceMsg(comp, "  priv arg remat: Can't fully remat [%p] due to [%p] - unsafe arithmetic child %d", privArg, currentNode, i);
            candidateForRemat = TR_no;
            break;
            }
         else if (childResult == TR_yes)
            {
            candidateForRemat = TR_yes;
            }
         }
      // only union in child symrefs if there is benefit from doing so
      if (candidateForRemat == TR_yes)
         {
         scanTargets |= childScanTargets;
         symRefsToCheck |= childSymRefsToCheck;
         }
      return candidateForRemat;
      }
   else
      {
      // anything we haven't considered is dangerous as a conservative assumption
      if (trace)
         traceMsg(comp, "  guarded call remat: Can't fully remat [%p] due to [%p] - unhandled case", privArg, currentNode);
      return TR_no;
      }
   }
   
TR_YesNoMaybe RematTools::gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace)
   {
   TR::SparseBitVector visitedNodes(comp->allocator());
   return gatherNodesToCheck(comp, privArg, currentNode, scanTargets,
                             symRefsToCheck, trace, visitedNodes);
   }
   
void RematTools::walkNodesCalculatingRematSafety(TR::Compilation *comp,
      TR::Node *currentNode, TR::SparseBitVector &scanTargets, 
      TR::SparseBitVector &enabledSymRefs, TR::SparseBitVector &unsafeSymRefs,
      bool trace, TR::SparseBitVector &visitedNodes)
   {
   for (uint16_t i = 0; i < currentNode->getNumChildren(); ++i)
      {
      if (visitedNodes.ValueAt(currentNode->getGlobalIndex()))
         return;

      visitedNodes[currentNode->getGlobalIndex()] = true;

      walkNodesCalculatingRematSafety(comp, currentNode->getChild(i),
            scanTargets, enabledSymRefs, unsafeSymRefs, trace, visitedNodes);
      }

   // step 1 - add the current node kills to our running kills
   currentNode->mayKill().getAliasesAndUnionWith(unsafeSymRefs);

   TR::ILOpCode &opCode = currentNode->getOpCode();
   if (opCode.isLikeDef() && opCode.hasSymbolReference())
      unsafeSymRefs[currentNode->getSymbolReference()->getReferenceNumber()] = true;

   // update unsafeSymRefs - only set enabledSymRefs since the others are not "live"
   unsafeSymRefs &= enabledSymRefs;

   // step 2 - if we have encountered a node we are checking for, update
   // enabledSymRefs
   if (scanTargets.ValueAt(currentNode->getGlobalIndex()) && opCode.hasSymbolReference())
      {
      // enable the symref we just found
      enabledSymRefs[currentNode->getSymbolReference()->getReferenceNumber()] = true;
      }
   }
   
/*
 * This method walks the nodes between the start treetop inclusive and
 * the end treetop exclusive. When it encounters a node in the scanTargets
 * it adds the symref of the scan target to its set of interest and continues
 * scanning. Any symrefs killed after they have been added to the set of interest
 * are added to the unsafeSymRefs vector for later use
 */
void RematTools::walkTreesCalculatingRematSafety(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::SparseBitVector &scanTargets, 
   TR::SparseBitVector &unsafeSymRefs, bool trace)
   {
   TR::SparseBitVector enabledSymRefs(comp->allocator());
   TR::SparseBitVector visitedNodes(comp->allocator());
   while (start != end)
      {
      if (start->getNode())
         {
         walkNodesCalculatingRematSafety(comp, start->getNode(),
            scanTargets, enabledSymRefs, unsafeSymRefs, trace, visitedNodes);
         }
      start = start->getNextTreeTop();
      }
   }
   
/*
 * This method is used to find alternative temps which we might be able to load from
 * if we can't full rematerialize a privatized inliner arg. If we find a store of
 * one of the failed arguments, we record the store node and add the symref to the
 * dependencies for the arg. If the symref is not killed before the end of the block
 * we do a partial rematerialization by simply loading the temp.
 */
void RematTools::walkTreeTopsCalculatingRematFailureAlternatives(TR::Compilation *comp,
   TR::TreeTop *start, TR::TreeTop *end, TR::list<TR::TreeTop*> &failedArgs,
   TR::SparseBitVector &scanTargets, RematSafetyInformation &rematInfo, bool trace)
   {
   TR::SparseBitVector failedTargets(comp->allocator());
   for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
      {
      failedTargets[(*iter)->getNode()->getFirstChild()->getGlobalIndex()] = true;
      }

   while (start != end)
      {
      if (trace)
         traceMsg(comp, "  priv arg remat: visiting [%p]: isStore %d privArg %d failedTargetMatch %d", start->getNode(),
            start->getNode()->getOpCode().isStoreDirect(),
            start->getNode()->getSymbol() && start->getNode()->getSymbol()->isAuto(),
            failedTargets.ValueAt(start->getNode()->getFirstChild()->getGlobalIndex()));
      if (start->getNode() && start->getNode()->getOpCode().isStoreDirect() &&
          start->getNode()->getSymbol()->isAuto() &&
          failedTargets.ValueAt(start->getNode()->getFirstChild()->getGlobalIndex()))
         {
         if (trace)
            traceMsg(comp, "  priv arg remat: considering partial remat with node [%p]", start->getNode());
         for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
            {
            if ((*iter) &&
                (*iter)->getNode()->getOpCode().hasSymbolReference() &&
                (*iter)->getNode()->getSymbolReference()->getReferenceNumber() != start->getNode()->getSymbolReference()->getReferenceNumber() &&
                (*iter)->getNode()->getFirstChild() == start->getNode()->getFirstChild())
                {
                if (trace)
                   traceMsg(comp, "  priv arg remat: attempting partial remat with node [%p] for [%p]", start->getNode(), (*iter)->getNode());
                rematInfo.add(*iter, start);
                scanTargets[start->getNode()->getGlobalIndex()] = true;
                failedTargets[start->getNode()->getFirstChild()->getGlobalIndex()] = false;
                *iter = NULL;
                }
            }
         }
      start = start->getNextTreeTop();
      }
   }
