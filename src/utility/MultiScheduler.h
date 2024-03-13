////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH.
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef SIM_ENABLED
#include <type_traits>
#endif
#include <Utility.h>
#include "FmTypes.h"

enum class EventTimeType : u8
{
    RELATIVE, /* The event occurs at regular intervals relative to the base time. */
    SYNCED,   /* The event occurs at specific absolute time points. */
};

/**
 * A Scheduler for multiple arbitrary, repeating events.
 * 
 * An event can be added to the Scheduler, together with its time between
 * occurrences. Time can be advanced and then the scheduler can be
 * asked which events have occurred.
 * 
 * Events can be both in relative time and in absolute time. An absolute
 * event does not trigger until the absolute time is set (see setAbsoluteTime). 
 * 
 * T = The type of event data that the scheduler will manage.
 * CAPACITY = The maximum number of events the scheduler can handle.
 */
template<typename T, int CAPACITY>
class MultiScheduler
{
private:

    struct Event
    {
        T t = {};
        EventTimeType type = EventTimeType::RELATIVE;
        u32 timeBetweenEventsDs = 0;
        u32 nextOccurrenceDs = 0;
        u32 offset = 0;
    };
    u32 length = 0;
    u32 baseTimeDs = 0;
    u32 absoluteTimeDs = 0;
    Event events[CAPACITY];

    void addEvent(/*mutable*/ Event& ev)
    {
        if (absoluteTimeDs == 0 && ev.type == EventTimeType::SYNCED)
        {
            // Synced time wasn't synced yet, but the event is meant to be used in synced time.
            // Move the occurrence so far into the future that it will basically never happen.
            ev.nextOccurrenceDs = 0xFFFFFFFF;
        }
        if (length == 0)
        {
            events[0] = ev;
            length++;
            return;
        }
        u32 i = 0;
        for (; i < length; i++)
        {
            if (events[i].nextOccurrenceDs > ev.nextOccurrenceDs) break;
        }
        for (u32 end = length - 1; end >= i && end != (u32)-1; end--)
        {
            events[end + 1] = events[end];
        }
        events[i] = ev;
        length++;
    }

    Event popFirst()
    {
        Event retVal = events[0];
        length--;
        for (u32 i = 0; i < length; i++)
        {
            events[i] = events[i + 1];
        }
        return retVal;
    }

public:
    MultiScheduler() {}

    //Advances the base time of the scheduler by the specified amount.
    //If absolute time is set, it also advances the absolute time.
    void advanceTime(u32 timeDs)
    {
        baseTimeDs += timeDs;
        if (absoluteTimeDs) absoluteTimeDs += timeDs;
    }

    void addEvent(const T& t, u32 timeBetweenEventsDs, u32 offset, EventTimeType type)
    {
        if (length == CAPACITY)
        {
            SIMEXCEPTION(BufferTooSmallException);
            return;
        }
        if (timeBetweenEventsDs == 0)
        {
            // There must be at least some time between events.
            SIMEXCEPTION(IllegalArgumentException);
            return;
        }
        Event ev;
        ev.t = t;
        ev.type = type;
        ev.timeBetweenEventsDs = timeBetweenEventsDs;
        ev.offset = offset;
        if (type == EventTimeType::RELATIVE)
        {
            ev.nextOccurrenceDs = baseTimeDs + timeBetweenEventsDs;
        }
        else if (type == EventTimeType::SYNCED)
        {
            ev.nextOccurrenceDs = Utility::NextMultipleOf(absoluteTimeDs, timeBetweenEventsDs) + offset - (absoluteTimeDs - baseTimeDs);
        }
        else
        {
            SIMEXCEPTION(IllegalArgumentException);
        }
        addEvent(ev);
    }

    // Checks if the first event in the scheduler is ready to occur.
    bool isEventReady() const
    {
        if (length == 0) return false;

        return events[0].nextOccurrenceDs <= baseTimeDs;
    }

    // Retrieves and re-enters the first event in the scheduler.
    T getAndReenter()
    {
        if (length == 0)
        {
            SIMEXCEPTION(IllegalStateException);
            return T();
        }

        Event ev = popFirst();
        const T retVal = ev.t;
        ev.nextOccurrenceDs = baseTimeDs + ev.timeBetweenEventsDs;
        addEvent(ev);
        return retVal;
    }

    bool removeEvent(const T& t)
    {
        u32 i = 0;
        bool found = false;
        for (; i < length; i++)
        {
            if (events[i].t == t)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            for (; i < length - 1; i++)
            {
                events[i] = events[i + 1];
            }
            events[length - 1].t.~T();
            length--;
        }
        return found;
    }

    // Sets the absolute time of the scheduler and updates the occurrence time for all absolute events accordingly.
    void setAbsoluteTime(u32 absoluteTimeDs)
    {
        // Early out in case nothing needs to be changed.
        if (this->absoluteTimeDs == absoluteTimeDs)
        {
            return;
        }
        this->absoluteTimeDs = absoluteTimeDs;

        // Go through all events and update the absolut events
        for (u32 i = 0; i < length; i++)
        {
            if (events[i].type == EventTimeType::SYNCED)
            {
                events[i].nextOccurrenceDs = Utility::NextMultipleOf(absoluteTimeDs, events[i].timeBetweenEventsDs) + events[i].offset - (absoluteTimeDs - baseTimeDs);
            }
        }

        // Edge case that makes sorting easier (We can ignore unsigned int underflows)
        if (length < 2)
        {
            return;
        }

        // Sort the events
        // TODO: This is a bubble sort, do something more clever
        for (u32 i = 0; i < length - 1; i++)
        {
            bool changed = false;
            for (u32 k = 0; k < length - i - 1; k++)
            {
                if (events[k].nextOccurrenceDs > events[k + 1].nextOccurrenceDs)
                {
                    changed = true;
                    Event temp = events[k];
                    events[k] = events[k + 1];
                    events[k + 1] = temp;
                }
            }
            if (!changed) break;
        }
    }
};