/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Config.h>
#include <LedWrapper.h>

extern "C"{
#include <nrf.h>
}

#ifdef SIM_ENABLED
extern void setSimLed(bool state);
#endif

LedWrapper::LedWrapper(i8 io_num, bool active_high)
{
	if(io_num == -1){
		active = false;
		return;
	}
	active = true;
    m_active_high = active_high;
    m_io_msk = 1 << io_num;
    NRF_GPIO->DIRSET = m_io_msk;

#ifdef SIM_ENABLED
	setSimLed(false);
#endif
}

void LedWrapper::On(void)
{
	if(!active) return;
    if(m_active_high) NRF_GPIO->OUTSET = m_io_msk;
    else NRF_GPIO->OUTCLR = m_io_msk;

#ifdef SIM_ENABLED
	setSimLed(true);
#endif
}

void LedWrapper::Off(void)
{
	if(!active) return;
    if(m_active_high) NRF_GPIO->OUTCLR = m_io_msk;
    else NRF_GPIO->OUTSET = m_io_msk;

#ifdef SIM_ENABLED
	setSimLed(false);
#endif
}

void LedWrapper::Toggle(void)
{
	if(!active) return;
    NRF_GPIO->OUT ^= m_io_msk;

#ifdef SIM_ENABLED
	setSimLed(m_io_msk != 0);
#endif
}

/* EOF */
