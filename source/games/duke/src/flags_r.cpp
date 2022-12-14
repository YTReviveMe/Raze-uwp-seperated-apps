//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2017-2019 Nuke.YKT
Copyright (C) 2020 - Christoph Oelckers

This file is part of Duke Nukem 3D version 1.5 - Atomic Edition

Duke Nukem 3D is free software, you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program, if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Original Source, 1996 - Todd Replogle
Prepared for public release, 03/21/2003 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------

#include "ns.h"
#include "global.h"
#include "names_r.h"

BEGIN_DUKE_NS

void initactorflags_r()
{
	for (auto& fa : gs.actorinfo)
	{
		fa.falladjustz = 24;
	}
	if (isRRRA())
	{
		gs.actorinfo[RTILE_HULKBOAT].falladjustz = 12;
		gs.actorinfo[RTILE_MINIONBOAT].falladjustz = 3;
		gs.actorinfo[RTILE_CHEERBOAT].falladjustz = gs.actorinfo[RTILE_EMPTYBOAT].falladjustz = 6;
	}
	gs.actorinfo[RTILE_DRONE].falladjustz = 0;


	setflag(SFLAG_INTERNAL_BADGUY | SFLAG_KILLCOUNT, {
		RTILE_BOULDER,
		RTILE_BOULDER1,
		RTILE_RAT,
		RTILE_TORNADO,
		RTILE_BILLYCOCK,
		RTILE_BILLYRAY,
		RTILE_BILLYRAYSTAYPUT,
		RTILE_BRAYSNIPER,
		RTILE_DOGRUN,
		RTILE_LTH,
		RTILE_HULKJUMP,
		RTILE_BUBBASTAND,
		RTILE_HULK,
		RTILE_HULKSTAYPUT,
		RTILE_DRONE,
		RTILE_RECON,
		RTILE_MINION,
		RTILE_MINIONSTAYPUT,
		RTILE_COOT,
		RTILE_COOTSTAYPUT,
		RTILE_VIXEN });

	if (isRRRA()) setflag(SFLAG_INTERNAL_BADGUY | SFLAG_KILLCOUNT, {
		RTILE_SBSWIPE,
		RTILE_BIKERB,
		RTILE_BIKERBV2,
		RTILE_BIKER,
		RTILE_MAKEOUT,
		RTILE_CHEERB,
		RTILE_CHEER,
		RTILE_CHEERSTAYPUT,
		RTILE_MINIONBOAT,
		RTILE_HULKBOAT,
		RTILE_CHEERBOAT,
		RTILE_RABBIT,
		RTILE_MAMA,
		RTILE_UFO1_RRRA });
	else setflag(SFLAG_INTERNAL_BADGUY | SFLAG_KILLCOUNT, {
		RTILE_SBMOVE,
		RTILE_UFO1_RR,
		RTILE_UFO2,
		RTILE_UFO3,
		RTILE_UFO4,
		RTILE_UFO5 });

	setflag(SFLAG_INTERNAL_BADGUY, { RTILE_PIG, RTILE_HEN });

	gs.actorinfo[RTILE_DRONE].flags |= SFLAG_NOWATERDIP;
	gs.actorinfo[RTILE_VIXEN].flags |= SFLAG_NOCANSEECHECK;
	if (isRRRA())
	{
		setflag(SFLAG_NODAMAGEPUSH, { RTILE_HULK, RTILE_MAMA, RTILE_BILLYPLAY, RTILE_COOTPLAY, RTILE_MAMACLOUD });
		setflag(SFLAG_NOCANSEECHECK, { RTILE_COOT, RTILE_COOTSTAYPUT, RTILE_BIKERB, RTILE_BIKERBV2, RTILE_CHEER, RTILE_CHEERB,
			RTILE_CHEERSTAYPUT, RTILE_MINIONBOAT, RTILE_HULKBOAT, RTILE_CHEERBOAT, RTILE_RABBIT, RTILE_COOTPLAY, RTILE_BILLYPLAY, RTILE_MAKEOUT, RTILE_MAMA });
		setflag(SFLAG_INTERNAL_BADGUY, { RTILE_COOTPLAY, RTILE_BILLYPLAY });
	}
	else
	{
		setflag(SFLAG_NODAMAGEPUSH, { RTILE_HULK, RTILE_SBMOVE });
	}

	setflag(SFLAG_INVENTORY, {
		RTILE_FIRSTAID,
		RTILE_STEROIDS,
		RTILE_HEATSENSOR,
		RTILE_BOOTS,
		RTILE_COWPIE,
		RTILE_HOLODUKE,
		RTILE_AIRTANK });

	setflag(SFLAG_HITRADIUSCHECK, {
		RTILE_STATUEFLASH,
		RTILE_BOWLINGPIN,
		RTILE_QUEBALL,
		RTILE_STRIPEBALL
		});

	setflag(SFLAG_MOVEFTA_CHECKSEE, { RTILE_VIXEN });
	if (isRRRA())
	{
		setflag(SFLAG_MOVEFTA_CHECKSEE, { RTILE_COOT, RTILE_COOTSTAYPUT, RTILE_BIKER, RTILE_BIKERB, RTILE_BIKERBV2, RTILE_CHEER, RTILE_CHEERB,
				RTILE_CHEERSTAYPUT, RTILE_MINIONBOAT, RTILE_HULKBOAT, RTILE_CHEERBOAT, RTILE_RABBIT, RTILE_COOTPLAY, RTILE_BILLYPLAY, RTILE_MAKEOUT, RTILE_MAMA });
	}

	setflag(SFLAG_TRIGGER_IFHITSECTOR, { RTILE_EXPLOSION2, RTILE_EXPLOSION3 });

	setflag(SFLAG_MOVEFTA_MAKESTANDABLE, {
		RTILE_CANWITHSOMETHING,
		});

	// non-STAT_ACTOR classes that need CON support. For compatibility this must be explicitly enabled.
	setflag(SFLAG3_FORCERUNCON, {
			RTILE_EXPLODINGBARREL,
			RTILE_TOILETWATER,
			RTILE_STEAM,
			RTILE_CEILINGSTEAM,
			RTILE_SHOTSPARK1,
			RTILE_BURNING,
			RTILE_WATERBUBBLE,
			RTILE_WATERBUBBLEMAKER,
			RTILE_SMALLSMOKE,
			RTILE_EXPLOSION2,
			RTILE_EXPLOSION3,
			RTILE_BLOOD,
			RTILE_FORCERIPPLE,
			RTILE_TRANSPORTERSTAR,
			RTILE_TRANSPORTERBEAM
		});

	setflag(SFLAG_NOINTERPOLATE, { RTILE_CRANEPOLE });
	setflag(SFLAG_FALLINGFLAMMABLE, { RTILE_BOX });
	setflag(SFLAG_INFLAME, { RTILE_RADIUSEXPLOSION, RTILE_RPG, RTILE_FIRELASER, RTILE_HYDRENT, RTILE_DYNAMITE, RTILE_POWDERKEG, RTILE_VIXENSHOT, RTILE_OWHIP, RTILE_UWHIP });
	if (isRRRA()) setflag(SFLAG_INFLAME, { RTILE_RPG2 });
	setflag(SFLAG_NOFLOORFIRE, { RTILE_TREE1, RTILE_TREE2 });
	setflag(SFLAG_HITRADIUS_FLAG1, { RTILE_BOX, RTILE_TREE1, RTILE_TREE2, RTILE_TIRE });
	setflag(SFLAG_HITRADIUS_FLAG2, { RTILE_QUEBALL, RTILE_STRIPEBALL, RTILE_BOWLINGPIN, RTILE_DUKELYINGDEAD });
	setflag(SFLAG_NOTELEPORT, { RTILE_TRANSPORTERSTAR, RTILE_TRANSPORTERBEAM, RTILE_BULLETHOLE, RTILE_WATERSPLASH2, RTILE_BURNING, RTILE_FIRE, RTILE_MUD });
	setflag(SFLAG_SE24_NOCARRY, { RTILE_BULLETHOLE, RTILE_BOLT1, RTILE_BOLT2, RTILE_BOLT3, RTILE_BOLT4, RTILE_CRANE, RTILE_CRANE1, RTILE_CRANE2, RTILE_BARBROKE });
	setflag(SFLAG_SE24_REMOVE, { RTILE_BLOODPOOL, RTILE_PUKE, RTILE_FOOTPRINTS, RTILE_FOOTPRINTS2, RTILE_FOOTPRINTS3 });
	setflag(SFLAG_NOFALLER, { RTILE_CRACK1, RTILE_CRACK2, RTILE_CRACK3, RTILE_CRACK4 });
	setflag(SFLAG2_EXPLOSIVE, {RTILE_RPG, RTILE_RADIUSEXPLOSION, RTILE_SEENINE, RTILE_OOZFILTER });
	if (isRRRA()) setflag(SFLAG2_EXPLOSIVE, { RTILE_RPG2 });
	setflag(SFLAG2_BRIGHTEXPLODE, { RTILE_SEENINE, RTILE_OOZFILTER });
	setflag(SFLAG2_DOUBLEDMGTHRUST, { RTILE_RADIUSEXPLOSION, RTILE_RPG, RTILE_HYDRENT, RTILE_DYNAMITE, RTILE_SEENINE, RTILE_OOZFILTER, RTILE_POWDERKEG });
	if (isRRRA()) setflag(SFLAG2_DOUBLEDMGTHRUST, { RTILE_RPG2 });
	setflag(SFLAG2_BREAKMIRRORS, { RTILE_RADIUSEXPLOSION, RTILE_RPG, RTILE_HYDRENT, RTILE_DYNAMITE, RTILE_SEENINE, RTILE_OOZFILTER, RTILE_POWDERKEG });
	if (isRRRA()) setflag(SFLAG2_BREAKMIRRORS, { RTILE_RPG2 });
	setflag(SFLAG2_CAMERA, { RTILE_CAMERA1 });
	setflag(SFLAG2_GREENBLOOD, { RTILE_OOZFILTER });
	setflag(SFLAG2_ALWAYSROTATE1, { RTILE_RAT, RTILE_CAMERA1, RTILE_CHAIR3 });
	setflag(SFLAG2_ALWAYSROTATE2, { RTILE_RPG });
	setflag(SFLAG2_DIENOW, { RTILE_RADIUSEXPLOSION });
	setflag(SFLAG2_NORADIUSPUSH, { RTILE_HULK });
	setflag(SFLAG2_FREEZEDAMAGE | SFLAG2_REFLECTIVE, { RTILE_FREEZEBLAST });
	setflag(SFLAG2_FLOATING, { RTILE_DRONE });
	setflag(SFLAG3_BLOODY, { RTILE_BLOODPOOL });

	setflag(SFLAG2_NOFLOORPAL, {
		RTILE_RESPAWNMARKERRED,
		RTILE_RESPAWNMARKERYELLOW,
		RTILE_RESPAWNMARKERGREEN,
		RTILE_FORCESPHERE,
		RTILE_BURNING,
		RTILE_ATOMICHEALTH,
		RTILE_CRYSTALAMMO,
		RTILE_SHITBALL,
		RTILE_RPG,
		RTILE_RECON,
		RTILE_POWDERKEG,
		RTILE_WATERBUBBLE,
		});
	// Animals were not supposed to have this, but due to a coding bug the logic was unconditional for everything in the game.
	for (auto& ainf : gs.actorinfo)
	{
		ainf.flags |= SFLAG_MOVEFTA_WAKEUPCHECK;
	}


	if (isRRRA())
	{
		setflag(SFLAG_MOVEFTA_CHECKSEEWITHPAL8, { RTILE_MINION });
		setflag(SFLAG2_TRANFERPALTOJIBS, { RTILE_MINION });
		setflag(SFLAG2_NORADIUSPUSH, { RTILE_MAMA, RTILE_BILLYPLAY, RTILE_COOTPLAY, RTILE_MAMACLOUD });
		setflag(SFLAG2_DONTDIVE, { RTILE_CHEERBOAT, RTILE_HULKBOAT, RTILE_MINIONBOAT, RTILE_UFO1_RRRA });
		setflag(SFLAG2_FLOATING, { RTILE_UFO1_RRRA });
		setflag(SFLAG2_SPAWNRABBITGUTS, { RTILE_MAMA });
		setflag(SFLAG2_ALTPROJECTILESPRITE, { RTILE_CHEER, RTILE_CHEERSTAYPUT });
		setflag(SFLAG2_UNDERWATERSLOWDOWN, { RTILE_RPG2 });
		setflag(SFLAG2_ALWAYSROTATE2, { RTILE_RPG2, RTILE_EMPTYBIKE, RTILE_EMPTYBOAT });
		setflag(SFLAG2_NOFLOORPAL, { RTILE_CHEERBOMB, RTILE_RPG2 });
	}
	else
	{
		setflag(SFLAG2_NORADIUSPUSH, { RTILE_SBMOVE });
		setflag(SFLAG2_DONTDIVE, { RTILE_UFO1_RR, RTILE_UFO2, RTILE_UFO3, RTILE_UFO4, RTILE_UFO5 });
		setflag(SFLAG2_FLOATING, { RTILE_UFO1_RR, RTILE_UFO2, RTILE_UFO3, RTILE_UFO4, RTILE_UFO5 });
	}

	gs.actorinfo[RTILE_RPG2].flags |= SFLAG_FORCEAUTOAIM;

	// clear some bad killcount defaults
	for (auto t : { RTILE_COW, RTILE_HEN, RTILE_PIG, RTILE_MINECARTKILLER, RTILE_UFOBEAM }) gs.actorinfo[t].flags &= ~SFLAG_KILLCOUNT;
	if (isRRRA()) gs.actorinfo[RTILE_WACOWINDER].flags &= ~SFLAG_KILLCOUNT;

	gs.weaponsandammosprites[0] = RTILE_CROSSBOWSPRITE;
	gs.weaponsandammosprites[1] = RTILE_RIFLEGUNSPRITE;
	gs.weaponsandammosprites[2] = RTILE_DEVISTATORAMMO;
	gs.weaponsandammosprites[3] = RTILE_RPGAMMO;
	gs.weaponsandammosprites[4] = RTILE_RPGAMMO;
	gs.weaponsandammosprites[5] = RTILE_COWPIE;
	gs.weaponsandammosprites[6] = RTILE_SHIELD;
	gs.weaponsandammosprites[7] = RTILE_FIRSTAID;
	gs.weaponsandammosprites[8] = RTILE_STEROIDS;
	gs.weaponsandammosprites[9] = RTILE_RPGAMMO;
	gs.weaponsandammosprites[10] = RTILE_RPGAMMO;
	gs.weaponsandammosprites[11] = RTILE_CROSSBOWSPRITE;
	gs.weaponsandammosprites[12] = RTILE_RPGAMMO;
	gs.weaponsandammosprites[13] = RTILE_TITSPRITE;
	gs.weaponsandammosprites[14] = RTILE_FREEZEAMMO;

	TILE_APLAYER = RTILE_APLAYER;
	TILE_DRONE = RTILE_DRONE;
	TILE_WATERBUBBLE = RTILE_WATERBUBBLE;
	TILE_BLOODPOOL = RTILE_BLOODPOOL;
	TILE_MIRRORBROKE = RTILE_MIRRORBROKE;
	TILE_CROSSHAIR = RTILE_CROSSHAIR;

	gs.firstdebris = RTILE_SCRAP6;
	gs.gutsscale = 0.125;
}

END_DUKE_NS
