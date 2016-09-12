#pragma once
#include <OgreLog.h>
#include <iostream>

class GameLogListener : public Ogre::LogListener
{
	void messageLogged(const Ogre::String& message, Ogre::LogMessageLevel lml, bool maskDebug, const Ogre::String &logName, bool& skipThisMessage)
	{
		std::cout << std::endl << message;
	}
};