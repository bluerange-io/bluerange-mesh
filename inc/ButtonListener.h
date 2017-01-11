#pragma once

#include <Config.h>

class ButtonListener
{
private:

public:
	ButtonListener();
	virtual ~ButtonListener();

#ifdef USE_BUTTONS
	virtual void ButtonHandler(u8 buttonId, u32 holdTime) = 0;
#endif

};
