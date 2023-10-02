#pragma once

#include "d_net.h"
#include "packet.h"
#include "gameinput.h"

struct CorePlayer
{
	InputPacket input;
	PlayerAngles Angles;
	DCoreActor* actor;

	virtual DCoreActor* GetActor() = 0;
};

extern CorePlayer* PlayerArray[MAXPLAYERS];
