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

/*
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */

      NoReg             = 0,
      gr0               = 1,
      FirstGPR          = gr0,
      gr1               = 2,
      gr2               = 3,
      gr3               = 4,
      gr4               = 5,
      gr5               = 6,
      gr6               = 7,
      gr7               = 8,
      gr8               = 9,
      gr9               = 10,
      gr10              = 11,
      gr11              = 12,
      gr12              = 13,
      gr13              = 14,
      gr14              = 15,
      gr15              = 16,
      LastGPR           = gr15,
      LastAssignableGPR = gr10,
      fp0               = 17,
      FirstFPR          = fp0,
      fp1               = 18,
      fp2               = 19,
      fp3               = 20,
      fp4               = 21,
      fp5               = 22,
      fp6               = 23,
      fp7               = 24,
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      fp8               = 25,
      fp9               = 26,
      fp10              = 27,
      fp11              = 28,
      fp12              = 29,
      fp13              = 30,
      fp14              = 31,
      fp15              = 32,
      LastFPR           = fp15,
      LastAssignableFPR = fp15,
      fs0               = 33,
      FirstFSR          = fs0,
      fs1               = 34,
      fs2               = 35,
      fs3               = 36,
      fs4               = 37,
      fs5               = 38,
      fs6               = 39,
      fs7               = 40,
      fs8               = 41,
      fs9               = 42,
      fs10              = 43,
      fs11              = 44,
      fs12              = 45,
      fs13              = 46,
      fs14              = 47,
      fs15              = 48,
      LastFSR           = fs15,
      LastAssignableFSR = fs15,
      SpilledReg        = 49,
#else
      LastFPR           = fp7,
      LastAssignableFPR = fp7,
      SpilledReg        = 25,
#endif
      NumRegisters      = SpilledReg + 1

