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

#ifndef ANNOTATIONS_INCL
#define ANNOTATIONS_INCL

/**
 * \file Annotations.hpp
 *
 * This file provides annotation macros useful for improving static
 * analsyis and code generation.
 *
 * These macros are OMR_ prefixed to avoid collisions with existing macros in
 * some projects, i.e. Ruby.
 *
 * To use these macros, insert them at function declaration time,
 *
 *     OMR_NORETURN     void assert(bool test);
 *
 * or when defining or declaring a class,
 *
 *     class OMR_EXTENSIBLE Foo;
 *
 * or inside a branching expression:
 *
 *     if(OMR_LIKELY(isTrue()))
 *
 * \def OMR_NORETURN   Marks a function as non-returning for the purposes of
 * code generation and optimization.
 *
 * \def OMR_EXTENSIBLE Marks a class as an 'Extensible' class.
 *
 * \def OMR_LIKELY     Marks a branch as being more likely.
 *
 * \def OMR_UNLIKELY   Marks a branch as being less likely.
 *
 * \todo Provide OMR_RETURNS_NONNULL annotation
 * \todo Provide OMR_DEPRECATED      annotation
 * \todo Provide OMR_MALLOC          annotation.
 *
 */


// OMR_NORETURN
#if defined(_MSC_VER)
#define OMR_NORETURN _declspec(noreturn)
#elif defined(__GNUC__)
#define OMR_NORETURN __attribute__ ((noreturn))
#elif defined(__IBMCPP__)
#define OMR_NORETURN __attribute__ ((noreturn))
#else
#warning "Noreturn attribute undefined for this platform."
#define OMR_NORETURN
#endif

// OMR_EXTENSIBLE
#if defined(__clang__) // Only clang is checking this macro for now
#define OMR_EXTENSIBLE __attribute__((annotate("OMR_Extensible")))
#else
#define OMR_EXTENSIBLE
#endif

// OMR_LIKELY and OMR_UNLIKELY
// TODO: check if the definition of these macros is too broad,
//       __builtin_expect() may not have any effect on xlC
#if defined(TR_HOST_X86) && defined(WIN32)
   #define OMR_LIKELY(expr) (expr)
   #define OMR_UNLIKELY(expr) (expr)
#else
   #define OMR_LIKELY(expr)   __builtin_expect((expr), 1)
   #define OMR_UNLIKELY(expr) __builtin_expect((expr), 0)
#endif

#endif
