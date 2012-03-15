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
/*******************************************************
QMB: 2.10

  FEATURES:
  
  Basic physics,  including: world collisions, velocity and constant acceleration.
  Easy extendable particle types.
  Scriptable particle.script file
  Drawing optimised for speed (not correctness).

  THINGS TO DO:
  Particle emitters.
  More advanced physics such as area effects, point gravity etc...
  Add Fuh's enhancments/particle types.
  maybe add some more documentation (never too many comments)

  REQUIREMENTS:
  **32bit texture loading (from www.quakesrc.org)**
  
  *include this file (gl_rpart.c and gl_rpart.h) in your source code
  *remove particle_t etc from glquake.h
  *remove the old r_part.c
  *compile :D
  *customise to taste
  
********************************************************
*/

#include "quakedef.h"
#include "gl_rpart.h"

#ifdef JAVA
#include "java_vm.h"
#endif

//sin and cos tables for the particle triangle fans
static double sint[7] = {0.000000, 0.781832,  0.974928,  0.433884, -0.433884, -0.974928, -0.781832};
static double cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521,  0.623490};

extern vec3_t lightcolor; // for particle lighting
 
//linked lists pointers for the first of the active, free and the a spare list for
//particles
particle_t			*particles;
//particle emitters
particle_tree_t		*particle_type_active,		*particle_type_free,	*particle_types;
//particle emitters
particle_emitter_t	*particle_emitter_active,	*particle_emitter_free,	*particle_emitter;

//vertex array for particles
float				*part_VertexArray;
float				*part_TexCoordArray;
float				*part_ColourArray;
int					part_NumQuads = 0;
#define				MAX_VERTEXQUADS 1024

//Holder of the particle texture
//FIXME: wont work for custom particles
//		needs a structure system with {id, custom id, tex num}
int			part_tex, blood_tex, smoke_tex, trail_tex, bubble_tex, lightning_tex, spark_tex, fire_tex;

int			r_numparticles;			//number of particles in total
int			r_numparticletype;		//number of particle types
int			r_numparticleemitter;	//number of particle emitters

int			numParticles;			//current number of alive particles (used in fps display)

double		timepassed, timetemp;	//for use with emitters when programmed right

vec3_t		darkfire = {0.83f, 0.33f, 0.04f};

#define BEAMLENGTH 16

									//these will allow for calculating how many particle to add

vec3_t		zerodir = {1,1,1};		//particles with no direction bias. (really a const)
vec3_t		zerodir2 = {-1,-1,-1};	//some direction bias - Entar
vec3_t		zero = {0,0,0};			// zero.  Simple.
vec3_t		coord[4];				//used in drawing, saves working it out for each particle

float		grav;

//was to stop particles not draw particles which were too close to the screen
//currently not used
//may be used again after depth sorting is implemented
cvar_t	gl_clipparticles = {"gl_clipparticles","0",true};
cvar_t	gl_smoketrail = {"gl_smoketrail","0",true};

cvar_t	r_decal_bullets = {"r_decal_bullets","1",true};
cvar_t	r_decal_misc = {"r_decal_misc","1",true};
cvar_t	r_decal_explosions = {"r_decal_explosions","1",true};
cvar_t	r_decal_blood = {"r_decal_blood","1",true};
cvar_t	r_decaltime = {"r_decaltime", "12", true};

cvar_t	r_part_flame = {"r_part_flame", "1", true};
cvar_t	r_part_lighting = {"r_part_lighting", "0", true};
cvar_t	r_part_lighting_update = {"r_part_lighting_update", "0.08", true};
cvar_t	r_part_lightning = {"r_part_lightning", "1", true};

// particle scripts on regular (TE_*) particle effects
cvar_t	r_part_scripts = {"r_part_scripts","1",true};

//used for nv_pointsprits
//pointsprits dont work right anyway (prob some crazy quake matrix stuff)
pointPramFUNCv qglPointParameterfvEXT;
pointPramFUNC qglPointParameterfEXT;

//internal functions
void TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int accurate); //Physics, checks to see if a particle hit the world
float TraceLineN2 (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, const int accurate);

extern int Mod_Q1BSP_RecursiveHullCheck (RecursiveHullCheckTraceInfo_t *t, int num, double p1f, double p2f, double p1[3], double p2[3]);

extern int flareglow_tex;

void R_UpdateAll(void);						//used to run the particle update (do accelration and kill of old particles)
void MakeParticleTexure(void);				//loads the builtin textures
void DrawParticles(void);					//draws the particles
void LoadParticleScript(char *script);

extern ls_t	*partscript;

int CL_TruePointContents (vec3_t p)
{
	return SV_HullPointContents (&cl.worldmodel->hulls[0], 0, p);
}

/*****Flat array particle system*****/
int		FirstFreeParticle;
int		LastUsedParticle;

//returns the position on the array for the particle
particle_t *AddParticleToArray(void)
{
	int i;
	
	if ((LastUsedParticle < r_numparticles) && (FirstFreeParticle < r_numparticles)){
		//search for a free spot
		for (i = FirstFreeParticle; i < LastUsedParticle; i++){
			if (particles[i].alive == 0){
				particles[i].alive = 1;				//the particle is now in use
				particles[i].update_verts = 1;		//if it becomes a decal its verts will need updating

				FirstFreeParticle = i + 1;	//start next search after this particle

				numParticles++;

				return &particles[i];		//return the free spot
			}
		}

		if (i < r_numparticles) {
			//Adding to end of list because no free spots before end of used particles
			FirstFreeParticle = i + 1;			//first free particle is after this one now
			LastUsedParticle = i;					//We have added a new particle to the end of the list

			particles[i].alive = 1;

			numParticles++;

			return &particles[i];
		}else{
			//there are no free particles
			FirstFreeParticle = i + 1;
			LastUsedParticle = i;

			return NULL;
		}
	}else{
		//there are no free particles
		return NULL;
	}
}

//clean out dead particles
void RemoveDeadParticlesFromArray(void)
{
	int i;

	for (i = 0; i < LastUsedParticle; i++){
		if (particles[i].alive == 1 && particles[i].die <= cl.time){
			particles[i].alive = 0;
			particles[i].updateafter = 0;	//reset lighting
			VectorCopy (zero, particles[i].lighting);

			numParticles--;
			if (i<FirstFreeParticle){
				FirstFreeParticle = i;
			}
		}
	}
}
/*****End Flat Particle Array*****/


/** R_ClearParticles
 * Reset all the pointers, reset the linklists
 * Clear all the particle data
 * Remake all the particle types
 */
//FIXME: needs to call qc to get custom particle types remade
void R_ClearParticles (void)
{
	int		i;
	vec3_t	zero_offset = {0,0,0};
	
	numParticles = 0;						//no particles alive
	timepassed = cl.time;					//reset emitter times
	timetemp = cl.time;

	FirstFreeParticle = 0;
	LastUsedParticle = 0;

	//particles
	for (i=0 ;i<r_numparticles ; i++)		//reset all particles
	{
		particles[i].alive = 0;
	}

	//particle types
	particle_type_free = &particle_types[0];
	particle_type_active = NULL;
	for (i=0 ;i<r_numparticletype ; i++)	//reset all particle types
	{
		particle_types[i].next = &particle_types[i+1];
	}
	particle_types[r_numparticletype-1].next = NULL; //no next particle type for last type...

	//particle emitters
	particle_emitter_free = &particle_emitter[0];
	particle_emitter_active = NULL;
	for (i=0 ;i<r_numparticleemitter ; i++){
		particle_emitter[i].next = &particle_emitter[i+1];
	}
	particle_emitter[r_numparticleemitter-1].next = NULL;

	/*
	particles are drawn in order here...
	some orders may look strange when particles overlap

	to add a new type:
	
	  AddParticleType(int src, int dst, part_move_t move, part_grav_t grav, part_type_t id, int custom_id, int texture, float startalpha)

  //blend mode
  //some examples are (gl_one,gl_one), (gl_src_alpha,gl_one_minus_src_alpha)
					src = GL_SRC_ALPHA;
					dst = GL_ONE;
	
  //colision&physics:	pm_static		:particle ignores velocity
  						pm_nophysics	:particle with velocity but ignores the map
						pm_normal		:particle with velocity, stops moving when it hits a wall
						pm_float		:particle with velocity, only alive in the water
						pm_bounce		:particle with velocity, bounces off the world 
						pm_bounce_fast	:particle with velocity, bounces off the world (no energy loss)
						pm_shrink		:particle with velocity, shrinks over time
						pm_die			:particle with velocity, dies after touching the map
						pm_grow			:particle with velocity, grows over time
					move = pm_die;

  //gravity effects:	pg_none				:no gravity acceleration
						pg_grav_low			:low gravity acceleration
						pg_grav_belownormal	:below normal gravity  acceleration
						pg_grav_normal		:normal gravity acceleration
						pg_grav_abovenormal	:above normal gravity acceleration
						pg_grav_high		:high gravity acceleration
						pg_grav_extreme		:slightly higher than pg_grav_high
						pg_rise_low			:low negitive gravity
						pg_rise				:normal negitive gravity
						pg_rise_high		:high negitive gravity
					grav = pg_none;

  //type of particle
  //list of set particles:
			built in to engine:
						p_sparks, p_smoke, p_fire, p_blood, p_chunks, p_lightning, p_bubble, p_trail
			for use with QC customisation
						p_custom
					id = p_fire;
			Entar : new particle types
						p_radius (for dev_findradius)
						p_fire2 (p_fire, only circle-textured)

  //will be used for QC custom controled particle types
			this field is ignored unless the 'id' field has p_custom
			used to uniquely define diffrent custom particles
					custom_id = 0;

	
  //what texture to use
					texture = part_tex;

  //the starting alpha value of the particles
					startalpha = 1;

	*/

	//make new particle types : sparks, blood, fire, chunks, bubble smoke, trail
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_die, pg_none, p_fire2, -1, part_tex, 1, zero_offset);// non-textured fire
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_die, pg_none, p_fire, -1, fire_tex, 0.95f, zero_offset); // textured fire
	//AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_bounce, pg_grav_high, p_sparks, 0, spark_tex, 1);//sparks
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_bounce, pg_grav_extreme, p_sparks, -1, spark_tex, 1, zero_offset);//sparks
	//AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_normal, pg_grav_normal, p_blood, 0, blood_tex, 1);//blood
	AddParticleType(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, pm_normal, pg_grav_high, p_blood, -1, blood_tex, 1, zero_offset);//blood
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_nophysics, pg_rise_low, p_smoke, -1, smoke_tex, 0.75f, zero_offset);//smoke
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_bounce_fast, pg_grav_normal, p_chunks, -1, part_tex, 1, zero_offset);//chunks
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_float, pg_rise, p_bubble, -1, bubble_tex, 1, zero_offset);//bubble
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_static, pg_none, p_trail, -1, trail_tex, 1, zero_offset);//trail
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_static, pg_none, p_lightning, -1, lightning_tex, 1, zero_offset);//lightning

	// findradius particles
	AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_static, pg_none, p_radius, -1, part_tex, 0.6f, zero_offset);

	//AddParticleType(GL_SRC_ALPHA, GL_ONE, pm_decal, pg_grav_high, p_decal, 0, part_tex, 1);
	AddParticleType(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, pm_decal, pg_grav_high, p_decal, -1, part_tex, 1, zero_offset);
	//FIXME: add QC function call to reset the QC Custom particles
}

void AddParticleType(int src, int dst, part_move_t move, part_grav_t grav, part_type_t id, int custom_id, int texture, float startalpha, vec3_t offset)
{
	particle_tree_t *pt;

	pt = particle_type_free;
	particle_type_free = pt->next;
	pt->next = particle_type_active;
	particle_type_active = pt;

	particle_type_active->SrcBlend = src;
	particle_type_active->DstBlend = dst;
	particle_type_active->move = move;
	particle_type_active->grav = grav;
	particle_type_active->id = id;
	particle_type_active->custom_id = custom_id;
	particle_type_active->texture = texture;
	particle_type_active->startalpha = startalpha;
	VectorCopy (offset, particle_type_active->offset);
}

char lastscript[64] = "particles.script";
/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	extern cvar_t sv_gravity;
	int		i;
	float texcoord[] = {0,1, 0,0, 1,0, 1,1};

	//check the command line to see if a number of particles was given particle
	i = COM_CheckParm ("-particles");
	if (i){
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;	//cant have less than set min
	}
	else{
		r_numparticles = MAX_PARTICLES / 2;					//default to set half the 'max'
	}
	r_numparticletype = MAX_PARTICLE_TYPES;
	r_numparticleemitter = MAX_PARTICLE_EMITTER;

	//allocate memory for the particles and particle type linked lists
	particles = (particle_t *)Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");
	particle_types = (particle_tree_t *)Hunk_AllocName (r_numparticletype * sizeof(particle_tree_t), "particlestype");
	particle_emitter = (particle_emitter_t *)Hunk_AllocName (r_numparticleemitter * sizeof(particle_emitter_t), "particleemitters");

	//allocate memory for vertex arrays
	part_VertexArray = (float *)Hunk_AllocName (MAX_VERTEXQUADS * 3 * 12 * sizeof(float), "particles vertex array");
	part_TexCoordArray = (float *)Hunk_AllocName (MAX_VERTEXQUADS * 2 * 12 * sizeof(float), "particles tex coord array");
	part_ColourArray = (float *)Hunk_AllocName (MAX_VERTEXQUADS * 4 * 12 * sizeof(float), "particles colour coord array");
	//setup texture coords (they dont change)
	//for (i=0; i < MAX_VERTEXQUADS; i++){
	//	memcpy(&part_TexCoordArray[i*8], &texcoord[0], 8 * sizeof(float));
	//}

	//make the particle textures
	MakeParticleTexure();

	//reset the particles
//	R_ClearParticles();
	partscript = NULL;
	LoadParticleScript(lastscript);

	grav = 9.8*(sv_gravity.value/800);
}

void R_InitParticles_Register(void)
{
	//Regester particle cvars
	Cvar_RegisterVariable (&gl_clipparticles);
	Cvar_RegisterVariable (&gl_smoketrail);
	Cvar_RegisterVariable (&r_decal_bullets);
	Cvar_RegisterVariable (&r_decal_misc);
	Cvar_RegisterVariable (&r_decal_explosions);
	Cvar_RegisterVariable (&r_decal_blood);
	Cvar_RegisterVariable (&r_decaltime);

	Cvar_RegisterVariable (&r_part_flame);
	Cvar_RegisterVariable (&r_part_lighting);
	Cvar_RegisterVariable (&r_part_lighting_update);
	Cvar_RegisterVariable (&r_part_lightning);

	Cvar_RegisterVariable (&r_part_scripts);
}

/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;

void R_EntityParticles (entity_t *ent)
{
//	int *colour = &d_8to24table[0x6f];
//	R_ParticleExplosion2(ent->origin, *colour, 1);

	int			count;
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	byte		*colourByte;

	dist = 64;
	count = 50;

	if (!avelocities[0][0])
	{
	for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
	avelocities[0][i] = (rand()&255) * 0.01;
	}


	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		p = AddParticleToArray();

		if (!p)
			break;	//no free particles

		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		p->hit = 0;
		p->die = cl.time + 0.01;
	
		colourByte = (byte *)&d_8to24table[0x6f];
		p->colour[0] = colourByte[0]/255.0;
		p->colour[1] = colourByte[1]/255.0;
		p->colour[2] = colourByte[2]/255.0;

		//p->type = pt_explode;
		p->type = pm_static;
		for (p->particle_type = particle_type_active; (p->particle_type) && (p->particle_type->id != p_fire2); p->particle_type = p->particle_type->next);
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;			
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;			
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;			
	}
	if (extra_info)
		Con_Printf ("EntityParticles");
}

// Entar : Rewritten with help from LordHavoc
float randsize(float size, float variance)
{
	//return lhrandom((size - (size*variance)), (size + (size*variance)));
	return size*lhrandom(1.0f-variance, 1.0f+variance);
}

int compareValue (ls_t *script, char *section, char *key, char *value)
{
	return (!strcmp (getValue(script, section, key, 0), value));
}

void LoadParticleScript(char *script)
{
	// lamescript
	vfsfile_t   *fin; 
	char   buf[256]; 
	int i;

	if (partscript)
		freeLameScript (partscript);

	//No need to allocate space for script, lamescript handles it
	//partscript = malloc(sizeof(ls_t));
	//memset(partscript, 0x0, sizeof(ls_t));

	R_ClearParticles();

	sprintf (buf, "%s", script); // which script file to load
//	fin = fopen (buf, "rb"); 
	fin = FS_OpenVFS(buf, "rb", FS_GAMEONLY);
	if (!fin)
	{
		sprintf (buf, "%s", script);
//		fin = fopen (buf, "rb");
		fin = FS_OpenVFS(buf, "rb", FS_GAME);
	}

	if (fin) 
	{ 
		Con_Printf ("Loading \"%s\"... ", buf); 

		// Note that you must pass a FILE * to loadLameScript(), 
		// NOT THE FILE NAME!:D 
		// 
		partscript = loadLameScript (fin); // fin gets closed in loadLameScript, so we don't need to close it
		if (partscript != NULL) 
		{ 
			Con_Printf ("done.\n");
			if (extra_info)
				Con_Printf ("    %d section(s) read.\n", partscript->numSections); 
		} 
		else 
		{ 
			Con_Printf ("failed.\n");
			Con_Printf ("Lamescript error code: %i\n",lsErrorCode);
		} 
//		fclose (fin);

		for (i=0; partscript && i < partscript->numSections; i++)
		{
			int srcblend = GL_SRC_ALPHA, dstblend = GL_ONE, texture = part_tex;
			float startalpha = 1;
			part_move_t physicstype = pm_nophysics;
			part_type_t type = p_fire;
			part_grav_t grav = pg_none;
			vec3_t	offset = {0,0,0};

			particle_tree_t	*pt; // fallbacks (put regular type into the script for default fallbacks)

			if (hasKey(partscript, partscript->sections[i]->name, "type") != -1)
			{
				if (compareValue(partscript, partscript->sections[i]->name, "type", "p_fire"))
					type = p_fire;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_fire2"))
					type = p_fire2;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_sparks"))
					type = p_sparks;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_smoke"))
					type = p_smoke;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_blood"))
					type = p_blood;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_bubble"))
					type = p_bubble;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_chunks"))
					type = p_chunks;
				else if (compareValue(partscript, partscript->sections[i]->name, "type", "p_decal"))
					type = p_decal;
			}

			//get the particle type its based off (only use original ones, don't want to get confused with new types)
			for (pt = particle_type_active; (pt) && ((pt->id != type) || (pt->custom_id >= 0)); pt = pt->next);
			srcblend = pt->SrcBlend;
			dstblend = pt->DstBlend;
			texture = pt->texture;
			physicstype = pt->move;
			grav = pt->grav;
			startalpha = pt->startalpha;

			if (hasKey(partscript, partscript->sections[i]->name, "offset_x") != -1)
				offset[0] = atof(getValue(partscript, partscript->sections[i]->name, "offset_x", 0));
			if (hasKey(partscript, partscript->sections[i]->name, "offset_y") != -1)
				offset[1] = atof(getValue(partscript, partscript->sections[i]->name, "offset_y", 0));
			if (hasKey(partscript, partscript->sections[i]->name, "offset_z") != -1)
				offset[2] = atof(getValue(partscript, partscript->sections[i]->name, "offset_z", 0));

			if (hasKey(partscript, partscript->sections[i]->name, "srcblend") != -1)
			{
				if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_SRC_ALPHA"))
					srcblend = GL_SRC_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_ONE"))
					srcblend = GL_ONE;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_DST_COLOR"))
					srcblend = GL_DST_COLOR;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_ZERO"))
					srcblend = GL_ZERO;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_ONE_MINUS_DST_COLOR"))
					srcblend = GL_ONE_MINUS_DST_COLOR;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_ONE_MINUS_SRC_ALPHA"))
					srcblend = GL_ONE_MINUS_SRC_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_DST_ALPHA"))
					srcblend = GL_DST_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_ONE_MINUS_DST_ALPHA"))
					srcblend = GL_ONE_MINUS_DST_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "srcblend", "GL_SRC_ALPHA_SATURATE"))
					srcblend = GL_SRC_ALPHA_SATURATE;
			}

			if (hasKey(partscript, partscript->sections[i]->name, "dstblend") != -1)
			{
				if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_SRC_ALPHA"))
					dstblend = GL_SRC_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_ONE"))
					dstblend = GL_ONE;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_SRC_COLOR"))
					dstblend = GL_SRC_COLOR;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_ZERO"))
					dstblend = GL_ZERO;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_ONE_MINUS_DST_ALPHA"))
					dstblend = GL_ONE_MINUS_DST_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_ONE_MINUS_SRC_COLOR"))
					dstblend = GL_ONE_MINUS_SRC_COLOR;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_DST_ALPHA"))
					dstblend = GL_DST_ALPHA;
				else if (compareValue(partscript, partscript->sections[i]->name, "dstblend", "GL_ONE_MINUS_SRC_ALPHA"))
					dstblend = GL_ONE_MINUS_SRC_ALPHA;
			}

			if (hasKey(partscript, partscript->sections[i]->name, "texture") != -1)
			{
				if (compareValue(partscript, partscript->sections[i]->name, "texture", "part_tex"))
					texture = part_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "blood_tex"))
					texture = blood_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "smoke_tex"))
					texture = smoke_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "spark_tex"))
					texture = spark_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "fire_tex"))
					texture = fire_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "bubble_tex"))
					texture = bubble_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "flareglow_tex"))
					texture = flareglow_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "trail_tex"))
					texture = trail_tex;
				else if (compareValue(partscript, partscript->sections[i]->name, "texture", "lightning_tex"))
					texture = lightning_tex;
			}

			if (hasKey (partscript, partscript->sections[i]->name, "physics") != -1)
			{
				if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_static"))
					physicstype = pm_static;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_normal"))
					physicstype = pm_normal;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_float"))
					physicstype = pm_float;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_bounce"))
					physicstype = pm_bounce;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_bounce_fast"))
					physicstype = pm_bounce_fast;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_shrink"))
					physicstype = pm_shrink;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_die"))
					physicstype = pm_die;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_grow"))
					physicstype = pm_grow;
				else if (compareValue(partscript, partscript->sections[i]->name, "physics", "pm_nophysics"))
					physicstype = pm_nophysics;
			}

			if (hasKey(partscript, partscript->sections[i]->name, "startalpha") != -1)
				startalpha = atof(getValue(partscript, partscript->sections[i]->name, "startalpha", 0));

			AddParticleType (srcblend, dstblend, physicstype, grav, type, i, texture, startalpha, offset);
		}
	} 
	else 
	{ 
		Con_Printf ("\"%s\" not found.\n", buf); 
	} 
}

void LoadParticleScript_f (void)
{
	char buf[256];

	if (Cmd_Argc() == 1)
		strcpy (buf, lastscript);
	else
	{
		sprintf (buf, "%s", Cmd_Argv(1));
		strcpy(lastscript, buf);
	}

	LoadParticleScript(buf);
}

//==========================================================
//Particle emitter code
//==========================================================

/** R_AddParticleEmitter
 * Will add a new emitter
 * Emitters will be able to be linked to a entity
 */
void R_AddParticleEmitter (vec3_t org, int count, int type, int size, float time, vec3_t colour, vec3_t dir)
{
	particle_emitter_t	*pe;

	if (!particle_emitter_free)
		return;

	//add new emitter to the list
	pe = particle_emitter_free;
	particle_emitter_free = pe->next;
	pe->next = particle_emitter_active;
	particle_emitter_active = pe;

	VectorCopy(org, pe->org);
	pe->count = count;
	pe->type = type;
	pe->size = size;
	pe->time = time + cl.time;
	VectorCopy(colour, pe->colour);
	VectorCopy(dir, pe->dir);
}

/** R_UpdateEmitters
 * Lets the emitters emit the particles :)
 * will be called every frame
 */
void R_UpdateEmitters (void)
{
	particle_emitter_t	*pe;
	double				frametime, halfframetimesqed;

	if ((cl.time == cl.oldtime))
		return;

	frametime = (cl.time - cl.oldtime);
	halfframetimesqed = 0.5 * frametime * frametime;

	//loop through list and kill off dead emitters
	for (pe = particle_emitter_active; pe; pe=pe->next)
	{
		if (pe->next){
			if (pe->next->time < cl.time){
				particle_emitter_t	*kill;

				kill = pe->next;						//emitter to kill is the next one

				pe->next = kill->next;					//remove kill from list
				kill->next = particle_emitter_free;		//make killed point to start of free emitter list
				particle_emitter_free = kill;			//free emitters now starts at killed emitter
			}

		}
	}

	if (particle_emitter_active){					//if we have active emitters
		if (particle_emitter_active->time < cl.time){	//check if the first emitter is dead
			particle_emitter_t *kill;
			
			kill =  particle_emitter_active;		//we want to remove the first emitter

			particle_emitter_active = kill->next;	//start the active emitter list at second item
			kill->next = particle_emitter_free;		//make killed point to start of free emitter list
			particle_emitter_free = kill;			//free emitters now starts at killed emitter
		}
	}
	
	//loop through list and emit particles
	for (pe = particle_emitter_active;pe;pe=pe->next)
	{
		//fix this so that if the emitter is moving it will leave a trail of particles along its path insted of dumping them at the end of its move

		//find new position (newton physics)
		//position = time * velocity + 1/2 * acceleration * time^2
		VectorMA(pe->org, frametime, pe->vel, pe->org);
		VectorMA(pe->org, halfframetimesqed, pe->acc, pe->org);

		//calculate new velocity
		//velocity = time * acceleration
		VectorMA(pe->vel, frametime, pe->acc, pe->vel);

		AddParticleColor(pe->org,zero,max(1,(int)(pe->count*frametime)),pe->size,pe->time,pe->type,pe->colour,pe->dir);
	}
}

//==========================================================================
//Old particle calling code
//Now the functions call the new ones to make the particles
//This saves changing the whole engine, and makes it easy just to drop in
//the particle system.
//==========================================================================

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, colour;
	
	for (i=0 ; i<3 ; i++)			//read in org
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)			//read in direction
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();		//read in number
	colour = MSG_ReadByte ();		//read in 8bit colour

	if (msgcount == 255)	//255 is a special number
		count = 1024;		//its actually a particle explosion
	else
		count = msgcount;
	
	R_RunParticleEffect (org, dir, colour, count);
}

// Taken from DP, then edited.  Thanks LH!
void R_ParticleBloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	int i;
	vec3_t org, vel, diff, center, velscale;

	VectorSubtract(maxs, mins, diff);
	center[0] = (mins[0] + maxs[0]) * 0.5;
	center[1] = (mins[1] + maxs[1]) * 0.5;
	center[2] = (mins[2] + maxs[2]) * 0.5;
	velscale[0] = velspeed * 2.0 / diff[0];
	velscale[1] = velspeed * 2.0 / diff[1];
	velscale[2] = velspeed * 2.0 / diff[2];

	for (i=0; i < count; i++)
	{
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		AddParticle (org, 1, 10, 1.1f, p_blood, vel);
	}

}

/*===============
R_ParticleExplosionQuad
===============*/
void R_ParticleExplosionQuad (vec3_t org)
{
	vec3_t	color;

	switch (CL_TruePointContents(org)) {
	case CONTENTS_WATER:
	case CONTENTS_SLIME:
	case CONTENTS_LAVA:
		AddParticle(org, 14, 18, 0.9f, p_fire, zerodir);
		AddParticle(org, 8, 3.7f, 2.5, p_bubble, zerodir);
//		AddParticle(org, 58, 1, 0.75, p_sparks, zerodir);
//		AddParticle(org, 26, 1, 0.75, p_sparks, zerodir);
		break;
	default:
		AddParticle(org, 35, 63, 0.6f, p_fire, zero);
		AddParticle(org, 43, 1, 0.935f, p_sparks, zerodir);
		AddParticle(org, 24, 1, 0.955f, p_sparks, zerodir);
		AddParticle(org, 7, 74, 0.715f, p_smoke, zerodir);
	}

	if (r_decal_explosions.value)
	{
		color[0] = 0;
		color[1] = 0;
		color[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 20, 47, part_tex, color);
	}
}

/*===============
R_ParticleExplosionRGB
===============*/
void R_ParticleExplosionRGB (vec3_t org, float color0, float color1, float color2)
{
	vec3_t	colour;

	colour[0] = color0;
	colour[1] = color1;
	colour[2] = color2;

	switch (CL_TruePointContents(org)) {
	case CONTENTS_WATER:
	case CONTENTS_SLIME:
	case CONTENTS_LAVA:
		AddParticleColor(org, zero, 12, 14, 0.8f, p_fire, colour, zerodir);
		AddParticleColor(org, zero, 6, 3.4f, 2.5, p_bubble, colour, zerodir);
		AddParticleColor(org, zero, 64, 1, 0.75, p_sparks, colour, zerodir);
		AddParticleColor(org, zero, 32, 1, 0.75, p_sparks, colour, zerodir);
		break;
	default:
		AddParticleColor(org, zero, 31, 46, 0.6f, p_fire, colour, zero);
		AddParticleColor(org, zero, 3, 43, 0.58f, p_fire2, colour, zero);
		AddParticleColor(org, zero, 46, 1, 0.935f, p_sparks, colour, zerodir);
		AddParticleColor(org, zero, 24, 1, 0.955f, p_sparks, colour, zerodir);
		AddParticle(org, 7, 65, 0.715f, p_smoke, zerodir);
	}

	if (r_decal_explosions.value)
	{
		CL_SpawnDecalParticleForPoint(org, 20, 40, part_tex, colour);
	}
}

/*===============
R_ParticleExplosionCustom
===============*/
void R_ParticleExplosionCustom (vec3_t org, float color0, float color1, float color2, int size)
{
	vec3_t	colour;

	colour[0] = color0;
	colour[1] = color1;
	colour[2] = color2;

	switch (CL_TruePointContents(org)) {
	case CONTENTS_WATER:
	case CONTENTS_SLIME:
	case CONTENTS_LAVA:
		AddParticleColor(org, zero, 12, (size / 3), 0.8f, p_fire, colour, zerodir);
		AddParticleColor(org, zero, 6, 3.4f, 2.5, p_bubble, colour, zerodir);
		AddParticleColor(org, zero, 64, 1, 0.75, p_sparks, colour, zerodir);
		AddParticleColor(org, zero, 32, 1, 0.75, p_sparks, colour, zerodir);
		break;
	default:
		AddParticleColor(org, zero, 35, size, 0.6f, p_fire, colour, zero);
		AddParticleColor(org, zero, 3, size, 0.58f, p_fire2, colour, zero);
		AddParticleColor(org, zero, 46, 1, 0.935f, p_sparks, colour, zerodir);
		AddParticleColor(org, zero, 24, 1, 0.955f, p_sparks, colour, zerodir);
		AddParticle(org, 7, 65, 0.715f, p_smoke, zerodir);
	}

	if (r_decal_explosions.value)
	{
		CL_SpawnDecalParticleForPoint(org, 20, 45, part_tex, colour);
	}
}

/*===============
R_ParticleExplosion2
===============*/
//Needs to be made to call new functions so that old ones can be removed
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
/*	vec3_t	colour;
	byte	*colourByte;

	colourByte = (byte *)&d_8to24table[colorStart];
	colour[0] = (float)colourByte[0]/255.0;
	colour[1] = (float)colourByte[1]/255.0;
	colour[2] = (float)colourByte[2]/255.0;
	
	AddParticleColor(org, zero, 64, 200, 1.5f, p_sparks, colour, zerodir);
	AddParticle(org, 64, 1, 3, p_smoke, zerodir);*/

	// Entar : TEMP
	AddParticle(org, 8, 9, 3, p_smoke, zerodir);
	AddParticle(org, 12, 1, 1.5f, p_sparks, zerodir);
}

/*===============
R_BlobExplosion
===============*/
//also do colored fires
//JHL; tweaked to look better
void R_BlobExplosion (vec3_t org)
{
	vec3_t	colour;
	int		i;

	colour[0] = colour[1] = 0;
	colour[2]=1;

	AddParticleColor (org, zero, 20,  2, 2.0f, p_blood, colour, zerodir);

	colour[0] = colour[1] = 0.4f;
	AddParticleColor (org, zero, 444, 1, 1.5f, p_sparks, colour, zerodir);

	for (i=0; i<10; i++)
	{
		colour[0] = colour[1] = (rand()%90)/255.0;
		AddParticleColor(org, zero, 1, 25, 1, p_fire, colour, zerodir);
	}

	if (r_decal_explosions.value)
		CL_SpawnDecalParticleForPoint(org, 20, 40, part_tex, colour);
}

void addGrav(particle_tree_t *pt, particle_t *p)
{
//add gravity to acceleration
	switch (pt->grav) {
//fall
	case (pg_grav_low):
		//p->vel[2] -= grav*4; old value
		p->acc[2] += -grav * 0.125; //fuh value
		break;
	case (pg_grav_belownormal):
		p->acc[2] += -grav;
		break;
	case (pg_grav_normal):
		p->acc[2] += -grav * 8;
		break;
	case (pg_grav_abovenormal):
		p->acc[2] += -grav * 16;
		break;
	case (pg_grav_high):
		p->acc[2] += -grav * 32;
		break;
	case (pg_grav_extreme):
		p->acc[2] += -grav * 38;
		break;
//rise
	case (pg_rise_low):
		p->acc[2] += grav * 4;
		break;
	case (pg_rise):
		p->acc[2] += grav * 8;
		break;
	case (pg_rise_high):
		p->acc[2] += grav * 16;
		break;
//none
	case (pg_none):
		//do nothing
		break;
	}
}

vec3_t script_setcolor = {0, 0, 0};
int	   script_setcount = 0;

void R_ParticleScript(char *section, vec3_t org) // Entar : must have exact org, but there are modifiers!
{
	// script system
	particle_tree_t	*pt;
	part_type_t type = p_fire, parttrail = p_fire2;
	vec3_t color = {1, 1, 1}, decalcolor = {0, 0, 0};
	vec3_t vel = {1, 1, 1};
	float wait = 0, trailtime = 0;
	int count = 1, size = 10, rotation = 0, rotation_speed = 0, areaspread = 32, areaspreadvert = 32, offsetspread = 32;
	float time = 0.69f, grav = 8, grow = 0, speed = 1;
	float randomsize = 0, randomvel = 0, randomcolor = 0, randomtime = 0, randomrotspeed = 0;
	int randomvelchange = false, randomorgchange = false, checkforwater = true;
	int decalsize = 0, trailsize = 0, customid = -1;
	char additional[64], trailextra[64];
	char spawnmode[16];

	if (!partscript || (customid = hasSection(partscript, section)) == -1)
		return;

//	Con_Printf("%s\n", section);	

	// paramaters

	if (hasKey(partscript, section, "wait") != -1)
		wait = atof(getValue(partscript, section, "wait", 0));

	if (hasKey(partscript, section, "checkwater") != -1)
	{
		if (atoi(getValue(partscript, section, "checkwater", 0)) == 0)
			checkforwater = false;
		else
			checkforwater = true;
	}

	if (hasKey(partscript, section, "rotation") != -1)
		rotation = atoi(getValue(partscript, section, "rotation", 0));
	if (hasKey(partscript, section, "rotation_speed") != -1)
		rotation_speed = atoi(getValue(partscript, section, "rotation_speed", 0));
	if (hasKey(partscript, section, "count") != -1)
	{
		count = atoi(getValue(partscript, section, "count", 0));
		if (count < 1)
			return;
	}
	else if (script_setcount > 0)
	{
		count = script_setcount;
		script_setcount = 0; // reset it
	}
	if (hasKey(partscript, section, "size") != -1)
		size = atoi(getValue(partscript, section, "size", 0));
	if (hasKey(partscript, section, "time") != -1)
	{
		time = atof(getValue(partscript, section, "time", 0));
		if (time <= 0)
			time = r_decaltime.value;
	}

	if (hasKey(partscript, section, "gravity") != -1)
		grav = atof(getValue(partscript, section, "gravity", 0));
	if (hasKey(partscript, section, "growrate") != -1)
		grow = atof(getValue(partscript, section, "growrate", 0));

	if (!VectorCompare (script_setcolor, zero))
	{
		VectorCopy(script_setcolor, color);
		VectorCopy(zero, script_setcolor); // make sure it doesn't do it next time
	}

	// colors (accepts red, green, blue, color0, color1, color2)
	if (hasKey(partscript, section, "red") != -1)
		color[0] = atof(getValue(partscript, section, "red", 0));
	else if (hasKey(partscript, section, "color0") != -1)
		color[0] = atof(getValue(partscript, section, "color0", 0));

	if (hasKey(partscript, section, "green") != -1)
		color[1] = atof(getValue(partscript, section, "green", 0));
	else if (hasKey(partscript, section, "color1") != -1)
		color[1] = atof(getValue(partscript, section, "color1", 0));

	if (hasKey(partscript, section, "blue") != -1)
		color[2] = atof(getValue(partscript, section, "blue", 0));
	else if (hasKey(partscript, section, "color2") != -1)
		color[2] = atof(getValue(partscript, section, "color2", 0));

	// velocity
	if (hasKey(partscript, section, "velocity0") != -1)
		vel[0] = atof(getValue(partscript, section, "velocity0", 0));
	if (hasKey(partscript, section, "velocity1") != -1)
		vel[1] = atof(getValue(partscript, section, "velocity1", 0));
	if (hasKey(partscript, section, "velocity2") != -1)
		vel[2] = atof(getValue(partscript, section, "velocity2", 0));

	for (pt = particle_type_active; (pt) && (pt->custom_id != customid); pt = pt->next);
	type = pt->id;

	if (hasKey(partscript, section, "speed") != -1)
		speed = atof(getValue(partscript, section, "speed", 0));
	else
	{
		if (type == p_sparks)
			speed = 3.3f;
		else if (type == p_fire || type == p_fire2)
			speed = size/25;
	}

	if (hasKey(partscript, section, "spawnmode") != -1)
	{
		strcpy(spawnmode, getValue(partscript, section, "spawnmode", 0));
	
		if (hasKey(partscript, section, "areaspread") != -1)
			areaspread = atoi(getValue(partscript, section, "areaspread", 0));
		if (hasKey(partscript, section, "areaspreadvert") != -1)
			areaspreadvert = atoi(getValue(partscript, section, "areaspreadvert", 0));
//		if (hasKey(partscript, section, "offsetspread") != -1)
//			offsetspread = atoi(getValue(partscript, section, "offsetspread", 0));
	}
	else
	{
		areaspread = areaspreadvert = 0;
	}

	if (hasKey(partscript, section, "trail") == -1)
	{
		parttrail = p_none;
		goto notrail;
	}
	if (compareValue(partscript, section, "trail", "0") ||
		compareValue(partscript, section, "trail", "p_none"))
	{
		parttrail = p_none;
		goto notrail;
	}
	if (compareValue(partscript, section, "trail", "p_fire"))
		parttrail = p_fire;
	else if (compareValue(partscript, section, "trail", "p_fire2"))
		parttrail = p_fire2;
	else if (compareValue(partscript, section, "trail", "p_sparks"))
		parttrail = p_sparks;
	else if (compareValue(partscript, section, "trail", "p_smoke"))
		parttrail = p_smoke;
	else if (compareValue(partscript, section, "trail", "p_blood"))
		parttrail = p_blood;
	else if (compareValue(partscript, section, "trail", "p_bubble"))
		parttrail = p_bubble;
	else if (compareValue(partscript, section, "trail", "p_chunks"))
		parttrail = p_chunks;

	trailsize = atoi(getValue(partscript, section, "trailsize", 0));
	trailtime = atof(getValue(partscript, section, "trailtime", 0));
notrail:
	if (hasKey(partscript, section, "trailextra") != -1)
		Q_strcpy(trailextra, getValue(partscript, section, "trailextra", 0));
	else
		memset (trailextra, 0, sizeof(char)*64); // Entar : causes problems otherwise

	// modifiers

	if (hasKey(partscript, section, "randsize") != -1)
		randomsize = atof(getValue(partscript, section, "randsize", 0));
	if (randomsize)
	{
		size = (int)randsize((float)size, randomsize);
		if (size < 1)
			return; // sanity check
	}

	if (hasKey(partscript, section, "randtime") != -1)
		randomtime = atof(getValue(partscript, section, "randtime", 0));
	if (randomtime)
	{
		time = randsize(time, randomtime);
		if (time <= 0)
			return;
	}

	if (hasKey(partscript, section, "randrotatespeed") != -1)
		randomrotspeed = atof(getValue(partscript, section, "randrotatespeed", 0));
	if (randomrotspeed)
		rotation_speed = (int)randsize((float)rotation_speed, randomrotspeed);

	if (hasKey(partscript, section, "randvel") != -1)
		randomvel = atof(getValue(partscript, section, "randvel", 0));
	if (randomvel > 0)
	{
		vel[0] = randsize(vel[0], randomvel);
		vel[1] = randsize(vel[1], randomvel);
		vel[2] = randsize(vel[2], randomvel);
	}

	if (hasKey(partscript, section, "randchangevel") != -1)
		if (compareValue(partscript, section, "randchangevel", "1"))
			randomvelchange = true;

	if (hasKey(partscript, section, "randchangeorg") != -1)
		randomorgchange = atoi(getValue(partscript, section, "randchangeorg", 0));

	if (hasKey(partscript, section, "randcolor") != -1)
	{
		if (compareValue(partscript, section, "randcolor", "1"))
		{
			color[0] = Random();
			color[1] = Random();
			color[2] = Random();
		}
	}

	//test
	//AddParticleColor(org, zerodir, count, size, time, type, color, zerodir);

	//if (vel[0] == 1 && vel[1] == 1 && vel[2] == 1) // zerodir
		//AddParticle(org, count, size, time, type, zerodir);
		AddParticleCustom(org, count, size, time, customid, type, color, vel, checkforwater, wait, grav, parttrail, trailsize, trailtime, trailextra, randomvelchange, randomorgchange, grow, speed, rotation, rotation_speed, spawnmode, areaspread, areaspreadvert);
	//else
	//	AddParticleCustom(org, count, size, time, customid, type, color, vel, checkforwater, wait, grav, parttrail, trailsize, trailtime, trailextra, randomvelchange, randomorgchange, grow, speed, rotation, rotation_speed, spawnmode, areaspread, areaspreadvert);

	// decals ;)
	if (hasKey(partscript, section, "decalsize") == -1)
		goto nodecals;
	decalsize = atoi(getValue(partscript, section, "decalsize", 0));
	if (decalsize > 0)
	{
		decalcolor[0] = atof(getValue(partscript, section, "decalcolor0", 0));
		decalcolor[1] = atof(getValue(partscript, section, "decalcolor1", 0));
		decalcolor[2] = atof(getValue(partscript, section, "decalcolor2", 0));
		CL_SpawnDecalParticleForPoint(org, 20, decalsize, part_tex, decalcolor);
	}
nodecals:

	// additionals ;)
	if (hasKey(partscript, section, "extra") != -1)
	{
		Q_strcpy(additional, getValue(partscript, section, "extra", 0));
		R_ParticleScript(additional, org);
	}
}

/*===============
R_ParticleExplosion
===============*/
void R_ParticleExplosion (vec3_t org)
{
	vec3_t	color;

	if (r_part_scripts.value && (hasSection(partscript, "TE_EXPLOSION") != -1))
		R_ParticleScript("TE_EXPLOSION", org);
	else
	{
		switch (CL_TruePointContents(org)) {
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
			AddParticle(org, 10, 18, 0.8f, p_fire, zerodir);
			AddParticle(org, 6, 3.4f, 2.5, p_bubble, zerodir);
			break;
		default:
//			AddParticle(org, 18, 16, 1, p_fire, zerodir);
//			AddParticle(org, 64, 1, 0.925f, p_sparks, zerodir);
//			AddParticle(org, 32, 1, 0.925f, p_sparks, zerodir);
			AddParticle(org, 35, 54, 0.59f, p_fire, zero);
			AddParticle(org, 40, 1, 0.935f, p_sparks, zerodir);
			AddParticle(org, 20, 1, 0.955f, p_sparks, zerodir);
			AddParticle(org, 6, 65, 0.715f, p_smoke, zerodir);
			AddParticleColor(org, zero, 3, 48, 0.61f, p_fire, darkfire, zerodir);
		}

		//AddParticle(org, 64, 1, 1.5f, p_sparks, zerodir);
		//AddParticle(org,  32, 1, 1.5f, p_sparks, zerodir);
		//AddParticle(org,  20,  25, 2.0f, p_fire, zerodir);

		if (r_decal_explosions.value)
		{
			color[0] = 0;
			color[1] = 0;
			color[2] = 0;

			CL_SpawnDecalParticleForPoint(org, 20, 40, part_tex, color);
		}
	}
}

void R_ParticleGunshot (vec3_t org)
{
	vec3_t colour;
	//JHL:HACK; better looking gunshot (?)
	colour[0] = colour[1] = colour[2] = 0.6f;

	AddParticleColor(org, zero, 1, 2, 1, p_smoke, colour, zerodir);
	AddParticle(org, 5, 7, 0.9f, p_smoke, zerodir); // Entar : 3/29/05
	AddParticle(org, 1, 1, 1.0f, p_sparks, zerodir);
	
	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 7, part_tex, colour);
	}
}

void R_ParticleSuperSpike (vec3_t org)
{
	vec3_t tempdir, colour;
	VectorCopy(zerodir,tempdir);

//	AddParticleColor(org, zero, 5, 1, 4, p_chunks, colour, tempdir);
	AddParticle(org, 7, 1, 1.0f, p_sparks, zerodir);
	AddParticle(org, 5, 7, 0.9f, p_smoke, zerodir); // Entar : 4/4/05

	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 7, part_tex, colour);
	}
}

void R_ParticleWizSpike (vec3_t org)
{
	vec3_t colour;
	//AddParticleColor(org, zero, 10, 1, 4, p_chunks, colour, tempdir);
	AddParticle(org, 5, 1, 1.0f, p_sparks, zerodir);
	AddParticle(org, 15, 1, 0.5f, p_sparks, zerodir);

	if (r_decal_misc.value)
	{
		colour[0] = 0.1f;
		colour[1] = 0.9f;
		colour[2] = 0.1f;

		CL_SpawnDecalParticleForPoint(org, 15, 8, part_tex, colour);
	}
}

void R_ParticleSpike (vec3_t org)
{
	vec3_t colour;
	AddParticle(org, 3, 7, 0.34f, p_smoke, zerodir);
	AddParticle(org, 2, 1, 1.0f, p_sparks, zerodir); // original size == 55

	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 7, part_tex, colour);
	}
}

void R_ParticlePlasma (vec3_t org)
{
	vec3_t colour;
	colour[0] = 0.2f;
	colour[1] = 0.37f;
	colour[2] = 0.95f;
	AddParticleColor(org, zero, 5, 14, 0.28f, p_fire, colour, zerodir);
	AddParticleColor(org, zero, 2, 1, 0.98f, p_sparks, colour, zerodir);

	if (r_decal_misc.value)
	{
		colour[0] = 0.1f;
		colour[1] = 0.35f;
		colour[2] = 0.9f;

		CL_SpawnDecalParticleForPoint(org, 15, 9, part_tex, colour);
	}
}

void R_ParticleSpikeQuad (vec3_t org)
{
	vec3_t colour;
	AddParticle(org, 3, 7, 0.34f, p_smoke, zerodir);
	AddParticle(org, 2, 1, 1.0f, p_sparks, zerodir); // original size == 55
	AddParticle(org, 1, 7, 0.24f, p_fire, zerodir);

	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 8, part_tex, colour);
	}
}

void R_ParticleSuperSpikeQuad (vec3_t org)
{
	vec3_t colour;
	AddParticle(org, 4, 7, 0.34f, p_smoke, zerodir);
	AddParticle(org, 3, 1, 1.0f, p_sparks, zerodir); // original size == 55
	AddParticle(org, 1, 8, 0.24f, p_fire, zerodir);

	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 8, part_tex, colour);
	}
}

void R_ParticleGunshotQuad (vec3_t org)
{
	vec3_t colour;
	colour[0] = colour[1] = colour[2] = 0.6f;
	AddParticleColor(org, zero, 1, 5, 1, p_smoke, colour, zerodir);
	AddParticle(org, 4, 7, 0.9f, p_smoke, zerodir); // Entar : 3/29/05
	AddParticle(org, 1, 1, 1.0f, p_sparks, zerodir); 
	AddParticle(org, 1, 1, 0.91f, p_sparks, zerodir);
	AddParticle(org, 1, 8, 0.24f, p_fire, zerodir);
	
	if (r_decal_bullets.value)
	{
		colour[0] = 0;
		colour[1] = 0;
		colour[2] = 0;

		CL_SpawnDecalParticleForPoint(org, 15, 8, part_tex, colour);
	}
}

/*===============
R_RunParticleEffect
===============*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	byte		*colourByte;
	vec3_t		colour, tempdir;
	int			i;
	char		scriptnumber[32];

	if ((dir[0] == 0) && (dir[1] == 0) && (dir[2] == 0))
		VectorCopy(zerodir,tempdir);
	else
		VectorCopy(dir, tempdir);

	colourByte = (byte *)&d_8to24table[color];
	colour[0] = colourByte[0]/255.0;
	colour[1] = colourByte[1]/255.0;
	colour[2] = colourByte[2]/255.0;

	//QMB :REMOVE FOR OTHER ENGINES
	//START :REMOVE block comment "/* */" out this whole section
   //JHL:HACK; do qmb specific particles
	if (qmb_mod && color > 240 && color < 246)
	{
		//JHL:NOTE; ADD THE BUBBLE!!
		if (color == 241)		// water bubbles
			AddParticle (org, count, 2, 6, p_bubble, tempdir);
		else if (color == 242)	// sparks
			AddParticle (org, count, 1, 1.0f, p_sparks, tempdir);
		else if (color == 243)	// chunks
		{
			/*
			for (i=0; i<count; i++)
			{
				colour[0] = colour[1] = colour[2] = (rand()%128)/255.0+64;
				AddParticleColor (org, zero, 1, 1, 4, p_chunks, colour, tempdir);
			}
			*/
		}
		//JHL:NOTE; ADD ELECTRIC BUZZ (p_lightning?)!!
		else if (color == 244)	// electric sparks
		{
			colour[2] = 1.0f;
			for (i=0; i<count; i++)
			{
				colour[0] = colour[1] = 0.4 + ((rand()%90)/255.0);
				AddParticleColor (org, zero, 1, 1, 1.0f, p_sparks, colour, tempdir);
			}
		}
		//JHL:NOTE; ADD WATER DROPS!!
		else if (color == 245)	// rain
			AddParticle (org, count, 1, 1.0f, p_sparks, tempdir);
	}
	else
	//END :REMOVE
	{
		if (count == 1024)
			R_ParticleExplosion(org);
		else if (count > 1999)
		{
			sprintf (scriptnumber, "script_%i", count);
			R_ParticleScript (scriptnumber, org);
		}
		else
		{
			if (hasSection(partscript, "particle") != -1)
			{
				for (i=0; i<count*2; i++)
				{
					VectorCopy(colour, script_setcolor);
					R_ParticleScript("particle", org);
				}
			}
			else
			{
				//AddParticleColor(org, zero, count*2, 3, 3, p_blood, colour, tempdir);
				// new one is controlled by decal-time cvar
				// original count value = count*2
				AddParticleColor(org, zero, count*2, 3, 3.1 + r_decaltime.value, p_blood, colour, tempdir);
				AddParticleColor(org, zero, 2, 8, 0.58f, p_smoke, colour, zerodir);
			}
		}
	}
}

/*===============
R_LavaSplash
===============*/
//Need to find out when this is called
//JHL:NOTE; When Chthon sinks to lava...
//QMB: yep i worked that out, thanx (found out its also used in TF for the spy gren...)
void R_LavaSplash (vec3_t org)
{
/*	AddParticle(org, 83, 1, 10, p_sparks, zerodir);
	AddParticle(org, 72, 1, 10, p_sparks, zerodir);
	AddParticle(org, 100, 50, 6, p_fire, zerodir);*/

	vec3_t color = {1, 0.95f, 0.9f};
	vec3_t color2 = {1, 0.7f, 0.15f};

	AddParticleCustom(org, 43, 49, 8.3f, -1, p_smoke, color, zerodir, false, 0, -1, p_smoke, 0, 0, NULL, 0, 128, 0.1f, 2, 9, 41, "telebox", 192, 192);
	AddParticleCustom(org, 43, 49, 8.3f, -1, p_fire2, color2, zerodir, false, 0, -2, p_fire2, 0, 0, NULL, 0, 312, 0.1f, 1, 9, 41, "none", 0, 0);
}

// Entar : for new splash extension
void R_WaterSplash (vec3_t org)
{
	vec3_t	colour;

	colour[0]=0.84f;
	colour[1]=0.84f;
	colour[2]=0.92f;

	AddParticleCustom(org, 7, 42, 0.81f, -1, p_smoke, colour, zerodir, false, 0, 0.5f, p_none, 0, 0, "none", 0, 9, 0.1f, 4, 9, 3, "none", 0, 0);
}

// Entar : for new 'event' extension
void R_EventSplash (vec3_t org)
{
	vec3_t	color, neworg;
	int	i;

	color[0]=0.6f;
	color[1]=0.56f;
	color[2]=0.5f;

	for (i=0 ; i < 6 ; i++)
	{
		VectorCopy (org, neworg);
		
		neworg[0] = (randsize (neworg[0], 0.1f));
		neworg[1] = (randsize (neworg[1], 0.1f));
		neworg[2] = (randsize (neworg[2], 0.01f));
		AddParticleColor (neworg, zero, 2, 31, 0.91f, p_smoke, color, zerodir);
	}

	AddParticle(org, 40, 1, 0.935f, p_sparks, zerodir);
}

// Entar : for rain particle extension
void R_ParticleRain (vec3_t org1, vec3_t org2, vec3_t dir, int count, int color, int type)
{
	vec3_t	colour, org;
	float	dist, time = 3.16f;
	int		i;
	byte	*colourByte;

	colourByte = (byte *)&d_8to24table[color];
	colour[0] = colourByte[0]/255.0;
	colour[1] = colourByte[1]/255.0;
	colour[2] = colourByte[2]/255.0;

	dist = VectorDistance(org1, org2);

	for (i=0;i<count;i++)
	{
		VectorCopy(org1, org);

		if (org[0] < 0)
		{
			if (org2[0] < org[0])
				org[0] -= Random() * dist;
			else
				org[0] += Random() * dist;
		}
		else
		{
			if (org2[0] > org[0])
				org[0] += Random() * dist;
			else
				org[0] -= Random() * dist;
		}

		if (org[1] < 0)
		{
			if (org2[1] < org[1])
				org[1] -= Random() * dist;
			else
				org[1] += Random() * dist;
		}
		else
		{
			if (org2[1] > org[1])
				org[1] += Random() * dist;
			else
				org[1] -= Random() * dist;
		}

		if (type == 0) // rain
			AddParticleCustom(org, count, 1, time, -1, p_sparks, zerodir, dir, false, 0, 5, p_none, 0, 0, "none", 0, 0, -0.02f, 1, 0, 0, "none", 0, 0);
		else // snow
			AddParticleCustom(org, 1, 1, time, -1, p_fire, zerodir, dir, false, 0, 1, p_none, 0, 0, "none", 1, 1, -0.01f, 1, 34, 1, "none", 0, 0);
			//AddParticleCustom(org, 1, 6, time, p_fire, zerodir, dir, pm_float, false, 0, 1, p_none, 0, 0, "none", 1, -0.1f, 1, 34, 1);
	}
}

/*===============
R_TeleportSplash
===============*/
//Need to be changed so that they spin down into the ground
//whould look very cool
//maybe coloured blood (new type?)
void R_TeleportSplash (vec3_t org)
{
	vec3_t	colour;

	colour[0]=0.5f; // Entar : original values = 0.9f, 0.9f, 0.9f
	colour[1]=0.5f;
	colour[2]=0.8f;

	AddParticleColor(org, zero, 111, 1, 0.9f, p_sparks, colour, zerodir); // orginal values = (org, zer, 256, 200, 1.0f, p_sparks, colour, zerodir);
	AddParticleColor(org, zero, 8, 14, 0.9f, p_fire, colour, zerodir); // Entar : added bluefire
}

//Should be made to call the new functions to keep compatablity
void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		colour;
	vec3_t		blooddir;
	float			count;
	vec3_t			point, endpos;
	int i;

	blooddir[0] = 22 - (rand() % 42);
	blooddir[1] = 22 - (rand() % 42);
	blooddir[2] = 22 - (rand() % 42);

	VectorSubtract(start, end, point);
	count = Length(point);
	if (count == 0)
		return;
	else
		count = count/8;

	VectorScale(point, 1.0/count, point);

	switch (type)
	{
		case 0:	// rocket trail
			if (r_part_scripts.value && hasSection(partscript, "RocketTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					//work out the pos
					VectorMA (end, i, point, endpos);
					R_ParticleScript("RocketTrail", endpos);
				}
			}
			else
			{
				AddTrail(start, end, p_fire, 0.1f, 6, zerodir);
				AddTrail(start, end, p_smoke, 1, 6, zerodir);
				AddTrail(start, end, p_sparks, 0.2f, 1, zerodir);
			}
			break;

		case 1:	// smoke smoke
			if (r_part_scripts.value && hasSection(partscript, "GrenadeTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);
					R_ParticleScript("GrenadeTrail", endpos);
				}
			}
			else
				AddTrail(start, end, p_smoke, 1, 2, zerodir);
			break;

		case 2:	// blood
			if (r_part_scripts.value && hasSection(partscript, "BloodTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);
					R_ParticleScript("BloodTrail", endpos);
				}
			}
			else
			{
				//AddTrail(start, end, p_blood, 2, 3, zerodir);
				AddTrail(start, end, p_blood, r_decaltime.value, 6, blooddir);
			}
			break;

		case 3:	//wizard/Scrag trail
			if (r_part_scripts.value && hasSection(partscript, "ScragTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);
					R_ParticleScript("ScragTrail", endpos);
				}
			}
			else
			{
				colour[0]=0.1f;	colour[1]=0.75f;	colour[2]=0.1f;
				AddTrailColor (start, end, p_smoke, 1.32f, 3, colour, zerodir);
			}
			break;

		case 5:	// hknight trail
			if (r_part_scripts.value && hasSection(partscript, "HKnightTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);
					R_ParticleScript("HKnightTrail", endpos);
				}
			}
			else
			{
				colour[0]=1;	colour[1]=0.85f;	colour[2]=0;
				AddTrailColor (start, end, p_smoke, 1.32f, 3, colour, zerodir);
			}
			break;

		case 4:	// Blood trail #2
			if (r_part_scripts.value && hasSection(partscript, "BloodTrail2") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);
					R_ParticleScript("BloodTrail2", end);
				}
			}
			else
				AddParticle(end, 10, 1, r_decaltime.value, p_blood, zerodir);
			break;

		case 6:	// vore trail
			if (r_part_scripts.value && hasSection(partscript, "VoreTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);				
					R_ParticleScript("VoreTrail", end);
				}
			}
			else
			{
				colour[0]=1;	colour[1]=0;	colour[2]=0.75f;
				AddTrailColor (start, end, p_smoke, 2, 2, colour, zerodir);
			}
			break;

		case 128: // rail trail
			if (r_part_scripts.value && hasSection(partscript, "RailTrail") != -1)
			{
				for (i=0 ; i<count; i++)
				{
					VectorMA (end, i, point, endpos);				
					R_ParticleScript("RailTrail", end);
				}
			}
			else
			{
				colour[0]=1;	colour[1]=0;	colour[2]=0.75f;
				AddTrailColor (start, end, p_fire, 5, 2, colour, zerodir);
			}
			break;
	}
}

//============================================================
//Particle drawing code
//============================================================

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if (cl.time != cl.oldtime && !g_drawing_refl)
		R_UpdateAll();

	glDepthMask(GL_FALSE);							//not depth sorted so dont let particles block other particles...
	glEnable (GL_BLEND);					//all particles are blended
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//only done once per frame, not for every particle
	VectorAdd (vup, vright, coord[2]);
	VectorSubtract (vright, vup, coord[3]);
	VectorNegate (coord[2], coord[0]);
	VectorNegate (coord[3], coord[1]);

	DrawParticles();

	glDepthMask(GL_TRUE);
	glDisable (GL_BLEND);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

#ifdef JAVA
void Java_DrawParticle(JNIEnv *env, jclass this, float x, float y, float z, float size, float r, float g, float b, float a, float rotation){
	vec3_t	org;
	vec3_t	distance;

	org[0] = x;
	org[1] = y;
	org[2] = z;

	//test to see if particle is too close to the screen (high fill rate usage)
	if (gl_clipparticles.value) {
		VectorSubtract(org, r_origin, distance);
		if (distance[0] * distance[0] + distance[1] * distance[1] + distance[2] * distance[2] < 3200)
			return;
	}

	glColor4f (r,g,b,a);

	glPushMatrix ();
		glTranslatef(org[0], org[1], org[2]);				
		glScalef (size, size, size);

		glRotatef (rotation,  vpn[0], vpn[1], vpn[2]);

		glBegin (GL_QUADS);
			glTexCoord2f (0, 1);	glVertex3fv (coord[0]);
			glTexCoord2f (0, 0);	glVertex3fv (coord[1]);
			glTexCoord2f (1, 0);	glVertex3fv (coord[2]);
			glTexCoord2f (1, 1);	glVertex3fv (coord[3]);
		glEnd ();

	glPopMatrix();
}
#endif

__inline void Part_RenderAndClearArray()
{
	if (part_NumQuads > 0){
		//draw particles
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glTexCoordPointer(2, GL_FLOAT, 0, part_TexCoordArray);
		glVertexPointer(3, GL_FLOAT, 0, part_VertexArray);
		glColorPointer(4, GL_FLOAT, 0, part_ColourArray);

		glDrawArrays (GL_QUADS, 0, part_NumQuads * 4);

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		//reset
		part_NumQuads = 0;
	}
}

__inline void Part_AddQuadToArray(float *quad, float *texcoord, float *colour)
{
	if (part_NumQuads + 1 >= MAX_VERTEXQUADS)
	{
		//Con_DPrintf("Particle Array: Not enough space to render all particles in one go");
		//need to render and clear
		Part_RenderAndClearArray();
	}
	
	//copy all vertices across
	memcpy(&part_VertexArray[12 * part_NumQuads], quad, 12*sizeof(float));
	//copy texcoord
	memcpy(&part_TexCoordArray[8 * part_NumQuads], texcoord, 8*sizeof(float));
	//copy the same colour to all vertices
	memcpy(&part_ColourArray[16 * part_NumQuads +  0], colour, 4*sizeof(float));
	memcpy(&part_ColourArray[16 * part_NumQuads +  4], colour, 4*sizeof(float));
	memcpy(&part_ColourArray[16 * part_NumQuads +  8], colour, 4*sizeof(float));
	memcpy(&part_ColourArray[16 * part_NumQuads + 12], colour, 4*sizeof(float));

	part_NumQuads++;
}

/** GL_QuadPointsForBeam
 * Draws a beam sprite between 2 points
 * LH's code previously, changed to Entar's code
 */
void GL_QuadPointsForBeam(vec3_t start, vec3_t end, vec3_t offset, float t1, float t2, float *colour)
{
	GLfloat texcoord[] = {1+t1, 0, 1+t1, 1, 0+t2, 1, 0+t2, 0};
	GLfloat vert[12];
	
	VectorAdd(start, offset, &vert[0]);
	VectorSubtract(start, offset, &vert[3]);
	VectorSubtract(end, offset, &vert[6]);
	VectorAdd(end, offset, &vert[9]);

	Part_AddQuadToArray(&vert[0], &texcoord[0], colour);
}

void Part_OpenGLSetup(particle_tree_t *pt)
{
	//render any quads for the lasts OpenGL Setup
	Part_RenderAndClearArray();		//draw old particles

	if (!pt)
		return;

	//set up new state
	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
	glDisable(GL_FOG); // Entar : fix problem with fog
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(GL_FALSE);

	glBlendFunc(pt->SrcBlend, pt->DstBlend);
	glBindTexture(GL_TEXTURE_2D,pt->texture);

	if (pt->id == p_decal){
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1,1);
	} else {
		glPolygonOffset(0,0);
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

void Part_OpenGLReset(void)
{
	//render all remaining quads before resetting OpenGL Setup
	Part_RenderAndClearArray();		//draw old particles

	glPolygonOffset(0,0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	if (gl_cull.value)
		glEnable(GL_CULL_FACE);

	glDepthMask(GL_TRUE);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_BLEND);
	if (gl_fogglobal.value)
		glEnable(GL_FOG);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


//texture coords for the drawing of particles, not ever changed, moved outside function so only init once
float texcoord[] = {0,1, 0,0, 1,0, 1,1};

/** DrawParticles
 * Main drawing code...
 */
void DrawParticles_Specific(particle_t *p, particle_tree_t *pt)
{
	vec3_t			decal_vec[4], decal_up, decal_right;

	float vert[12];
	float colour[4];

    int         lnum;
    vec3_t      dist;
    float       add;

	vec3_t			distance;
	int				lasttype=0;
	//point sprite thingo
	static GLfloat constant[3] = { 1.0f, 0.0f, 0.0f };
	static GLfloat linear[3] = { 1.0f, -0.001f, 0.0f };
	static GLfloat quadratic[3] = { 0.25f, 0.0f, -1.0f };

	if (p->start > cl.time)
		return;

	if (pt->texture!=0&&pt->id != p_sparks &&pt->move!=pm_decal)
	{
		if ((pt->id != p_trail)&&(pt->id != p_lightning)){
			//all textured particles except trails and lightning
			
				//test to see if particle is too close to the screen (high fill rate usage)
				if (gl_clipparticles.value) {
					VectorSubtract(p->org, r_origin, distance);
					if (distance[0] * distance[0] + distance[1] * distance[1] + distance[2] * distance[2] < 3200)
						return;
//							continue;
				}

//********************** particle lighting

				// don't do lighting on sparks and fire; don't bother
				// doing it on decals either - they're all either black (bullet holes, etc),
				// or meant to glow in the dark (wizard spike spots, lightning)
				// don't do it when r_fullbright is set to 1 either :) - Entar
				// bubbles look weird when colored - leave 'em be
				// blood is already lit by the blendmode
				if (r_part_lighting.value && !r_fullbright.value && (pt->id != p_sparks)&&(pt->id != p_fire)&&(pt->id != p_fire2)&&(pt->id != p_decal)&&(pt->id != p_radius)&&(pt->id != p_bubble)&&(pt->id != p_blood))
				{
					if (p->updateafter <= cl.time){
						// Entar : light stuff here
						//get the light for the little bugger
						R_LightPoint(p->org); // LordHavoc: lightcolor is all that matters from this

						//work out lighting from the dynamic lights
						for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
						{
						//if the light is alive
							if (cl_dlights[lnum].die >= cl.time)
							{
							//work out the distance to the light
								//VectorSubtract (e->origin, cl_dlights[lnum].origin, dist);
								VectorSubtract (p->org, cl_dlights[lnum].origin, dist);
								add = cl_dlights[lnum].radius - Length(dist);
							//if its close enough add light from it
								if (add > 0)
								{
									lightcolor[0] += add * cl_dlights[lnum].colour[0];
									lightcolor[1] += add * cl_dlights[lnum].colour[1];
									lightcolor[2] += add * cl_dlights[lnum].colour[2];
								}
							}
						}

						//scale lighting to floating point
						VectorScale(lightcolor, 1.0f / 200.0f, lightcolor); 
						VectorCopy(lightcolor,p->lighting);

						p->updateafter = cl.time + r_part_lighting_update.value;
					} else {
						VectorCopy(p->lighting,lightcolor);
					}

					colour[0] = (lightcolor[0] * 0.75) * p->colour[0];
					colour[1] = (lightcolor[1] * 0.75) * p->colour[1];
					colour[2] = (lightcolor[2] * 0.75) * p->colour[2];
					colour[3] = p->ramp;
				}
				else
				{
					// special blendmode
					if (pt->SrcBlend == GL_ZERO && pt->DstBlend == GL_ONE_MINUS_SRC_COLOR)
					{
						colour[0] = (1 - p->colour[0]) * p->ramp;
						colour[1] = (1 - p->colour[1]) * p->ramp;
						colour[2] = (1 - p->colour[2]) * p->ramp;
					}
					else
					{
						colour[0] = p->colour[0];
						colour[1] = p->colour[1];
						colour[2] = p->colour[2];
					}
					colour[3] = p->ramp;
				}

//********************** end lighting

				if (p->hit == 0){ 
					RotatePointAroundVector(&vert[0],vpn,coord[0],p->rotation);
					VectorNegate(&vert[0],&vert[6]);
					RotatePointAroundVector(&vert[3],vpn,coord[1],p->rotation);
					VectorNegate(&vert[3],&vert[9]);

					//Use Vector add insted of glTranslatef
					VectorMA(p->org, p->size, &vert[0], &vert[0]);
					VectorMA(p->org, p->size, &vert[3], &vert[3]);
					VectorMA(p->org, p->size, &vert[6], &vert[6]);
					VectorMA(p->org, p->size, &vert[9], &vert[9]);

					//Add Quad to array to be drawn later
					Part_AddQuadToArray(&vert[0], &texcoord[0], &colour[0]);
				}else{ // hit == 1
					//check what side the of the decal we are drawing
					if (DotProduct(p->vel, r_origin) > DotProduct(p->vel, p->org))
					{
						VectorNegate(p->vel, p->vel);			//save on negating vector next time
						p->update_verts = 1;					//verts will need updating because of changed side
					}
					
					if (p->update_verts == 1){
						VectorVectors(p->vel, decal_right, decal_up);

						VectorScale(decal_right, p->size, decal_right);
						VectorScale(decal_up, p->size, decal_up);

						VectorAdd (decal_up, decal_right, decal_vec[2]);
						VectorSubtract (decal_right, decal_up, decal_vec[3]);
						VectorNegate (decal_vec[2], decal_vec[0]);
						VectorNegate (decal_vec[3], decal_vec[1]);

						VectorAdd(p->org, decal_vec[0], &p->verts[0]);
						VectorAdd(p->org, decal_vec[1], &p->verts[3]);
						VectorAdd(p->org, decal_vec[2], &p->verts[6]);
						VectorAdd(p->org, decal_vec[3], &p->verts[9]);
						
						p->update_verts = 0;
					}
					
//					colour[0] = p->colour[0];
//					colour[1] = p->colour[1];
//					colour[2] = p->colour[2];
//					colour[3] = p->ramp;

					Part_AddQuadToArray(&p->verts[0], &texcoord[0], &colour[0]);
				}
		}else{
			//trails and lightning
			int		lengthscale;
			float	t1, t2, scrollspeed, radius, length;
			vec3_t	temp, offset;

			glDisable(GL_CULL_FACE);

			colour[0] = p->colour[0];
			colour[1] = p->colour[1];
			colour[2] = p->colour[2];
			colour[3] = p->ramp;

			VectorSubtract(p->org2, p->org, temp);
			length = VectorLength(temp);

			// configurable numbers
			radius = p->size; // thickness of beam
			scrollspeed = -6.0; // scroll speed, 1 means it scrolls the entire height of the texture each second
			lengthscale = 40; // how much distance in quake units it takes for the texture to repeat once

			t1 = cl.time * scrollspeed + p->size;
			t1 -= (int)t1; // remove the unnecessary integer portion of the number
			t2 = t1 + (length / lengthscale);

			VectorMA(vright, radius, vright, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);

			VectorAdd(vright, vup, offset);
			VectorNormalize(offset);
			VectorScale(offset, radius, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);

			VectorSubtract(vright, vup, offset);
			VectorNormalize(offset);
			VectorScale(offset, radius, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);

			// configurable numbers
			radius = p->size*1.5; // thickness of beam
			scrollspeed = -3.0; // scroll speed, 1 means it scrolls the entire height of the texture each second
			lengthscale = 80; // how much distance in quake units it takes for the texture to repeat once

			t1 = cl.time * scrollspeed + p->size;
			t1 -= (int)t1; // remove the unnecessary integer portion of the number
			t2 = t1 + (length / lengthscale);

			VectorMA(vright, radius, vright, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);

			VectorAdd(vright, vup, offset);
			VectorNormalize(offset);
			VectorScale(offset, radius, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);

			VectorSubtract(vright, vup, offset);
			VectorNormalize(offset);
			VectorScale(offset, radius, offset);
			GL_QuadPointsForBeam(p->org, p->org2, offset, t1, t2, &colour[0]);
		}
	}else{
		if (pt->id!=p_decal){
			//Sparks...
			glDisable(GL_CULL_FACE);

			{
				vec3_t dup, offset;
				
				VectorScale(p->vel, 0.07f*p->size, dup);
				VectorSubtract(p->org, dup, dup);
				
				colour[0] = p->ramp * p->colour[0];
				colour[1] = p->ramp * p->colour[1];
				colour[2] = p->ramp * p->colour[2];
				colour[3] = p->ramp;

				VectorMA(vright, p->size, vright, offset);
				GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f, &colour[0]);

				VectorAdd(vright, vup, offset);
				VectorNormalize(offset);
				VectorScale(offset, p->size, offset);
				GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f, &colour[0]);

				VectorSubtract(vright, vup, offset);
				VectorNormalize(offset);
				VectorScale(offset, p->size, offset);
				GL_QuadPointsForBeam(p->org, dup, offset, 1.0f, 1.0f, &colour[0]);
			}
		} else {
			//check what side the of the decal we are drawing
			if (DotProduct(p->vel, r_origin) > DotProduct(p->vel, p->org))
			{
				VectorNegate(p->vel, p->vel);			//save on negating vector next time
				p->update_verts = 1;					//verts will need updating because of changed side
			}
			
			if (p->update_verts == 1){
				VectorVectors(p->vel, decal_right, decal_up);

				VectorScale(decal_right, p->size, decal_right);
				VectorScale(decal_up, p->size, decal_up);

				VectorAdd (decal_up, decal_right, decal_vec[2]);
				VectorSubtract (decal_right, decal_up, decal_vec[3]);
				VectorNegate (decal_vec[2], decal_vec[0]);
				VectorNegate (decal_vec[3], decal_vec[1]);

				VectorAdd(p->org, decal_vec[0], &p->verts[0]);
				VectorAdd(p->org, decal_vec[1], &p->verts[3]);
				VectorAdd(p->org, decal_vec[2], &p->verts[6]);
				VectorAdd(p->org, decal_vec[3], &p->verts[9]);
				
				p->update_verts = 0;
			}
			
			colour[0] = p->colour[0];
			colour[1] = p->colour[1];
			colour[2] = p->colour[2];
			colour[3] = p->ramp;

			Part_AddQuadToArray(&p->verts[0], &texcoord[0], &colour[0]);
		}
	}
}

void DrawParticles(void)
{
	particle_tree_t	*pt, *last_pt = NULL;
	particle_t		*p;
	int				i;

	//loop through all particles and find active ones
	for (i=0; i<LastUsedParticle; i++)
	{
		//if the particle is active add it to renderque or render it
		if (particles[i].alive)
		{
			p = &particles[i];
			pt = p->particle_type;

			if (r_depthsort.value)
				RQ_AddDistReorder(NULL, p, pt, p->org);
			else {
				if ((!last_pt) || (pt->id != last_pt->id) || (pt->custom_id != last_pt->custom_id)){
					Part_OpenGLSetup(pt);	//setup opengl state
				}

				Part_OpenGLSetup(pt);	//setup opengl state
				DrawParticles_Specific(p, pt);
				last_pt = pt;
			}
		}
	}
	Part_OpenGLReset();	//reset opengl state
}

//================================================================
//Particle physics code
//================================================================
/** R_UpdateAll
 * Do the physics, kill off old particles
 */
void R_UpdateAll(void)
{
	extern cvar_t sv_gravity;
	particle_tree_t	*pt;
	particle_t		*p;
	float			halfframetimesqed, frametime, dist;
//	double			tempVelocity;
	int				contents;

	int				i;

	vec3_t			oldOrg, stop, normal;//, tempVec;
	vec3_t			normal1;

	// script stuff
	part_type_t		trailtype;
	part_move_t		physicstype = pm_none;
	char			trailextra[64];
	trailtype = p_none;
	trailextra[0] = '0';
//	Q_strcpy(trailextra, "none");
//	trailextra = NULL;

	//Work out gravity and time
	grav = 9.8*(sv_gravity.value/800);	//just incase sv_gravity has changed - (original value was 9.8*)
	frametime = (cl.time - cl.oldtime);
	halfframetimesqed = 0.5 * frametime * frametime;

	//removed the dead particles
	RemoveDeadParticlesFromArray();

	//update emitters
	R_UpdateEmitters();

	for (i=0; i<LastUsedParticle; i++){
		if (particles[i].alive){
			//map current particle to work on
			p = &particles[i];
			if (!p || !p->particle_type)
				continue;
			pt = p->particle_type;

			p->ramp = (1-(cl.time-p->start)/(p->die-p->start)) * pt->startalpha;

			if (p->script_growrate != 0)
				p->size += p->script_growrate * (frametime * 21);
				// Entar : a bit of a hack, but it works (fixes slowmo and makes it more constant)

			if (p->hit)
				continue;	//if hit is set dont do any physics code

			VectorCopy(p->org, oldOrg);

			if (pt->move != pm_static && p->hit == 0){
				//find new position (newton physics)
				//position = time * velocity + 1/2 * acceleration * time^2
				VectorMA(p->org, frametime, p->vel, p->org);
				VectorMA(p->org, halfframetimesqed, p->acc, p->org);

				//calculate new velocity
				//velocity = time * acceleration
				VectorMA(p->vel, frametime, p->acc, p->vel);

				//create the trail particle(s)
				trailtype = p->script_trail;
				Q_strcpy(trailextra, p->script_trailextra);
				if (trailextra && trailextra[0] != '0')
				{
					R_ParticleScript(p->script_trailextra, p->org);
					trailextra[0] = '0';
				}
				else if (trailtype != p_none)
				{
					AddParticle(p->org, 1, p->script_trailsize, p->die - p->start, trailtype, zerodir);
					trailtype = p_none;
				}
			}

			if (p->type && p->type != pm_none)
				physicstype = p->type;
			else
				physicstype = pt->move;

//			switch (pt->move)
			switch (physicstype)
			{
			case (pm_static):
			case (pm_nophysics):
				//do nothing :)
				break;
			case (pm_decal):
				//if the particle hits a wall stop physics and setup as decal
				//could use this to kill of particle and call decal code
				//to seperate decal and particle systems
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					TraceLineN(oldOrg, p->org, stop, normal,1);
					if ((stop != p->org)&&(Length(stop)!=0))
					{
						p->hit = 1;
						//work out exactly where we hit and what the normal was
						
						//copy our hit position to our position
						VectorCopy(stop, p->org);
						//VectorMA(stop,-1,normal,p->org);

						//set our velocity to be the normal
						//this is used by the rendering code to work out what direction to face
						VectorCopy(normal,p->vel);

						//that is all
					}
				}
				break;
			case (pm_normal): // only really ever used for blood
				//if the particle hits a wall stop physics
//				if (CONTENTS_SOLID == CL_TruePointContents (p->org)){
					if (TraceLineN2(oldOrg, p->org, stop, normal1, 0) < 1)
					{
						if (r_decal_blood.value)
						{
							// Entar : FIXME
							if ((stop != p->org)&&(Length(stop)!=0))
							{
								p->hit = 1;
								VectorCopy(stop, p->org);
								VectorCopy(normal1, p->vel);
							}
						}
						else
							p->die=0;
					}
//				}
				break;
			case (pm_float):
				//if the particle leaves water/slime/lava kill it off
				contents = CL_TruePointContents (p->org);
				if (contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA)
					p->die = 0;
				break;
			case (pm_bounce):
				//make the particle bounce off walls
				if (CONTENTS_SOLID == CL_TruePointContents (p->org)){
					if (TraceLineN2(oldOrg, p->org, stop, normal, 0) < 1)
					{
						if ((stop != p->org)&&(Length(stop)!=0))
						{
							dist = DotProduct(p->vel, normal) * -1.3;
						
							VectorMA(p->vel, dist, normal, p->vel);
							VectorCopy(stop, p->org);
						}
					}
				}
				break;
			case (pm_bounce_fast):
				//make it bounce higher
				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
				{
					TraceLineN(oldOrg, p->org, stop, normal,0);
					if ((stop != p->org)&&(Length(stop)!=0))
					{
						dist = DotProduct(p->vel, normal) * -1.4;
					
						VectorMA(p->vel, dist, normal, p->vel);
						VectorCopy(stop, p->org);
					}
				}

				break;
			case (pm_die):
				//acceleration should take care of this
				//tempVelocity = max(0.001,min(1,frametime*3));
				//VectorScale(p->vel, (1-tempVelocity), p->vel);
  				if (CONTENTS_SOLID == CL_TruePointContents (p->org))
					p->die=0;

				break;
			case (pm_shrink):
				//this should never happen
				p->size -= 6*(cl.time-cl.oldtime);
				break;
			}

			if (p->type == pm_shrink)
				p->size -= 6*(cl.time-cl.oldtime);
			if (p->type == pm_grow)
				p->size += 6*(cl.time-cl.oldtime);

			if (p->rotation_speed){
				p->rotation += p->rotation_speed * frametime;
			}

			if (pt->id == p_lightning)
			{
				AddParticleColor(p->org2, zero, 1, 1, 0.5f, p_sparks, p->colour, zerodir);
				if (r_decal_misc.value)
					CL_SpawnDecalParticleForPoint(p->org2, 16, 7, part_tex, p->colour);
			}
		}
	}
}

/** TraceLineN
 * same as the TraceLine but returns the normal as well
 * which is needed for bouncing particles
 */
void TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int accurate)
{
	trace_t	trace;
//	vec3_t	temp;

	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;
	VectorCopy (end, trace.endpos);

	if (!cl.worldmodel)
		return;

	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

/*	if (accurate) {
		//calculate actual impact
		VectorSubtract(end, start, temp);
		VectorScale(temp, trace.realfraction, temp);
		VectorAdd(start, temp, impact);
	}else{*/
		VectorCopy (trace.endpos, impact);
//	}

	VectorCopy (trace.plane.normal, normal);
}

// Entar : made specifically for decal spawning
float TraceLineN2 (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, const int accurate)
{
	trace_t	trace;
//	vec3_t	temp;

	memset (&trace, 0, sizeof(trace_t));
	trace.fraction = 1;
	VectorCopy (end, trace.endpos);

	if (!cl.worldmodel)
		return 1;

	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

/*	if (accurate) {
		//calculate actual impact
		VectorSubtract(end, start, temp);
		VectorScale(temp, trace.realfraction, temp);
		VectorAdd(start, temp, impact);
	}else{*/
		VectorCopy (trace.endpos, impact);
//	}

	VectorCopy (trace.plane.normal, normal);

	return trace.fraction;
}

//=================================================================
//New particle adding code
//=================================================================

/** AddParticle
 * Just calls AddParticleColor with the default color variable for that particle
 */
void AddParticle(vec3_t org, int count, float size, float time, int type, vec3_t dir)
{
	//Times: smoke=3, spark=2, blood=3, chunk=4
	vec3_t	colour;

	switch (type)
	{
	case (p_smoke):
		colour[0] = colour[1] = colour[2] = (rand()%255)/255.0;
		break;
	case (p_sparks):
		colour[0] = 1;
		colour[1] = 0.5;
		colour[2] = 0;
		break;
	case (p_fire):
	case (p_fire2):	// non-textured fire too
		//JHL; more QUAKE palette alike color (?)
		colour[0] = 0.92f;
		colour[1] = 0.54f;
		colour[2] = 0.22f;
		break;
	case (p_radius):
		//JHL; more QUAKE palette alike color (?)
		colour[0] = 0.91f;
		colour[1] = 0.91f;
		colour[2] = 1;
		break;
	case (p_blood):
		//colour[0] = (rand()%128)/256.0+0.25f;;
		colour[0] = (rand()%158)/256.0+0.25f;;
		colour[1] = 0;
		colour[2] = 0;
		break;
	case (p_chunks):
		colour[0] = colour[1] = colour[2] = (rand()%182)/255.0;
	}		

	AddParticleColor(org, zero, count, size, time, type, colour, dir);
}

//same as addparticle with a colour var

/** AddParticleColor
 * This is where it all happends, well not really this is where
 * most of the particles (and soon all) are added execpt for trails
 */
void AddParticleColor(vec3_t org, vec3_t org2, int count, float size, float time, int type, vec3_t colour, vec3_t dir)
{
	particle_t		*p;
	particle_tree_t	*pt;
	vec3_t			stop, normal;
	int				i;
	float			tempSize;

	if (dir[0]==0&&dir[1]==0&&dir[2]==0)
		VectorCopy(zerodir, dir);

	for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);

	if (type == p_sparks) // Entar : no sparks underwater!
	{
		switch (CL_TruePointContents(org))
		{
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
			return;
			break;
		default:
			break;
		}
	}
	//find correct particle tree to add too...

	for (i=0 ; (i<count)&&(pt) ; i++)
	{
		p = AddParticleToArray();

		if (!p)
			break;	//no free particles

		p->particle_type = pt;	//set particle type

		p->size = size;
		p->rotation_speed = 0;

		switch (type)
		{
		case (p_decal):
			VectorCopy(org, p->org);
			//VectorCopy(dir, p->vel);
			//velocity
			p->vel[0] = ((rand()%20)-10)/5.0f*size;
			p->vel[1] = ((rand()%20)-10)/5.0f*size;
			p->vel[2] = ((rand()%20)-10)/5.0f*size;

			break;
		case (p_fire):
		case (p_fire2):
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}			//velocity
			p->vel[0] = ((rand()%200)-100)*(size/25)*dir[0];
			p->vel[1] = ((rand()%200)-100)*(size/25)*dir[1];
			p->vel[2] = ((rand()%200)-100)*(size/25)*dir[2];
			break;

		case (p_sparks):
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			tempSize = 150; // Entar : edited sparks velocity settings
			p->vel[0] = (rand()%(int)tempSize)-((int)tempSize/2);
			p->vel[1] = (rand()%(int)tempSize)-((int)tempSize/2);
			p->vel[2] = (rand()%(int)tempSize)-((int)tempSize/3);
			VectorMultiply(p->vel, dir, p->vel);
			break;

		case (p_smoke):
			//pos
			if (VectorCompare(zero,org2)) {
				p->org[0] = org[0] + ((rand()%30)-15)/2;
				p->org[1] = org[1] + ((rand()%30)-15)/2;
				p->org[2] = org[2] + ((rand()%30)-15)/2;
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			//make sure the particle is inside the world
			TraceLineN(org, p->org, stop, normal,0);
			if (Length(stop) != 0)
				VectorCopy(stop, p->org);

			//velocity
			p->vel[0] = ((rand()%10)-5)/20*dir[0];
			p->vel[1] = ((rand()%10)-5)/20*dir[1];
			p->vel[2] = ((rand()%10)-5)/20*dir[2];

			//smoke should rotate
			p->rotation_speed = (rand() & 31) + 32;
			p->rotation = rand()%90;
			break;

		case (p_blood):
			p->size = size * (rand()%20)/10;
			p->type = pm_grow;
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}

			//velocity
			p->vel[0] = (rand()%40)-20*dir[0];
			p->vel[1] = (rand()%40)-20*dir[1];
			p->vel[2] = (rand()%40)-20*dir[2];
			break;

		case (p_chunks):
			//pos
			if (VectorCompare(zero,org2)) {
				VectorCopy(org, p->org);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand();
				p->org[1] = org[1] + (org2[0] - org[1])*rand();
				p->org[2] = org[2] + (org2[0] - org[2])*rand();
			}
			//velocity
			p->vel[0] = (rand()%40)-20*dir[0];
			p->vel[1] = (rand()%40)-20*dir[1];
			p->vel[2] = (rand()%40)-5*dir[2];
			break;

		case (p_bubble):
			//pos
			if (VectorCompare(zero,org2)) {
				p->org[0] = org[0] + ((rand() & 31) - 16);
				p->org[1] = org[1] + ((rand() & 31) - 16);
				p->org[2] = org[2] + ((rand() & 63) - 32);
			} else {
				p->org[0] = org[0] + (org2[0] - org[0])*rand()*dir[0];
				p->org[1] = org[1] + (org2[0] - org[1])*rand()*dir[1];
				p->org[2] = org[2] + (org2[0] - org[2])*rand()*dir[2];
			}
			//velocity
			p->vel[0] = 0;
			p->vel[1] = 0;
			p->vel[2] = 0;
			break;
		}

		p->decal_texture = 0;
		p->hit = 0;
		p->start = cl.time;
		p->die = cl.time + time;
		p->updateafter = cl.time-1; // update immediately
		p->type = pm_static;

		// script stuff
		p->script_trail = p_none;
		p->script_trailsize = 0;
		p->script_growrate = 0;
		Q_strcpy(p->script_trailextra, "none");

		if (pt->move == pm_die){
			VectorNegate(p->vel, p->acc);
			VectorScale(p->acc, 1/time, p->acc);
		} else {
			VectorCopy(zero, p->acc);
		}

		addGrav(pt, p);

		VectorCopy(colour, p->colour);
	}
}

// FIXME: clean up the spawnmode code
/** AddParticleCustom
 * This is where the script system calls the actual particle(s).
 * The difference in this one: WAY more customizable
 */
void AddParticleCustom(vec3_t org1, int count, float size, float time, int customid, part_type_t type, vec3_t colour, vec3_t dir, int checkwater, float wait, float gravity, part_type_t trail, int trailsize, float trailtime, char *trailextra, int randvelchange, int randorgchange, float growrate, float speed, int rotation, int rotation_speed, char *spawnmode, float areaspread, float areaspreadvert)
{
	particle_t		*p;
	particle_tree_t	*pt;
	vec3_t			arsvec, ofsvec, org;
	int				i, l = 0, m = 0, j = 0, k = 0, spawnspc;
	qboolean			domode=true;
	float			tempSize;

//	if (dir[0]==0&&dir[1]==0&&dir[2]==0)
//		VectorCopy(zerodir, dir);

	VectorCopy(org1, org);

	if (type == p_sparks && checkwater) // Entar : no sparks underwater!
	{
		switch (CL_TruePointContents(org))
		{
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
			return;
			break;
		default:
			break;
		}
	}
	if (customid == -1)
		for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);
	else
		for (pt = particle_type_active; (pt) && (pt->custom_id != customid); pt = pt->next);

	VectorAdd(org, pt->offset, org); // offset

	if (!strcmp(spawnmode, "none"))
	{
		domode=false;
	}
	else if (!strcmp(spawnmode, "uniformcircle"))
	{
		m = count;
//		if (ptype->type == PT_BEAM)
//			m--;

		if (m < 1)
			m = 0;
		else
			m = (M_PI*2)/m;

//		if (p->spawnparam1) /* use for weird shape hacks */
//			m *= p->spawnparam1;
	}
	else if (!strcmp(spawnmode, "telebox"))
	{
		spawnspc = 4;
//		l = -ptype->areaspreadvert;
		l = areaspreadvert;
		j = k = 2; //
	}
	else if (!strcmp(spawnmode, "lavasplash")){
		j = k = areaspread;
//		if (ptype->spawnparam1)
//			m = ptype->spawnparam1;
//		else
			m = 0.57552; /* some default number for tele/lavasplash used in vanilla Q1 */

//		if (p->spawnparam2)
//			spawnspc = (int)ptype->spawnparam2;
	}
	else if (!strcmp(spawnmode, "syncfield")){
		if (!avelocities[0][0])
		{
			for (j=0 ; j<NUMVERTEXNORMALS*2 ; j++)
				avelocities[0][j] = (rand()&255) * 0.01;
		}

		j = 0;
		m = 0;
	}
	else if (strcmp(spawnmode, "ball") && strcmp(spawnmode, "circle"))
		domode=false;

	for (i=0 ; (i<count)&&(pt) ; i++)
	{
		p = AddParticleToArray();

		if (!p)
			break;	//no free particles

		p->particle_type = pt;	//set particle type

		p->size = size;
		p->rotation_speed = 0;

		//set up position
		VectorCopy(org, p->org);

		switch (type)
		{
		case (p_decal):
			//VectorCopy(dir, p->vel);
			//velocity
			p->vel[0] = ((rand()%20)-10)/5.0f;
			p->vel[1] = ((rand()%20)-10)/5.0f;
			p->vel[2] = ((rand()%20)-10)/5.0f;

			if (rotation)
				p->rotation = rotation;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;
		case (p_fire):
		case (p_fire2):
			//velocity
			if (!domode)
			{
				p->vel[0] = ((rand()%200)-100)*speed*dir[0];
				p->vel[1] = ((rand()%200)-100)*speed*dir[1];
				p->vel[2] = ((rand()%200)-100)*speed*dir[2];
			}

			if (rotation)
				p->rotation = rotation;
			else
				p->rotation = rand()%90;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;

		case (p_sparks):
			//velocity
			if (!domode)
			{
				//tempSize = size * 2; // 3.3?
				tempSize = speed; // Entar : edited sparks velocity settings
				p->vel[0] = (rand()%(int)tempSize)-((int)tempSize/2);
				p->vel[1] = (rand()%(int)tempSize)-((int)tempSize/2);
				p->vel[2] = (rand()%(int)tempSize)-((int)tempSize/3);
				VectorMultiply(p->vel, dir, p->vel);
			}

			if (rotation)
				p->rotation = rotation;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;

		case (p_smoke):
			//velocity
			if (!domode)
			{
				p->vel[0] = (((rand()%10)-5)/20)*dir[0]*speed;
				p->vel[1] = (((rand()%10)-5)/20)*dir[1]*speed;
				p->vel[2] = (((rand()%10)-5)/20)*dir[2]*speed;
			}

			//smoke should rotate
			if (rotation)
				p->rotation = rotation + rand()%10;
			else
				p->rotation = rand()%90;

			if (rotation_speed)
				p->rotation_speed = rand()%rotation_speed;
			else
				p->rotation_speed = (rand() & 31) + 32;
			break;

		case (p_blood):
			p->size = size * (rand()%20)/10;
//			p->type = pm_grow;
			//velocity
			if (!domode)
			{
				p->vel[0] = ((rand()%40)-20)*dir[0]*speed;
				p->vel[1] = ((rand()%40)-20)*dir[1]*speed;
				p->vel[2] = ((rand()%40)-20)*dir[2]*speed;
			}

			if (rotation)
				p->rotation = rotation;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;

		case (p_chunks):
			//velocity
			if (!domode)
			{
				p->vel[0] = ((rand()%40)-20)*dir[0]*speed;
				p->vel[1] = ((rand()%40)-20)*dir[1]*speed;
				p->vel[2] = ((rand()%40)-5)*dir[2]*speed;
			}
			
			if (rotation)
				p->rotation = rotation;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;

		case (p_bubble):
			//pos
			p->org[0] += ((rand() & 31) - 16);
			p->org[1] += ((rand() & 31) - 16);
			p->org[2] += ((rand() & 63) - 32);
			//velocity
			p->vel[0] = 0;
			p->vel[1] = 0;
			p->vel[2] = 0;

			if (rotation)
				p->rotation = rotation;
			if (rotation_speed)
				p->rotation_speed = rotation_speed;
			break;
		}

		p->decal_texture = 0;
		p->hit = 0;
		if (wait)
		{
			p->start = cl.time + wait;
			p->die = cl.time + time + wait;
		}
		else
		{
			p->start = cl.time;
			p->die = cl.time + time;
		}
		p->updateafter = cl.time-1;
		p->type = pt->move;
		p->script_trail = trail;
		p->script_trailsize = trailsize;
		if (pt->move == pm_die){
			VectorNegate(p->vel, p->acc);
			VectorScale(p->acc, 1/time, p->acc);
		} else {
			VectorCopy(zero, p->acc);
		}

		p->script_growrate = growrate;
		if (trailextra)
			Q_strcpy(p->script_trailextra, trailextra);
		else
			Q_strcpy(p->script_trailextra, "none");

		if (!strcmp(spawnmode, "circle") || !strcmp(spawnmode, "ball"))
		{
			ofsvec[0] = hrandom();
			ofsvec[1] = hrandom();
			if (areaspreadvert)
				ofsvec[2] = hrandom();
			else
				ofsvec[2] = 0;

			VectorNormalize(ofsvec);
			if (!strcmp(spawnmode, "ball"))
				VectorScale(ofsvec, frandom(), ofsvec);

			arsvec[0] = ofsvec[0]*areaspread;
			arsvec[1] = ofsvec[1]*areaspread;
			arsvec[2] = ofsvec[2]*areaspreadvert;
		}
		else if (!strcmp(spawnmode, "telebox"))
		{
			ofsvec[0] = k;
			ofsvec[1] = j;
			ofsvec[2] = l+4;
			VectorNormalize(ofsvec);
			VectorScale(ofsvec, 1.0-(frandom())*m, ofsvec);

			// org is just like the original
			arsvec[0] = j + (rand()%spawnspc);
			arsvec[1] = k + (rand()%spawnspc);
			arsvec[2] = rand()%l*2 - l + (rand()%spawnspc);

			// advance telebox loop
			j += 4;
			if (j >= areaspread)
			{
				j = -areaspread;
				k += 4;
				if (k >= areaspread)
				{
					k = -areaspread;
					l += 4;
					if (l >= areaspreadvert)
						l = -areaspreadvert;
				}
			}
		}
		else if (!strcmp(spawnmode, "lavasplash"))
		{
			// calc directions, org with temp vector
			ofsvec[0] = k + (rand()%spawnspc);
			ofsvec[1] = j + (rand()%spawnspc);
			ofsvec[2] = 256;

			arsvec[0] = ofsvec[0];
			arsvec[1] = ofsvec[1];
			arsvec[2] = frandom()*areaspreadvert;

			VectorNormalize(ofsvec);
			VectorScale(ofsvec, 1.0-(frandom())*m, ofsvec);

			// advance splash loop
			j += spawnspc;
			if (j >= areaspread)
			{
				j = -areaspread;
				k += spawnspc;
				if (k >= areaspread)
					k = -areaspread;
			}
		}
		else if (!strcmp(spawnmode, "uniformcircle"))
		{
			m = count;
//			if (p->type == PT_BEAM)
//				m--;

			if (m < 1)
				m = 0;
			else
				m = (M_PI*2)/m;

//			if (p->spawnparam1) /* use for weird shape hacks */
//				m *= ptype->spawnparam1;
			
			ofsvec[0] = cos(m*i);
			ofsvec[1] = sin(m*i);
			ofsvec[2] = 0;
			VectorScale(ofsvec, areaspread, arsvec);
		}
		else if (!strcmp(spawnmode, "syncfield"))
		{
			arsvec[0] = cl.time * (avelocities[i][0] + m);
			arsvec[1] = cl.time * (avelocities[i][1] + m);
			arsvec[2] = cos(arsvec[1]);

			ofsvec[0] = arsvec[2]*cos(arsvec[0]);
			ofsvec[1] = arsvec[2]*sin(arsvec[0]);
			ofsvec[2] = -sin(arsvec[1]);
			
			arsvec[0] = r_avertexnormals[j][0]*areaspread + ofsvec[0]*BEAMLENGTH;
			arsvec[1] = r_avertexnormals[j][1]*areaspread + ofsvec[1]*BEAMLENGTH;
			arsvec[2] = r_avertexnormals[j][2]*areaspreadvert + ofsvec[2]*BEAMLENGTH;

			VectorNormalize(ofsvec);

			j++;
			if (j >= NUMVERTEXNORMALS)
			{
				j = 0;
				m += 0.1982671; // some strange number to try to "randomize" things
			}
		}

		if (domode)
		{
			p->vel[0] = ofsvec[0]*dir[0]*speed;
			p->vel[1] = ofsvec[1]*dir[1]*speed;
			p->vel[2] = ofsvec[2]*dir[2]*speed;
		}
		else
		{
			p->vel[0] += ofsvec[0]*speed;
			p->vel[1] += ofsvec[1]*speed;
			p->vel[2] += ofsvec[2]*speed;
		}
		p->org[0] += arsvec[0];
		p->org[1] += arsvec[1];
		p->org[2] += arsvec[2];

		// gravity
		p->acc[2] = (-grav * 8) * gravity;

		//random change effects
		if (randvelchange)
		{
			p->acc[0] += (180 - (rand()%360))*speed; // Entar : made it based on speed
			p->acc[1] += (180 - (rand()%360))*speed;
			p->acc[2] += (180 - (rand()%360))*speed;
		}

		if (randorgchange)
		{
			p->org[0] += (rand()%randorgchange*2) - randorgchange;
			p->org[1] += (rand()%randorgchange*2) - randorgchange;
			p->org[2] += (rand()%randorgchange*2) - randorgchange;
		}

		VectorCopy(colour, p->colour);
	}
}

/** AddFire
 * This is used for the particle fires
 * Somewhat reliant on framerate.
 */
//FIXME: will be replaced when emitters work right
void AddFire(vec3_t org, float size)
{
	particle_t		*p;
	particle_tree_t *pt;
	vec3_t			colour;
	int				i, count;
	
	if ((cl.time == cl.oldtime))
		return;
	
	for (pt = particle_type_active; (pt) && (pt->id != p_fire); pt = pt->next);

	count = 1;//(int)((cl.time-timepassed)*1.5);
	
	if (timetemp <= cl.oldtime){
		timepassed = cl.time;
		timetemp = cl.time;
	}

	colour[0] = 0.68 + ((rand() & 3) / 10);
	colour[1] = 0.38f;
	colour[2] = 0.08f;

	for (i=0 ; i<count ; i++)
	{
		p = AddParticleToArray();

		if (!p)
			break;	//no free particles

		p->particle_type = pt;	//set particle type

		if (size >= 12)
			p->size = randsize(size, 0.32f);
		else
			p->size = size;

		//pos
		VectorCopy(org, p->org);

		//velocity
		p->vel[0] =(rand()%15)-(rand()%15);
		p->vel[1] =(rand()%15)-(rand()%15);
		p->vel[2] =(rand()%45)+28;

		if (p->vel[2] <= 25)
			p->vel[2] = 30;

		p->rotation_speed = (rand() & 64) - 32;
		p->rotation = rand()%90;

		VectorCopy(zero, p->acc);

		if (pt->move == pm_die){ 
		VectorNegate(p->vel, p->acc); 
		VectorScale(p->acc, 0.90f, p->acc); 
		} else { 
		VectorCopy(zero, p->acc); 
		}

		p->hit = 0;
		p->start = cl.time;
		p->die = cl.time + 0.70;
		p->updateafter = cl.time-1; // update immediately
		p->type = pm_shrink;

		// script stuff
		p->script_trail = p_none;
		p->script_trailsize = 0;
		p->script_growrate = 0;
		Q_strcpy(p->script_trailextra, "none");

		VectorCopy(colour, p->colour);
	}
}

/** AddSmoke
 * This is used for the particle fires
 * Somewhat reliant on framerate.
 * - - - - - -
 * By Entar, works great
 */
void AddSmoke(vec3_t org, float size)
{
	particle_t		*p;
	particle_tree_t *pt;
	vec3_t			colour;
	int				i, count;
	
	if ((cl.time == cl.oldtime))
		return;
	
	for (pt = particle_type_active; (pt) && (pt->id != p_smoke); pt = pt->next);

	count = 1;//(int)((cl.time-timepassed)*1.5);
	
	if (timetemp <= cl.oldtime){
		timepassed = cl.time;
		timetemp = cl.time;
	}

	colour[0] = 0.9f;
	colour[1] = 0.9f;
	colour[2] = 0.9f;

	for (i=0 ; i<count ; i++)
	{
		p = AddParticleToArray();

		if (!p)
			break;	//no free particles

		p->particle_type = pt;	//set particle type

		p->size = size;				

		//pos
		VectorCopy(org, p->org);

		//velocity
		p->vel[0] =(rand()%15)-(rand()%15);
		p->vel[1] =(rand()%15)-(rand()%15);
		p->vel[2] =(rand()%45)+28;

		if (p->vel[2] <= 15)
			p->vel[2] = 20;

		//smoke should rotate some
		p->rotation_speed = (rand() & 31) + 32;
		p->rotation = rand()%90;

		VectorCopy(zero, p->acc);

		if (pt->move == pm_die){ 
		VectorNegate(p->vel, p->acc); 
		VectorScale(p->acc, 0.90f, p->acc); 
		} else { 
		VectorCopy(zero, p->acc); 
		}

		p->hit = 0;
		p->start = cl.time - 4;
		p->die = cl.time + 1.41;
		p->updateafter = cl.time-1; // update immediately
		//p->type = pm_shrink;
		p->type = pm_grow;
		VectorCopy(colour, p->colour);

		p->script_trail = p_none;
		p->script_trailsize = 0;
		p->script_growrate = 0;
		Q_strcpy(p->script_trailextra, "none");
	}
}

/** AddTrail
 * Calls AddTrailColor with the default colours
 */
void AddTrail(vec3_t start, vec3_t end, int type, float time, float size, vec3_t dir)
{
	vec3_t colour;

	//colour set when first update called (check if needs to be fixed)
	switch (type)
	{
	case (p_smoke):
		colour[0] = colour[1] = colour[2] = (rand()%128)/255.0;
		break;
	case (p_sparks):
		colour[0] = 1;
		colour[1] = 0.5;
		colour[2] = 0;
		break;
	case (p_fire):
	case (p_fire2): // non textured fire too
		colour[0] = 0.75f;
		colour[1] = 0.45f;
		colour[2] = 0.15f;
		break;
	case (p_blood):
		colour[0] = 0.45f; // original was 0.5f, 0, 0
		colour[1] = 0;
		colour[2] = 0;
		break;
	case (p_chunks):
		colour[0] = colour[1] = colour[2] = (rand()%182)/255.0;
	}
	//AddTrailColor(start, end, type, time, size, colour, zerodir);
	AddTrailColor(start, end, type, time, size, colour, dir);
}

/** AddTrailColor
 * This will add a trail of particles of the specified type.
 * and return a lightning particle (for player movement updates)
 */
particle_t *AddTrailColor(vec3_t start, vec3_t end, int type, float time, float size, vec3_t color, vec3_t dir)
{
	particle_tree_t	*pt, *bubbles;
	particle_t		*p;
	int				i, typetemp, bubble = 0;
	float			count;
	vec3_t			point;

	//work out vector for trail
	VectorSubtract(start, end, point);
	//work out the length and therefore the amount of particles
	count = Length(point);
	//make sure its at least 1 long
	//quater the amount of particles (speeds it up a bit)
	if (count == 0)
		return NULL;
	else
		count = count/8;

	//find correct particle tree to add too...
	if (type == p_smoke)
		typetemp = p_trail;
	else
		typetemp = type;

	for (pt = particle_type_active; (pt) && (pt->id != typetemp); pt = pt->next);
	for (bubbles = particle_type_active; (bubbles) && (bubbles->id != p_bubble); bubbles = bubbles->next);

	if (((typetemp == p_trail)||(typetemp == p_lightning))&&(pt)){
		if (type == p_smoke)
		{
			//test to see if its in water
			switch (CL_TruePointContents(start)) {
			case CONTENTS_WATER:
			case CONTENTS_SLIME:
			case CONTENTS_LAVA:
				bubble = 1;
				break;
			default:	//not in water do it normally
				;
			}
		}

		if ((type == p_smoke && !bubble && gl_smoketrail.value != 0) || type != p_smoke){
			p = AddParticleToArray();

			if (p){
				p->particle_type = pt;	//set particle type

				VectorCopy(start, p->org);
				VectorCopy(end, p->org2);

				p->type = pm_static;
				p->die = cl.time + time;
				p->size = size;

				VectorCopy(color, p->colour);
				p->hit = 0;
				p->start = cl.time;
				p->vel[0] =	p->vel[1] = p->vel[2] = 0;

				if (type != p_smoke || gl_smoketrail.value == 1)
					return p;
			}
		}
		for (pt = particle_type_active; (pt) && (pt->id != type); pt = pt->next);
	}

	//the vector from the current pos to the next particle
	VectorScale(point, 1.0/count, point);

	if ((pt)&&(bubbles))		//only need to test once....
		for (i=0 ; i<count; i++)
		{
			p = AddParticleToArray();

			if (!p)
				break;	//no free particles

			//work out the pos
			VectorMA (end, i, point, p->org);

			//make it a bit more random
			p->org[0] += ((rand()%16)-8)/4;
			p->org[1] += ((rand()%16)-8)/4;
			p->org[2] += ((rand()%16)-8)/4;

			p->particle_type = pt;	//set particle type

			//reset the particle vars
			p->hit = 0;
			p->start = cl.time;
			p->die = cl.time + time;
			p->decal_texture = 0;
			VectorCopy(color, p->colour);

			//small starting velocity
			if (VectorCompare(dir, zerodir)){
				p->vel[0]=((rand()%16)-8)/2;
				p->vel[1]=((rand()%16)-8)/2;
				p->vel[2]=((rand()%16)-8)/2;
			}else{
				p->vel[0]=dir[0] + ((rand()%16)-8)/16;
				p->vel[1]=dir[1] + ((rand()%16)-8)/16;
				p->vel[2]=dir[2] + ((rand()%16)-8)/16;
			}
			p->type = pm_static;
			p->size = size;
			p->rotation_speed = 0;
		
			//add the particle to the correct one
			switch (type)
			{
			case (p_sparks):
				//need bigger starting velocity (affected by grav)
				p->vel[0]=((rand()%32)-16)*2;
				p->vel[1]=((rand()%32)-16)*2;
				p->vel[2]=((rand()%32))*3;
				break;
			case (p_smoke):
				//smoke should rotate
				if (!bubble){
					p->rotation_speed = (rand() & 31) + 32;
					p->rotation = rand()%20;
					p->type = pm_grow;
					p->rotation_speed = (rand() & 31) + 32;
					p->rotation = rand()%90;
				} else {
					//velocity
					p->vel[0] = 0;
					p->vel[1] = 0;
					p->vel[2] = 0;
					break;					
				}
				break;
			case (p_blood):
				p->size = size * (rand()%20)/10;
				//p->type = pm_grow;
				p->type = pm_normal;
				p->start = cl.time - time * 0.5f;
				break;
			case (p_chunks):
			case (p_fire):
			case (p_fire2):
				break;
			}

			if (pt->move == pm_die){
				VectorNegate(p->vel, p->acc);
				VectorScale(p->acc, 1/time, p->acc);
			} else {
				VectorCopy(zero, p->acc);
			}

			// script stuff
			p->script_trail = p_none;
			p->script_trailsize = 0;
			p->script_growrate = 0;
			p->updateafter = cl.time-1; // update immediately

			Q_strcpy(p->script_trailextra, "none");

			addGrav(pt, p);
			if (bubble)
				addGrav(bubbles,p);
		}

	return p;
}

/** AddDecalColor
 * This is where it all happens, well not really this is where
 * most of the particles (and soon all) are added execpt for trails
 */
void AddDecalColor(vec3_t org, vec3_t normal, float size, float time, int texture, vec3_t colour)
{
	particle_t		*p;
	particle_tree_t	*pt;

	for (pt = particle_type_active; (pt) && (pt->id != p_decal); pt = pt->next);
	//find correct particle tree to add too...

	p = AddParticleToArray();

	if (p)		//check if there are free particles
	{
		p->particle_type = pt;	//set particle type

		p->size = size;
		p->rotation_speed = 0;

		VectorCopy(org, p->org);	//set position
		VectorCopy(normal, p->vel);	//set direction
			
		p->hit = 1;					//turn off physics and tell renderer to draw it

		p->start = cl.time;			//current time
		p->die = cl.time + time;	//time it will have fully faded by
		p->type = pm_static;
		p->decal_texture = texture;	//set the texture

		VectorCopy(zero, p->acc);	//no acceleration
		VectorCopy(colour, p->colour);	//copy across colour

		p->script_trail = p_none;
		p->script_trailsize = 0;
		p->script_growrate = 0;
		Q_strcpy(p->script_trailextra, "none");
	}
}

// CL_SpawnDecalParticleForPoint
// From DP, edited by Entar

// new version, slightly butchered by Entar
void CL_SpawnDecalParticleForPoint(vec3_t org, float maxdist, float size, int texture, vec3_t color)
{
	int i;
	float bestfrac, bestorg[3], bestnormal[3];
	float frac, v[3], normal[3], org2[3];

	bestfrac = 10;
	for (i = 0;i < 32;i++)
	{
		VectorRandom(org2);
		VectorMA(org, maxdist, org2, org2);
//		frac = CL_TraceLine(org, org2, v, normal, true, &hitent, SUPERCONTENTS_SOLID);
		frac = TraceLineN2(org, org2, v, normal, 0);
		if (bestfrac > frac)
		{
			bestfrac = frac;
//			besthitent = hitent;
			VectorCopy(v, bestorg);
			VectorCopy(normal, bestnormal);
		}
	}
	if (bestfrac < 1)
		AddDecalColor(bestorg, bestnormal, size, r_decaltime.value, texture, color);
}

//==================================================
//Particle texture code
//==================================================
int LoadParticleTexture (char *texture)
{
	return GL_LoadTexImage (texture, false, true);
}

/** MakeParticleTexture
 * Makes the particle textures only 2 the 
 * smoke texture (which could do with some work and
 * the others which is just a round circle.
 *
 * I should really make it an alpha texture which would save 32*32*3
 * bytes of space but *shrug* it works so i wont stuff with it
 *
void MakeParticleTexure(void)
{
    int i, j, k, centreX, centreY, separation, max;
    byte	data[128][128][4];

	//Normal texture (0->256 in a circle)
	//If you change this texture you will change the textures of
	//all particles except for smoke (other texture) and the
	//sparks which dont use textures
    for(j=0;j<128;j++){
        for(i=0;i<128;i++){
			data[i][j][0]	= 255;
			data[i][j][1]	= 255;
			data[i][j][2]	= 255;
            separation = (int) sqrt((64-i)*(64-i) + (64-j)*(64-j));
            if(separation<63)
                data[i][j][3] = 255 - separation * 256/63;
            else data[i][j][3] =0;
        }
    }
	//Load the texture into vid mem and save the number for later use
	part_tex = GL_LoadTexture ("particle", 128, 128, &data[0][0][0], true, true, 4);

	//Clear the data
	max=64;
    for(j=0;j<128;j++){
        for(i=0;i<128;i++){
			data[i][j][0]	= 255;
			data[i][j][1]	= 255;
			data[i][j][2]	= 255;
			separation = (int) sqrt((i - 64)*(i - 64));
			data[i][j][3] = (max - separation)*2; 
		}
    }
	
	//Add 128 random 4 unit circles
	for(k=0;k<128;k++){
		centreX = rand()%122+3;
		centreY = rand()%122+3;
		for(j=-3;j<3;j++){
			for(i=-3;i<3;i++){
				separation = (int) sqrt((i*i) + (j*j));
				if (separation <= 5)
					data[i+centreX][j+centreY][3]	+= (5-separation);
			}
	    }
	}
	trail_tex = GL_LoadTexture ("trail_part", 128, 128, &data[0][0][0], false, true, 4);

	blood_tex = LoadParticleTexture	("textures/particles/blood");
	bubble_tex =LoadParticleTexture	("textures/particles/bubble");
	smoke_tex = LoadParticleTexture	("textures/particles/smoke");
	lightning_tex =LoadParticleTexture ("textures/particles/lightning");
	spark_tex = LoadParticleTexture ("textures/particles/spark");
}*/

extern byte	particle[32][32];
extern byte	smoke1[32][32];
//extern byte	smoke2[32][32];
//extern byte	smoke3[32][32];
//extern byte	smoke4[32][32];
extern byte	blood2[32][32];
extern byte	bubble[32][32];
extern byte	snow[32][32];
extern byte	rain[32][32];
extern byte	flare[32][32];
extern byte	flama[32][32];
//extern byte spark[8][8];

#define PARTICLETEXTURESIZE 32

extern int flama_tex;

// From DP, edited for use in Vr2
void particletextureblotch(unsigned char *data, float radius, float red, float green, float blue, float alpha)
{
	int x, y;
	float cx, cy, dx, dy, f, iradius;
	unsigned char *d;
	cx = (lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius) + lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius)) * 0.5f;
	cy = (lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius) + lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius)) * 0.5f;
	iradius = 1.0f / radius;
	alpha *= (1.0f / 255.0f);
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - cx);
			dy = (y - cy);
			f = (1.0f - sqrt(dx * dx + dy * dy) * iradius) * alpha;
			if (f > 0)
			{
				d = data + (y * PARTICLETEXTURESIZE + x) * 4;
				d[0] += (int)(f * (red   - d[0]));
				d[1] += (int)(f * (green - d[1]));
				d[2] += (int)(f * (blue  - d[2]));
			}
		}
	}
}

void particletextureinvert(byte *data)
{
	int i;
	for (i = 0;i < PARTICLETEXTURESIZE*PARTICLETEXTURESIZE;i++, data += 4)
	{
		data[0] = 255 - data[0];
		data[1] = 255 - data[1];
		data[2] = 255 - data[2];
	}
}

extern void fractalnoise(unsigned char *noise, int size, int startgrid);

void MakeParticleTexure(void) // function taken from TomazQuake, edited by Entar
{
	int		x, y;
	byte	data[32][32][4];

	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[x][y][0]	= 255;
			data[x][y][1]	= 255;
			data[x][y][2]	= 255;
			data[x][y][3]	= particle[x][y];
		}
	}
	part_tex = GL_LoadTexture ("particle", 32, 32, &data[0][0][0], true, true, 4);

	smoke_tex = LoadParticleTexture	("textures/particles/smoke");
	if (!smoke_tex)
	{
		unsigned char noise1[PARTICLETEXTURESIZE*2][PARTICLETEXTURESIZE*2];
		fractalnoise(&noise1[0][0], PARTICLETEXTURESIZE*2, PARTICLETEXTURESIZE/8);
		for (x=0 ; x<32 ; x++)
		{
			for (y=0 ; y<32 ; y++)
			{
				data[x][y][0]	= 255;
				data[x][y][1]	= 255;
				data[x][y][2]	= 255;
				data[x][y][3]	= (smoke1[x][y]+(smoke1[x][y]*((float)noise1[x][y]/255.0f)))/2;
			}
		}
		smoke_tex = GL_LoadTexture ("smoke1", 32, 32, &data[0][0][0], true, true, 4);
	}

	fire_tex = LoadParticleTexture ("textures/particles/fire");
	if (!fire_tex)
		fire_tex = smoke_tex;

	blood_tex = LoadParticleTexture	("textures/particles/blood");
	if (!blood_tex) // if no external one, make our own!
	{
		for (x = 0;x < 24; x++)
		{
			particletextureblotch(&data[0][0][0], PARTICLETEXTURESIZE/10, 0, 0, 0, 220);
			particletextureblotch(&data[0][0][0], PARTICLETEXTURESIZE/14, 10, 10, 10, 255);
		}

		particletextureinvert(&data[0][0][0]);
		blood_tex = GL_LoadTexture ("blood", 32, 32, &data[0][0][0], true, true, 4);
	}
	
	bubble_tex =LoadParticleTexture	("textures/particles/bubble");
	if (!bubble_tex)
	{
		for (x=0 ; x<32 ; x++)
		{
			for (y=0 ; y<32 ; y++)
			{
				data[x][y][0]	= 255;
				data[x][y][1]	= 255;
				data[x][y][2]	= 255;
				data[x][y][3]	= bubble[x][y];
			}
		}
		bubble_tex = GL_LoadTexture ("bubble", 32, 32, &data[0][0][0], true, true, 4);
		//bubble_tex = GL_LoadTexture ("bubble", 16, 16, &data[0][0][0], true, true, 4);
	}

	spark_tex = LoadParticleTexture ("textures/particles/spark");
	if (!spark_tex)
		spark_tex = part_tex;
/*	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[x][y][0]	= 255;
			data[x][y][1]	= 255;
			data[x][y][2]	= 255;
			data[x][y][3]	= spark[x][y];
		}
	}
	spark_tex = GL_LoadTexture ("spark", 8, 8, &data[0][0][0], true, true, 4);*/
	
	lightning_tex =LoadParticleTexture ("textures/particles/lightning");
	if (!lightning_tex)
		lightning_tex = part_tex;

	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[x][y][0]	= 255;
			data[x][y][1]	= 255;
			data[x][y][2]	= 255;
			data[x][y][3]	= flare[x][y];
		}
	}
	flareglow_tex = GL_LoadTexture ("flare", 32, 32, &data[0][0][0], true, true, 4);

/*	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[x][y][0]	= 255;
			data[x][y][1]	= 255;
			data[x][y][2]	= 255;
			data[x][y][3]	= snow[x][y];
		}
	}
	snow_tex = GL_LoadTexture ("snow", 32, 32, &data[0][0][0], true, true, 4);

	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[x][y][0]	= 255;
			data[x][y][1]	= 255;
			data[x][y][2]	= 255;
			data[x][y][3]	= rain[x][y];
		}
	}
	rain_tex = GL_LoadTexture ("rain", 32, 32, &data[0][0][0], true, true, 4);*/
}
