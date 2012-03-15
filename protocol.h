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
// protocol.h -- communications protocols

#define	PROTOCOL_VERSION	15

#define U_MOREBITS		(1<<0)
#define U_ORIGIN1		(1<<1)
#define U_ORIGIN2		(1<<2)
#define U_ORIGIN3		(1<<3)
#define U_ANGLE2		(1<<4)
#define U_NOLERP		(1<<5)
#define U_FRAME			(1<<6)
// just differentiates from other updates
#define U_SIGNAL		(1<<7)

#define U_ANGLE1		(1<<8)
#define U_ANGLE3		(1<<9)
#define U_MODEL			(1<<10)
#define U_COLORMAP		(1<<11)
#define U_SKIN			(1<<12)
#define U_EFFECTS		(1<<13)
#define U_LONGENTITY	(1<<14)

// LordHavoc: protocol extension
#define U_EXTEND1		(1<<15)
// LordHavoc: first extend byte
#define U_DELTA			(1<<16) // no data, while this is set the entity is delta compressed (uses previous frame as a baseline, meaning only things that have changed from the previous frame are sent, except for the forced full update every half second)
#define U_ALPHA			(1<<17) // 1 byte, 0.0-1.0 maps to 0-255, not sent if exactly 1, and the entity is not sent if <=0 unless it has effects (model effects are checked as well)
#define U_SCALE			(1<<18) // 1 byte, scale / 16 positive, not sent if 1.0
#define U_EFFECTS2		(1<<19) // 1 byte, this is .effects & 0xFF00 (second byte)
#define U_GLOWSIZE		(1<<20) // 1 byte, encoding is float/4.0, unsigned, not sent if 0
#define U_GLOWCOLOR		(1<<21) // 1 byte, palette index, default is 254 (white), this IS used for darklight (allowing colored darklight), however the particles from a darklight are always black, not sent if default value (even if glowsize or glowtrail is set)
#define U_COLORMOD		(1<<22) // 1 byte, 3 bit red, 3 bit green, 2 bit blue, this lets you tint an object artifically, so you could make a red rocket, or a blue fiend...
#define U_EXTEND2		(1<<23) // another byte to follow
// LordHavoc: second extend byte
#define U_GLOWTRAIL		(1<<24) // leaves a trail of particles (of color .glowcolor, or black if it is a negative glowsize)
#define U_VIEWMODEL		(1<<25) // attachs the model to the view (origin and angles become relative to it), only shown to owner, a more powerful alternative to .weaponmodel and such
#define U_FRAME2		(1<<26) // 1 byte, this is .frame & 0xFF00 (second byte)
#define U_MODEL2		(1<<27) // 1 byte, this is .modelindex & 0xFF00 (second byte)
#define U_EXTERIORMODEL	(1<<28) // causes this model to not be drawn when using a first person view (third person will draw it, first person will not)
#define U_UNUSED29		(1<<29) // future expansion
#define U_UNUSED30		(1<<30) // future expansion
#define U_EXTEND3		(1<<31) // another byte to follow, future expansion

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

// a sound with no channel is a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_LOOPING		(1<<2)		// a long


// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	22

// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_updatestat		3	// [byte] [long]
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
								// the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value
	
#define	svc_serverinfo		11	// [long] version
						// [string] signon string
						// [string]..[0]model cache
						// [string]...[0]sounds cache
#define	svc_lightstyle		12	// [byte] [string]
#define	svc_updatename		13	// [byte] [string]
#define	svc_updatefrags		14	// [byte] [short]
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_updatecolors	17	// [byte] [byte]
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19
	
#define	svc_spawnstatic		20
//	svc_spawnbinary		21
#define	svc_spawnbaseline	22
	
#define	svc_temp_entity		23

#define	svc_setpause		24	// [byte] on / off
#define	svc_signonnum		25	// [byte]  used for the signon sequence

#define	svc_centerprint		26	// [string] to put in center of the screen

#define	svc_killedmonster	27
#define	svc_foundsecret		28

#define	svc_spawnstaticsound	29	// [coord3] [byte] samp [byte] vol [byte] aten

#define	svc_intermission	30		// [string] music
#define	svc_finale			31		// [string] music [string] text

#define	svc_cdtrack			32		// [byte] track [byte] looptrack
#define svc_sellscreen		33

#define svc_cutscene		34

#define	svc_showlmp			35		// [string] slotname [string] lmpfilename [short] x [short] y
#define	svc_hidelmp			36		// [string] slotname

//qmb extended
//QMB temp range 90-100
#define svc_setentityflags	90
//QMB end range

//qmb extended
//Darkie temp range 101-110
#define svc_showico		101
#define svc_hideico		102
//Darkie end range

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_disconnect	2
#define	clc_move		3			// [usercmd_t]
#define	clc_stringcmd	4		// [string] message


//
// temp entity events
//
#define	TE_SPIKE			0
#define	TE_SUPERSPIKE		1
#define	TE_GUNSHOT			2
#define	TE_EXPLOSION		3
#define	TE_TAREXPLOSION		4
#define	TE_LIGHTNING1		5
#define	TE_LIGHTNING2		6
#define	TE_WIZSPIKE			7
#define	TE_KNIGHTSPIKE		8
#define	TE_LIGHTNING3		9
#define	TE_LAVASPLASH		10
#define	TE_TELEPORT			11
#define TE_EXPLOSION2		12

// PGM 01/21/97 
#define TE_BEAM				13
// PGM 01/21/97 

#ifdef QUAKE2
#define TE_IMPLOSION		14
#endif
#define TE_RAILTRAIL		15

// Nehahra effects used in the movie (TE_EXPLOSION3 also got written up in a QSG tutorial, hence it's not marked NEH)
#define	TE_EXPLOSION3		16 // [vector] origin [coord] red [coord] green [coord] blue
//#define TE_LIGHTNING4NEH	17 // [string] model [entity] entity [vector] start [vector] end

// LordHavoc: added some TE_ codes (block1 - 50-60)
#define	TE_BLOOD			50 // [vector] origin [byte] xvel [byte] yvel [byte] zvel [byte] count
#define	TE_SPARK			51 // [vector] origin [byte] xvel [byte] yvel [byte] zvel [byte] count
#define	TE_BLOODSHOWER		52 // [vector] min [vector] max [coord] explosionspeed [short] count
#define	TE_EXPLOSIONRGB		53 // [vector] origin [byte] red [byte] green [byte] blue
#define TE_PARTICLECUBE		54 // [vector] min [vector] max [vector] dir [short] count [byte] color [byte] gravity [coord] randomvel
#define TE_PARTICLERAIN		55 // [vector] min [vector] max [vector] dir [short] count [byte] color
#define TE_PARTICLESNOW		56 // [vector] min [vector] max [vector] dir [short] count [byte] color
#define TE_GUNSHOTQUAD		57 // [vector] origin
#define TE_SPIKEQUAD		58 // [vector] origin
#define TE_SUPERSPIKEQUAD	59 // [vector] origin
// LordHavoc: block2 - 70-80
#define TE_EXPLOSIONQUAD	70 // [vector] origin
#define	TE_BLOOD2			71 // [vector] origin
#define TE_SMALLFLASH		72 // [vector] origin
#define TE_CUSTOMFLASH		73 // [vector] origin [byte] radius / 8 - 1 [byte] lifetime / 256 - 1 [byte] red [byte] green [byte] blue
#define TE_FLAMEJET			74 // [vector] origin [vector] velocity [byte] count
#define TE_PLASMABURN		75 // [vector] origin

// Entar : new effects
#define	TE_VEN_WATERSPLASH		125 // [vector] origin
#define TE_VEN_EVENTSPLASH		126 // [vector] origin
#define	TE_VEN_EXPLOSIONCUSTOM		127 // [vector] origin [byte] red [byte] green [byte] blue [byte] size
#define TE_VEN_PARTSCRIPT		128

