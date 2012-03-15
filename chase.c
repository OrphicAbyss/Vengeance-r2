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
// chase.c -- chase camera code
// much of the code here was taken from Telejano.
// Some editing by Entar; chase_pitch and _roll only work in
// custom chase mode (chase_active 2) which is created by Entar
// for "cinematic" screenshots; chase_active 1 is your regular chasecam.

#include "quakedef.h"

cvar_t	chase_back = {"chase_back", "55"};
cvar_t	chase_up = {"chase_up", "20"};
cvar_t	chase_right = {"chase_right", "-5"};
cvar_t	chase_active = {"chase_active", "0"};

cvar_t	chase_pitch = {"chase_pitch", "5", true};
cvar_t	chase_roll = {"chase_roll", "0", true};
cvar_t	chase_yaw = {"chase_yaw", "0", true};

vec3_t	chase_pos;
vec3_t	chase_angles;

vec3_t	chase_dest;
vec3_t	chase_dest_angles;

#define MAXCAMS 100

typedef struct
{
	vec3_t org;
	vec3_t angle;
	float lastchange;	
	int used;

} infocam;

infocam cams[MAXCAMS];

/*void TraceLine (vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);
}*/

extern entity_t *traceline_entity[MAX_EDICTS];
extern int traceline_entities;

float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t	trace;
	extern cvar_t env_engine;
	int	x;
	model_t      *mod;
	float		shortest;
	vec3_t		newstart, newend;

	memset (&trace, 0, sizeof(trace));
	VectorCopy (end, trace.endpos);
	trace.fraction = 1;
	trace.startsolid = true;
	
//	if (sv.active)
//		SV_RecursiveHullCheck (sv.worldmodel->hulls, 0, 0, 1, start, end, &trace);
//	else
	if (cl.worldmodel)
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy (trace.endpos, impact);
	VectorCopy (trace.plane.normal, normal);
	shortest = trace.fraction;	
	
	VectorCopy (end, trace.endpos);
	trace.fraction = 1;
	trace.startsolid = true;

	for (x = 0; x < traceline_entities; x++) 
	{ 
		mod = traceline_entity[x]->model; 

		if (mod->type != mod_brush) continue; 

		if (mod->name[0] != '*') continue; 

		// set up start point 
		// offset the start [0] and [1] points by entity origin to do func_train bmodels 
		// this is positioned above the model to account for moving bmodels (note the "-" in [2]) 
		newstart[0] = start[0] + cl_visedicts[x]->origin[0]; 
		newstart[1] = start[1] + cl_visedicts[x]->origin[1]; 
		newstart[2] = start[2] + cl_visedicts[x]->origin[2]; 

		// set up end point 
		newend[0] = end[0] + cl_visedicts[x]->origin[0]; 
		newend[1] = end[1] + cl_visedicts[x]->origin[1]; 
		newend[2] = end[2] + cl_visedicts[x]->origin[2]; 

		// reset trace
		memset (&trace, 0, sizeof(trace));
		VectorCopy (end, trace.endpos);
		trace.fraction = 1;
		trace.startsolid = true;

//		r = RecursiveLightPoint (mod->nodes + mod->hulls[0].firstclipnode, start, end); 
		SV_RecursiveHullCheck (mod->hulls, mod->hulls[0].firstclipnode, 0, 1, newstart, newend, &trace);

		// check for shortest path 
		if (trace.fraction < shortest) 
		{ 
			shortest = trace.fraction;
			VectorCopy (trace.endpos, impact);
			VectorSubtract(trace.endpos, cl_visedicts[x]->origin, trace.endpos);
			VectorCopy (trace.plane.normal, normal);
		} 

		VectorCopy (end, trace.endpos);
		trace.fraction = 1;
		trace.startsolid = true;
	} 

	return shortest;
}

int GetNearCam(vec3_t org)
{
	int i;
	vec3_t vec,stop;
    int min_num = 0; // = no near cam
    int min_dist = 9999999;

    for (i = 0; i < MAXCAMS; i++)
    {
		if(cams[i].used)
		{

			VectorSubtract (org, cams[i].org, vec);
			if (Length(vec) < min_dist)
			{
				//TODO: avoid that?
				if (TraceLine(org,cams[i].org,stop,stop)==1)
				{
					min_num = i;
					min_dist = Length(vec);
				}
			}
		}
    }

    return min_num;
}

int GetAliasCam(vec3_t org, vec3_t out)
{
	int i;

	//TODO: smooth here 
	i = GetNearCam(org);
	if (!i)
	{
		VectorCopy(org,out);
		return 0;
	}

	//sucesfull alias cam!
	VectorCopy(cams[i].org, out);

	return i;
}

void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_right);
	Cvar_RegisterVariable (&chase_active);

	Cvar_RegisterVariable (&chase_pitch);
	Cvar_RegisterVariable (&chase_roll);
	Cvar_RegisterVariable (&chase_yaw);
}

void Chase_Reset (void)
{
	// for respawning and teleporting
//	start position 12 units behind head
}

void vectoangles (vec3_t value1,vec3_t out);;

void Chase_Update (void)
{
//TODO: reformulation 
	
	int		i, camavail;
	float	dist;
	vec3_t	forward, up, right, normal, out, bak;
	vec3_t	dest, stop;
	
	camavail =0; // this need to be set to zero to all 


	// if can't see player, reset
	if (chase_active.value == 1)
		AngleVectors (cl.viewangles, forward, right, up);
	//Tei chase zenit
	else
	if (chase_active.value == 2)
	{
		VectorClear(dest);
		dest[2] = -1; 
		AngleVectors (dest, forward, right, up);	
	}
	else
	{
		camavail = 0;

		//Backup vieworg
		VectorCopy(r_refdef.vieworg, bak);

		if (camavail = GetAliasCam(r_refdef.vieworg, out))
		{
			VectorCopy(out, r_refdef.vieworg);
			AngleVectors (cams[camavail].angle, forward, right, up);
		}
		else
		{
			//Normal chase
			AngleVectors (cl.viewangles, forward, right, up);
		}
	}
	//Tei chase zenit

	
	if (chase_active.value == 4) // absolute values
	{
		chase_dest[0] = chase_right.value;
		chase_dest[1] = chase_back.value;
		chase_dest[2] = chase_up.value;
	}
	else
	{
		for (i=0 ; i<3 ; i++) // calc exact destination
			chase_dest[i] = r_refdef.vieworg[i] 
			- forward[i]*chase_back.value
			- right[i]*chase_right.value;
		chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;
	}
	// find the spot the player is looking at

	if (chase_active.value == 1 )
	{
		VectorMA (r_refdef.vieworg, 4096, forward, dest);
		TraceLine (r_refdef.vieworg, dest, stop, normal);	
	
		// calculate pitch to look at the same spot from camera
		VectorSubtract (stop, r_refdef.vieworg, stop);
		dist = DotProduct (stop, forward);
		if (dist < 1)
			dist = 1;
		//r_refdef.viewangles[PITCH] = -atan(stop[2] / dist) / M_PI * 180;
		r_refdef.viewangles[PITCH] = -stop[2] / dist / M_PI * 180;
	}
	//Tei chase zenit
	else if (chase_active.value == 2 || chase_active.value == 4)
	{
		r_refdef.viewangles[PITCH] = chase_pitch.value;
		r_refdef.viewangles[ROLL] = chase_roll.value;
		r_refdef.viewangles[YAW] = chase_yaw.value;
	}
	else if (chase_active.value == 3)
	{
		r_refdef.viewangles[PITCH] += chase_pitch.value;
		r_refdef.viewangles[ROLL] += chase_roll.value;
		r_refdef.viewangles[YAW] += chase_yaw.value;
	}
	else
	{
		if (camavail)
		{
			VectorSubtract( bak, r_refdef.vieworg ,normal);
			
			vectoangles(normal, r_refdef.viewangles);//out);

			r_refdef.viewangles[PITCH] = - r_refdef.viewangles[PITCH];

			//VectorCopy(out, r_refdef.viewangles);

		}
		else
		{
			VectorMA (r_refdef.vieworg, 4096, forward, dest);
			TraceLine (r_refdef.vieworg, dest, stop, normal);	
		
			// calculate pitch to look at the same spot from camera
			VectorSubtract (stop, r_refdef.vieworg, stop);
			dist = DotProduct (stop, forward);
			if (dist < 1)
				dist = 1;
			r_refdef.viewangles[PITCH] = -atan(stop[2] / dist) / M_PI * 180;
		}

	}
	//Tei chase zenit

	if ( !camavail)
	{
		// Tomaz - Chase Cam Fix Begin
		TraceLine(r_refdef.vieworg, chase_dest, stop, normal);
		if (stop[0] != 0 || stop[1] != 0 || stop[2] != 0)
		{
			// Entar : move the camera away from the wall it's hitting, so we don't see behind it
			VectorAdd(normal, stop, stop);
			VectorCopy(stop, chase_dest);
		}
		// Tomaz - Chase Cam Fix End


		TraceLine(r_refdef.vieworg, chase_dest, stop, normal);

		if (Length(stop) != 0)
			VectorCopy(stop, chase_dest);

		// move towards destination	
		VectorCopy (chase_dest, r_refdef.vieworg);
	}
}

