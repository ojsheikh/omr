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

#ifndef S390BINARYANALYSER_INCL
#define S390BINARYANALYSER_INCL

#include <stdint.h>                   // for uint8_t
#include "codegen/Analyser.hpp"       // for NUM_ACTIONS, TR_Analyser
#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "codegen/InstOpCode.hpp"     // for InstOpCode, etc
#include "compile/Compilation.hpp"    // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

namespace TR { class Node; }
namespace TR { class Register; }

#define EvalChild1   0x01
#define EvalChild2   0x02
#define CopyReg1     0x04
#define BinaryReg1Reg2  0x08
#define BinaryReg3Reg2  0x10
#define BinaryReg1Mem2  0x20
#define BinaryReg3Mem2  0x40
#define InvalidBACEMap  0x80

class TR_S390BinaryAnalyser : public TR_Analyser
   {

   static const uint8_t  actionMap[NUM_ACTIONS];
   static const uint8_t  clobEvalActionMap[NUM_ACTIONS];

   TR::CodeGenerator *_cg;

   public:

   TR_S390BinaryAnalyser(TR::CodeGenerator *cg) : _cg(cg) {}

   void genericAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode,
                                TR::InstOpCode::Mnemonic copyOpCode);

   void intBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {

     TR::InstOpCode::Mnemonic loadOp = TR::InstOpCode::getLoadRegOpCode();
     if (cg()->supportsHighWordFacility() && !cg()->comp()->getOption(TR_DisableHighWordRA))
        {
        loadOp = TR::InstOpCode::LR;
        }
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,loadOp);
     }

   void longBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LGR);
     }

   void floatBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LER);
     }


   void doubleBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LDR);
     }

   void longSubtractAnalyser(TR::Node *root);

   bool getEvalChild1()  {return (actionMap[getInputs()] & EvalChild1)  ? true : false;}
   bool getEvalChild2()  {return (actionMap[getInputs()] & EvalChild2)  ? true : false;}

   bool getCopyReg1()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & CopyReg1)    ? true : false;
         }
      return (actionMap[getInputs()] & CopyReg1)    ? true : false;
      }

   bool getBinaryReg1Reg2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg1Reg2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg1Reg2) ? true : false;
      }

   bool getBinaryReg3Reg2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg3Reg2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg3Reg2) ? true : false;
      }

   bool getBinaryReg1Mem2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg1Mem2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg1Mem2) ? true : false;
      }

   bool getBinaryReg3Mem2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg3Mem2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg3Mem2) ? true : false;
      }

   bool getInvalid()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & InvalidBACEMap) ? true : false;
         }
      return false;
      }

   TR::CodeGenerator *cg()               {return _cg;}

   private:

   void remapInputs(TR::Node *, TR::Register *, TR::Node *, TR::Register *);
   };

#endif
