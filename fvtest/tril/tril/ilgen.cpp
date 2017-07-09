/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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

#include "ilgen.hpp"

#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

#include <unordered_map>
#include <string>

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

/**
 * @brief A table to map the string representation of opcode names to corresponding TR::OpCodes value
 */
class OpCodeTable : public TR::ILOpCode {
   public:
      explicit OpCodeTable(TR::ILOpCodes opcode) : TR::ILOpCode{opcode} {}
      explicit OpCodeTable(const std::string& name) : TR::ILOpCode{getOpCodeFromName(name)} {}

      /**
       * @brief Given an opcode name, returns the corresponding TR::OpCodes value
       */
      static TR::ILOpCodes getOpCodeFromName(const std::string& name) {
         auto opcode = _opcodeNameMap.find(name);
         if (opcode == _opcodeNameMap.cend()) {
            for (const auto& p : _opCodeProperties) {
               if (name == p.name) {
                  _opcodeNameMap[name] = p.opcode;
                  return p.opcode;
               }
            }
            return TR::BadILOp;
         }
         else {
            return opcode->second;
         }
      }

   private:
      static std::unordered_map<std::string, TR::ILOpCodes> _opcodeNameMap;
};

std::unordered_map<std::string, TR::ILOpCodes> OpCodeTable::_opcodeNameMap;

/*
 * The general algorithm for generating a TR::Node from it's AST representation
 * is like this:
 *
 * 1. Allocate a new TR::Node instance, using the AST node's name
 *    to detemine the opcode
 * 2. Set any special values, flags, or properties on the newly created TR::Node
 *    based on the AST node arguments
 * 3. Recursively call this function to generate the child nodes and set them
 *    as children of the current TR::Node
 *
 * However, certain opcodes must be created using a special interface. For this
 * reason, special opcodes are detected using opcode properties.
 */
TR::Node* Tril::TRLangBuilder::toTRNode(const ASTNode* const tree) {
     TR::Node* node = nullptr;

     auto childCount = countNodes(tree->children);

     if (strcmp("@common", tree->name) == 0) {
         auto idArg = getArgByName(tree, "id");
         auto id = idArg->value->value.str;
         auto iter = _nodeMap.find(id);
         if (iter != _nodeMap.end()) {
             auto n = iter->second;
             TraceIL("Commoning node n%dn (%p) from ASTNode %p (ID \"%s\")\n", n->getGlobalIndex(), n, tree, id);
             return n;
         }
         else {
             TraceIL("Failed to find node for commoning (id=\"%s\")\n", id)
             return nullptr;
         }
     }

     auto opcode = OpCodeTable{tree->name};

     TraceIL("Creating %s from ASTNode %p\n", opcode.getName(), tree);
     if (opcode.isLoadConst()) {
        TraceIL("  is load const of ", "");
        node = TR::Node::create(opcode.getOpCodeValue(), childCount);

        // assume the constant to be loaded is the first argument of the AST node
        if (opcode.isIntegerOrAddress()) {
           node->set64bitIntegralValue(tree->args->value->value.int64);
           TraceIL("integral value %d\n", tree->args->value->value.int64);
        }
        else {
           switch (opcode.getType()) {
              case TR::Float:
                 node->setFloat(static_cast<float>(tree->args->value->value.f64));
                 break;
              case TR::Double:
                 node->setDouble(tree->args->value->value.f64);
                 break;
              default:
                 return nullptr;
           }
           TraceIL("floating point value %f\n", tree->args->value->value.f64);
        }
     }
     else if (opcode.isLoadDirect()) {
        TraceIL("  is direct load of ", "");

        // the name of the first argument tells us what kind of symref we're loading
        if (strcmp("parm", tree->args->name) == 0) {
             auto arg = tree->args->value->value.int64;
             TraceIL("parameter %d\n", arg);
             auto symref = symRefTab()->findOrCreateAutoSymbol(_methodSymbol,
                                                               static_cast<int32_t>(arg),
                                                               opcode.getType() );
             node = TR::Node::createLoad(symref);
         }
         else if (strcmp("temp", tree->args->name) == 0) {
             const auto symName = tree->args->value->value.str;
             TraceIL("temporary %s\n", symName);
             auto symref = _symRefMap[symName];
             node = TR::Node::createLoad(symref);
         }
         else {
             // symref kind not recognized
             return nullptr;
         }
     }
     else if (opcode.isStoreDirect()) {
        TraceIL("  is direct store of ", "");

        // the name of the first argument tells us what kind of symref we're storing to
        if (strcmp("temp", tree->args->name) == 0) {
            const auto symName = tree->args->value->value.str;
            TraceIL("temporary %s\n", symName);

            // check if a symref has already been created for the temp
            // and if not, create one
            if (_symRefMap.find(symName) == _symRefMap.end()) {
                _symRefMap[symName] = symRefTab()->createTemporary(methodSymbol(), opcode.getDataType());
            }

            auto symref =_symRefMap[symName];
            node = TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, symref);
        }
        else {
            // symref kind not recognized
            return nullptr;
        }
     }
     else if (opcode.isLoadIndirect() || opcode.isStoreIndirect()) {
         // the first AST node argument holds the offset
         auto offset = tree->args->value->value.int64;
         TraceIL("  is indirect store/load with offset %d\n", offset);
         const auto name = tree->name;
         auto type = opcode.getType();
         auto compilation = TR::comp();
         TR::Symbol *sym = TR::Symbol::createNamedShadow(compilation->trHeapMemory(), type, TR::DataType::getSize(opcode.getType()), name);
         TR::SymbolReference *symref = new (compilation->trHeapMemory()) TR::SymbolReference(compilation->getSymRefTab(), sym, compilation->getMethodSymbol()->getResolvedMethodIndex(), -1);
         symref->setOffset(offset);
         node = TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, symref);
     }
     else if (opcode.isIf()) {
         const auto targetName = tree->args->value->value.str;
         auto targetId = _blockMap[targetName];
         auto targetEntry = _blocks[targetId]->getEntry();
         TraceIL("  is if with target block %d (%s, entry = %p", targetId, targetName, targetEntry);

         /* If jumps must be created using `TR::Node::createif()`, which expected
          * two child nodes to be given as argument. However, because children
          * are only processed at the end, we create a dummy `BadILOp` node and
          * pass it as both the first and second child. When the children are
          * eventually created, they will override the dummy.
          */
         auto c1 = TR::Node::create(TR::BadILOp);
         auto c2 = c1;
         TraceIL("  created temporary %s n%dn (%p)\n", c1->getOpCode().getName(), c1->getGlobalIndex(), c1);
         node = TR::Node::createif(opcode.getOpCodeValue(), c1, c2, targetEntry);
     }
     else if (opcode.isBranch()) {
         const auto targetName = tree->args->value->value.str;
         auto targetId = _blockMap[targetName];
         auto targetEntry = _blocks[targetId]->getEntry();
         TraceIL("  is branch to target block %d (%s, entry = %p", targetId, targetName, targetEntry);
         node = TR::Node::create(opcode.getOpCodeValue(), childCount);
         node->setBranchDestination(targetEntry);
     }
     else {
        TraceIL("  unrecognized opcode; using default creation mechanism\n", "");
        node = TR::Node::create(opcode.getOpCodeValue(), childCount);
     }
     TraceIL("  node address %p\n", node);
     TraceIL("  node index n%dn\n", node->getGlobalIndex());

     auto nodeIdArg = getArgByName(tree, "id");
     if (nodeIdArg != nullptr) {
         auto id = nodeIdArg->value->value.str;
         _nodeMap[id] = node;
         TraceIL("  node ID %s\n", id);
     }

     // create a set child nodes
     const ASTNode* t = tree->children;
     int i = 0;
     while (t) {
         auto child = toTRNode(t);
         TraceIL("Setting n%dn (%p) as child %d of n%dn (%p)\n", child->getGlobalIndex(), child, i, node->getGlobalIndex(), node);
         node->setAndIncChild(i, child);
         t = t->next;
         ++i;
     }

     return node;
}

/*
 * The CFG is generated by doing a post-order walk of the AST and creating edges
 * whenever opcodes that affect control flow are visited. As is the case in
 * `toTRNode`, the opcode properties are used to determine how a particular
 * opcode affects the control flow.
 *
 * For the fall-through edge, the assumption is that one is always needed unless
 * a node specifically adds one (e.g. goto, return, etc.).
 */
bool Tril::TRLangBuilder::cfgFor(const ASTNode* const tree) {
   auto isFallthroughNeeded = true;

   // visit the children first
   const ASTNode* t = tree->children;
   while (t) {
       isFallthroughNeeded = isFallthroughNeeded && cfgFor(t);
       t = t->next;
   }

   auto opcode = OpCodeTable{tree->name};

   if (opcode.isReturn()) {
       cfg()->addEdge(_currentBlock, cfg()->getEnd());
       isFallthroughNeeded = false;
       TraceIL("Added CFG edge from block %d to @exit -> %s\n", _currentBlockNumber, tree->name);
   }
   else if (opcode.isBranch()) {
      const auto targetName = tree->args->value->value.str;
      auto targetId = _blockMap[targetName];
      cfg()->addEdge(_currentBlock, _blocks[targetId]);
      isFallthroughNeeded = isFallthroughNeeded && opcode.isIf();
      TraceIL("Added CFG edge from block %d to block %d (\"%s\") -> %s\n", _currentBlockNumber, targetId, targetName, tree->name);
   }

   if (!isFallthroughNeeded) {
       TraceIL("  (no fall-through needed)\n", "");
   }

   return isFallthroughNeeded;
}

/*
 * Generating IL from a Tril AST is done in three steps:
 *
 * 1. Generate basic blocks for each block represented in the AST
 * 2. Generate the IL itself (Trees) by walking the AST
 * 3. Generate the CFG by walking the AST
 */
bool Tril::TRLangBuilder::injectIL() {
    TraceIL("=== %s ===\n", "Generating Blocks");

    // the top level nodes of the AST should be all the basic blocks
    createBlocks(countNodes(_trees));

    // evaluate the arguments for each basic block
    const ASTNode* block = _trees;
    auto blockIndex = 0;

    // iterate over each argument for each basic block
    while (block) {
       const ASTNodeArg* a = block->args;
       while (a) {
           if (strcmp("name", a->name) == 0) {
               _blockMap[a->value->value.str] = blockIndex;
               TraceIL("Name of block %d set to \"%s\" (%p)\n", blockIndex, a->value->value.str);
           }
           a = a->next;
       }
       ++blockIndex;
       block = block->next;
    }

    TraceIL("=== %s ===\n", "Generating IL");
    block = _trees;
    generateToBlock(0);

    // iterate over each treetop in each basic block
    while (block) {
       const ASTNode* t = block->children;
       while (t) {
           auto node = toTRNode(t);
           const auto tt = genTreeTop(node);
           TraceIL("Created TreeTop %p for node n%dn (%p)\n", tt, node->getGlobalIndex(), node);
           t = t->next;
       }
       generateToBlock(_currentBlockNumber + 1);
       block = block->next;
    }

    TraceIL("=== %s ===\n", "Generating CFG");
    block = _trees;
    generateToBlock(0);

    // iterate over each basic block
    while (block) {
       auto isFallthroughNeeded = true;

       // create CFG edges from the nodes withing the current basic block
       const ASTNode* t = block->children;
       while (t) {
           isFallthroughNeeded = isFallthroughNeeded && cfgFor(t);
           t = t->next;
       }

       // create fall-through edge
       auto fallthroughArg = getArgByName(block, "fallthrough");
       if (fallthroughArg != nullptr) {
           auto target = std::string(fallthroughArg->value->value.str);
           if (target == "@exit") {
               cfg()->addEdge(_currentBlock, cfg()->getEnd());
               TraceIL("Added fallthrough edge from block %d to \"%s\"\n", _currentBlockNumber, target.c_str());
           }
           else if (target == "@none") {
               // do nothing, no fall-throught block specified
           }
           else {
               auto destBlock = _blockMap.at(target);
               cfg()->addEdge(_currentBlock, _blocks[destBlock]);
               TraceIL("Added fallthrough edge from block %d to block %d \"%s\"\n", _currentBlockNumber, destBlock, target.c_str());
           }
       }
       else if (isFallthroughNeeded) {
           auto dest = _currentBlockNumber + 1 == numBlocks() ? cfg()->getEnd() : _blocks[_currentBlockNumber + 1];
           cfg()->addEdge(_currentBlock, dest);
           TraceIL("Added fallthrough edge from block %d to following block\n", _currentBlockNumber);
       }

       generateToBlock(_currentBlockNumber + 1);
       block = block->next;
    }

    return true;
}
