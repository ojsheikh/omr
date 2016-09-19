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

#ifndef OMR_CRITICALSECTION_INCL
#define OMR_CRITICALSECTION_INCL

#include <algorithm>
#include "infra/Monitor.hpp"   // for Monitor

namespace OMR
{

/**
 * @brief The OMR::CriticalSection class wraps TR::Monitors with RAII functionality
 * to help automate the lifetime of a monitor acquisition.
 */
class CriticalSection
   {
   public:

   /**
    * @brief Declare the beginning of a critical section, constructing the OMR::CriticalSection
    * object and entering the monitor.
    * @param monitor The TR::Monitor object used to guard concurrent access
    */
   CriticalSection(TR::Monitor *monitor)
      : _monitor(monitor)
      {
      _monitor->enter();
      }

   /**
    * @brief Copy an existing critical section.  Both OMR::CriticalSection objects'
    * lifetimes must expire before the monitor is fully exited.  This requires that
    * the monitor be re-entrant.
    *
    * This function exists mainly to facilitate the ability to extend the lifespan
    * of critical sections by passing and returning them by value.
    *
    * @param copy The existing OMR::CriticalSection object being used to guard the
    * critical section.
    */
   CriticalSection(const CriticalSection &copy)
      : _monitor(copy._monitor)
      {
      _monitor->enter();
      }

   /**
    * @brief Automatically notify the end of the critical section, destroying the OMR::CriticalSection
    * object and exiting the monitor.
    */
   ~CriticalSection()
      {
      _monitor->exit();
      }

   /**
    * @brief Reconcile a given critical section with another, redirecting the monitor
    * of interest to that of the other critical section.  Holds both monitors temporarily.
    * Releases the currently held monitor.
    * @param rValue OMR::CriticalSection object containing the new monitor of interest
    * @return const reference to this object
    */
   const CriticalSection & operator =(CriticalSection rValue)
      {
      swap(*this, rValue);
      return *this;
      }

   /**
    * @brief Swaps the monitors of interest being of two OMR::CriticalSection objects
    * @param left the first OMR::CriticalSection object.  This object will have its
    * monitor replaced with the monitor of the second object.
    * @param right the second OMR::CriticalSection object.  This object will have its
    * monitor replaced with the monitor of the first object.
    */
   friend void swap(CriticalSection &left, CriticalSection &right)
      {
      using std::swap;
      swap(left._monitor, right._monitor);
      }

   /**
    * @brief Compares two OMR::CriticalSection objects
    * @param left first OMR::CriticalSection for comparison.
    * @param right second OMR::CriticalSection for comparison.
    * @return true if both OMR::CriticalSection objects are using the same monitor, false otherwise
    */
   friend bool operator ==(const CriticalSection &left, const CriticalSection &right)
      {
      return left._monitor == right._monitor;
      }

   /**
    * @brief Compares two OMR::CriticalSection objects
    * @param left first OMR::CriticalSection for comparison.
    * @param right second OMR::CriticalSection for comparison.
    * @return false if both OMR::CriticalSection objects are using the same monitor, true otherwise
    */
   friend bool operator !=(const CriticalSection &left, const CriticalSection &right)
      {
      return !(left == right);
      }

   private:

   TR::Monitor *_monitor;
};


}

#endif
