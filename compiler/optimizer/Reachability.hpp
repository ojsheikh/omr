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

#ifndef REACHABILITY_INCL
#define REACHABILITY_INCL

#include <stdint.h>  // for int32_t

class TR_BitVector;
namespace TR { class Block; }
namespace TR { class Compilation; }

typedef int32_t blocknum_t;

/* This is essentially a mini dataflow engine that can compute reachability in
 * linear time, in one pass over any CFG.  It does not have the full generality
 * of the real dataflow engine (which can't be done in linear time due to kill
 * sets), but can very quickly compute which blocks are reachable from a given
 * set of blocks.
 *
 * TO USE THIS INSTEAD OF THE DATAFLOW ENGINE
 * - You must be computing just one bit per block
 * - The dataflow problem must be "any-path"
 * - The dataflow problem must have no kill sets
 *
 * To add a new reachability analysis, derive your class from either
 * TR_ForwardReachability or TR_BackwardReachability, and implement the
 * isOrigin function.  Then, the perform function will populate a
 * bitvector with the set of blocks reachable from any block for which
 * isOrigin returned "true".
 *
 * IMPLEMENTATION NOTES
 * The algorithm is DeRemer and Penello's so-called "digraph" algorithm which,
 * despite its odd name, can indeed handle cyclic graphs.
 *
 * See: "Efficient Computation of LALR(1) Look-Ahead Sets" p.625, DeRemer and Penello 1982.
 *
 * AREAS FOR IMPROVEMENT
 * - Compute a bitvector at each block, instead of a single boolean, so
 *   multiple reachability problems can be solved simultaneously in one pass.
 * - Change recursion into iteration to avoid stack overflow on large CFGs
 * - Perhaps separate propagateInputs from isOrigin into separate classes so
 *   they could be composed in arbitrary ways.
 * - Perhaps use templates to improve efficiency by using static dispatch
 *   instead of virtual functions.
 */

class TR_ReachabilityAnalysis
   {
   TR::Compilation *_comp;
   TR::Block      **_blocks;

   protected:

   TR::Compilation *comp(){ return _comp; }

   TR::Block *getBlock(blocknum_t n){ return _blocks[n]; }

   virtual bool isOrigin(TR::Block *block) = 0;

   void traverse(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure);

   virtual void propagateInputs(
      blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure)=0;

   void propagateOneInput(blocknum_t inputBlockNum,
      blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure);

   public:

   TR_ReachabilityAnalysis(TR::Compilation *comp);

   void perform(TR_BitVector *result);

   };

// Forward Reachability answers "which blocks can be reached from an origin block"
//
class TR_ForwardReachability: public TR_ReachabilityAnalysis
   {
   protected:
   virtual void propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure);

   public:

   TR_ForwardReachability(TR::Compilation *comp):TR_ReachabilityAnalysis(comp){}
   };

class TR_ForwardReachabilityWithoutExceptionEdges: public TR_ReachabilityAnalysis
   {
   protected:
   virtual void propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure);

   public:

   TR_ForwardReachabilityWithoutExceptionEdges(TR::Compilation *comp):TR_ReachabilityAnalysis(comp){}
   };

// Backward Reachability answers "which blocks can reach an origin block"
//
class TR_BackwardReachability: public TR_ReachabilityAnalysis
   {
   protected:
   virtual void propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure);

   public:

   TR_BackwardReachability(TR::Compilation *comp):TR_ReachabilityAnalysis(comp){}
   };

/////////////////////////////
//
// Specific analyses
//

class TR_CanReachGivenBlocks: public TR_BackwardReachability
   {
   TR_BitVector *_originBlocks;

   protected:
   virtual bool isOrigin(TR::Block *block);

   public:

   TR_CanReachGivenBlocks(TR::Compilation *comp, TR_BitVector *originBlocks):TR_BackwardReachability(comp),_originBlocks(originBlocks){}
   };

class TR_CanReachNonColdBlocks: public TR_BackwardReachability
   {
   protected:
   virtual bool isOrigin(TR::Block *block);

   public:

   TR_CanReachNonColdBlocks(TR::Compilation *comp):TR_BackwardReachability(comp){}
   };

class TR_CanBeReachedFromCatchBlock: public TR_ForwardReachability
   {
   protected:
   virtual bool isOrigin(TR::Block *block);

   public:

   TR_CanBeReachedFromCatchBlock(TR::Compilation *comp):TR_ForwardReachability(comp){}
   };

class TR_CanBeReachedWithoutExceptionEdges: public TR_ForwardReachabilityWithoutExceptionEdges
   {
   protected:
   virtual bool isOrigin(TR::Block *block);

   public:

   TR_CanBeReachedWithoutExceptionEdges(TR::Compilation *comp):TR_ForwardReachabilityWithoutExceptionEdges(comp){}
   };

#endif
