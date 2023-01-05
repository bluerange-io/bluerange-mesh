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

#include <ErrorLog.h>

ErrorLog::ErrorLog()
{
    Reset();
}

void ErrorLog::Reset()
{
    storage.Reset();
}

void ErrorLog::PushError(const ErrorLogEntry & entry)
{
    // If the error log is full, we drop the entry.
    bool pushed = storage.Push(entry);
    (void)pushed;
}

void ErrorLog::PushCount(const ErrorLogEntry &entry)
{
    ErrorLogEntry *counterEntry = storage.FindByPredicate([&entry](const ErrorLogEntry &otherEntry) -> bool {
      return otherEntry.errorType == entry.errorType && otherEntry.errorCode == entry.errorCode;
    });

    if (counterEntry != nullptr)
    {
        counterEntry->extraInfo += entry.extraInfo;
        return;
    }

    PushError(entry);
}

bool ErrorLog::PopEntry(ErrorLogEntry & entry)
{
    return storage.TryPeekAndPop(entry);
}
