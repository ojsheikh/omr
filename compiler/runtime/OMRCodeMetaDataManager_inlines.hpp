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

#ifndef OMR_CODE_METADATA_MANAGER_INLINE_INCL
#define OMR_CODE_METADATA_MANAGER_INLINE_INCL

#include "runtime/CodeMetaDataManager.hpp"

TR::CodeMetaDataManager * OMR::CodeMetaDataManager::self()
   {
   return static_cast<TR::CodeMetaDataManager *>(this);
   }

#endif
