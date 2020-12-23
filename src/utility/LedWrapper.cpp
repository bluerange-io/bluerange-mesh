////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

#include <Config.h>
#include <LedWrapper.h>


LedWrapper::LedWrapper(i8 io_num, bool active_high)
{
    Init(io_num, active_high);
}

LedWrapper::LedWrapper()
{
    //Leave uninit
}

void LedWrapper::Init(i8 io_num, bool active_high)
{
    if(io_num == -1){
        active = false;
        return;
    }
    active = true;

    m_io_pin = io_num;
    m_active_high = active_high;
    FruityHal::GpioConfigureOutput(io_num);
    //Initially disable LED
    Off();
}

void LedWrapper::On(void)
{
    if(!active) return;
        if(m_active_high) FruityHal::GpioPinSet(m_io_pin);
        else FruityHal::GpioPinClear(m_io_pin);
}

void LedWrapper::Off(void)
{
    if(!active) return;
        if(m_active_high) FruityHal::GpioPinClear(m_io_pin);
        else FruityHal::GpioPinSet(m_io_pin);
}

void LedWrapper::Toggle(void)
{
    if(!active) return;
        FruityHal::GpioPinToggle(m_io_pin);
}

/* EOF */
