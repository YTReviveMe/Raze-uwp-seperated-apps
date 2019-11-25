//-------------------------------------------------------------------------
/*
Copyright (C) 2016 EDuke32 developers and contributors
Copyright (C) 2019 Christoph Oelckers

This is free software; you can redistribute it and/or
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

#include "cheats.h"
#include "compat.h"
#include "demo.h"
#include "duke3d.h"
#include "input.h"
#include "menus.h"
#include "osdcmds.h"
#include "savegame.h"
#include "game.h"
#include "superfasthash.h"
#include "gamecvars.h"
#include "gamecontrol.h"
#include "c_bind.h"
#include "menu/menu.h"
#include "../../glbackend/glbackend.h"

BEGIN_DUKE_NS

#define MENU_MARGIN_REGULAR 40
#define MENU_MARGIN_WIDE    32
#define MENU_MARGIN_CENTER  160
#define MENU_HEIGHT_CENTER  100

// This is for intermediate levels in the episode selection chain. Ion Fury uses this.
MenuGameplayStemEntry g_MenuGameplayEntries[MAXMENUGAMEPLAYENTRIES];
int ME_NEWGAMECUSTOMENTRIES[MAXMENUGAMEPLAYENTRIES];
int ME_NEWGAMECUSTOMSUBENTRIES[MAXMENUGAMEPLAYENTRIES][MAXMENUGAMEPLAYENTRIES];

enum MenuTextFlags_t
{
	MT_Selected = 1 << 0,
	MT_Disabled = 1 << 1,
	MT_XCenter = 1 << 2,
	MT_XRight = 1 << 3,
	MT_YCenter = 1 << 4,
	MT_Literal = 1 << 5,
	MT_RightSide = 1 << 6,
};


// common font types
// tilenums are set after namesdyn runs.
// These are also modifiable by scripts.
//                                      emptychar x,y       between x,y         zoom                cursorLeft          cursorCenter        cursorScale         textflags
//                                      tilenum             shade_deselected    shade_disabled      pal                 pal_selected        pal_deselected      pal_disabled
MenuFont_t MF_Redfont =               { { 5<<16, 15<<16 },  { 0, 0 },           65536,              20<<16,             110<<16,            65536,              TEXT_BIGALPHANUM | TEXT_UPPERCASE,
                                        -1,                 10,                 0,                  0,                  0,                  0,                  1,
                                        0,                  0,                  1 };
MenuFont_t MF_Bluefont =              { { 5<<16, 7<<16 },   { 0, 0 },           65536,              10<<16,             110<<16,            32768,              0,
                                        -1,                 10,                 0,                  0,                  10,                 10,                 16,
                                        0,                  0,                  16 };
MenuFont_t MF_Minifont =              { { 4<<16, 5<<16 },   { 1<<16, 1<<16 },   65536,              10<<16,             110<<16,            32768,              0,
                                        -1,                 10,                 0,                  0,                  2,                  2,                  0,
                                        0,                  0,                  16 };


/*
This function prepares data after ART and CON have been processed.
It also initializes some data in loops rather than statically at compile time.
*/
void Menu_Init(void)
{

	// prepare menu fonts
	// check if tilenum is -1 in case it was set in EVENT_SETDEFAULTS
	if ((unsigned)MF_Redfont.tilenum >= MAXTILES) MF_Redfont.tilenum = BIGALPHANUM;
	if ((unsigned)MF_Bluefont.tilenum >= MAXTILES) MF_Bluefont.tilenum = STARTALPHANUM;
	if ((unsigned)MF_Minifont.tilenum >= MAXTILES) MF_Minifont.tilenum = MINIFONT;
	MF_Redfont.emptychar.y = tilesiz[MF_Redfont.tilenum].y << 16;
	MF_Bluefont.emptychar.y = tilesiz[MF_Bluefont.tilenum].y << 16;
	MF_Minifont.emptychar.y = tilesiz[MF_Minifont.tilenum].y << 16;
	if (!minitext_lowercase)
		MF_Minifont.textflags |= TEXT_UPPERCASE;

#if 0
	//int32_t i, j, k;
	if (FURY)
	{
		MMF_Top_Skill.pos.x = (320 << 15);
		ME_SKILL_TEMPLATE.format = &MEF_LeftMenu;
	}

	// prepare gamefuncs and keys
	MEOSN_Gamefuncs[0] = MenuGameFuncNone;
	MEOSV_Gamefuncs[0] = -1;
	k = 1;
	for (i = 0; i < NUMGAMEFUNCTIONS; ++i)
	{
		MenuGameFuncs[i] = buttonMap.GetButtonAlias(i);
		MenuGameFuncs[i].Substitute('_', ' ');

		if (MenuGameFuncs[i][0] != '\0')
		{
			MEOSN_Gamefuncs[k] = MenuGameFuncs[i];
			MEOSV_Gamefuncs[k] = i;
			++k;
		}
	}
	MEOS_Gamefuncs.numOptions = k;

	for (i = 0; i < NUMKEYS; ++i)
		MEOSN_Keys[i] = KB_ScanCodeToString(i);
	MEOSN_Keys[NUMKEYS - 1] = MenuKeyNone;


	// prepare episodes
	k = 0;

	if (gVolumeNames[i].IsNotEmpty())
	{
		if (!(gVolumeFlags[i] & EF_HIDEFROMSP))
		{
			MEL_EPISODE[i] = &ME_EPISODE[i];
			ME_EPISODE[i] = ME_EPISODE_TEMPLATE;
			ME_EPISODE[i].name = gVolumeNames[i];
		}

		// if (!(EpisodeFlags[i] & EF_HIDEFROMMP))
		{
			MEOSN_NetEpisodes[k] = gVolumeNames[i];
			MEOSV_NetEpisodes[k] = i;

			k++;
		}
	}
	M_EPISODE.numEntries = g_volumeCnt + 2;

	MEL_EPISODE[g_volumeCnt] = &ME_Space4_Redfont;
	MEL_EPISODE[g_volumeCnt + 1] = &ME_EPISODE_USERMAP;
	MEOSN_NetEpisodes[k] = MenuUserMap;
	MEOSV_NetEpisodes[k] = MAXVOLUMES;

	MEOS_NETOPTIONS_EPISODE.numOptions = k + 1;
	NetEpisode = MEOSV_NetEpisodes[0];
	MMF_Top_Episode.pos.y = (58 + (3 - k) * 6) << 16;
	if (g_skillCnt == 0)
		MEO_EPISODE.linkID = MENU_NULL;
	M_EPISODE.currentEntry = ud.default_volume;

	// prepare new game custom :O
	if (g_MenuGameplayEntries[0].entry.isValid())
	{
		MEO_MAIN_NEWGAME.linkID = M_NEWVERIFY.linkID = MENU_NEWGAMECUSTOM;

		int e = 0;
		for (MenuGameplayStemEntry const& stem : g_MenuGameplayEntries)
		{
			MenuGameplayEntry const& entry = stem.entry;
			if (!entry.isValid())
				break;

			MenuEntry_t& e_me = ME_NEWGAMECUSTOMENTRIES[e];
			e_me = ME_EPISODE_TEMPLATE;
			MenuLink_t& e_meo = MEO_NEWGAMECUSTOM[e];
			e_meo = MEO_NEWGAMECUSTOM_TEMPLATE;
			e_me.entry = &e_meo;

			e_me.name = entry.name;
			if (entry.flags & MGE_Locked)
				e_me.flags |= MEF_Disabled;
			if (entry.flags & MGE_Hidden)
				e_me.flags |= MEF_Hidden;

			int s = 0;
			for (MenuGameplayEntry const& subentry : stem.subentries)
			{
				if (!subentry.isValid())
					break;

				MenuEntry_t& s_me = ME_NEWGAMECUSTOMSUBENTRIES[e][s];
				s_me = ME_EPISODE_TEMPLATE;
				MenuLink_t& s_meo = MEO_NEWGAMECUSTOMSUB[e][s];
				s_meo = MEO_NEWGAMECUSTOMSUB_TEMPLATE;
				s_me.entry = &s_meo;

				s_me.name = subentry.name;
				if (subentry.flags & MGE_Locked)
					s_me.flags |= MEF_Disabled;
				if (subentry.flags & MGE_Hidden)
					s_me.flags |= MEF_Hidden;

				++s;
			}

			if (entry.flags & MGE_UserContent)
				e_meo.linkID = MENU_USERMAP;
			else if (s == 0)
				e_meo.linkID = MENU_SKILL;

			++e;
		}

		Menu_PopulateNewGameCustom();
	}

	// prepare skills
	k = -1;
	for (i = 0; i < g_skillCnt && g_skillNames[i][0]; ++i)
	{
		MEL_SKILL[i] = &ME_SKILL[i];
		ME_SKILL[i] = ME_SKILL_TEMPLATE;
		ME_SKILL[i].name = g_skillNames[i];

		MEOSN_NetSkills[i] = g_skillNames[i];

		k = i;
	}
	++k;
	M_SKILL.numEntries = g_skillCnt; // k;
	MEOS_NETOPTIONS_MONSTERS.numOptions = g_skillCnt + 1; // k+1;
	MEOSN_NetSkills[g_skillCnt] = MenuSkillNone;
	MMF_Top_Skill.pos.y = (58 + (4 - g_skillCnt) * 6) << 16;
	M_SKILL.currentEntry = ud.default_skill;
	Menu_AdjustForCurrentEntryAssignmentBlind(&M_SKILL);

	// prepare multiplayer gametypes
	k = -1;
	for (i = 0; i < MAXGAMETYPES; ++i)
		if (g_gametypeNames[i][0])
		{
			MEOSN_NetGametypes[i] = g_gametypeNames[i];
			k = i;
		}
	++k;
	MEOS_NETOPTIONS_GAMETYPE.numOptions = k;
	if (NAM_WW2GI)
		ME_NETOPTIONS_MONSTERS.name = "Enemies";

	// prepare cheats
	for (i = 0; i < NUMCHEATFUNCS; ++i)
		MEL_CHEATS[i + 1] = &ME_CheatCodes[i];

	// prepare text chat macros
	for (i = 0; i < MAXRIDECULE; ++i)
	{
		MEL_MACROS[i] = &ME_MACROS[i];
		ME_MACROS[i] = ME_MACROS_TEMPLATE;
		ME_MACROS[i].entry = &MEO_MACROS[i];
		MEO_MACROS[i] = MEO_MACROS_TEMPLATE;

		MEO_MACROS[i].variable = sink;// ud.ridecule[i];	temporarily disabled
	}

	// prepare input
	for (i = 0; i < NUMGAMEFUNCTIONS; ++i)
	{
		if (MenuGameFuncs[i][0] == '\0')
		{
			MEL_KEYBOARDSETUPFUNCS[i] = NULL;
			continue;
		}

		MEL_KEYBOARDSETUPFUNCS[i] = &ME_KEYBOARDSETUPFUNCS[i];
		ME_KEYBOARDSETUPFUNCS[i] = ME_KEYBOARDSETUPFUNCS_TEMPLATE;
		ME_KEYBOARDSETUPFUNCS[i].name = MenuGameFuncs[i];
		ME_KEYBOARDSETUPFUNCS[i].entry = &MEO_KEYBOARDSETUPFUNCS[i];
		MEO_KEYBOARDSETUPFUNCS[i] = MEO_KEYBOARDSETUPFUNCS_TEMPLATE;
	}
	M_KEYBOARDKEYS.numEntries = NUMGAMEFUNCTIONS;
	for (i = 0; i < 2 * joystick.numButtons + 8 * joystick.numHats; ++i)
	{
		if (i < 2 * joystick.numButtons)
		{
			if (i & 1)
				Bsnprintf(MenuJoystickNames[i], MAXJOYBUTTONSTRINGLENGTH, "Double %s", joyGetName(1, i >> 1));
			else
				Bstrncpy(MenuJoystickNames[i], joyGetName(1, i >> 1), MAXJOYBUTTONSTRINGLENGTH);
		}
		else
		{
			Bsnprintf(MenuJoystickNames[i], MAXJOYBUTTONSTRINGLENGTH, (i & 1) ? "Double Hat %d %s" : "Hat %d %s", ((i - 2 * joystick.numButtons) >> 3), MenuJoystickHatDirections[((i - 2 * joystick.numButtons) >> 1) % 4]);
		}
	}
	for (i = 0; i < joystick.numAxes; ++i)
	{
		ME_JOYSTICKAXES[i] = ME_JOYSTICKAXES_TEMPLATE;
		Bstrncpy(MenuJoystickAxes[i], joyGetName(0, i), MAXJOYBUTTONSTRINGLENGTH);
		ME_JOYSTICKAXES[i].name = MenuJoystickAxes[i];
		MEL_JOYSTICKAXES[i] = &ME_JOYSTICKAXES[i];
	}
	M_JOYSTICKAXES.numEntries = joystick.numAxes;

	// prepare sound setup
#ifndef EDUKE32_STANDALONE
	if (WW2GI)
		ME_SOUND_DUKETALK.name = "GI talk:";
	else if (NAM)
		ME_SOUND_DUKETALK.name = "Grunt talk:";
#endif

	if (FURY)
	{
		MF_Redfont.between.x = 2 << 16;
		MF_Redfont.cursorScale = 32768;
		MF_Redfont.zoom = 16384;
		MF_Bluefont.zoom = 16384;

		// hack; should swap out pointers
		MF_Minifont = MF_Bluefont;

		MMF_Top_Main.pos.x = 40 << 16;
		MMF_Top_Main.pos.y = 130 << 16;
		MMF_Top_Main.bottomcutoff = 190 << 16;
		M_OPTIONS.format = &MMF_Top_Main;

		MEF_MainMenu.width = MEF_OptionsMenu.width = -(160 << 16);
		MEF_MainMenu.marginBottom = 7 << 16;

		M_OPTIONS.title = NoTitle;

		SELECTDIR_z = 16384;
	}

	// prepare shareware
	if (VOLUMEONE)
	{
		// blue out episodes beyond the first
		for (i = 1; i < g_volumeCnt; ++i)
		{
			if (MEL_EPISODE[i])
			{
				ME_EPISODE[i].entry = &MEO_EPISODE_SHAREWARE;
				ME_EPISODE[i].flags |= MEF_LookDisabled;
			}
		}
		M_EPISODE.numEntries = g_volumeCnt; // remove User Map (and spacer)
		MEOS_NETOPTIONS_EPISODE.numOptions = 1;
		MenuEntry_DisableOnCondition(&ME_NETOPTIONS_EPISODE, 1);
	}

	// prepare pre-Atomic
	if (!VOLUMEALL || !PLUTOPAK)
	{
		// prepare credits
		M_CREDITS.title = M_CREDITS2.title = M_CREDITS3.title = s_Credits;
	}

	MenuEntry_HideOnCondition(&ME_MAIN_HELP, G_GetLogoFlags() & LOGO_NOHELP);
#ifndef EDUKE32_SIMPLE_MENU
	MenuEntry_HideOnCondition(&ME_MAIN_CREDITS, G_GetLogoFlags() & LOGO_NOCREDITS);
#endif
#endif
}

static void Menu_DrawTopBar(const vec2_t origin)
{
    if ((G_GetLogoFlags() & LOGO_NOTITLEBAR) == 0)
        rotatesprite_fs(origin.x + (MENU_MARGIN_CENTER<<16), origin.y + (19<<16), MF_Redfont.cursorScale, 0,MENUBAR,16,0,10);
}

static void Menu_DrawTopBarCaption(const char *caption, const vec2_t origin)
{
    static char t[64];
    size_t const srclen = strlen(caption);
    size_t const dstlen = min(srclen, ARRAY_SIZE(t)-1);
    memcpy(t, caption, dstlen);
    t[dstlen] = '\0';
    char *p = &t[dstlen-1];
    if (*p == ':')
        *p = '\0';
    captionmenutext(origin.x + (MENU_MARGIN_CENTER<<16), origin.y + (24<<16) + ((15>>1)<<16), t);
}

static void Menu_GetFmt(const MenuFont_t* font, uint8_t const status, int32_t* s, int32_t* z)
{
	if (status & MT_Selected)
		*s = VM_OnEventWithReturn(EVENT_MENUSHADESELECTED, -1, myconnectindex, sintable[((int32_t)totalclock << 5) & 2047] >> 12);
	else
		*s = font->shade_deselected;
	// sum shade values
	if (status & MT_Disabled)
		*s += font->shade_disabled;

	if (FURY && status & MT_Selected)
		*z += (*z >> 4);
}

static vec2_t Menu_Text(int32_t x, int32_t y, const MenuFont_t* font, const char* t, uint8_t status, int32_t ydim_upper, int32_t ydim_lower)
{
	int32_t s, p, ybetween = font->between.y;
	int32_t f = font->textflags;
	if (status & MT_XCenter)
		f |= TEXT_XCENTER;
	if (status & MT_XRight)
		f |= TEXT_XRIGHT;
	if (status & MT_YCenter)
	{
		f |= TEXT_YCENTER | TEXT_YOFFSETZERO;
		ybetween = font->emptychar.y; // <^ the battle against 'Q'
	}
	if (status & MT_Literal)
		f |= TEXT_LITERALESCAPE;

	int32_t z = font->zoom;

	if (status & MT_Disabled)
		p = (status & MT_RightSide) ? font->pal_disabled_right : font->pal_disabled;
	else if (status & MT_Selected)
		p = (status & MT_RightSide) ? font->pal_selected_right : font->pal_selected;
	else
p = (status & MT_RightSide) ? font->pal_deselected_right : font->pal_deselected;

Menu_GetFmt(font, status, &s, &z);

return G_ScreenText(font->tilenum, x, y, z, 0, 0, t, s, p, 2 | 8 | 16 | ROTATESPRITE_FULL16, 0, font->emptychar.x, font->emptychar.y, font->between.x, ybetween, f, 0, ydim_upper, xdim - 1, ydim_lower);
}

static int32_t Menu_CursorShade(void)
{
	return VM_OnEventWithReturn(EVENT_MENUCURSORSHADE, -1, myconnectindex, 4 - (sintable[((int32_t)totalclock << 4) & 2047] >> 11));
}

static void Menu_DrawCursorCommon(int32_t x, int32_t y, int32_t z, int32_t picnum, int32_t ydim_upper = 0, int32_t ydim_lower = ydim - 1)
{
	rotatesprite_(x, y, z, 0, picnum, Menu_CursorShade(), 0, 2 | 8, 0, 0, 0, ydim_upper, xdim - 1, ydim_lower);
}

static void Menu_DrawCursorLeft(int32_t x, int32_t y, int32_t z)
{
	if (FURY) return;
	Menu_DrawCursorCommon(x, y, z, VM_OnEventWithReturn(EVENT_MENUCURSORLEFT, -1, myconnectindex, SPINNINGNUKEICON + (((int32_t)totalclock >> 3) % 7)));
}

static void Menu_DrawCursorRight(int32_t x, int32_t y, int32_t z)
{
	if (FURY) return;
	Menu_DrawCursorCommon(x, y, z, VM_OnEventWithReturn(EVENT_MENUCURSORRIGHT, -1, myconnectindex, SPINNINGNUKEICON + 6 - ((6 + ((int32_t)totalclock >> 3)) % 7)));
}

static int Menu_GetFontHeight(int fontnum)
{
	auto& font = fontnum == NIT_BigFont ? MF_Redfont : fontnum == NIT_SmallFont ? MF_Bluefont : MF_Minifont;
	return font.get_yline();
}

void GameInterface::DrawNativeMenuText(int fontnum, int state, int xpos, int ypos, float fontscale, const char* text, int flags)
{
	int ydim_upper = 0;
	int ydim_lower = ydim - 1;
	int32_t const indent = 0;	// not set for any relevant menu
	int32_t x = xpos;

	uint8_t status = 0;
	if (state == NIT_SelectedState)
		status |= MT_Selected;
	if (state == NIT_InactiveState)
		status |= MT_Disabled;
	if (flags & LMF_Centered)
		status |= MT_XCenter;

	bool const dodraw = true;
	MenuFont_t& font = fontnum == NIT_BigFont ? MF_Redfont : fontnum == NIT_SmallFont ? MF_Bluefont : MF_Minifont;

	int32_t const height = font.get_yline();
	status |= MT_YCenter;
	int32_t const y_internal = ypos + ((height >> 17) << 16);// -menu->scrollPos;

	vec2_t textsize;
	if (dodraw)
		textsize = Menu_Text(x, y_internal, &font, text, status, ydim_upper, ydim_lower);

	if (dodraw && (status & MT_Selected) && state != 1)
	{
		if (status & MT_XCenter)
		{
			Menu_DrawCursorLeft(x + font.cursorCenterPosition, y_internal, font.cursorScale);
			Menu_DrawCursorRight(x - font.cursorCenterPosition, y_internal, font.cursorScale);
		}
		else
			Menu_DrawCursorLeft(x /*+ indent*/ - font.cursorLeftPosition, y_internal, font.cursorScale);
	}

}



class DukeListMenu : public DListMenu
{
	using Super = DListMenu;
protected:

	virtual void CallScript(int event, bool getorigin = false)
	{
		ud.returnvar[0] = origin.x;
		ud.returnvar[1] = origin.y;
		ud.returnvar[2] = mDesc->mSelectedItem;
		VM_OnEventWithReturn(event, g_player[screenpeek].ps->i, screenpeek, mDesc->mScriptId);
		if (getorigin)
		{
			origin.x = ud.returnvar[0];
			origin.y = ud.returnvar[1];
		}
	}

	void Ticker() override
	{
		auto lf = G_GetLogoFlags();
		help_disabled = (lf & LOGO_NOHELP);
		credits_disabled = (lf & LOGO_NOCREDITS);

		// Lay out the menu. Since scripts are allowed to mess around with the font this needs to be redone each frame.
		int32_t y_upper = mDesc->mYpos;
		int32_t y_lower = y_upper + mDesc->mYbotton;
		int32_t y = 0;
		int32_t calculatedentryspacing = 0;
		int32_t const height = Menu_GetFontHeight(mDesc->mNativeFontNum) >> 16;

		// None of the menus still being supported will hide entries - only decactivate them if not applicable.
		int32_t totalheight = 0, numvalidentries = mDesc->mItems.Size();

		for (unsigned e = 0; e < mDesc->mItems.Size(); ++e)
		{
			auto entry = mDesc->mItems[e];
			entry->mHidden = false;
			if (entry->GetAction(nullptr) == NAME_HelpMenu && help_disabled)
			{
				entry->mHidden = true;
				numvalidentries--;
				continue;
			}
			else if (entry->GetAction(nullptr) == NAME_CreditsMenu && credits_disabled)
			{
				entry->mHidden = true;
				numvalidentries--;
				continue;
			}
			entry->SetHeight(height);
			totalheight += height;
		}
		if (mDesc->mSpacing <= 0) calculatedentryspacing = std::max(0, (y_lower - y_upper - totalheight) / (numvalidentries > 1 ? numvalidentries - 1 : 1));
		if (calculatedentryspacing <= 0) calculatedentryspacing = mDesc->mSpacing;


		// totalHeight calculating pass
		int totalHeight;
		for (unsigned e = 0; e < mDesc->mItems.Size(); ++e)
		{
			auto entry = mDesc->mItems[e];
			if (!entry->mHidden)
			{
				entry->SetY(y_upper + y);
				y += height;
				totalHeight = y;
				y += calculatedentryspacing;
			}
		}
	}

	void PreDraw() override
	{
		CallScript(CurrentMenu == this ? EVENT_DISPLAYMENU : EVENT_DISPLAYMENUREST, true);
	}

	void PostDraw() override
	{
		CallScript(CurrentMenu == this ? EVENT_DISPLAYMENUREST : EVENT_DISPLAYINACTIVEMENUREST, false);
	}
};

class DukeNewGameCustomSubMenu : public DukeListMenu
{
	virtual void CallScript(int event, bool getorigin) override
	{
		// This needs to get the secondary ID to the script.
		ud.returnvar[3] = mDesc->mSecondaryId;
		DukeListMenu::CallScript(event, getorigin);
	}
};

class MainMenu : public DukeListMenu
{
	void PreDraw() override
	{
		DukeListMenu::PreDraw();
        if ((G_GetLogoFlags() & LOGO_NOGAMETITLE) == 0)
        {
            rotatesprite_fs((origin.x << 16) + (MENU_MARGIN_CENTER<<16), (origin.y << 16) + ((28)<<16), 65536L,0,INGAMEDUKETHREEDEE,0,0,10);
            if (PLUTOPAK)   // JBF 20030804
                rotatesprite_fs((origin.y << 16) + ((MENU_MARGIN_CENTER+100)<<16), (origin.y << 16) + (36<<16), 65536L,0,PLUTOPAKSPRITE+2,(sintable[((int32_t) totalclock<<4)&2047]>>11),0,2+8);
        }
		else if (mDesc->mCaption.IsNotEmpty())
		{
			Menu_DrawTopBar(origin);
			Menu_DrawTopBarCaption(mDesc->mCaption, origin);
		}
	}	
};


void GameInterface::MenuOpened()
{
	S_PauseSounds(true);
	if ((!g_netServer && ud.multimode < 2))
	{
		ready2send = 0;
		totalclock = ototalclock;
		screenpeek = myconnectindex;
	}
}

void GameInterface::MenuSelectSound()
{
	S_PlaySound(KICK_HIT);
}

void GameInterface::MenuChooseSound()
{
	S_PlaySound(PISTOL_BODYHIT);
}


/*
void GameInterface::MenuClosed()
{
	S_PlaySound(EXITMENUSOUND);
	if (!ud.pause_on)
		S_PauseSounds(false);
}
*/

bool GameInterface::CanSave()
{
	if (ud.recstat == 2) return false;
	auto &myplayer = *g_player[myconnectindex].ps;
	if (sprite[myplayer.i].extra <= 0)
	{
		P_DoQuote(QUOTE_SAVE_DEAD, &myplayer);
		return false;
	}
	return true;
}

END_DUKE_NS

static TMenuClassDescriptor<Duke::MainMenu> _mm("Duke.MainMenu");
static TMenuClassDescriptor<Duke::DukeListMenu> _lm("Duke.ListMenu");
static TMenuClassDescriptor<Duke::DukeNewGameCustomSubMenu> _ngcsm("Duke.NewGameCustomSubMenu");

void RegisterDukeMenus()
{
	menuClasses.Push(&_mm);
	menuClasses.Push(&_lm);
	menuClasses.Push(&_ngcsm);
}
