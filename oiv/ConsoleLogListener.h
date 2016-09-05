#pragma once
#include <OgreLog.h>
#include <iostream>
using namespace Ogre;
class GameLogListener : public LogListener
{
	void messageLogged(const String& message, LogMessageLevel lml, bool maskDebug, const String &logName, bool& skipThisMessage)
	{
		std::cout << std::endl << message;
	}
};