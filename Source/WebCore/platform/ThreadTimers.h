/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#ifndef ThreadTimers_h
#define ThreadTimers_h

#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#include "Timer.h"
#include <stdio.h>

namespace WebCore {

	// Happens before graph for event actions.
	class EventActionsHB {
		WTF_MAKE_NONCOPYABLE(EventActionsHB); WTF_MAKE_FAST_ALLOCATED;
	public:
		EventActionsHB();
		~EventActionsHB();

		// Allocates a new id for an event action.
		EventActionId allocateEventActionId();

		void addExplicitArc(EventActionId earlier, EventActionId later);
		void addTimedArc(EventActionId earlier, EventActionId later, double duration);

		bool areInOrder(EventActionId first, EventActionId second);
		bool haveAnyOrderRelation(EventActionId ea1, EventActionId ea2);

		EventActionId currentEventAction() const {
			return m_currentEventActionId;
		}

		// Allocates a new event action and enters in it.
		EventActionId splitCurrentEventActionIfNotInScope(
				bool add_arc_from_current_tonew_event_action);

		void setCurrentEventAction(EventActionId newId);
		void setCurrentEventActionInvalid();

		bool isCurrentEventActionValid() const {
			return !m_invalidEventAction;
		}

		void addDebugInfo(EventActionId eventActionId, const String& debug_info) {
		}

		EventActionId lastUIEventAction() const {
			 return m_lastUIEventAction;
		}

		void checkInValidEventAction();

		void userInterfaceModification();


		// Network response slices can be started recursively and it's required that all started slices are eventually ended.
		// Only one slice per network is allocated when multiple slices are started recursively.
		EventActionId startNetworkResponseEventAction();
		void finishNetworkReponseEventAction();

		// UI actions can also be  started recursively like network actions and they all
		// in the end belong to the same event action.
		// UI and network actions can't be started inside each other.
		EventActionId startUIAction();
		void endUIAction();

		// Timer slices can't be in a network or UI slice.
		void setInTimerEventAction(bool inTimer);

		void addDisableInstrumentationRequest() {
			++m_numDisabledInstrumentationRequests;
		}

		void removeDisableInstrumentationRequest() {
			--m_numDisabledInstrumentationRequests;
		}

		bool isInstrumentationDisabled() const {
			return m_numDisabledInstrumentationRequests > 0;
		}

	private:
		void addExtraTimeArcs(EventActionId earlier, EventActionId later, double duration);

		struct TimerEvent {
			TimerEvent() : m_parent(-1), m_time(0), m_wasEntered(false) {
			}

			EventActionId m_parent;
			double m_time;
			bool m_wasEntered;
		};

		EventActionId m_currentEventActionId;
		bool m_invalidEventAction;

		int m_networkResponseRecursion;
		int m_uiActionRecursion;
		bool m_inTimerEventAction;

		Vector<TimerEvent> m_timerInfo;

		EventActionId m_lastUIEventAction;

		int m_numDisabledInstrumentationRequests;
	};

    class SharedTimer;
    class TimerBase;

    // A collection of timers per thread. Kept in ThreadGlobalData.
    class ThreadTimers {
        WTF_MAKE_NONCOPYABLE(ThreadTimers); WTF_MAKE_FAST_ALLOCATED;
    public:
        ThreadTimers();

        // On a thread different then main, we should set the thread's instance of the SharedTimer.
        void setSharedTimer(SharedTimer*);

        Vector<TimerBase*>& timerHeap() { return m_timerHeap; }

        void updateSharedTimer();
        void fireTimersInNestedEventLoop();

        EventActionsHB& happensBefore() { return m_eventActionsHB; }

    private:
        static void sharedTimerFired();

        void sharedTimerFiredInternal();
        void fireTimersInNestedEventLoopInternal();

        Vector<TimerBase*> m_timerHeap;
        SharedTimer* m_sharedTimer; // External object, can be a run loop on a worker thread. Normally set/reset by worker thread.
        bool m_firingTimers; // Reentrancy guard.

        EventActionsHB m_eventActionsHB;
    };
}

#endif
