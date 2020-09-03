//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

#include "ns.h"	// Must come before everything else!

#include "build.h"
#include "mmulti.h"
#include "compat.h"
#include "common.h"
#include "common_game.h"
#include "g_input.h"

#include "db.h"
#include "blood.h"
#include "choke.h"
#include "controls.h"
#include "dude.h"
#include "endgame.h"
#include "eventq.h"
#include "fx.h"
#include "gib.h"
#include "globals.h"
#include "levels.h"
#include "loadsave.h"
#include "sectorfx.h"
#include "seq.h"
#include "sound.h"
#include "triggers.h"
#include "view.h"
#include "misc.h"
#include "gameconfigfile.h"
#include "gamecontrol.h"
#include "m_argv.h"
#include "statistics.h"
#include "menu.h"
#include "raze_sound.h"
#include "nnexts.h"
#include "secrets.h"
#include "gamestate.h"
#include "screenjob.h"
#include "mapinfo.h"
#include "choke.h"
#include "d_net.h"
#include "v_video.h"
#include "map2d.h"

BEGIN_BLD_NS

void InitCheats();

bool bNoDemo = false;
int gNetPlayers;
int gQuitRequest;
int gChokeCounter = 0;
bool gQuitGame;
int blood_globalflags;
PLAYER gPlayerTemp[kMaxPlayers];
int gHealthTemp[kMaxPlayers];
vec3_t startpos;
int16_t startang, startsectnum;
MapRecord* gStartNewGame = nullptr;


void QuitGame(void)
{
	throw CExitEvent(0);
}


void EndLevel(void)
{
	gViewPos = VIEWPOS_0;
	sndKillAllSounds();
	sfxKillAllSounds();
	ambKillAll();
	seqKillAll();
}

void StartLevel(MapRecord* level)
{
	if (!level) return;
	gFrameCount = 0;
	gFrameClock = 0;
	STAT_Update(0);
	EndLevel();
	gInput = {};
	gStartNewGame = nullptr;
	currentLevel = level;

	if (gGameOptions.nGameType == 0)
	{
		///////
		gGameOptions.weaponsV10x = gWeaponsV10x;
		///////
	}
#if 0
	else if (gGameOptions.nGameType > 0 && !(gGameOptions.uGameFlags & 1))
	{
		// todo
		gBlueFlagDropped = false;
		gRedFlagDropped = false;
	}
#endif
	if (gGameOptions.uGameFlags & 1)
	{
		for (int i = connecthead; i >= 0; i = connectpoint2[i])
		{
			memcpy(&gPlayerTemp[i], &gPlayer[i], sizeof(PLAYER));
			gHealthTemp[i] = xsprite[gPlayer[i].pSprite->extra].health;
		}
	}
	bVanilla = false;
	memset(xsprite, 0, sizeof(xsprite));
	memset(sprite, 0, kMaxSprites * sizeof(spritetype));
	//drawLoadingScreen();
	if (dbLoadMap(currentLevel->fileName, (int*)&startpos.x, (int*)&startpos.y, (int*)&startpos.z, &startang, &startsectnum, nullptr))
	{
		I_Error("%s: Unable to load map", level->DisplayName());
	}
	SECRET_SetMapName(currentLevel->DisplayName(), currentLevel->name);
	STAT_NewLevel(currentLevel->fileName);
	G_LoadMapHack(currentLevel->fileName);
	wsrand(dbReadMapCRC(currentLevel->LabelName()));
	gKillMgr.Clear();
	gSecretMgr.Clear();
	automapping = 1;

	int modernTypesErased = 0;
	for (int i = 0; i < kMaxSprites; i++)
	{
		spritetype* pSprite = &sprite[i];
		if (pSprite->statnum < kMaxStatus && pSprite->extra > 0) {

			XSPRITE* pXSprite = &xsprite[pSprite->extra];
			if ((pXSprite->lSkill & (1 << gGameOptions.nDifficulty)) || (pXSprite->lS && gGameOptions.nGameType == 0)
				|| (pXSprite->lB && gGameOptions.nGameType == 2) || (pXSprite->lT && gGameOptions.nGameType == 3)
				|| (pXSprite->lC && gGameOptions.nGameType == 1)) {

				DeleteSprite(i);
				continue;
			}


#ifdef NOONE_EXTENSIONS
			if (!gModernMap && nnExtEraseModernStuff(pSprite, pXSprite))
				modernTypesErased++;
#endif
		}
	}

#ifdef NOONE_EXTENSIONS
	if (!gModernMap && modernTypesErased > 0)
		Printf(PRINT_NONOTIFY, "> Modern types erased: %d.\n", modernTypesErased);
#endif

	startpos.z = getflorzofslope(startsectnum, startpos.x, startpos.y);
	for (int i = 0; i < kMaxPlayers; i++) {
		gStartZone[i].x = startpos.x;
		gStartZone[i].y = startpos.y;
		gStartZone[i].z = startpos.z;
		gStartZone[i].sectnum = startsectnum;
		gStartZone[i].ang = startang;

#ifdef NOONE_EXTENSIONS
		// Create spawn zones for players in teams mode.
		if (gModernMap && i <= kMaxPlayers / 2) {
			gStartZoneTeam1[i].x = startpos.x;
			gStartZoneTeam1[i].y = startpos.y;
			gStartZoneTeam1[i].z = startpos.z;
			gStartZoneTeam1[i].sectnum = startsectnum;
			gStartZoneTeam1[i].ang = startang;

			gStartZoneTeam2[i].x = startpos.x;
			gStartZoneTeam2[i].y = startpos.y;
			gStartZoneTeam2[i].z = startpos.z;
			gStartZoneTeam2[i].sectnum = startsectnum;
			gStartZoneTeam2[i].ang = startang;
		}
#endif
	}
	InitSectorFX();
	warpInit();
	actInit(false);
	evInit();
	for (int i = connecthead; i >= 0; i = connectpoint2[i])
	{
		if (!(gGameOptions.uGameFlags & 1))
		{
			if (numplayers == 1)
			{
				gProfile[i].skill = gSkill;
			}
			playerInit(i, 0);
		}
		playerStart(i, 1);
	}
	if (gGameOptions.uGameFlags & 1)
	{
		for (int i = connecthead; i >= 0; i = connectpoint2[i])
		{
			PLAYER* pPlayer = &gPlayer[i];
			pPlayer->pXSprite->health &= 0xf000;
			pPlayer->pXSprite->health |= gHealthTemp[i];
			pPlayer->weaponQav = gPlayerTemp[i].weaponQav;
			pPlayer->curWeapon = gPlayerTemp[i].curWeapon;
			pPlayer->weaponState = gPlayerTemp[i].weaponState;
			pPlayer->weaponAmmo = gPlayerTemp[i].weaponAmmo;
			pPlayer->qavCallback = gPlayerTemp[i].qavCallback;
			pPlayer->qavLoop = gPlayerTemp[i].qavLoop;
			pPlayer->weaponTimer = gPlayerTemp[i].weaponTimer;
			pPlayer->nextWeapon = gPlayerTemp[i].nextWeapon;
		}
	}
	gGameOptions.uGameFlags &= ~3;
	PreloadCache();
	InitMirrors();
	trInit();
	if (!bVanilla && !gMe->packSlots[1].isActive) // if diving suit is not active, turn off reverb sound effect
		sfxSetReverb(0);
	ambInit();
	Net_ClearFifo();
	gChokeCounter = 0;
	M_ClearMenus();
	// viewSetMessage("");
	viewSetErrorMessage("");
	paused = 0;
	levelTryPlayMusic();
	gChoke.reset();
}


bool gRestartGame = false;

static void commonTicker()
{
	if (TestBitString(gotpic, 2342))
	{
		FireProcess();
		ClearBitString(gotpic, 2342);
	}
	if (gStartNewGame)
	{
		auto sng = gStartNewGame;
		gStartNewGame = nullptr;
		gQuitGame = false;
		auto completion = [=](bool = false)
		{
			StartLevel(sng);

			gamestate = GS_LEVEL;
		};

		bool startedCutscene = false;
		if (!(sng->flags & MI_USERMAP))
		{
			int episode = volfromlevelnum(sng->levelNumber);
			int level = mapfromlevelnum(sng->levelNumber);
			if (gEpisodeInfo[episode].cutALevel == level && gEpisodeInfo[episode].cutsceneAName[0])
			{
				levelPlayIntroScene(episode, completion);
				startedCutscene = true;
			}

		}
		if (!startedCutscene) completion(false);
	}
	else if (gRestartGame)
	{
		Mus_Stop();
		soundEngine->StopAllChannels();
		gQuitGame = 0;
		gQuitRequest = 0;
		gRestartGame = 0;

		// Don't switch to startup if we're already outside the game.
		if (gamestate == GS_LEVEL)
		{
			gamestate = GS_MENUSCREEN;
			M_StartControlPanel(false);
			M_SetMenu(NAME_Mainmenu);
		}
	}
}



void GameInterface::Ticker()
{
	for (int i = connecthead; i >= 0; i = connectpoint2[i])
	{
		auto& inp = gPlayer[i].input;
		auto oldactions = inp.actions;

		inp = playercmds[i].ucmd;
		inp.actions |= oldactions & ~(SB_BUTTON_MASK | SB_RUN | SB_WEAPONMASK_BITS);  // should be everything non-button and non-weapon

		int newweap = inp.getNewWeapon();
		if (newweap > 0 && newweap < WeaponSel_MaxBlood) gPlayer[i].newWeapon = newweap;
	}

	viewClearInterpolations();

	if (!(paused || (gGameOptions.nGameType == 0 && M_Active())))
	{
		thinktime.Reset();
		thinktime.Clock();
		for (int i = connecthead; i >= 0; i = connectpoint2[i])
		{
			viewBackupView(i);
			playerProcess(&gPlayer[i]);
		}

		trProcessBusy();
		evProcess(gFrameClock);
		seqProcess(4);
		DoSectorPanning();

		actortime.Reset();
		actortime.Clock();
		actProcessSprites();
		actPostProcess();
		actortime.Unclock();

		viewCorrectPrediction();
		ambProcess();
		viewUpdateDelirium();
		viewUpdateShake();
		gi->UpdateSounds();
		if (gMe->hand == 1)
		{
			const int CHOKERATE = 8;
			const int COUNTRATE = 30;
			gChokeCounter += CHOKERATE;
			while (gChokeCounter >= COUNTRATE)
			{
				gChoke.at1c(gMe);
				gChokeCounter -= COUNTRATE;
			}
		}
		thinktime.Unclock();

		gFrameCount++;
		gFrameClock += 4;

		for (int i = 0; i < 8; i++)
		{
			dword_21EFD0[i] = dword_21EFD0[i] -= 4;
			if (dword_21EFD0[i] < 0)
				dword_21EFD0[i] = 0;
		}

		if ((gGameOptions.uGameFlags & 1) != 0 && !gStartNewGame)
		{
			seqKillAll();
			if (gGameOptions.uGameFlags & 2)
			{
				STAT_Update(true);
				if (gGameOptions.nGameType == 0)
				{
					auto completion = [](bool) {
						gamestate = GS_MENUSCREEN;
						M_StartControlPanel(false);
						M_SetMenu(NAME_Mainmenu);
						M_SetMenu(NAME_CreditsMenu);
						gGameOptions.uGameFlags &= ~3;
						gQuitGame = 1;
						gRestartGame = true;
					};

					if (gGameOptions.uGameFlags & 8)
					{
						levelPlayEndScene(volfromlevelnum(currentLevel->levelNumber), completion);
					}
					else completion(false);
				}
				else
				{
					gGameOptions.uGameFlags &= ~3;
					gRestartGame = 1;
					gQuitGame = 1;
				}
			}
			else
			{
				ShowSummaryScreen();
			}
		}
		r_NoInterpolate = false;
	}
	else r_NoInterpolate = true;
	commonTicker();
}

void GameInterface::DrawBackground()
{
	twod->ClearScreen();
	DrawTexture(twod, tileGetTexture(2518, true), 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, TAG_DONE);
	commonTicker();
}



void ReadAllRFS();

void GameInterface::app_init()
{
	InitCheats();
	memcpy(&gGameOptions, &gSingleGameOptions, sizeof(GAMEOPTIONS));
	gGameOptions.nMonsterSettings = !userConfig.nomonsters;
	ReadAllRFS();

	HookReplaceFunctions();

	Printf(PRINT_NONOTIFY, "Initializing Build 3D engine\n");
	engineInit();

	Printf(PRINT_NONOTIFY, "Loading tiles\n");
	if (!tileInit(0, NULL))
		I_FatalError("TILES###.ART files not found");

	levelLoadDefaults();

	loaddefinitionsfile(BLOODWIDESCREENDEF);

	const char* defsfile = G_DefFile();
	uint32_t stime = I_msTime();
	if (!loaddefinitionsfile(defsfile))
	{
		uint32_t etime = I_msTime();
		Printf(PRINT_NONOTIFY, "Definitions file \"%s\" loaded in %d ms.\n", defsfile, etime - stime);
	}
	TileFiles.SetBackup();
	powerupInit();
	Printf(PRINT_NONOTIFY, "Loading cosine table\n");
	trigInit();
	Printf(PRINT_NONOTIFY, "Initializing view subsystem\n");
	viewInit();
	Printf(PRINT_NONOTIFY, "Initializing dynamic fire\n");
	FireInit();
	Printf(PRINT_NONOTIFY, "Initializing weapon animations\n");
	WeaponInit();
	LoadSaveSetup();

	myconnectindex = connecthead = 0;
	gNetPlayers = numplayers = 1;
	connectpoint2[0] = -1;
	gGameOptions.nGameType = 0;
	UpdateNetworkMenus();

	Printf(PRINT_NONOTIFY, "Initializing sound system\n");
	sndInit();
	registerosdcommands();

	gChoke.init(518, sub_84230);
	UpdateDacs(0, true);

	enginecompatibility_mode = ENGINECOMPATIBILITY_19960925;//bVanilla;
}

static void gameInit()
{
	//RESTART:
	gViewIndex = myconnectindex;
	gMe = gView = &gPlayer[myconnectindex];

	PROFILE* pProfile = &gProfile[myconnectindex];
	strcpy(pProfile->name, playername);
	pProfile->skill = gSkill;

	UpdateNetworkMenus();
	gQuitGame = 0;
	gRestartGame = 0;
	if (gGameOptions.nGameType > 0)
	{
		inputState.ClearAllInput();
	}

}


void GameInterface::Startup()
{
	gameInit();
	if (userConfig.CommandMap.IsNotEmpty())
	{
	}
	else
	{
		if (!userConfig.nologo && gGameOptions.nGameType == 0) playlogos();
		else
		{
			startmainmenu();
		}
	}
}


static void nonsharedkeys(void)
{
	static int nonsharedtimer;
	int ms = screen->FrameTime;
	int interval;
	if (nonsharedtimer > 0 || ms < nonsharedtimer)
	{
		interval = ms - nonsharedtimer;
	}
	else
	{
		interval = 0;
	}
	nonsharedtimer = screen->FrameTime;

	if (System_WantGuiCapture())
		return;

	if (automapMode != am_off)
	{
		double j = interval * (120. / 1000);

		if (buttonMap.ButtonDown(gamefunc_Enlarge_Screen))
			gZoom += (int)fmulscale6(j, max(gZoom, 256));
		if (buttonMap.ButtonDown(gamefunc_Shrink_Screen))
			gZoom -= (int)fmulscale6(j, max(gZoom, 256));

		gZoom = clamp(gZoom, 48, 2048);
		gViewMap.nZoom = gZoom;
	}
}

void GameInterface::Render()
{
	nonsharedkeys();
	drawtime.Reset();
	drawtime.Clock();
	viewDrawScreen();
	drawtime.Unclock();
}


void sndPlaySpecialMusicOrNothing(int nMusic)
{
	if (!Mus_Play(nullptr, quoteMgr.GetQuote(nMusic), true))
	{
		Mus_Stop();
	}
}

extern  IniFile* BloodINI;
void GameInterface::FreeGameData()
{
	if (BloodINI) delete BloodINI;
}

void GameInterface::FreeLevelData()
{
	EndLevel();
	::GameInterface::FreeLevelData();
}


ReservedSpace GameInterface::GetReservedScreenSpace(int viewsize)
{
	int top = 0;
	if (gGameOptions.nGameType > 0 && gGameOptions.nGameType <= 3)
	{
		top = (tilesiz[2229].y * ((gNetPlayers + 3) / 4));
	}
	return { top, 25 };
}

::GameInterface* CreateInterface()
{
	return new GameInterface;
}

END_BLD_NS
