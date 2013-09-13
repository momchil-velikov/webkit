/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ThreadTimers.h"

#include "SharedTimer.h"
#include "ThreadGlobalData.h"
#include "Timer.h"
#include <wtf/ActionLogReport.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>
#include <stdio.h>

#include <vector>
#include <set>

using namespace std;

namespace WebCore {

EventActionsHB::EventActionsHB()
    : m_invalidEventAction(true),
      m_networkResponseRecursion(0),
      m_uiActionRecursion(0),
      m_inTimerEventAction(false),
      m_numDisabledInstrumentationRequests(0) {
	allocateEventActionId();  // event action with id 0 is unused (as empty value for hashtables).
	m_currentEventActionId = allocateEventActionId();
	m_lastUIEventAction = m_currentEventActionId;
}

EventActionsHB::~EventActionsHB() {
}

EventActionId EventActionsHB::allocateEventActionId() {
	EventActionId result = m_timerInfo.size();
	m_timerInfo.append(TimerEvent());
	if (result % 8192 == 8191) {
		ActionLogSave();
	}
	return result;
}

void EventActionsHB::addExplicitArc(EventActionId earlier, EventActionId later) {
	if (earlier <= 0 || later <= 0 || earlier == later) {
		CRASH();
	}
	ActionLogAddArc(earlier, later, -1);
}

void EventActionsHB::addTimedArc(EventActionId earlier, EventActionId later, double duration) {
	if (earlier <= 0 || later <= 0) {
		CRASH();
	}
	ActionLogAddArc(earlier, later, duration * 1000);
}

void EventActionsHB::setCurrentEventAction(EventActionId newEventActionId) {
	m_currentEventActionId = newEventActionId;
	m_timerInfo[newEventActionId].m_wasEntered = true;
	m_invalidEventAction = false;
	if (m_inTimerEventAction) {
		ActionLogEnterOperation(newEventActionId, ActionLog::TIMER);
	} else if (m_uiActionRecursion > 0) {
		ActionLogEnterOperation(newEventActionId, ActionLog::USER_INTERFACE);
	} else if (m_networkResponseRecursion > 0) {
		ActionLogEnterOperation(newEventActionId, ActionLog::NETWORK);
	} else {
		ActionLogEnterOperation(newEventActionId, ActionLog::UNKNOWN);
	}
}

EventActionId EventActionsHB::splitCurrentEventActionIfNotInScope(
		bool add_arc_from_current_tonew_event_action) {
	if (ActionLogScopeDepth() == 0) {
		EventActionId old_id = currentEventAction();
		EventActionId new_id = allocateEventActionId();
		setCurrentEventAction(new_id);
		if (add_arc_from_current_tonew_event_action) {
			addExplicitArc(old_id, new_id);
		}
	}
	return currentEventAction();
}

void EventActionsHB::setCurrentEventActionInvalid() {
	ActionLogExitOperation();
	m_invalidEventAction = true;
}

void EventActionsHB::checkInValidEventAction() {
	if (m_invalidEventAction) {
		fprintf(stderr, "Not in a valid event action.\n");
		fflush(stderr);
		CRASH();
	}
}

void EventActionsHB::userInterfaceModification() {
	// Note(veselin): We've decided not to insert arcs from
	// UI modification to UI event arcs.
}

EventActionId EventActionsHB::startUIAction() {
	if (m_inTimerEventAction || m_networkResponseRecursion != 0) {
		return m_currentEventActionId;
	}
//	printf("ui open %d\n", m_uiActionRecursion);
	if (m_uiActionRecursion++ == 0) {
		EventActionId newId = allocateEventActionId();
		addExplicitArc(m_lastUIEventAction, newId);
		setCurrentEventAction(newId);
		m_lastUIEventAction = newId;
	}
	return m_currentEventActionId;
}

void EventActionsHB::endUIAction() {
	if (m_inTimerEventAction || m_networkResponseRecursion != 0) {
		return;
	}
	--m_uiActionRecursion;
	if (m_uiActionRecursion == 0) {
		setCurrentEventActionInvalid();
	}
//	printf("ui close %d\n", m_uiActionRecursion);
}

EventActionId EventActionsHB::startNetworkResponseEventAction() {
	if (m_inTimerEventAction || m_uiActionRecursion != 0) {
		return m_currentEventActionId;
	}
//	printf("open %d\n", m_networkResponseRecursion);
	if (m_networkResponseRecursion++ == 0) {
		setCurrentEventAction(allocateEventActionId());
	}
	return m_currentEventActionId;
}

void EventActionsHB::finishNetworkReponseEventAction() {
	if (m_inTimerEventAction || m_uiActionRecursion != 0) {
		return;
	}
	--m_networkResponseRecursion;
	if (m_networkResponseRecursion == 0) {
		setCurrentEventActionInvalid();
	}
//	printf("close %d\n", m_networkResponseRecursion);
}

void EventActionsHB::setInTimerEventAction(bool inTimer) {
//	printf("in timer %d\n", static_cast<int>(inTimer));
	if (m_uiActionRecursion != 0 || m_networkResponseRecursion != 0) {
		CRASH();
	}
	m_inTimerEventAction = inTimer;
}

// Fire timers for this length of time, and then quit to let the run loop process user input events.
// 100ms is about a perceptable delay in UI, so use a half of that as a threshold.
// This is to prevent UI freeze when there are too many timers or machine performance is low.
static const double maxDurationOfFiringTimers = 0.050;

// Timers are created, started and fired on the same thread, and each thread has its own ThreadTimers
// copy to keep the heap and a set of currently firing timers.

static MainThreadSharedTimer* mainThreadSharedTimer()
{
    static MainThreadSharedTimer* timer = new MainThreadSharedTimer;
    return timer;
}

ThreadTimers::ThreadTimers()
    : m_sharedTimer(0)
    , m_firingTimers(false)
{
    if (isMainThread())
        setSharedTimer(mainThreadSharedTimer());
}

// A worker thread may initialize SharedTimer after some timers are created.
// Also, SharedTimer can be replaced with 0 before all timers are destroyed.
void ThreadTimers::setSharedTimer(SharedTimer* sharedTimer)
{
    if (m_sharedTimer) {
        m_sharedTimer->setFiredFunction(0);
        m_sharedTimer->stop();
    }
    
    m_sharedTimer = sharedTimer;
    
    if (sharedTimer) {
        m_sharedTimer->setFiredFunction(ThreadTimers::sharedTimerFired);
        updateSharedTimer();
    }
}

void ThreadTimers::updateSharedTimer()
{
    if (!m_sharedTimer)
        return;
        
    if (m_firingTimers || m_timerHeap.isEmpty())
        m_sharedTimer->stop();
    else
        m_sharedTimer->setFireInterval(max(m_timerHeap.first()->m_nextFireTime - monotonicallyIncreasingTime(), 0.0));
}

void ThreadTimers::sharedTimerFired()
{
    // Redirect to non-static method.
    threadGlobalData().threadTimers().sharedTimerFiredInternal();
}

void ThreadTimers::sharedTimerFiredInternal()
{
    // Do a re-entrancy check.
    if (m_firingTimers)
        return;
    m_firingTimers = true;

    double fireTime = monotonicallyIncreasingTime();
    double timeToQuit = fireTime + maxDurationOfFiringTimers;

    while (!m_timerHeap.isEmpty() && m_timerHeap.first()->m_nextFireTime <= fireTime) {
        TimerBase* timer = m_timerHeap.first();
        timer->m_nextFireTime = 0;
        timer->heapDeleteMin();

        if (!m_eventActionsHB.isInstrumentationDisabled()) {
			EventActionId newId = m_eventActionsHB.allocateEventActionId();
			m_eventActionsHB.setCurrentEventAction(newId);
			timer->m_lastFireEventAction = newId;
			if (timer->m_ignoreFireIntervalForHappensBefore) {
				m_eventActionsHB.addExplicitArc(timer->m_starterEventAction, newId);
			} else {
				m_eventActionsHB.addTimedArc(timer->m_starterEventAction, newId, timer->m_nextFireInterval);
			}
			m_eventActionsHB.setInTimerEventAction(true);
        }
        ActionLogEventTriggered(timer);

        double interval = timer->repeatInterval();
        timer->setNextFireTime(interval ? fireTime + interval : 0, interval);

        // Once the timer has been fired, it may be deleted, so do nothing else with it after this point.
        timer->fired();

        if (!m_eventActionsHB.isInstrumentationDisabled()) {
        	m_eventActionsHB.setInTimerEventAction(false);
        	m_eventActionsHB.setCurrentEventActionInvalid();
        }

        // Catch the case where the timer asked timers to fire in a nested event loop, or we are over time limit.
        if (!m_firingTimers || timeToQuit < monotonicallyIncreasingTime())
            break;
    }

    m_firingTimers = false;

    updateSharedTimer();
}

void ThreadTimers::fireTimersInNestedEventLoop()
{
    // Reset the reentrancy guard so the timers can fire again.
    m_firingTimers = false;
    updateSharedTimer();
}

} // namespace WebCore

