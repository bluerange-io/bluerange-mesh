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

#include <SimplePushStack.h>

extern "C"{
#include <malloc.h>
}

SimplePushStack::SimplePushStack(u16 maxSize)
{
	//Get some space
	buffer = (u8**)malloc(maxSize*sizeof(u8*));
	numItems = 0;
	this->maxSize = maxSize;
}

bool SimplePushStack::Push(u8* element)
{
	if(numItems >= maxSize || !buffer) return false;

	buffer[numItems] = element;
	numItems++;

	return true;
}

u8* SimplePushStack::GetItemAt(u16 position)
{
	if(position >= numItems || !buffer) return NULL;
	return buffer[position];
}

u16 SimplePushStack::size()
{
	return numItems;
}
