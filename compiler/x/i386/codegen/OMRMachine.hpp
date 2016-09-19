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

#ifndef OMR_IA32_MACHINE_INCL
#define OMR_IA32_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINEBASE_CONNECTOR
#define OMR_MACHINEBASE_CONNECTOR
namespace OMR { namespace X86 { namespace i386 { class Machine; } } }
namespace OMR { typedef OMR::X86::i386::Machine MachineConnector; }
#else
#error OMR::IA32::Machine expected to be a primary connector, but an OMR connector is already defined
#endif

#include "x/codegen/OMRMachine.hpp"

#include <stdint.h>                           // for uint32_t

namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }
namespace TR { class Register; }
namespace TR { class Machine; }

namespace OMR
{

namespace X86
{

namespace i386
{

class OMR_EXTENSIBLE Machine : public OMR::X86::Machine
   {

   enum
      {
      IA32_NUM_GPR              = 8,
      IA32_NUM_FPR              = 8,
      IA32_NUM_XMMR             = 8,
      IA32_MAX_GLOBAL_GPRS      = 7,
      IA32_MAX_8BIT_GLOBAL_GPRS = 4,
      IA32_MAX_GLOBAL_FPRS      = 8,
      };

   TR::RealRegister  *_registerFileStorage[TR_X86_REGISTER_FILE_SIZE];
   TR::Register         *_registerAssociationsStorage[TR_X86_REGISTER_FILE_SIZE];
   TR::Register         *_xmmGlobalRegisterStorage[IA32_NUM_XMMR];
   uint32_t _globalRegisterNumberToRealRegisterMapStorage[IA32_MAX_GLOBAL_GPRS + IA32_MAX_GLOBAL_FPRS];

   public:

  Machine(TR::CodeGenerator *cg)
      : OMR::X86::Machine
         (
         IA32_NUM_GPR,
         IA32_NUM_FPR,
         cg,
         _registerFileStorage,
         _registerAssociationsStorage,
         IA32_MAX_GLOBAL_GPRS,
         IA32_MAX_8BIT_GLOBAL_GPRS,
         IA32_MAX_GLOBAL_FPRS,
         _xmmGlobalRegisterStorage,
         _globalRegisterNumberToRealRegisterMapStorage
         )
      {}

   };

} // namespace i386

} // namespace X86

} // namespace OMR

#endif
