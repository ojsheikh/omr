/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "Pointer.hpp"

static void printString(int64_t ptr)
   {
   #define PRINTSTRING_LINE LINETOSTR(__LINE__)
   char *str = (char *) ptr;
   printf("%s", str);
   }

static void printInt32(int32_t val)
   {
   #define PRINTINT32_LINE LINETOSTR(__LINE__)
   printf("%d", val);
   }

static void printFloat(float val)
   {
   #define PRINTFLOAT_LINE LINETOSTR(__LINE__)
   printf("%f", val);
   }

static void printDouble(double val)
   {
   #define PRINTDOUBLE_LINE LINETOSTR(__LINE__)
   printf("%lf", val);
   }

static void printPointer(int64_t val)
   {
   #define PRINTPOINTER_LINE LINETOSTR(__LINE__)
   printf("%lx", val);
   }

PointerMethod::PointerMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("testPointer");

   pInt32 = d->PointerTo(Int32);
   pFloat = d->PointerTo(Float);
   pDouble = d->PointerTo(Double);
   ppDouble = d->PointerTo(pDouble);

   DefineParameter("pInt32", pInt32);
   DefineParameter("pFloat", pFloat);
   DefineParameter("ppDouble", ppDouble);

   DefineReturnType(NoType);

   DefineFunction((char *)"printString",
                  (char *)__FILE__,
                  (char *)PRINTSTRING_LINE,
                  (void *)&printString,
                  NoType,
                  1,
                  Int64);
   DefineFunction((char *)"printInt32",
                  (char *)__FILE__,
                  (char *)PRINTINT32_LINE,
                  (void *)&printInt32,
                  NoType,
                  1,
                  Int32);
   DefineFunction((char *)"printFloat",
                  (char *)__FILE__,
                  (char *)PRINTFLOAT_LINE,
                  (void *)&printFloat,
                  NoType,
                  1,
                  Float);
   DefineFunction((char *)"printDouble",
                  (char *)__FILE__,
                  (char *)PRINTDOUBLE_LINE,
                  (void *)&printDouble,
                  NoType,
                  1,
                  Double);
   DefineFunction((char *)"printPointer",
                  (char *)__FILE__,
                  (char *)PRINTPOINTER_LINE,
                  (void *)&printPointer,
                  NoType,
                  1,
                  Int64);
   }

void
PointerMethod::PrintString(TR::IlBuilder *bldr, const char *s)
   {
   bldr->Call("printString", 1,
   bldr->   ConstInt64((int64_t)(char *)s));
   }

bool
PointerMethod::buildIL()
   {
   PrintString(this, "Pointer parameters:\n");

   PrintString(this, "   pInt32[");
   Call("printPointer", 1,
      Load("pInt32"));
   PrintString(this, "] = ");
   Call("printInt32", 1,
      LoadAt(pInt32,
         Load("pInt32")));

   PrintString(this, "   pFloat[");
   Call("printPointer", 1,
      Load("pFloat"));
   PrintString(this, "] = ");
   Call("printFloat", 1,
      LoadAt(pFloat,
         Load("pFloat")));

   PrintString(this, "   ppDouble[");
   Call("printPointer", 1,
      Load("ppDouble"));
   PrintString(this, "] = ");
   Call("printDouble", 1,
      LoadAt(pDouble,
         LoadAt(ppDouble,
            Load("ppDouble"))));

   PrintString(this, "\n");

   Return();

   return true;
   }


int
main(int argc, char *argv[])
   {
   printf("Step 1: initialize JIT\n");
   bool initialized = initializeJit();
   if (!initialized)
      {
      fprintf(stderr, "FAIL: could not initialize JIT\n");
      exit(-1);
      }

   printf("Step 2: define type dictionary\n");
   TR::TypeDictionary types;

   printf("Step 3: compile method builder\n");
   PointerMethod method(&types);
   uint8_t *entry;
   int32_t rc = compileMethodBuilder(&method, &entry);
   if (rc != 0)
      {
      fprintf(stderr,"FAIL: compilation error %d\n", rc);
      exit(-2);
      }

   printf("Step 4: define values\n");
   PointerFunctionType *test = (PointerFunctionType *)entry;
   int32_t theI32=15;
   float theFloat=225.225;
   double theDouble=123.456789;
   double *pTheDouble = &theDouble;

   printf("Step 5: invoke compiled code and verify results\n");
   test(&theI32, &theFloat, &pTheDouble);

   printf ("Step 6: shutdown JIT\n");
   shutdownJit();

   printf("PASS\n");
   }
