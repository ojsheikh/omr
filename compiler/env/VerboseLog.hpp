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

#ifndef VERBOSELOG_HPP
#define VERBOSELOG_HPP

#pragma once

#include <stdarg.h>

/*
 * Each call to the vlog must now have a 'tag'
 * When adding a tag, update the table in VerboseLog.cpp
 * Tags should be of the format '\n#TAGNAME:  '
 */
enum TR_VlogTag
   {
   TR_Vlog_null=0,

   TR_Vlog_INFO,
   TR_Vlog_SAMPLING,
   TR_Vlog_IPROFILER,
   TR_Vlog_CODECACHE,
   TR_Vlog_JITSTATE,
   TR_Vlog_PERF,
   TR_Vlog_MEMORY,
   TR_Vlog_FAILURE,
   TR_Vlog_JITDUMP,
   TR_Vlog_SCHINTS,
   TR_Vlog_GCCYCLE,
   TR_Vlog_J2I,
   TR_Vlog_CR,       //(compilation request)
   TR_Vlog_RA,       //(runtime assumption)
   TR_Vlog_HK,       //(hook)
   TR_Vlog_HD,       //(hook details)
   TR_Vlog_MH,       //(method handle)
   TR_Vlog_MHD,      //(method handle details)
   TR_Vlog_DLT,
   TR_Vlog_COMP,
   TR_Vlog_COMPSTART,
   TR_Vlog_COMPFAIL,
   TR_Vlog_PRECOMP,
   TR_Vlog_MMAP,
   TR_Vlog_OSR,      //(on-stack replacement)
   TR_Vlog_OSRD,     //(on-stack replacement details)
   TR_Vlog_HWPROFILER,
   TR_Vlog_INL,      //(inlining)
   TR_Vlog_GC,
   TR_Vlog_GPU,
   TR_Vlog_PATCH,
   TR_Vlog_DISPATCH,
   TR_Vlog_RECLAMATION,
   TR_Vlog_numTags
   };

/*
 * TR_VerboseLog stores the config of the jit early on, and can use it for printing to the verbose log
 * Each verbose write should have a Tag (see TR_VlogTag for list of options).  The tag is of the form: '\n#TAG:  '
 */
class TR_VerboseLog
   {
   public:
   //writeLine and write provide multi line write capabilities, and are to be used with vlogAcquire() and vlogRelease(),
   //this ensures nice formatted verbose logs
   static void write(const char *format, ...);
   static void writeLine(TR_VlogTag tag, const char *format, ...);
   //writeLineLocked is a single line print function, it provides the locks for you
   static void writeLineLocked(TR_VlogTag tag, const char *format, ...);
   static void vlogAcquire(); //defined in each front end
   static void vlogRelease(); //defined in each front end
   //only called once early on, after we can print to the vlog
   static void initialize(void *config);
   private:
   static void vwrite(const char *format, va_list args); //defined in each front end
   static void writeTimeStamp();
   static const char *_vlogTable[];
   static void *_config; //we will use void * as the different implementations have different config types
   };

#endif // VERBOSELOG_HPP
