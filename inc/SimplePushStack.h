/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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

/*
 * This is a simple buffer for fixed length packets, only to remember a list of
 * things
 */

#pragma once

#include <types.h>

class SimplePushStack
{
	private:
		u8** buffer;
		u16 maxSize;
		u16 numItems;


	public:
		SimplePushStack(u16 maxSize);
		bool Push(u8* element);
		u8* GetItemAt(u16 position);
		u16 size();
};

