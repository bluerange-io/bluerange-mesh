#pragma once

#include <Config.h>

/**
 * Only used as an interface to be implemented by other classes when a Button is pressed
 */
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
