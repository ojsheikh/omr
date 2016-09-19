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

#include "env/IO.hpp"               // for IO

#include <stdint.h>                 // for int32_t
#include <stdio.h>                  // for FILE, fclose, fflush, fopen, etc
#include <stdarg.h>
#include "env/FilePointer.hpp"      // for FilePointer
#include "env/FilePointerDecl.hpp"  // for FILE

#ifdef LINUX
#include <unistd.h>                 // for getpid, intptr_t, pid_t
#endif

class TR_FrontEnd;

TR::FILE * OMR::IO::Null = TR::FilePointer::Null();
TR::FILE * OMR::IO::Stdin = TR::FilePointer::Stdin();
TR::FILE * OMR::IO::Stdout = TR::FilePointer::Stdout();
TR::FILE * OMR::IO::Stderr = TR::FilePointer::Stderr();


TR::FILE *
OMR::IO::fopen(char *fileName, const char *mode, bool encrypt)
   {
   return (TR::FILE *) ::fopen(fileName, mode);
   }


void
OMR::IO::fclose(TR::FILE *fileId)
   {
   ::fclose((::FILE *)fileId);
   }


void
OMR::IO::fseek(TR::FILE *fileId, intptr_t offset, int32_t whence)
   {
   ::fseek((::FILE *)fileId, offset, whence);
   }


long
OMR::IO::ftell(TR::FILE *fileId)
   {
   return ::ftell((::FILE *)fileId);
   }


void
OMR::IO::fflush(TR::FILE *fileId)
   {
   ::fflush((::FILE *)fileId);
   }


int32_t
OMR::IO::printf(const char * format, ...)
   {
   va_list args;
   va_start(args, format);
   int32_t length = ::vprintf(format, args);
   va_end(args);
   return length;
   }


int32_t
OMR::IO::fprintf(TR::FILE *fileId, const char * format, ...)
   {
   va_list args;
   va_start(args, format);
   int32_t length = ::vfprintf((::FILE*)fileId, format, args);
   va_end(args);
   return length;
   }


int32_t
OMR::IO::vfprintf(TR::FILE *fileId, const char *format, va_list args)
   {
   return ::vfprintf((::FILE*)fileId, format, args);
   }



TR::FILE *(*trfopen)(char *fileName, const char *attrs, bool encrypt) = TR::IO::fopen;
void (*trfclose)(TR::FILE *fileId) = TR::IO::fclose;
void (*trfflush)(TR::FILE *fileId) = TR::IO::fflush;
int32_t (*trfprintf)(TR::FILE *fileId, const char *format, ...) = TR::IO::fprintf;
int32_t (*trprintf)(const char *format, ...) = TR::IO::printf;
