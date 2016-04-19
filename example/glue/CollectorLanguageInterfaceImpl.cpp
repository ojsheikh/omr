/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
#include "modronbase.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentSafepointCallback.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "EnvironmentStandard.hpp"
#include "ForwardedHeader.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MarkingScheme.hpp"
#include "MixedObjectScanner.hpp"
#include "mminitcore.h"
#include "objectdescription.h"
#include "ObjectModel.hpp"
#include "omr.h"
#include "omrvm.h"
#include "OMRVMInterface.hpp"
#include "ScanClassesMode.hpp"
#include "ObjectIterator.hpp"
#include "omrExampleVM.hpp"

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cli = NULL;
	OMR_VM *omrVM = env->getOmrVM();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	cli = (MM_CollectorLanguageInterfaceImpl *)extensions->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cli) {
		new(cli) MM_CollectorLanguageInterfaceImpl(omrVM);
		if (!cli->initialize(omrVM)) {
			cli->kill(env);
			cli = NULL;
		}
	}

	return cli;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	OMR_VM *omrVM = env->getOmrVM();
	tearDown(omrVM);
	MM_GCExtensionsBase::getExtensions(omrVM)->getForge()->free(this);
}

void
MM_CollectorLanguageInterfaceImpl::tearDown(OMR_VM *omrVM)
{

}

bool
MM_CollectorLanguageInterfaceImpl::initialize(OMR_VM *omrVM)
{
	return true;
}

void
MM_CollectorLanguageInterfaceImpl::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
}

OMR_VMThread *
MM_CollectorLanguageInterfaceImpl::attachVMThread(OMR_VM *omrVM, const char *threadName, uintptr_t reason)
{
	OMR_VMThread *omrVMThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	rc = OMR_Glue_BindCurrentThread(omrVM, threadName, &omrVMThread);
	if (OMR_ERROR_NONE != rc) {
		return NULL;
	}
	return omrVMThread;
}

void
MM_CollectorLanguageInterfaceImpl::detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason)
{
	if (NULL != omrVMThread) {
		OMR_Glue_UnbindCurrentThread(omrVMThread);
	}
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForGC(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_scanRoots(MM_EnvironmentBase *env)
{
	OMR_VM_Example *omrVM = (OMR_VM_Example *)env->getOmrVM()->_language_vm;
	J9HashTableState state;
	RootEntry *rEntry = NULL;
	rEntry = (RootEntry *)hashTableStartDo(omrVM->rootTable, &state);
	while (rEntry != NULL) {
		_markingScheme->markObject(env, rEntry->rootPtr);
		rEntry = (RootEntry *)hashTableNextDo(&state);
	}
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_completeMarking(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_markLiveObjectsComplete(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForWalk(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterCleanupAfterGC(MM_EnvironmentBase *env)
{
}

uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason)
{
	uintptr_t objectSize = env->getExtensions()->objectModel.getSizeInBytesWithHeader(objectPtr);

	GC_ObjectIterator objectIterator(_omrVM, objectPtr);
	GC_SlotObject *slotObject = NULL;
	while (NULL != (slotObject = objectIterator.nextSlot())) {
		omrobjectptr_t slot = slotObject->readReferenceFromSlot();
		if (_markingScheme->isHeapObject(slot)) {
			_markingScheme->markObject(env, slot);
		}
	}
	return objectSize;
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason, uintptr_t sizeToDo)
{
#error implement an object scanner which scans up to sizeToDo bytes
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

void
MM_CollectorLanguageInterfaceImpl::parallelDispatcher_handleMasterThread(OMR_VMThread *omrVMThread)
{
	/* Do nothing for now.  only required for SRT */
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_reportObjectEvents(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterSetupForGC(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode)
{
	/* Do nothing for now */
	return false;
}

GC_ObjectScanner *
MM_CollectorLanguageInterfaceImpl::scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
{
#if defined(OMR_GC_MODRON_SCAVENGER_STRICT)
	Assert_MM_true((GC_ObjectScanner::scanHeap == flags) ^ (GC_ObjectScanner::scanRoots == flags));
#endif /* defined(OMR_GC_MODRON_SCAVENGER_STRICT) */
	GC_ObjectScanner *objectScanner = NULL;
	objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, allocSpace, flags);
	return objectScanner;
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_flushReferenceObjects(MM_EnvironmentStandard *env)
{
	/* Do nothing for now */
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented and return true an object may hold any object references that are live
	 * but not reachable by traversing the reference graph from the root set or remembered set. Otherwise this
	 * default implementation should be used.
	 */
	return false;
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::copyObjectSlot(..) for each such indirect object reference, ORing the boolean result
	 * from each call to MM_Scavenger::copyObjectSlot(..) into a single boolean value to be returned.
	 */
	return false;
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should
	 * call MM_Scavenger::backOutFixSlotWithoutCompression(..) for each uncompressed slot holding a reference to
	 * an indirect object that is associated with the object.
	 */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env)
{
	/* This method must be implemented if an object may hold any object references that are live but not reachable
	 * by traversing the reference graph from the root set or remembered set. In that case, this method should locate
	 * all such objects and call MM_Scavenger::backOutObjectScan(..) for each such object that is in the remembered
	 * set. For example,
	 *
	 * if (_extensions->objectModel.isRemembered(indirectObject)) {
	 *    _extensions->scavenger->backOutObjectScan(env, indirectObject);
	 * }
	 */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader)
{
	if (forwardedHeader->isForwardedPointer()) {
		omrobjectptr_t objectPtr = forwardedHeader->getObject();
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
		omrobjectptr_t fwdObjectPtr = forwardedHeader->getForwardedObject();

		/* A reverse forwarded object is a hole whose 'next' pointer actually points at the original object.
		 * This keeps tenure space walkable once the reverse forwarded objects are abandoned.
		 */
		UDATA evacuateObjectSizeInBytes = extensions->objectModel.getConsumedSizeInBytesWithHeader(fwdObjectPtr);
		MM_HeapLinkedFreeHeader* freeHeader = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(fwdObjectPtr);
		freeHeader->setNext((MM_HeapLinkedFreeHeader*)objectPtr);
		freeHeader->setSize(evacuateObjectSizeInBytes);
	}
}

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, MM_MemorySubSpaceSemiSpace *subSpaceNew)
{
	/* This method must be implemented if (and only if) the object header is stored in a compressed slot. in that
	 * case the other half of the full (omrobjectptr_t sized) slot may hold a compressed object reference that
	 * must be restored by this method.
	 */
	Assert_MM_unimplemented();
}
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_CollectorLanguageInterfaceImpl::compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_fixupRoots(MM_EnvironmentBase *env, MM_CompactScheme *compactScheme)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env)
{
	Assert_MM_unimplemented();
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env)
{
	Assert_MM_unimplemented();
}
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
MM_ConcurrentSafepointCallback*
MM_CollectorLanguageInterfaceImpl::concurrentGC_createSafepointCallback(MM_EnvironmentBase *env)
{
	MM_EnvironmentStandard *envStd = MM_EnvironmentStandard::getEnvironment(env);
	return MM_ConcurrentSafepointCallback::newInstance(envStd);
}

uintptr_t
MM_CollectorLanguageInterfaceImpl::concurrentGC_collectRoots(MM_EnvironmentStandard *env, ConcurrentStatus concurrentStatus, MM_ScanClassesMode *scanClassesMode, bool &collectedRoots, bool &paidTax)
{
	uintptr_t bytesScanned = 0;
	collectedRoots = true;
	paidTax = true;

	switch (concurrentStatus) {
	case CONCURRENT_ROOT_TRACING1:
		break;
	case CONCURRENT_ROOT_TRACING2:
		markingScheme_scanRoots(env);
		break;
	case CONCURRENT_ROOT_TRACING3:
		break;
	case CONCURRENT_ROOT_TRACING4:
		break;
	case CONCURRENT_ROOT_TRACING5:
		break;
#if defined(OMR_GC_DYNAMIC_CLASS_UNLOADING)
	case CONCURRENT_TRACE_ONLY:
		break;
#endif /* OMR_GC_DYNAMIC_CLASS_UNLOADING */
	default:
		Assert_MM_unreachable();
	}

	return bytesScanned;
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

omrobjectptr_t
MM_CollectorLanguageInterfaceImpl::heapWalker_heapWalkerObjectSlotDo(omrobjectptr_t object)
{
	return NULL;
}
