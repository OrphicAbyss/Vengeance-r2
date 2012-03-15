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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "CaptureHelpers.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net 
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full
	

*/


int			glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged 
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov;
cvar_t		scr_viewsize = {"viewsize","100", true};
cvar_t		scr_fov = {"fov","90", true};	// 10 - 170
cvar_t		scr_conspeed = {"scr_conspeed","300", true};
cvar_t		scr_centertime = {"scr_centertime","2", true};
cvar_t		scr_showturtle = {"showturtle","0"};
cvar_t		scr_showpause = {"showpause","1"};
cvar_t		scr_printspeed = {"scr_printspeed","8"};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", true};

cvar_t		r_letterbox = {"r_letterbox", "0"};

extern	cvar_t	crosshair;

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

int			sb_lines;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

void SCR_ScreenShot_f (void);
void SCR_ScreenShot_JPEG_f (void);

//QMB: hud stuff
//JHL; for modified resize functions
extern	cvar_t	hud;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

cvar_t	scr_centerfade = {"scr_centerfade", "1", true};

void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;
	float	fade;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	if (!cl.intermission && scr_centerfade.value)
	{
		fade = -(cl.time - scr_centertime_start) + 2;

		if (fade < 0.5)
			fade = 0.5;

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_BLEND);
	}
	else
	{
		fade = 1;
	}

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			if (!cl.intermission && scr_centerfade.value)
				glColor4f(1,1,1,fade);
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
			{
				glColor4f(1,1,1,1);
				return;
			}
		}
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);

	glDisable(GL_BLEND);
}

void SCR_CheckDrawCenterString (void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float   a;
	float   x;
	
	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);
	
	x = width/tan(fov_x/360*M_PI);
	
	a = atan (height/x);
	a = a*360/M_PI;
	
	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float		size;
	int		h;
	qboolean		full = false;


	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

	r_refdef.fovscale_x = 1;
	r_refdef.fovscale_y = 1;

// force the status bar to redraw
	Sbar_Changed ();

//========================================
	
// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

// intermission is always full screen	
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	/* Always draw status bar and inventory
	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else*/
		sb_lines = 24+16+8;

	if (scr_viewsize.value >= 100.0) {
		full = true;
		size = 100.0;
	} else
		size = scr_viewsize.value;
	if (cl.intermission)
	{
		full = true;
		size = 100;
		sb_lines = 0;
	}
	size /= 100.0;

	h = vid.height;// - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = (signed)vid.height * size;
	//if (r_refdef.vrect.height > (signed)vid.height - sb_lines)
	//	r_refdef.vrect.height = (signed)vid.height - sb_lines;
	if (r_refdef.vrect.height > (signed)vid.height)
			r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;

	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	if (scr_fov.value * r_refdef.fovscale_x < 1 || scr_fov.value * r_refdef.fovscale_y > 179)
	{
		Con_Printf ("Invalid FOV: %f - resetting.", scr_fov.value);
		Cvar_SetValue ("fov", 90);
	}

	r_refdef.fov_x = scr_fov.value*r_refdef.fovscale_x;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height*r_refdef.fovscale_y);
	//r_refdef.fov_y = (CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height)) * r_refdef.fovscale_y;

	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	//JHL:HACK; changed to affect the HUD, not SCR size
	if (hud.value < 3)
	{
		Cvar_SetValue ("hud", hud.value+1);
		vid.recalc_refdef = 1;
	}
	//qmb :hud
	//Cvar_SetValue ("viewsize",scr_viewsize.value+10);
	//vid.recalc_refdef = 1;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	//JHL:HACK; changed to affect the HUD, not SCR size
	if (hud.value > 0)
	{
		Cvar_SetValue ("hud", hud.value-1);
		vid.recalc_refdef = 1;
	}
	//qmb :hud
	//Cvar_SetValue ("viewsize",scr_viewsize.value-10);
	//vid.recalc_refdef = 1;
}

//============================================================================

extern char *screenshot_name;
cvar_t	scr_screenshot_name = {"scr_screenshot_name", "vr2-"};
cvar_t	scr_screenshot_gammaboost = {"scr_screenshot_gammaboost", "1", true};
cvar_t	scr_screenshot_jpeg = {"scr_screenshot_jpeg", "1", true};

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");

    // CAPTURE <anthony@planetquake.com>
    CaptureHelper_Init();

	scr_initialized = true;
}

void SCR_Init_Register(void)
{
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&gl_triplebuffer);
	Cvar_RegisterVariable (&r_letterbox);
	Cvar_RegisterVariable (&scr_screenshot_name);
	Cvar_RegisterVariable (&scr_screenshot_gammaboost);
	Cvar_RegisterVariable (&scr_screenshot_jpeg);
	if (screenshot_name)
	{
		Cvar_Set("scr_screenshot_name", screenshot_name);
		free (screenshot_name);
	}

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
}

/*
==================
SCR_Init
==================
*/
void SCR_Shutdown (void)
{
	scr_ram = NULL;
	scr_net = NULL;
	scr_turtle = NULL;

	scr_initialized = false;
}

/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
	//not used anymore
	return;
	Draw_AlphaPic (scr_vrect.x+32, scr_vrect.y, scr_ram, 1);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_AlphaPic (scr_vrect.x, scr_vrect.y, scr_turtle, 1);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_AlphaPic (scr_vrect.x+64, scr_vrect.y, scr_net, 1);
}

/* QMB
//muff - hacked out of SourceForge implementation + modified
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	extern cvar_t show_fps;
	static double lastframetime;
	double t;
	extern int fps_count;
	static float lastfps;
	static int totalfps;
	static int lastsecond;
	int x, y;
	char st[60];

	if (!show_fps.value)
		return;

	t = Sys_FloatTime ();
	lastfps= 1/(t - lastframetime);
	if (((int)(t)%100) > ((int)(lastframetime)%100))
	{
		lastsecond = totalfps;
		totalfps = 0;
	}
	lastframetime = t;
	totalfps += 1;

	if (lastfps < 1 && lastfps > 0)
		sprintf(st, "%4.2f SPF", 1/(float)lastfps);
	else
		sprintf(st, "%4.2f FPS", lastfps);
	x = vid.width - strlen(st) * 8 - 16;
	y = 0;
	if (r_speeds.value)
		y += 360;
	Draw_String(x, y, st);

	sprintf(st, "%i Last second", lastsecond);
	x = vid.width - strlen(st) * 8 - 16;
	y = 8;
	if (r_speeds.value)
		y += 360;
	Draw_String(x, y, st);
}

cvar_t	show_stats = {"show_stats", "0", true};

/*
===============
SCR_DrawStats
===============
*/
void SCR_DrawStats (void)
{
	int		mins, secs, tens;

	if (!show_stats.value || (show_stats.value == 3 || show_stats.value == 4))
		return;

	mins = cl.time / 60;
	secs = cl.time - 60 * mins;
	tens = (int)(cl.time * 10) % 10;

	Draw_String (vid.width - 72, 32, va("%2i:%02i:%i", mins, secs, tens));
	if (show_stats.value == 2 || show_stats.value == 4)
	{
		Draw_String (vid.width - 32, 40, va("%2i", cl.stats[STAT_SECRETS]));
		Draw_String (vid.width - 40, 48, va("%3i", cl.stats[STAT_MONSTERS]));
	}
}

void Scr_ShowNumP (void)
{
	extern cvar_t show_fps;
	int x, y;
	char st[80];
	extern int numParticles;

	if (!show_fps.value)
		return;

	sprintf(st, "%i Particles in world", numParticles);

	x = vid.width - strlen(st) * 8 - 16;
	y = 16 ; //vid.height - (sb_lines * (vid.height/240) )- 16;
	//Draw_TileClear(x, y, strlen(st)*16, 16);
	if (r_speeds.value)
		y += 360;
	Draw_String(x, y, st);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_AlphaPic ( (vid.width - pic->width)/2, (vid.height - 48 - pic->height)/2, pic, 1);
}



/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_AlphaPic ( (vid.width - pic->width)/2, (vid.height - 48 - pic->height)/2, pic, 1);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();
	
	if (scr_drawloading)
		return;		// never a console with loading plaque
		
// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
		con_notifylines = 0;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{
	extern void screenshotJPEG(char *filename, qbyte *screendata, int screenwidth, int screenheight);
	byte		*buffer;
	char		checkname[MAX_OSPATH];
	int			i, num;
// 
// find a file name to save it to 
// 

	for (num=0; num < 1000000; num++)
	{
		sprintf (checkname, "screenshots/%s%06d.%s", scr_screenshot_name.string, num, scr_screenshot_jpeg.value ? "jpg" : "tga");
		if (!COM_FCheckExists(checkname))
			break;
	}
	if (num == 1000000)
	{
		Con_Printf ("SCR_Screenshot_f: Couldn't create a new file\n");
		return;
	}

	if (scr_screenshot_jpeg.value)
	{
		buffer = malloc(glwidth*glheight*3);
		memset (buffer, 0, 18);

		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 

		if (scr_screenshot_gammaboost.value != 1)
		{
			double igamma = 1.0 / scr_screenshot_gammaboost.value;
			unsigned char ramp[256];
			for (i = 0;i < 256;i++)
				ramp[i] = (unsigned char) (pow(i * (1.0 / 255.0), igamma) * 255.0);
			for (i = 0;i < glwidth*glheight*3;i++)
				buffer[i] = ramp[buffer[i]];
		}

		screenshotJPEG(checkname, (qbyte *)buffer, glwidth, glheight);
	}
	else // tga
	{
		int c, temp;
		buffer = malloc(glwidth*glheight*3 + 18);
		memset (buffer, 0, 18);
		buffer[2] = 2;		// uncompressed type
		buffer[12] = glwidth&255;
		buffer[13] = glwidth>>8;
		buffer[14] = glheight&255;
		buffer[15] = glheight>>8;
		buffer[16] = 24;	// pixel size

		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

		// swap rgb to bgr
		c = 18+glwidth*glheight*3;
		for (i=18 ; i<c ; i+=3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}

		if (scr_screenshot_gammaboost.value != 1)
		{
			double igamma = 1.0 / scr_screenshot_gammaboost.value;
			unsigned char ramp[256];
			for (i = 0;i < 256;i++)
				ramp[i] = (unsigned char) (pow(i * (1.0 / 255.0), igamma) * 255.0);
			for (i = 18;i < glwidth*glheight*3;i++)
				buffer[i] = ramp[buffer[i]];
		}

		COM_WriteFile (checkname, buffer, glwidth*glheight*3 + 18 );
	}

	free (buffer);
	Con_Printf ("Wrote %s\n", checkname);
} 

//=============================================================================

// LordHavoc: SHOWLMP stuff
#define SHOWLMP_MAXLABELS 256
typedef struct showlmp_s
{
	qboolean	isactive;
	float		x;
	float		y;
	char		label[32];
	char		pic[128];
}
showlmp_t;

showlmp_t showlmp[SHOWLMP_MAXLABELS];

void SHOWLMP_decodehide(void)
{
	int i;
	qbyte *lmplabel;
	lmplabel = MSG_ReadString();
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive && strcmp(showlmp[i].label, lmplabel) == 0)
		{
			showlmp[i].isactive = false;
			return;
		}
}

size_t strlcpy(char *dst, const char *src, size_t siz);
void SHOWLMP_decodeshow(void)
{
	int i, k;
	qbyte lmplabel[256], picname[256];
	float x, y;
	strlcpy (lmplabel,MSG_ReadString(), sizeof (lmplabel));
	strlcpy (picname, MSG_ReadString(), sizeof (picname));
/*	if (gamemode == GAME_NEHAHRA) // LordHavoc: nasty old legacy junk
	{
		x = MSG_ReadByte();
		y = MSG_ReadByte();
	}
	else
	{*/
		x = MSG_ReadShort();
		y = MSG_ReadShort();
//	}
	k = -1;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
		{
			if (strcmp(showlmp[i].label, lmplabel) == 0)
			{
				k = i;
				break; // drop out to replace it
			}
		}
		else if (k < 0) // find first empty one to replace
			k = i;
	if (k < 0)
		return; // none found to replace
	// change existing one
	showlmp[k].isactive = true;
	strlcpy (showlmp[k].label, lmplabel, sizeof (showlmp[k].label));
	strlcpy (showlmp[k].pic, picname, sizeof (showlmp[k].pic));
	showlmp[k].x = x;
	showlmp[k].y = y;
}

void Sbar_DrawPic (int x, int y, qpic_t *pic);
void SHOWLMP_drawall(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
			Sbar_DrawPic(showlmp[i].x, showlmp[i].y, Draw_PicFromWad (showlmp[i].pic));
//			DrawQ_Pic(showlmp[i].x, showlmp[i].y, showlmp[i].pic, 0, 0, 1, 1, 1, 1, 0);
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}


/*
===============
Darkie testing range
SHOWICO (altered from lh showlmp)

===============
*/
#define SHOWICO_MAX 64
int	showico_icons[SHOWICO_MAX];
typedef struct showico_s
{
	float		fader;
	float		alpha;
	int			pic;
	qboolean	enabled;
}
showico_t;

showico_t showico;

void SHOWICO_load(void)
//called from engine
{
	showico_icons[0] = GL_LoadTexImage ("gfx/icons/o2.png", false, false);
}

void SHOWICO_hide(void)
{
	showico.fader = showico.alpha;
	showico.enabled = false;

}

void SHOWICO_display(void)
// [short] id
{
	int id;
	id = MSG_ReadShort();

	showico.alpha = 0.5f;
	showico.pic = id;
	showico.enabled = true;
}

void SHOWICO_drawall(void)
{
	if (showico.enabled)
	{
		glDisable(GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glColor4f (1,1,1,showico.alpha);
		glBindTexture(GL_TEXTURE_2D,showico_icons[showico.pic]);
		glBegin (GL_QUADS);
			glTexCoord2f (0, 0);	glVertex2f (0, 0);
			glTexCoord2f (1, 0);	glVertex2f (40, 0);
			glTexCoord2f (1, 1);	glVertex2f (40, 40);
			glTexCoord2f (0, 1);	glVertex2f (0, 40);
		glEnd ();
		glColor4f (1,1,1,1);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
}

/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;
	
// redraw with no console and the loading plaque
	Con_ClearNotify ();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	scr_fullupdate = 0;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
	Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_fullupdate = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents ();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_fullupdate = 0;
	SCR_UpdateScreen ();

	return key_lastpress == 'y';
}


//=============================================================================

/*
===============
SCR_BringDownConsole

Brings the console down and fades the palettes back to normal
================
*/
void SCR_BringDownConsole (void)
{
	int		i;
	
	scr_centertime_off = 0;
	
	for (i=0 ; i<20 && scr_conlines != scr_con_current ; i++)
		SCR_UpdateScreen ();

	cl.cshifts[0].percent = 0;		// no area contents palette on next frame
	VID_SetPalette (host_basepal);
}

void SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0, 
			vid.width - r_refdef.vrect.x + r_refdef.vrect.width, 
			vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, 
			r_refdef.vrect.x + r_refdef.vrect.width, 
			r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height, 
			r_refdef.vrect.width, 
			vid.height - sb_lines - 
			(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

/* 
=================== 
GL_BrightnessHack 

Enables setting of brightness without having to do any mondo fancy stuff. 
It's assumed that we're in the 2D view for this... 

Basically, what it does is multiply framebuffer colours by an incoming constant between 0 and 2 
=================== 
*/ 
void GL_BrightnessHack (float brightfactor) 
{ 
   // divide by 2 cos the blendfunc will sum src and dst 
   float brightblendcolour[4] = {1, 1, 1, brightfactor / 2.0f}; 
   float constantwhite[4] = {1, 1, 1, 1}; 

   glColor4fv (constantwhite); 
   glEnable (GL_BLEND); 
   glDisable (GL_ALPHA_TEST); 
   glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR); 

   // combine hack... 
   // this is weird cos it uses a texture but actually doesn't - the parameters of the 
   // combiner function only use the incoming fragment colour and a constant colour... 
   // you could actually bind any texture you care to mention and get the very same result... 
   // i've decided not to bind any at all... 
   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE); 
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE); 
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_CONSTANT); 
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA); 
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR); 
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR); 
   glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, brightblendcolour); 

   glBegin (GL_QUADS); 

   glTexCoord2f (0, 0); 
   glVertex2f (0, 0); 

   glTexCoord2f (0, 1); 
   glVertex2f (0, vid.height); 

   glTexCoord2f (1, 1); 
   glVertex2f (vid.width, vid.height); 

   glTexCoord2f (1, 0); 
   glVertex2f (vid.width, 0); 

   glEnd (); 

   // restore combiner function colour to white so as not to mess up texture state 
   glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constantwhite); 

   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); 
   glDisable (GL_BLEND); 
   glEnable (GL_ALPHA_TEST); 
   glColor4fv (constantwhite); 

   // my apologies. 
   // it was mondo fancy stuff in the end, wasn't it? 
} 

extern Draw_Crosshair (int texnum, vec3_t colour, float alpha);


#ifdef MENUENGINE
	M_Engine_Draw ();
#endif
/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
double	time1, time2;
int CL_TruePointContents (vec3_t p);
void SCR_UpdateScreen (void)
{
	GLenum error;
	static float	oldscr_viewsize;
	extern int	crosshair_tex[32];
	extern cvar_t hud_r, hud_g, hud_b, hud_a, r_waterwarp, v_psycho, vid_hwgamma;
	vec3_t	colour;

	colour[0] = hud_r.value;
	colour[1] = hud_g.value;
	colour[2] = hud_b.value;

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	
	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (cl.worldmodel && cls.state == ca_connected) // underwater view-warping
	{
		//if (r_waterwarp.value && Mod_PointInLeaf(r_refdef.vieworg, cl.worldmodel)->contents < -2)
		if (r_waterwarp.value && CL_TruePointContents (r_refdef.vieworg) < -2)
		{
			r_refdef.fovscale_x = 1 - (((sin(cl.time * 3.5f) + 1) * 0.025) * r_waterwarp.value); // was originally cl.time*4
			r_refdef.fovscale_y = 1 - (((cos(cl.time * 4.5f) + 1) * 0.015) * r_waterwarp.value); // was originally cl.time*5
		}
//		else
//		{
			if (scr_fov.value * r_refdef.fovscale_x < 1 || scr_fov.value * r_refdef.fovscale_y > 179)
			{
				Con_Printf ("Invalid FOV: %f - resetting.", scr_fov.value);
				Cvar_SetValue ("fov", 90);
			}
			r_refdef.fov_x = r_refdef.fovscale_x*scr_fov.value;
			r_refdef.fov_y = r_refdef.fovscale_y*CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
			scr_vrect = r_refdef.vrect;
//		}	
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();
	
	V_RenderView ();

	GL_Set2D ();

	//
	// draw any areas not covered by the refresh
	//
	SCR_TileClear ();
	Sbar_Draw ();

	if (scr_drawdialog)
	{
		//Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
		scr_copyeverything = true;
	}
	else if (scr_drawloading)
	{
		SCR_DrawLoading ();
		//Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game)
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		if (crosshair.value<32)
			Draw_Crosshair (crosshair_tex[(int)crosshair.value], colour, hud_a.value / 255);
		
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
//muff - to show FPS on screen QMB
		SCR_DrawFPS ();

		if (r_speeds.value)
		{
			char st[64];
			time2 = Sys_FloatTime ();
			sprintf (st, "%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys);
			Draw_String(0, 25, st);
			sprintf (st, "Player position: %+7.2f, %+7.2f, %+7.2f\n", r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
			Draw_String(0, 34, st);
			sprintf (st, "Player angle: %+3.2f, %+3.2f, (%+3.2f)\n", r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]);
			Draw_String(0, 43, st);
			sprintf (st, "cl.num_entities: %i\n", cl.num_entities);
			Draw_String(0, 52, st);
		}

		SCR_DrawStats ();
		Scr_ShowNumP ();

		SCR_DrawPause ();
		SHOWLMP_drawall();
		SHOWICO_drawall();
		SCR_CheckDrawCenterString ();
		//Sbar_Draw ();
		SCR_DrawConsole ();	
#ifdef MENUENGINE
		M_Engine_Draw ();
#else
		M_Draw (); // old menu
#endif
	}

	if (r_errors.value && developer.value)
	while ( (error = glGetError()) != GL_NO_ERROR )
		Con_DPrintf ("&c900Error:&c0092D&r %s\n", gluErrorString(error));

	V_UpdatePalette ();

	// CAPTURE <anthony@planetquake.com>
    CaptureHelper_OnUpdateScreen();

	if (!vid_hwgamma.value && v_gamma.value != 1) // exactly 1 doesn't make any difference, so save the trouble
		GL_BrightnessHack(v_gamma.value);

	GL_EndRendering ();
}
