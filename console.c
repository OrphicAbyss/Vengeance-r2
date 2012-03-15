/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// console.c

#ifdef NeXT
#include <libc.h>
#endif
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

int 		con_linewidth;

float		con_cursorspeed = 4;

#define		CON_TEXTSIZE	16384

qboolean 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text=0;

cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_spiral = {"con_spiral","0",true};

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

int			con_vislines;

qboolean	con_debuglog;

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

qboolean	con_initialized;

int			con_notifylines;		// scan lines to clear for notify lines

extern void M_Menu_Main_f (void);

/*
==========================================
Console Game

Yes, I, Entar, am starting a new fad in the Quake1 community.  Console games!
These fun little text-based games, whether they be Quake related or entirely novel, will
distract players from the 3D realm of Quake - and they get great FPS!
==========================================
-==Coded entirely by Entar==-
*/

/* Development notes:

Well, it's pretty hilarious, and has been fun to code so far.
Since I'm not that great a coder, there are probably a few things 
in there that could have been done in a better or more efficient way.
But it works for now.

TODO: Code more stages, give new weapons, etc.
*/

int congame = 0; // no game by default, 1 = game1, 2 = game2, etc.
int	gameon = false; // game is off by default

int		shells = 15;
int		nails = 0;
int		rockets = 0;
int		haveshotgun = true;
int		havedoublesg = false;
int		havenailgun = false;
int		haverl = false;

float	clear = 0;	// is the area clear? (false = bad, true = clear)
int		stage = 0;	// what part the player is at
int		health = 100;	// how much health the player has
int		goods = false;	// are there any goods to get?

void consolegame_stats(void)
{
	if (!gameon)
		return;

	if (congame = 1){
		Con_Printf("Console game status\n");
		Con_Printf("\n");
		Con_Printf("You are on stage %i.\n",stage);
		Con_Printf("You have %i health.\n",health);
		Con_Printf("You have %i shells.\n",shells);
		Con_Printf("You have %i nails.\n",nails);
		Con_Printf("You have %i rockets.\n",rockets);
		if (clear >= 1)
			Con_Printf("The area is clear.\n");
		else
			Con_Printf("The area is still dangerous.\n");
	}
}

void consolegame_hurt(void)
{
	int random;
	
	if (!gameon)
		return;

	random = (rand()&3);

	if (random >= 2)
	{
		Con_Print ("The enemy attacks and you are hit!");
		health -= 15;
		if (health <= 0)
		{
			Con_Print ("You have died, restarting.");
	
			// set everything up for beginning again
			stage = 1;
			congame = 1;
			shells = 15; // ammos
			nails = 0;
			rockets = 0;
			haveshotgun = true; // guns
			havedoublesg = false;
			havenailgun = false;
			haverl = false;
			health = 100; // full health
			Con_Print("Game 1 initialized.\n");
		}
	}
	else
		Con_Print ("The enemy attacks and misses!");
}

void consolegame_shoot(void)
{
	if (clear >= 1 || !gameon)
		return;

	if (haverl == true && rockets >= 1){ // have the RL?
		clear += 2;
		rockets -= 1;
		Con_Print ("You launch a rocket from your rocket\n");
		Con_Print ("launcher at the attacker and inflict 200 damage.\n");
	}
	else if (havenailgun == true && nails >= 1){ // have the Nailgun?
		clear += 1;
		nails -= 5;
		Con_Print ("You fire your nailgun at\n");
		Con_Print ("the attacker and inflict 100 damage.\n");
		goto last;
	}
	else if (havedoublesg == true && shells >= 2){ // have Double barreled shotgun?
		clear += 0.75;
		shells -= 2;
		Con_Print ("You discharge your double barreled shotgun at\n");
		Con_Print ("the attacker and inflict 75 damage.\n");
		goto last;
	}
	else if (haveshotgun == true && shells >= 1){ // have shotgun? always happens, unless out of ammo.
		clear += 0.5;
		shells -= 1;
		Con_Print ("You fire your shotgun at the attacker and\n");
		Con_Print ("inflict 50 damage.\n");
		goto last;
	}
	else // axe him!
	{
		clear += 0.5;
		Con_Print ("You charge with your axe and inflict 50 damage\n");
	}

last:
	if (clear >= 1)
	{
		Con_Print ("You have killed all attackers.\n");
		goods = 1;
	}
	else
	{
		Con_Print ("The attacker is hurt, but still remains.\n");
		consolegame_hurt ();
	}
}

void consolegame_get(void)
{
	if (!gameon)
		return;

	if (!clear == 1 || !goods == 1) // Is the room clear, and are there goods?
	{
		Con_Print ("You can't pick up stuff now.\n");
		return;
	}

	if (stage == 2)
		shells += 2;
	Con_Print ("You pick up some stuff.\n");
	goods = 0;
}

void consolegame_1(void)
{
	if (!gameon)
		return;

	if (stage == 2) // Stage 2, obviously
	{
		if (clear == 1)
		{
			Con_Printf("The end!\n");
			Con_Printf("You have just completed the shortest *and quite\n");
			Con_Printf("possibly the dumbest* text-based console game.\n");
			Con_Printf("\n");
			Con_Printf("All values reset, quitting console games.\n");

			congame = 0;
			gameon = false; // game is now off

			shells = 15;
			nails = 0;
			rockets = 0;
			haveshotgun = true;
			havedoublesg = false;
			havenailgun = false;
			haverl = false;
			clear = 0;
			stage = 0;
			health = 100;
			goods = false;
		}
		else
			goto none;
	}

	if (stage == 1) // Stage 1
	{
		clear = 0;
		Con_Printf("You step out of the slipgate you just came through, and pump\n");
		Con_Printf("a shell into your 12 gauge shotgun, anticipating of any action.\n");
		Con_Printf("A damaged light flickers on and off to your right, and working lights\n");
		Con_Printf("shine to your left and hang overhead.  You step cautiously forward,\n");
		Con_Printf("and as the door to your left jerks open, you see an obviously hostile\n");
		Con_Printf("soldier behind it as he lifts his gun to attack you.\n");
		stage = 2;
	}

	if (stage == 0) // set everything up for beginning
	{
		stage = 1;
		congame = 1;
		shells = 15; // ammos
		nails = 0;
		rockets = 0;
		haveshotgun = true; // guns
		havedoublesg = false;
		havenailgun = false;
		haverl = false;
		health = 100; // full health
		Con_Print("Game 1 initialized.\n");
	}
none:
	Con_Print("");
}

void consolegame1_start (void)
{
	if (stage != 0)
		return;
	else
		consolegame_1();
}

void consolegame_main (void) // called by 'console_game'
{
	gameon = true;
	
	Con_Clear_f();

	Con_Printf("Console MiniGames!\n");
	Con_Printf("\n");
	Con_Printf("Greetings, honored traveler!\n");
	Con_Printf("Choose a game, and stumble into a magical land.\n");
	Con_Printf("Your mission then is to get to the portal so you can return to your home.\n");
	Con_Printf("\n");

	Con_Printf("To begin, type game1 in the console, then congame_advance.\n");
	Con_Printf("\n");
}

void consolegame_init (void)
{
	Cmd_AddCommand ("console_game", consolegame_main); // initiate console games

	Cmd_AddCommand ("game1", consolegame1_start); // start game #1
	//Cmd_AddCommand ("game2", consolegame_2);

	Cmd_AddCommand ("congame_status", consolegame_stats); // how we doin'?
	Cmd_AddCommand ("congame_shoot", consolegame_shoot); // fire away, cap'n!
	Cmd_AddCommand ("congame_advance", consolegame_1); // must keep going!
	Cmd_AddCommand ("congame_pickup", consolegame_get); // grab them goods
}
//*********************************************************************//
//**************************END CONSOLE GAMES**************************//
//*********************************************************************//

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			key_dest = key_game;
			key_lines[edit_line][1] = 0;	// clear any typing
			key_linepos = 1;
		}
		else
		{
			M_Menu_Main_f ();
		}
	}
	else
		key_dest = key_console;
	
	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		Q_memset (con_text, ' ', CON_TEXTSIZE);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	key_dest = key_message;
	team_message = false;
}

						
/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	team_message = true;
}

						
/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 80;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset (con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;
	
		if (con_linewidth < numchars)
			numchars = con_linewidth;

		Q_memcpy (tbuf, con_text, CON_TEXTSIZE);
		Q_memset (con_text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/

void Con_OpenDebugLog(void);

void Con_Init (void)
{
	#define MAXGAMEDIRLEN	1000
	char	temp[MAXGAMEDIRLEN+1];
	char	*t2 = "/qconsole.log";

	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog)
	{
		if (strlen (com_gamedir) < (MAXGAMEDIRLEN - strlen (t2)))
		{
			sprintf (temp, "%s%s", com_gamedir, t2);
			unlink (temp);
		}
		Con_OpenDebugLog ();
	}

	con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
	Q_memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();
	
	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_RegisterVariable (&con_notifytime);
	Cvar_RegisterVariable (&con_spiral);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);

	// Console game!
	consolegame_init ();
	
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con_x = 0;
	con_current++;
	Q_memset (&con_text[(con_current%con_totallines)*con_linewidth]
	, ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;
	
	con_backscroll = 0;

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound ("misc/talk.wav");
	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		
		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
		
	}
}

/*
================
Con_DebugLog
================
*/

vfsfile_t *logfile;

void Con_OpenDebugLog(void)
{
	char *temp;
	temp = va("Vr2_console.log");
//	logfile = fopen(va("%s/QMB.log", com_gamedir),"w");
	logfile = FS_OpenVFS(temp, "wb", FS_GAMEONLY);
}

void Con_DebugLog(char *fmt)
{
//	va_list		args;

//	va_start(args, fmt);

	VFS_WRITE(logfile, fmt, strlen(fmt));

//	vfprintf(logfile, fmt, args);

//	va_end(args);
}

void Con_CloseDebugLog(void)
{
	VFS_CLOSE(logfile);
//	fclose (logfile);
}

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	4096
// FIXME: make a buffer size safe vsprintf?
void Con_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	
// also echo to debugging console
	Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
	if (con_debuglog)
		Con_DebugLog(msg);
//		Con_DebugLog("%s", msg);

	if (!con_initialized)
		return;
		
	if (cls.state == ca_dedicated)
		return;		// no graphics mode

// write it to the scrollable buffer
	Con_Print (msg);
	
// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading )
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
}

#ifdef JAVA
#include <jni.h>
jint JNICALL Con_fPrintf (FILE *empty, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	
// also echo to debugging console
	Sys_Printf ("%s", msg);	// also echo to debugging console

// log all messages to file
	if (con_debuglog)
		Con_DebugLog(msg);
//		Con_DebugLog("%s", msg);

	if (!con_initialized)
		return -1;
		
	if (cls.state == ca_dedicated)
		return -1;		// no graphics mode

// write it to the scrollable buffer
	Con_Print (msg);
	
// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading )
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}

	Con_Printf("Javamsg\n");
	return 0;
}
#endif

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	
	Con_Printf ("%s", msg);
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];
	int			temp;
		
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
==================
Con_SafeDPrintf

A Con_Printf that only shows up if the "developer" cvar is set
Okay to call even when the screen can't be updated
==================
*/
void Con_SafeDPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];
	int			temp;

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		y;
	int		i;
	char	*text;

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = key_lines[edit_line];
	
// add the cursor frame
	text[key_linepos] = 10+((int)(realtime*con_cursorspeed)&1);
	
// fill out remainder with spaces
	for (i=key_linepos+1 ; i< con_linewidth ; i++)
		text[i] = ' ';
		
//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;
		
// draw it
	y = con_vislines-16;

	for (i=0 ; i<con_linewidth ; i++)
		Draw_Character ( (i+1)<<3, con_vislines - 16, text[i]);

// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/

void Con_DrawNotify (void) 
{ 
	int  x, v; 
	char *text; 
	int  i; 
	float time; 
	int   j;
	extern char chat_buffer[];
	v = 0;
   
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
	{ 
		if (i < 0) 
			continue; 

		time = con_times[i % NUM_CON_TIMES]; 
		if (time == 0) 
			continue; 

		time = realtime - time; 
		if (time > con_notifytime.value)
			continue; 

		text = con_text + (i % con_totallines)*con_linewidth; 
		clearnotify = 0; 
		scr_copytop = 1; 
		j = 0; 
		for (x = 0 ; x < con_linewidth ; x++) 
		{ 
			if (text[x] == '&') 
			{ 
				if (text[x + 1] == 'c') 
				{ 
					x += 2; 
					glColor3f((float) (text[x] - '0') / 9, 
					(float)  (text[x + 1] - '0') / 9, 
					(float)  (text[x + 2] - '0') / 9); 
					x += 3; 
				} 
				else if (text[x + 1] == 'r') 
				{ 
					glColor3f(1, 1, 1); 
					x += 2; 
				} 
			}

			if (text[x] == '¦') 
				{ 
				if (text[x + 1] == 'ã'||text[x + 1] == 'Ã') 
				{ 
					x += 2; 
					glColor3f((float) (text[x] - '°') / 9, 
					(float)  (text[x + 1] - '°') / 9, 
					(float)  (text[x + 2] - '°') / 9); 
					x += 3; 
				} 
				else if (text[x + 1] == 'ò') 
				{ 
					glColor3f(1, 1, 1); 
					x += 2; 
				} 
			}
			
			Draw_Character ((j+1)<<3, v, text[x]); 
			j++; 
		} 
		glColor3f(1, 1, 1); 
		v += 8; 
	} 

	if (key_dest == key_message) 
	{ 
		clearnotify = 0; 
		scr_copytop = 1; 

		x = 0; 

		Draw_String (8, v, "say:"); 
		while(chat_buffer[x])
		{
			Draw_Character ( (x+5)<<3, v, chat_buffer[x]);
			x++;
		}
		Draw_Character ( (x+5)<<3, v, 10+((int)(realtime*con_cursorspeed)&1));
		v += 8;
	} 
	if (v > con_notifylines) 
		con_notifylines = v; 

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
extern void Draw_SpiralConsoleBackground(int lines);
void Con_DrawConsole (int lines, qboolean drawinput) 
{ 
	int    i, j, x, y; 
	int    rows; 
	char   *text; 
//	char   dlbar[1024]; 

//no console shown
	if (lines <= 0)
		return; 

// draw the background
	if (con_spiral.value) //Spiral Console - Eradicator
		Draw_SpiralConsoleBackground (lines);
	else
		Draw_ConsoleBackground (lines); 

	con_vislines = lines;
	rows = (lines-22)>>3;  // rows of text to draw 
	//y = lines - 30;
	y = lines - 16 - (rows<<3);	// may start slightly negative

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); 
	glColor3f(1, 1, 1);

	// draw from the bottom up 
	// its top down in quake (bottom up in quakeworld)
	if (con_backscroll) 
	{ 
		// draw arrows to show the buffer is backscrolled 
		for (x=0 ; x < con_linewidth ; x+=4) 
			Draw_Character ( (x+1)<<3, y, '^'); 
		y += 8; 
		rows--; 
	} 

	for (i= con_current - rows + 1 ; i<=con_current ; i++, y+=8 )
	{
		j = i - con_backscroll;
		if (j<0)
			j = 0;

		text = con_text + (j % con_totallines)*con_linewidth; 
		j = 0; 
		for (x=0 ; x < con_linewidth ; x++) 
		{ 
			if (text[x] == '&') 
			{ 
				if (text[x + 1] == 'c'||text[x + 1] == 'C') 
				{ 
					x += 2; 
					glColor3f((float) (text[x] - '0') / 9, 
					(float)  (text[x + 1] - '0') / 9, 
					(float)  (text[x + 2] - '0') / 9); 
					x += 3; 
				} 
				else if (text[x + 1] == 'r') 
				{ 
					glColor3f(1, 1, 1); 
					x += 2; 
				}
			}
			if (text[x] == '¦') 
				{ 
				if (text[x + 1] == 'ã'||text[x + 1] == 'Ã') 
				{ 
					x += 2; 
					glColor3f((float) (text[x] - '°') / 9, 
					(float)  (text[x + 1] - '°') / 9, 
					(float)  (text[x + 2] - '°') / 9); 
					x += 3; 
				} 
				else if (text[x + 1] == 'ò') 
				{ 
					glColor3f(1, 1, 1); 
					x += 2; 
				} 
			} 
			//Draw_Character ((x+1)<<3, y, text[x]);
			Draw_Character ((j+1)<<3, y, text[x]); 
			j++; 
		} 
	}

// draw the input prompt, user text, and cursor if desired
	if (drawinput)
	Con_DrawInput ();

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); 
}

/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	Con_Printf (text);

	Con_Printf ("Press a key.\n");
	Con_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_FloatTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_FloatTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}
