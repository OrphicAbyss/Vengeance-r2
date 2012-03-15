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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"
#include "glquake.h"
#include "gl_rpart.h"
//#include "gl_refl.h"	// MPO
extern vec3_t	zerodir;
extern int		multitex_go;

extern	model_t	*loadmodel;

char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static int skytexorder[6] = {0, 2, 1, 3, 4, 5};
int		skytex[6];

char	skyname[32];
model_t *skymodel;

int		oldsky = true;
int		R_Skybox = false;

msurface_t	*warpface;

cvar_t r_waterrefl = {"r_waterrefl", "0", true};
cvar_t gl_farclip = {"gl_farclip", "4096", true};

extern cvar_t gl_subdivide_size;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}

	warpface->numPolys++;
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	warpface->numPolys = 0; // mh - auto water trans

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================

// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))
#define toradians 3.1415926535897932384626433832795
// MrG - texture shader stuffs
//Thanx MrG
#define DST_SIZE 16
#define DTS_CAUSTIC_SIZE 128
#define DTS_CAUSTIC_MID 64

unsigned int dst_texture = 0;
unsigned int dst_caustic = 0;
/*
===============
CreateDSTTex

Create the texture which warps texture shaders
===============
*/
void CreateDSTTex()
{
	signed char data[DST_SIZE][DST_SIZE][2];
	signed char data2[DTS_CAUSTIC_SIZE][DTS_CAUSTIC_SIZE][2];
	int x,y;
//	int separation;

	for (x=0;x<DST_SIZE;x++)
		for (y=0;y<DST_SIZE;y++)
		{
			data[x][y][0]=rand()%255-128;
			data[x][y][1]=rand()%255-128;
		}

//	glGenTextures(1,&dst_texture);
	dst_texture = texture_extension_number;
	texture_extension_number++;
	glBindTexture(GL_TEXTURE_2D, dst_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DSDT8_NV, DST_SIZE, DST_SIZE, 0, GL_DSDT_NV, GL_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	
	for (x=0;x<DTS_CAUSTIC_SIZE;x++)
		for (y=0;y<DTS_CAUSTIC_SIZE;y++)
		{
			data2[x][y][0]=sin(((float)x*2.0f/(DTS_CAUSTIC_SIZE-1))*toradians)*128;
			data2[x][y][1]=sin(((float)y*2.0f/(DTS_CAUSTIC_SIZE-1))*toradians)*128;
			//data2[x][y][2]=0;
		}

//	glGenTextures(1,&dst_caustic);
	dst_caustic = texture_extension_number;
	texture_extension_number++;
	glBindTexture(GL_TEXTURE_2D, dst_caustic);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, DTS_CAUSTIC_SIZE, DTS_CAUSTIC_SIZE, 0, GL_RGB, GL_BYTE, data2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DSDT8_NV, DTS_CAUSTIC_SIZE, DTS_CAUSTIC_SIZE, 0, GL_DSDT_NV, GL_BYTE, data2);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

}

//#if 0 // incomplete, new depth sorting code
// MPO (actually i wrote this:)
double calc_wave(GLfloat x, GLfloat y)
{
	return ((int)((x*3+cl.time) * TURBSCALE) & 255/4) + ((int)((y*5+cl.time) * TURBSCALE) & 255/4 );
}

void EmitWaterPolys_original (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;
	vec3_t		nv;	//qmb :water wave

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);
 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glColor4f (1,1,1,r_wateralpha.value);

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];
 
			s = os + turbsin[(int)((ot*0.125+cl.time) * TURBSCALE) & 255];
			s *= (1.0/64);
 
			t = ot + turbsin[(int)((os*0.125+cl.time) * TURBSCALE) & 255];
			t *= (1.0/64);
 
			VectorCopy(v, nv);
 
			if (r_wave.value)
				nv[2] = v[2] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);
 
			glTexCoord2f (s, t);
			glVertex3fv (nv);
		}
		glEnd ();
	}
}

void DoWater_Multi (glpoly_t *p, msurface_t	*fa)
{
	float		args[4] = {0.05f,0.0f,0.0f,0.02f};
	vec3_t		nv;
	int			i;
	float		*v, os, ot, s, ss, t, tt;
	GLenum		error;

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);
//	glShadeModel(GL_SMOOTH); // ?

	/*
	Texture Shader waterwarp
	Shoot this looks fantastic

	WHY texture shaders? because I can!
	- MrG
	*/

	//Texture shader
	if (gl_shader)
	{
		if (!dst_texture)
			CreateDSTTex();
		
		glBindTexture(GL_TEXTURE_2D,dst_texture);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		
		GL_EnableTMU(GL_TEXTURE1_ARB);
		glDisable (GL_BLEND);
		glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		glTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_SHADER_NV);
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (gamemode == GAME_RO && strstr(fa->texinfo->texture->name, "lava"))
	{
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}
	else
	{
		glEnable (GL_BLEND);
		glColor4f (1,1,1,r_wateralpha.value);
	}

	glBegin (GL_POLYGON);
	for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
	{
		os = v[3];
		ot = v[4];

		s = os + turbsin[(int)((ot*0.125+cl.time) * TURBSCALE) & 255];
		s *= (1.0/64);

		ss = os + turbsin[(int)((ot*0.25+(cl.time*2)) * TURBSCALE) & 255];
		ss *= (0.5/64)*(-1);

		t = ot + turbsin[(int)((os*0.125+cl.time) * TURBSCALE) & 255];
		t *= (1.0/64);

		tt = ot + turbsin[(int)((os*0.25+(cl.time*2)) * TURBSCALE) & 255];
		tt *= (0.5/64)*(-1);

		VectorCopy(v, nv);

		if (r_wave.value) // IDEA: if (model in the water) then do the wave
			nv[2] = v[2] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);

		if (multitex_go)
		{
			qglMTexCoord2fARB (GL_TEXTURE0_ARB, s, t);
			if (gl_shader)
				qglMTexCoord2fARB (GL_TEXTURE1_ARB, ss + sin(cl.time/10), tt + cos(cl.time/10));
		}
		else
		{
			glTexCoord2f (s, t);
		}

		glVertex3fv (nv);
	}
	glEnd ();

	glColor4f (1,1,1,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);

	if (gl_shader)
		glDisable(GL_TEXTURE_SHADER_NV);

	GL_DisableTMU(GL_TEXTURE1_ARB);
	GL_SelectTexture(GL_TEXTURE0_ARB);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: after:&r %s\n", gluErrorString(error));

	if (!r_waterrefl.value)
		return;

	// find out which reflection we have that corresponds to the surface that we're drawing	
	for (g_active_refl = 0; g_active_refl < g_num_refl; g_active_refl++) {
		
		// if we find which reflection to bind
		if( fa->plane->normal[0]==waterNormals[g_active_refl][0] &&
			fa->plane->normal[1]==waterNormals[g_active_refl][1] &&
			fa->plane->normal[2]==waterNormals[g_active_refl][2] &&
			fa->plane->dist==g_waterDistance2[g_active_refl]) {

				//GL_SelectTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D,g_tex_num[g_active_refl]);// Reflection texture
				break;
		}
	}


	// if we found a reflective surface correctly, then go ahead and draw it
	if (g_active_refl < g_num_refl) {

		glColor4f		( 1, 1, 1, 0.4f);// add some alpha transparency
		glEnable		( GL_BLEND		);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel	( GL_SMOOTH		);
		glEnable		( GL_POLYGON_OFFSET_FILL );		//to stop z buffer fighting
		glPolygonOffset (-1, -2);

		R_LoadReflMatrix();	

  		// draw reflected water layer on top of regular
// 		for (bp=fa->polys ; bp ; bp=bp->next)
  		{
			GLfloat		vert1[2048];
			
//			p = bp;

			//glBegin (GL_TRIANGLE_FAN);
//			glBegin (GL_POLYGON);

			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
			{
				vert1[0 + (i*3)] = v[0];
				vert1[1 + (i*3)] = v[1];
				vert1[2 + (i*3)] = v[2];

				if (r_wave.value)
					vert1[2 + (i*3)] = vert1[2 + (i*3)] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);
			}

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);

				glTexCoordPointer(3, GL_FLOAT, 0, vert1);
				glVertexPointer(3, GL_FLOAT, 0, vert1);

				glDrawArrays (GL_POLYGON, 0, p->numverts);

		//		glTexCoord3f(v[0], v[1] , v[2] );			
		//		glVertex3f  (v[0], v[1] , v[2] );

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);
//			glEnd (); 
  		}

		R_ClearReflMatrix();

		glDisable ( GL_POLYGON_OFFSET_FILL  );
    }
}

// Entar : for more accurate depth sorting (Thanks to Dukey)
void getCentreOfSurf(msurface_t *surf, vec3_t centre) {
	
	//====================
	vec3_t		min;
	vec3_t		max;
	int			i;
	int			nv;
	glpoly_t	*p;
	float		*v;
	//====================

	//initilise values

	p		= surf->polys;
	v		= p->verts[0];
	nv		= surf->polys->numverts;

	min[0]	= v[0]; min[1] = v[1]; min[2] = v[2];
	max[0]	= v[0]; max[1] = v[1]; max[2] = v[2];		
		
	for ( ; p; p = p->chain ) {
			for (i=0 ; i< nv; i++, v+= VERTEXSIZE) {
				
				if(v[0] > max[0])	max[0] = v[0];
				if(v[1] > max[1])	max[1] = v[1];
				if(v[2] > max[2])	max[2] = v[2];
				if(v[0] < min[0])	min[0] = v[0];
				if(v[1] < min[1])	min[1] = v[1];
				if(v[2] < min[2])	min[2] = v[2];
			}
	}

	//get centre position ..
	centre[0] = (min[0] + max[0]) /2;
	centre[1] = (min[1] + max[1]) /2;
	centre[2] = (min[2] + max[2]) /2;
}

void EmitWaterPolysMulti (msurface_t *fa)
{
	glpoly_t	*p;
//	float		*v;
//	float		support_vertex_shaders = false;
	vec3_t		spot;	

	glDisable ( GL_POLYGON_OFFSET_FILL  );

	if (g_drawing_refl) return;	// we don't want any water drawn while we are doing our reflection

	/*
	args[0] = gl_test0.value;
	args[1] = gl_test1.value;
	args[2] = gl_test2.value;
	args[3] = gl_test3.value;
	*/

	for (p=fa->polys ; p ; p=p->next)
	{
		if (r_depthsort.value)
		{
			getCentreOfSurf(fa, spot);
			RQ_AddDistReorder(DoWater_Multi, p, fa, spot);
		}
		else
			DoWater_Multi(p, fa);
	}

	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f (1,1,1,1);
}

// MPO : this is my version...
void EmitWaterPolysMulti_old (msurface_t *fa) {

	//==============================
	glpoly_t	*p;
	glpoly_t	*bp;
	float		*v;
	int			i;
	//==============================

	glDisable ( GL_POLYGON_OFFSET_FILL  );

	if (g_drawing_refl) return;	// we don't want any water drawn while we are doing our reflection

//	if(1) {

//	if (gl_shader)
//		EmitWaterPolys(fa);
//	else
		EmitWaterPolys_original(fa);
		//return;
//	}

	if (!r_waterrefl.value)
		return;

	// find out which reflection we have that corresponds to the surface that we're drawing	
	for (g_active_refl = 0; g_active_refl < g_num_refl; g_active_refl++) {
		
		// if we find which reflection to bind
		if( fa->plane->normal[0]==waterNormals[g_active_refl][0] &&
			fa->plane->normal[1]==waterNormals[g_active_refl][1] &&
			fa->plane->normal[2]==waterNormals[g_active_refl][2] &&
			fa->plane->dist==g_waterDistance2[g_active_refl]) {

				//GL_SelectTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D,g_tex_num[g_active_refl]);// Reflection texture
				break;
		}
	}


	// if we found a reflective surface correctly, then go ahead and draw it
	if (g_active_refl < g_num_refl) {

		glColor4f		( 1, 1, 1, 0.4f);// add some alpha transparency
		glEnable		( GL_BLEND		);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel	( GL_SMOOTH		);
		glEnable		( GL_POLYGON_OFFSET_FILL );		//to stop z buffer fighting
		glPolygonOffset( -1, -2);

		R_LoadReflMatrix();	

  		// draw reflected water layer on top of regular
  		for (bp=fa->polys ; bp ; bp=bp->next)
  		{
			GLfloat		vert1[1024];
			
			p = bp;

			//glBegin (GL_TRIANGLE_FAN);
//			glBegin (GL_POLYGON);

			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
			{
				vert1[0 + (i*3)] = v[0];
				vert1[1 + (i*3)] = v[1];
				vert1[2 + (i*3)] = v[2];
			}

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnableClientState(GL_VERTEX_ARRAY);

				glTexCoordPointer(3, GL_FLOAT, 0, vert1);
				glVertexPointer(3, GL_FLOAT, 0, vert1);

				glDrawArrays (GL_POLYGON, 0, p->numverts);

		//		glTexCoord3f(v[0], v[1] , v[2] );			
		//		glVertex3f  (v[0], v[1] , v[2] );

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);
//			glEnd (); 
  		}

		R_ClearReflMatrix();

		glDisable ( GL_POLYGON_OFFSET_FILL  );
    }

	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f (1,1,1,1);
	
}

/*
================
R_DrawWaterChain
================
*/
void R_DrawWaterChain (msurface_t *s)
{
	msurface_t *removelink;
//	float	*v;
//	vec3_t	nv;

	if ((r_wateralpha.value == 0)||!s)
		return;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	while(s) {
		EmitWaterPolysMulti (s);

		removelink = s;
		s = s->texturechain;
		removelink->texturechain = NULL;
	}
}
//#endif
#if 0 // old code, bad depth sorting - fixed code above
/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
JHL:HACK; modified to render two layers...
=============
*
void EmitWaterPolysMulti (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		support_vertex_shaders = 0;
	float		os, ot;
	vec3_t		nv;	//qmb :water wave
	//Texture shader
	float		args[4] = {0.05f,0.0f,0.0f,0.02f};

	/*
	args[0] = gl_test0.value;
	args[1] = gl_test1.value;
	args[2] = gl_test2.value;
	args[3] = gl_test3.value;
	*

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);
//	glShadeModel(GL_SMOOTH); // ?

	/*
	Texture Shader waterwarp
	Shoot this looks fantastic

	WHY texture shaders? because I can!
	- MrG
	*
	if (gl_shader) {
		if (!dst_texture)
			CreateDSTTex();
		glBindTexture(GL_TEXTURE_2D,dst_texture);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		
		GL_EnableTMU(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		glTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_SHADER_NV);
	}else {
		GL_EnableTMU(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);

		if (gl_combine)
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
		else
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glColor4f (1,1,1,r_wateralpha.value);

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			VectorCopy(v, nv);

//			if (r_wave.value) // IDEA: if (model in the water) then do the wave
//				nv[2] = v[2] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);

			//qglMTexCoord2fARB (GL_TEXTURE0_ARB, s, t); // old code
			//qglMTexCoord2fARB (GL_TEXTURE1_ARB, ss + cl.time/10, tt + cl.time/10);

			qglMTexCoord2fARB (GL_TEXTURE0_ARB, ((os + sin(ot*0.125+cl.time)) * (1.0/64)), ((ot + sin(os*0.125+cl.time)) * (1.0/64)));
			qglMTexCoord2fARB (GL_TEXTURE1_ARB, ((os + sin(ot*0.25+cl.time*2)) * (0.5/64)*(-1)) + cl.time/10, ((ot + sin(os*0.25+cl.time*2)) * (0.5/64)*(-1)) + cl.time/10);

			glVertex3fv (nv);
		}
		glEnd ();
	}

	glColor4f (1,1,1,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);

	if (gl_shader) { // MrG - texture shader waterwarp
		glDisable(GL_TEXTURE_SHADER_NV);
	}

	GL_DisableTMU(GL_TEXTURE1_ARB);
	GL_SelectTexture(GL_TEXTURE0_ARB);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

}*/

// MPO (actually i wrote this:)
double calc_wave(GLfloat x, GLfloat y)
{
	return ((int)((x*3+cl.time) * TURBSCALE) & 255/4) + ((int)((y*5+cl.time) * TURBSCALE) & 255/4 );
}

void EmitWaterPolys_original (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;
	vec3_t		nv;	//qmb :water wave

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);
 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (gamemode == GAME_RO && strstr(fa->texinfo->texture->name, "lava"))
	{
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}
	else
	{
		glEnable (GL_BLEND);
		glColor4f (1,1,1,r_wateralpha.value);
	}

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];
 
			s = os + turbsin[(int)((ot*0.125+cl.time) * TURBSCALE) & 255];
			s *= (1.0/64);
 
			t = ot + turbsin[(int)((os*0.125+cl.time) * TURBSCALE) & 255];
			t *= (1.0/64);
 
			VectorCopy(v, nv);
 
			if (r_wave.value)
				nv[2] = v[2] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);
 
			glTexCoord2f (s, t);
			glVertex3fv (nv);
		}
		glEnd ();
	}
}

void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
//	float		support_vertex_shaders = 0;
	float		os, ot;
	vec3_t		nv;	//qmb :water wave
	//Texture shader
	float		args[4] = {0.05f,0.0f,0.0f,0.02f};

	/*
	args[0] = gl_test0.value;
	args[1] = gl_test1.value;
	args[2] = gl_test2.value;
	args[3] = gl_test3.value;
	*/

	GL_SelectTexture(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);
//	glShadeModel(GL_SMOOTH); // ?

	/*
	Texture Shader waterwarp
	Shoot this looks fantastic

	WHY texture shaders? because I can!
	- MrG
	*/

	if (gl_shader)
	{
		if (!dst_texture)
			CreateDSTTex();
		glBindTexture(GL_TEXTURE_2D,dst_texture);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		
		GL_EnableTMU(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);

		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		glTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glEnable(GL_TEXTURE_SHADER_NV);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (gamemode == GAME_RO && strstr(fa->texinfo->texture->name, "lava"))
	{
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}
	else
	{
		glEnable (GL_BLEND);
		glColor4f (1,1,1,r_wateralpha.value);
	}

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			VectorCopy(v, nv);

//			if (r_wave.value) // IDEA: if (model in the water) then do the wave
//				nv[2] = v[2] + r_wave.value *sin(v[0]*0.02+cl.time)*sin(v[1]*0.02+cl.time)*sin(v[2]*0.02+cl.time);

			//qglMTexCoord2fARB (GL_TEXTURE0_ARB, s, t); // old code
			//qglMTexCoord2fARB (GL_TEXTURE1_ARB, ss + cl.time/10, tt + cl.time/10);

			qglMTexCoord2fARB (GL_TEXTURE0_ARB, ((os + sin(ot*0.125+cl.time)) * (1.0/64)), ((ot + sin(os*0.125+cl.time)) * (1.0/64)));
			qglMTexCoord2fARB (GL_TEXTURE1_ARB, ((os + sin(ot*0.25+cl.time*2)) * (0.5/64)*(-1)) + cl.time/10, ((ot + sin(os*0.25+cl.time*2)) * (0.5/64)*(-1)) + cl.time/10);

			glVertex3fv (nv);
		}
		glEnd ();
	}

	glColor4f (1,1,1,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);

	if (gl_shader)
		glDisable(GL_TEXTURE_SHADER_NV);

	GL_DisableTMU(GL_TEXTURE1_ARB);
	GL_SelectTexture(GL_TEXTURE0_ARB);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

}


// MPO : this is my version...
void EmitWaterPolysMulti (msurface_t *fa) {

	//==============================
	glpoly_t	*p;
	glpoly_t	*bp;
	float		*v;
	int			i;
	//==============================

	glDisable ( GL_POLYGON_OFFSET_FILL  );

	if (g_drawing_refl) return;	// we don't want any water drawn while we are doing our reflection

//	if(1) {

	if (gl_shader)
		EmitWaterPolys(fa);
	else
		EmitWaterPolys_original(fa);
		//return;
//	}

	if (gamemode == GAME_RO)
		if (strstr(fa->texinfo->texture->name, "lava"))
			return;

	if (!r_waterrefl.value)
		return;

	// find out which reflection we have that corresponds to the surface that we're drawing	
	for (g_active_refl = 0; g_active_refl < g_num_refl; g_active_refl++) {
		
		// if we find which reflection to bind
		if( fa->plane->normal[0]==waterNormals[g_active_refl][0] &&
			fa->plane->normal[1]==waterNormals[g_active_refl][1] &&
			fa->plane->normal[2]==waterNormals[g_active_refl][2] &&
			fa->plane->dist==g_waterDistance2[g_active_refl]) {

				//GL_SelectTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D,g_tex_num[g_active_refl]);// Reflection texture
				break;
		}
	}


	// if we found a reflective surface correctly, then go ahead and draw it
	if (g_active_refl < g_num_refl) {

		glColor4f		( 1, 1, 1, 0.4f);// add some alpha transparency
		glEnable		( GL_BLEND		);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel	( GL_SMOOTH		);
		glEnable		( GL_POLYGON_OFFSET_FILL );		//to stop z buffer fighting
		glPolygonOffset( -1, -2);

		R_LoadReflMatrix();	

  		// draw reflected water layer on top of regular
  		for (bp=fa->polys ; bp ; bp=bp->next)
  		{
			p = bp;

			//glBegin (GL_TRIANGLE_FAN);
			glBegin (GL_POLYGON);
			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE) {

				glTexCoord3f(v[0], v[1] , v[2] );			
				glVertex3f  (v[0], v[1] , v[2] );
	
			}
			glEnd (); 
  		}

		R_ClearReflMatrix();

		glDisable ( GL_POLYGON_OFFSET_FILL  );
    }

	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f (1,1,1,1);
	
}

/*
================
R_DrawWaterChain
================
*/
void R_DrawWaterChain (msurface_t *s)
{
	msurface_t *removelink;
	float	*v;
	vec3_t	nv;

	if ((r_wateralpha.value == 0)||!s)
		return;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	while(s) {
		v = s->polys->verts[0];
		VectorCopy(v, nv);

		if (r_depthsort.value)
			RQ_AddDistReorder(EmitWaterPolysMulti, s, NULL, nv); // depth sorting
		else
			EmitWaterPolysMulti (s);

		removelink = s;
		s = s->texturechain;
		removelink->texturechain = NULL;
	}
}
#endif

cvar_t	r_skyfactor = {"r_skyfactor", "2", true};

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolysMulti (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i, j=0;
	float	s, ss, t, tt;
	vec3_t	dir;
	float	length;
	
	GL_SelectTexture(GL_TEXTURE0_ARB);
	glColor3f(1,1,1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_texturenum);

	GL_EnableTMU(GL_TEXTURE1_ARB);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBindTexture(GL_TEXTURE_2D,fa->texinfo->texture->gl_skynum);

	if (r_skyfactor.value <= 0)
	{
		Con_Print("Invalid r_skyfactor value, resetting to 1\n");
		Cvar_Set("r_skyfactor", "1");
	}

	for (p=fa->polys ; p ; p=p->next, j++)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			//VectorNormalize (dir);
			dir[2] *= 8*r_skyfactor.value;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (realtime*8/r_skyfactor.value + dir[0]) * (1.0/128);
			t = (realtime*8/r_skyfactor.value + dir[1]) * (1.0/128);

			ss = (realtime*16/r_skyfactor.value + dir[0]) * (1.0/128);
			tt = (realtime*16/r_skyfactor.value + dir[1]) * (1.0/128);

			if (multitex_go)
			{
				qglMTexCoord2fARB (GL_TEXTURE0_ARB, s, t);
				qglMTexCoord2fARB (GL_TEXTURE1_ARB, ss, tt);
			}
			else
				glTexCoord2f(s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}

	GL_DisableTMU(GL_TEXTURE1_ARB);
	GL_SelectTexture(GL_TEXTURE0_ARB);
}

/*
==================
R_LoadSky
==================
*/
int R_LoadSky (char *newname)
{
	extern void GL_UploadLightmap (void);
	extern void BuildSurfaceDisplayList (model_t *m, msurface_t *fa);
	extern void GL_CreateSurfaceLightmap (msurface_t *surf);
	extern qboolean vid_initialized;

	int		i;
	char	name[96];
	char	dirname[64];

	sprintf (skyname,"%s",newname);

	// this way, it won't try to load when it can't, but the skyname will be set so it does when it can
	if (!vid_initialized)
		return true; // no reason to think otherwise, yet

	oldsky=false;
	skymodel = Mod_ForName(skyname, false);
	if (skymodel)
	{
		//GL_BuildLightmaps ();
		for (i=0 ; i<skymodel->numsurfaces ; i++)
		{
//			if ( skymodel->surfaces[i].flags & SURF_DRAWTURB )
//				continue;
			GL_CreateSurfaceLightmap (skymodel->surfaces + i);
			BuildSurfaceDisplayList (skymodel, skymodel->surfaces + i);
			GL_UploadLightmap();

		}
		Con_Printf("Loaded sky model: %s\n",newname);
		return true;
	}

	sprintf (dirname,"gfx/env/");
	sprintf (name, "%s%s%s", dirname, skyname, suf[0]);

	//find where the sky is
	//some are in /env others /gfx/env
	//some have skyname?? others skyname_??
	skytex[0]=GL_LoadTexImage(name,false,false);
	if (skytex[0]==0)
	{
		sprintf (dirname,"env/");
		sprintf (name, "%s%s%s", dirname, skyname, suf[0]);
		skytex[0]=GL_LoadTexImage(name,false,false);
		if (skytex[0]==0)
		{
			sprintf (skyname,"%s_",skyname);
			sprintf (name, "%s%s%s", dirname, skyname, suf[0]);
			skytex[0]=GL_LoadTexImage(name,false,false);
			if (skytex[0]==0)
			{
				sprintf (dirname,"gfx/env/");
				sprintf (name, "%s%s%s", dirname, skyname, suf[0]);
				skytex[0]=GL_LoadTexImage(name,false,false);
				if (skytex[0]==0)
				{
					oldsky=true;
					return false;
				}
			}
		}
	}

	for (i=1 ; i<6 ; i++)
	{

		sprintf (name, "gfx/env/%s%s", skyname, suf[i]);

		skytex[i]=GL_LoadTexImage(name,false,false);
		if (skytex[i]==0)
		{
			oldsky=true;
			return false;
		}
	}
	return true;
}

// LordHavoc: added LoadSky console command
void R_LoadSky_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		if (skyname[0])
			Con_Printf("current sky: %s\n", skyname);
		else
			Con_Printf("no skybox has been set\n");
		break;
	case 2:
		if (R_LoadSky(Cmd_Argv(1)))
		{
			if (skyname[0])
				Con_Printf("skybox set to %s\n", skyname);
			else
				Con_Printf("skybox disabled\n");
		}
		else
			Con_Printf("failed to load skybox %s\n", Cmd_Argv(1));
		break;
	default:
		Con_Printf("usage: loadsky skyname\n");
		break;
	}
}

// LordHavoc: added LoadSky console command
void R_CurrentCoord_f (void)
{
	Con_Printf("Current Position: %f,%f,%f\n", r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
}

void EmitFlatPoly (msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
			glVertex3fv (v);
		glEnd ();
	}
}

static vec3_t skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

// 1 = s, 2 = t, 3 = 2048
static int	st_to_vec[6][3] = {
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

static int	vec_to_st[6][3] = {
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

static float skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon (int nump, vec3_t vecs) {
	int i,j, axis;
	vec3_t v, av;
	float s, t, dv, *vp;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3) {
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage) {
	float *norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qboolean front, back;
	int sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");
	if (stage == 6) {	
		// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		d = DotProduct (v, norm);
		if (d > ON_EPSILON) {
			front = true;
			sides[i] = SIDE_FRONT;
		} else if (d < -ON_EPSILON) {
			back = true;
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if (!front || !back) {	
		// not clipped
		ClipSkyPolygon (nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		switch (sides[i]) {
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j = 0; j < 3; j++) {
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon (newc[1], newv[1][0], stage + 1);
}

void R_AddSkyBoxSurface (msurface_t *fa) {
	int i;
	vec3_t verts[MAX_CLIP_VERTS];
	glpoly_t *p;

	// calculate vertex values for sky box
	for (p = fa->polys; p; p = p->next) {
		for (i = 0; i < p->numverts; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}

void R_ClearSkyBox (void) {
	int i;

	for (i = 0; i < 6; i++) {
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

void MakeSkyVec (float s, float t, int axis) {
	vec3_t v, b;
	int j, k, farclip;

	farclip = max((int) gl_farclip.value, 4096);
	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for (j = 0; j < 3; j++) {
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	s = bound(1.0 / 512, s, 511.0 / 512);
	t = bound(1.0 / 512, t, 511.0 / 512);

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

void R_DrawSkyBox (msurface_t *skychain) {
	int i;
	msurface_t *fa;

//	if (!skychain)
//		return;

	R_ClearSkyBox();
	for (fa = skychain; fa; fa = fa->texturechain)
		R_AddSkyBoxSurface (fa);

//	GL_DisableMultitexture();
//	glDisable (GL_TEXTURE_2D);
	GL_SelectTexture (GL_TEXTURE0_ARB);

	for (i = 0; i < 6; i++) {
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

//		GL_Bind (skyboxtextures + skytexorder[i]);
		glBindTexture(GL_TEXTURE_2D, skytex[skytexorder[i]]);

		glBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		glEnd ();
	}

	glDisable(GL_TEXTURE_2D);
	glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_ONE);

	for (fa = skychain; fa; fa = fa->texturechain)
		EmitFlatPoly (fa);

	
	glEnable (GL_TEXTURE_2D);
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	skychain = NULL;
//	skychain_tail = &skychain;
}


/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*removelink;
	
	if (gl_fogglobal.value)
		glDisable(GL_FOG);

	if (!oldsky)
	{
		R_DrawSkyBox(s);
	}
	else
	{
		while (s)
		{
			EmitSkyPolysMulti (s);

			removelink = s;
			s = s->texturechain;
			removelink->texturechain = NULL;	
		}
	}

	if (gl_fogglobal.value)
		glEnable(GL_FOG);
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	int			i, j, p;
	byte		*src;
	unsigned 	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	char		name[64];

	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i*128) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	((byte *)&transpix)[0] = r/(128*128);
	((byte *)&transpix)[1] = g/(128*128);
	((byte *)&transpix)[2] = b/(128*128);
	((byte *)&transpix)[3] = 0;

	sprintf(name,"%ssolid",mt->name);
	mt->gl_texturenum = GL_LoadTexture (name, 128, 128, (byte *)&trans[0], false, false, 4);

	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24table[p];
		}

	sprintf(name,"%salpha",mt->name);
	mt->gl_skynum = GL_LoadTexture (name, 128, 128, (byte *)&trans[0], false, true, 4);
}
