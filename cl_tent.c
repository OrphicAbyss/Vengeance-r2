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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"
#include "gl_rpart.h"

int			num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

sfx_t			*cl_sfx_wizhit;
sfx_t			*cl_sfx_knighthit;
sfx_t			*cl_sfx_tink1;
sfx_t			*cl_sfx_ric1;
sfx_t			*cl_sfx_ric2;
sfx_t			*cl_sfx_ric3;
sfx_t			*cl_sfx_r_exp3;
#ifdef QUAKE2
sfx_t			*cl_sfx_imp;
sfx_t			*cl_sfx_rail;
#endif

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
#ifdef QUAKE2
	cl_sfx_imp = S_PrecacheSound ("shambler/sattck1.wav");
	cl_sfx_rail = S_PrecacheSound ("weapons/lstart.wav");
#endif
}

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam (model_t *m)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort ();
	
	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();
	
	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow!\n");	
}

void CL_ParseBeam2 ()
{
	extern  vec3_t zerodir;
	int		ent;
	vec3_t	start, end, point;
	beam_t	*b;
	dlight_t	*dl;
	int		i;//, count;
//	vec3_t	last, next;
	vec3_t	colour;
	
	ent = MSG_ReadShort ();
	
	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();
	
	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

	//qmb :lightning dlight
	dl = CL_AllocDlight (0);
	VectorCopy (end, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

	VectorSubtract(end, start, point);
	VectorScale(point, 0.5f, point);
	VectorAdd(start, point, point);

	dl = CL_AllocDlight (0);
	VectorCopy (point, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

	dl = CL_AllocDlight (0);
	VectorCopy (start, dl->origin);
	dl->radius = 300;
	dl->die = cl.time + 0.1;
	dl->decay = 300;
	// CDL - epca@powerup.com.au
	dl->colour[0] = 0.0f; dl->colour[1] = 0.2f; dl->colour[2] = 1.0f; //qmb :coloured lighting
	// CDL

/*

	//set colour for main beam
	colour[0] = 0.7f; colour[1] = 0.7f; colour[2] = 1.0f;

	VectorSubtract(start, end, point);
	//work out the length and therefore the amount of trails
	count = Length(point)/5;
//	count /= 10;

	VectorScale(point, 1.0/count, point);
	VectorCopy(start, last);

	for (i=0; i<count; i++){
		VectorMA (start, -i, point, next);

		AddTrailColor(last, next, p_lightning, 0.15f, 3, colour, zerodir);
		VectorCopy(next, last);
	}*/
	// replaced by particles
// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent && cl.time < b->endtime)
		{
			b->entity = ent;
			b->endtime = cl.time + 0.2f;

			if (!b->p1 || !b->p2 || !b->p3) // causes problems if the particles aren't there
				continue;

			VectorCopy(start,b->p1->org);
			VectorCopy(end,b->p1->org2);
			b->p1->start = cl.time;
			b->p1->die = b->endtime;

			VectorCopy(start,b->p2->org);
			VectorCopy(end,b->p2->org2);
			b->p2->start = cl.time;
			b->p2->die = b->endtime;

			VectorCopy(start,b->p3->org);
			VectorCopy(end,b->p3->org2);
			b->p3->start = cl.time;
			b->p3->die = b->endtime;

			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->endtime < cl.time)
		{
			colour[0] = 0.4f; colour[1] = 0.7f; colour[2] = 1.0f;

			b->entity = ent;
			b->endtime = cl.time + 0.2f;

			b->model = NULL;
			b->p1 = AddTrailColor(start, end, p_lightning, 0.2f, 3, colour, zerodir);
			b->p2 = AddTrailColor(start, end, p_lightning, 0.2f, 4, colour, zerodir);
			b->p3 = AddTrailColor(start, end, p_lightning, 0.2f, 5, colour, zerodir);

			VectorCopy (start, b->start);
			VectorCopy (end, b->end);

			return;
		}
	}
	Con_Printf ("beam list overflow!\n");
	//*/
}

extern void R_WaterSplash(vec3_t org);
extern void R_EventSplash(vec3_t org);
extern void R_ParticleExplosionQuad(vec3_t org), R_ParticleSpikeQuad(vec3_t org), R_ParticleSuperSpikeQuad(vec3_t org), R_ParticlePlasma(vec3_t org);
extern void R_ParticleExplosionRGB(vec3_t org, float color0, float color1, float color2);
extern void R_ParticleExplosionCustom(vec3_t org, float color0, float color1, float color2, int size);

extern ls_t	*partscript;
extern cvar_t r_part_scripts, r_part_lightning, r_decaltime;

extern int script_setcount;
/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	extern	void R_ParticleScript (char *section, vec3_t org);
	extern	void R_ParticleBloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count);
	extern	void R_ParticleRain (vec3_t org1, vec3_t org2, vec3_t dir, int count, int color, int type);

	extern  vec3_t zerodir;
	int		type, count, velspeed;
	vec3_t	pos, pos2, dir;
	vec3_t	endpos;
	dlight_t	*dl;
	int		rnd;
	int		colorStart, colorLength;
	unsigned char	*colourByte;
	vec3_t color;
	int		size;
	char	script[32];

	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_VEN_PARTSCRIPT:
		// scripted particles
		MSG_ReadVector(pos);
		count = MSG_ReadByte ();
		sprintf(script, "script_%i", count);
		if (hasSection(partscript, script) != -1)
		{
			R_ParticleScript(script, pos);
		}
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_GUNSHOT") != -1)
		{
			R_ParticleScript("TE_GUNSHOT", pos);
		}
		else
			R_ParticleGunshot(pos);
		break;
		
	case TE_SPIKE:			// spike hitting wall
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_SPIKE") != -1)
			R_ParticleScript("TE_SPIKE", pos);
		else
			R_ParticleSpike (pos);
		
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}

		break;
	case TE_SUPERSPIKE:			// super spike hitting wall
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_SUPERSPIKE") != -1)
		{
			R_ParticleScript("TE_SUPERSPIKE", pos);
		}
		else
			R_ParticleSuperSpike (pos);

		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_EXPLOSION:			// rocket explosion
		MSG_ReadVector(pos);	
		R_ParticleExplosion (pos);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		dl->colour[0] = 0.8f; dl->colour[1] = 0.4f; dl->colour[2] = 0.2f; //qmb :coloured lighting

     	S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_WIZSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		R_ParticleWizSpike (pos);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;
		
	case TE_KNIGHTSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		R_ParticleSuperSpike(pos);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_TELEPORT:
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_TELEPORT") != -1)
		{
			R_ParticleScript("TE_TELEPORT", pos);
		}
		else
			R_TeleportSplash (pos);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		// CDL - epca@powerup.com.au
		dl->colour[0] = 1.0f; dl->colour[1] = 1.0f; dl->colour[2] = 1.0f; //qmb :coloured lighting
		// CDL
		break;

	case TE_LIGHTNING1:				// lightning bolts
		if (r_part_lightning.value)
			CL_ParseBeam2 ();
		else
			CL_ParseBeam(Mod_ForName("progs/bolt.mdl", true));
		break;
	
	case TE_LIGHTNING2:				// lightning bolts
		if (r_part_lightning.value)
			CL_ParseBeam2 ();
		else
			CL_ParseBeam(Mod_ForName("progs/bolt2.mdl", true));
		break;
	
	case TE_LIGHTNING3:				// lightning bolts
		if (r_part_lightning.value)
			CL_ParseBeam2 ();
		else
			CL_ParseBeam(Mod_ForName("progs/bolt3.mdl", true));
		break;
	
// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		if (r_part_lightning.value)
			CL_ParseBeam2 ();
		else
			CL_ParseBeam(Mod_ForName("progs/beam.mdl", true));
		break;
// PGM 01/21/97

	case TE_LAVASPLASH:	
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_LAVASPLASH") != -1)
		{
			R_ParticleScript("TE_LAVASPLASH", pos);
		}
		else
			R_LavaSplash (pos);
		break;

	case TE_SPIKEQUAD:
		// quad spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to spark shower
		if (r_part_scripts.value && hasSection(partscript, "TE_SPIKEQUAD") != -1)
		{
			R_ParticleScript("TE_SPIKEQUAD", pos);
		}
		else
			R_ParticleSpikeQuad(pos);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.2;
		dl->decay = 1000;
		dl->colour[0] = 0.1f; dl->colour[1] = 0.1f; dl->colour[2] = 1.0f; //qmb :coloured lighting

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKEQUAD:
		// quad super spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to dust shower
		if (r_part_scripts.value && hasSection(partscript, "TE_SUPERSPIKEQUAD") != -1)
		{
			R_ParticleScript("TE_SUPERSPIKEQUAD", pos);
		}
		else
			R_ParticleSuperSpikeQuad (pos);

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.2;
		dl->decay = 1000;
		dl->colour[0] = 0.1f; dl->colour[1] = 0.1f; dl->colour[2] = 1.0f; //qmb :coloured lighting
		
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		// LordHavoc: added for improved blood splatters
	case TE_BLOOD:
		// blood puff
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte ();
		if (r_part_scripts.value && hasSection(partscript, "TE_BLOOD") != -1)
		{
			script_setcount = count;
			R_ParticleScript("TE_BLOOD", pos);
		}
		else
			AddParticle(pos, count, 12, r_decaltime.value, p_blood, dir);
		break;
	case TE_BLOOD2:
		// blood puff
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_BLOOD2") != -1)
		{
			R_ParticleScript("TE_BLOOD2", pos);
		}
		else
			AddParticle(pos, 10, 2, 1.5f, p_blood, zerodir);
		break;
	case TE_SPARK:
		// spark shower
		MSG_ReadVector(pos);
		dir[0] = MSG_ReadCoord ();
		dir[1] = MSG_ReadCoord ();
		dir[2] = MSG_ReadCoord ();
		count = MSG_ReadByte ();
		if (r_part_scripts.value && hasSection(partscript, "TE_SPARK") != -1)
		{
			script_setcount = count;
			R_ParticleScript("TE_SPARK", pos);
		}
		else
			AddParticle(pos, count, 1, 1.5f, p_sparks, dir);
		break;

	case TE_PLASMABURN:		// plasma effect
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_PLASMABURN") != -1)
		{
			R_ParticleScript("TE_PLASMABURN", pos);
		}
		else
			R_ParticlePlasma(pos);
		break;

		// LordHavoc: added for improved gore
	case TE_BLOODSHOWER:
		// vaporized body
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		velspeed = MSG_ReadCoord (); // speed
		count = MSG_ReadShort (); // number of particles
		R_ParticleBloodShower(pos, pos2, velspeed, count);
		break;

	case TE_GUNSHOTQUAD:			// quad bullet hitting wall
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_GUNSHOTQUAD") != -1)
		{
			R_ParticleScript("TE_GUNSHOTQUAD", pos);
		}
		else
			R_RunParticleEffect (pos, vec3_origin, 0, 270);
		break;		

	case TE_EXPLOSIONQUAD:			// quad rocket explosion
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_EXPLOSIONQUAD") != -1)
		{
			R_ParticleScript("TE_EXPLOSIONQUAD", pos);
		}
		else
			R_ParticleExplosionQuad (pos);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		dl->colour[0] = 0.8f; dl->colour[1] = 0.4f; dl->colour[2] = 0.2f; //qmb :coloured lighting

     	S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONRGB: // colored explosion
		MSG_ReadVector(pos);
		//CL_FindNonSolidLocation(pos, pos, 10);
		//CL_ParticleExplosion(pos);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		R_ParticleExplosionRGB(pos, color[0], color[1], color[2]);
		//Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		//CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		CL_AllocDlightDP(pos, 10 + (rand() & 2), color[0], color[1], color[2], 700, 15);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_VEN_EXPLOSIONCUSTOM: // custom colored/resized explosion
		MSG_ReadVector(pos);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		size = MSG_ReadByte (); // size
		R_ParticleExplosionCustom(pos, color[0], color[1], color[2], size);
		CL_AllocDlightDP(pos, 10 + (rand() & 2), color[0], color[1], color[2], 700, 15);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_TAREXPLOSION") != -1)
		{
			R_ParticleScript("TE_TAREXPLOSION", pos);
		}
		else
			R_BlobExplosion (pos);

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_FLAMEJET: // Entar : in terrible need of some bugfixing
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte();
		AddParticle(pos, count, 12, 1.07f, p_fire, dir);
		break;

	case TE_VEN_WATERSPLASH:		// water splash effect
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_VEN_WATERSPLASH") != -1)
		{
			R_ParticleScript("TE_VEN_WATERSPLASH", pos);
		}
		else
			R_WaterSplash (pos);
		break;

	case TE_VEN_EVENTSPLASH:		// 'event' splash effect
		MSG_ReadVector(pos);
		if (r_part_scripts.value && hasSection(partscript, "TE_VEN_EVENTSPLASH") != -1)
		{
			R_ParticleScript("TE_VEN_EVENTSPLASH", pos);
		}
		else
			R_EventSplash (pos);
		break;

	case TE_PARTICLERAIN:
		// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = (unsigned short) MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		//CL_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		R_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		break;		

	case TE_PARTICLESNOW:
		// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = (unsigned short) MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		//CL_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		R_ParticleRain(pos, pos2, dir, count, colorStart, 1);
		break;

	case TE_EXPLOSION2:				// color mapped explosion
		MSG_ReadVector(pos);
		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();
		R_ParticleExplosion2 (pos, colorStart, colorLength);
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		//qmb :coloured lighting
		colourByte = (byte *)&d_8to24table[colorStart];
		dl->colour[0] = (float)colourByte[0]/255.0;
		dl->colour[1] = (float)colourByte[1]/255.0;
		dl->colour[2] = (float)colourByte[2]/255.0; 
		break;
		
#ifdef QUAKE2
	case TE_IMPLOSION:
		MSG_ReadVector(pos);
		S_StartSound (-1, 0, cl_sfx_imp, pos, 1, 1);
		break;
#endif

	case TE_RAILTRAIL:
		MSG_ReadVector(pos);
		MSG_ReadVector(endpos);
//		S_StartSound (-1, 0, cl_sfx_rail, pos, 1, 1);
//		S_StartSound (-1, 1, cl_sfx_r_exp3, endpos, 1, 1);
		R_RocketTrail (pos, endpos, 0+128);
		R_ParticleExplosion (endpos);
		dl = CL_AllocDlight (-1);
		VectorCopy (endpos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		break;

	default:
		Con_Printf ("CL_ParseTEnt: temp ent type not supported: %i", type);
		//Sys_Error ("CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return NULL;
	if (num_temp_entities == MAX_TEMP_ENTITIES)
		return NULL;
	ent = &cl_temp_entities[num_temp_entities];
	memset (ent, 0, sizeof(*ent));
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts] = ent;
	cl_numvisedicts++;

	ent->colormap = vid.colormap;
	ent->alpha = 1; // LH
	return ent;
}

/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	*ent;
	float		yaw, pitch;
	float		forward;

	num_temp_entities = 0;

// update lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (b->endtime < cl.time)
			continue;

	// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
			if (b->p1)
				VectorCopy (cl_entities[cl.viewentity].origin, b->p1->org);
			if (b->p2)
				VectorCopy (cl_entities[cl.viewentity].origin, b->p2->org);
			if (b->p3)
				VectorCopy (cl_entities[cl.viewentity].origin, b->p3->org);
		}

	// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		
	// add new entities for the lightning
		if (b->model)
		{
			VectorCopy (b->start, org);
			d = VectorNormalize(dist);
			while (d > 0)
			{
				ent = CL_NewTempEntity ();
				if (!ent)
					return;
				VectorCopy (org, ent->origin);
				ent->model = b->model;
				ent->angles[0] = pitch;
				ent->angles[1] = yaw;
				ent->angles[2] = rand()%360;

				for (i=0 ; i<3 ; i++)
					org[i] += dist[i]*30;
				d -= 30;
			}
		}
	}
	
}

void R_ClearBeams (void)
{
	int			i;
	beam_t		*b;

// clear lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		b->endtime = cl.time - 1;
		b->entity = 0;
		b->p1 = NULL;
		b->p2 = NULL;
		b->p3 = NULL;
	}
}
