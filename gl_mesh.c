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
// gl_mesh.c: triangle model functions

#include "quakedef.h"
#include "gl_md3.h"
#include "gl_rpart.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

extern	ls_t	*partscript;

extern	int multitex_go;

model_t		*aliasmodel;
aliashdr_t	*paliashdr;

qboolean	used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
int		commands[8192];
int		numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int		vertexorder[8192];
int		numorder;

int		allverts, alltris;

int		stripverts[128];
int		striptris[128];
int		stripcount;

int			vertshade = true;
qboolean	extra_light = false;

vec3_t		lightVector;
vec3_t		lightRegular = {0, 1, 2};

#define		EF_NOMD3SHADOWFLAG		"noMD3shadowflag"
#define		EF_QUADSHELLFLAG		"quadshellflag" // note: when using this flag on moving/animating
													// objects, a on/off alternating effect
													// will sometimes set in.  Better on still objects.
#define		EF_FIREFLAG				"fireflag"
#define		EF_NOSHADEFLAG			"noshadeflag"
extern qboolean model_checkValue (char *key, char *value);
extern void loadmodelScript(char *filename);
extern char *modelScript;


cvar_t	r_dirlighting = {"r_dirlighting", "1", true};
cvar_t	r_quadshell = {"r_quadshell", "1", true};


/*
================
StripLength
================
*/
int	StripLength (int starttri, int startv)
{
	int			m1, m2;
	int			j;
	mtriangle_t	*last, *check;
	int			k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[ (k+2)%3 ];
			else
				m1 = check->vertindex[ (k+2)%3 ];

			stripverts[stripcount+2] = check->vertindex[ (k+2)%3 ];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int	FanLength (int starttri, int startv)
{
	int		m1, m2;
	int		j;
	mtriangle_t	*last, *check;
	int		k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+0)%3];
	m2 = last->vertindex[(startv+2)%3];


	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[ (k+2)%3 ];

			stripverts[stripcount+2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris (void)
{
	int		i, j, k;
	int		startv;
	float	s, t;
	int		len, bestlen, besttype;
	int		bestverts[1024];
	int		besttris[1024];
	int		type;

	//
	// build tristrips
	//
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));
	for (i=0 ; i<pheader->numtris ; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0 ; type < 2 ; type++)
//	type = 1;
		{
			for (startv =0 ; startv < 3 ; startv++)
			{
				if (type == 1)
					len = StripLength (i, startv);
				else
					len = FanLength (i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j=0 ; j<bestlen+2 ; j++)
						bestverts[j] = stripverts[j];
					for (j=0 ; j<bestlen ; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j=0 ; j<bestlen ; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen+2);
		else
			commands[numcommands++] = -(bestlen+2);

		for (j=0 ; j<bestlen+2 ; j++)
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;	// on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*(float *)&commands[numcommands++] = s;
			*(float *)&commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Con_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}


/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	int		i, j;
	int			*cmds;
	trivertx_t	*verts;
	//char	cache[MAX_QPATH], fullpath[MAX_OSPATH];
	//FILE	*f;

	aliasmodel = m;
	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	/*
	NEVER LOOK FOR CACHED VERSION
	//
	// look for a cached version
	//
	strcpy (cache, "glquake/");
	COM_StripExtension (m->name+strlen("progs/"), cache+strlen("glquake/"));
	strcat (cache, ".ms2");

	COM_FOpenFile (cache, &f);	
	if (f)
	{
		fread (&numcommands, 4, 1, f);
		fread (&numorder, 4, 1, f);
		fread (&commands, numcommands * sizeof(commands[0]), 1, f);
		fread (&vertexorder, numorder * sizeof(vertexorder[0]), 1, f);
		fclose (f);
	}
	else*/
	{
		//
		// build it from scratch
		//
		
		// **QMB REM**
		//dont print stuff
		//Con_Printf ("meshing %s...\n",m->name);

		BuildTris ();		// trifans or lists

		/*
		// **QMB REM**
		//
		// save out the cached version
		//

		sprintf (fullpath, "%s/%s", com_gamedir, cache);
		f = fopen (fullpath, "wb");
		if (f)
		{
			fwrite (&numcommands, 4, 1, f);
			fwrite (&numorder, 4, 1, f);
			fwrite (&commands, numcommands * sizeof(commands[0]), 1, f);
			fwrite (&vertexorder, numorder * sizeof(vertexorder[0]), 1, f);
			fclose (f);
		}
		*/
	}


	// save the data out

	paliashdr->poseverts = numorder;

	cmds = Hunk_Alloc (numcommands * 4);
	paliashdr->commands = (byte *)cmds - (byte *)paliashdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_Alloc (paliashdr->numposes * paliashdr->poseverts 
		* sizeof(trivertx_t) );
	paliashdr->posedata = (byte *)verts - (byte *)paliashdr;
	for (i=0 ; i<paliashdr->numposes ; i++)
		for (j=0 ; j<numorder ; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}

/*
=============================================================

  ALIAS MODELS DRAWING

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;

//qmb :entity lighting
// CSL - epca@powerup.com.au
//float   shadelight[4];
// CSL
//float   ambientlight;
extern vec3_t lightcolor; // LordHavoc: .lit support to the definitions at the top

     
// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];


// fenix@io.com: model animation interpolation
int lastposenum0; //qmb :interpolation
int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
GLfloat	vert[MAXALIASVERTS*3];
GLfloat	texture[MAXALIASVERTS*2];
GLfloat	texture2[MAXALIASVERTS];
GLfloat	normal[MAXALIASVERTS*3];
GLfloat	color[MAXALIASVERTS*4];
GLfloat	color2[MAXALIASVERTS*4];

void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, qboolean shell, float setcolors[4])
{
	float 	l;
	vec3_t	colour;
	trivertx_t	*verts;
	int		*order;
	int		count, i, backwards;
	float		shade;
	
	//cell shading
	extern	unsigned int	celtexture;
	extern	unsigned int	vertextexture;

	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);
	
	//	while (1)
	for (;;)
	{
		count = *order++;
		if (!count)
			break;

		if (count < 0)
		{
			count = -count;
			backwards = true;
		}
		else
			backwards = false;

		for (i=0; i < count; i++)
		{
			vec3_t	normal1;

			vert[i*3] = verts->v[0];
			vert[1 + (i*3)] = verts->v[1];
			vert[2 + (i*3)] = verts->v[2];

			normal[i*3] = (r_avertexnormals[verts->lightnormalindex][0]);
			normal[1 + (i*3)] = (r_avertexnormals[verts->lightnormalindex][1]);
			normal[2 + (i*3)] = (r_avertexnormals[verts->lightnormalindex][2]);

			normal1[0] = normal[0 + (i*3)];
			normal1[1] = normal[1 + (i*3)];
			normal1[2] = normal[2 + (i*3)];

			if (!shell)
			{
				if (r_dirlighting.value && vertshade)
				{
					shade = 0;
					if (extra_light)
					{
						shade = DotProduct (lightVector, normal1);
						if (shade < 0)
							shade = 0;
					}
					shade += DotProduct(lightRegular, normal1);
					if (shade < 0)
						shade = 0;
					colour[0] = colour[1] = colour[2] = shade * (r_vertexshading.value/10) + (1 - (r_vertexshading.value/10));
					VectorMultiply(setcolors, colour, colour);
				}
				else
				{
					l = shadedots[verts->lightnormalindex];
					VectorScale(setcolors, l, colour);
				}
				color[(i*4)] = colour[0];
				color[1 + (i*4)] = colour[1];
				color[2 + (i*4)] = colour[2];
				color[3 + (i*4)] = setcolors[3];
			}
			else
			{
				color[(i*4)] = 1;
				color[1 + (i*4)] = 1;
				color[2 + (i*4)] = 1;
				color[3 + (i*4)] = 1;

				vert[i*3] += normal[i*3]*2;
				vert[1 + (i*3)] += normal[1+(i*3)]*2;
				vert[2 + (i*3)] += normal[2+(i*3)]*2;
			}

			color2[(i*4)] = colour[0];
			color2[1 + (i*4)] = colour[1];
			color2[2 + (i*4)] = colour[2];
			color2[3 + (i*4)] = setcolors[3];
			verts++;

			if (!shell)
			{
				texture[i*2] = ((float *)order)[0];
				texture[1+(i*2)] = ((float *)order)[1];
			}
			else
			{
				texture[i*2] = ((float *)order)[0] + realtime*slowmo.value*2;
				texture[1+(i*2)] = ((float *)order)[1] + realtime*slowmo.value*2;
			}
			order += 2;

			if (r_celshading.value && vertshade)
				texture2[i] = bound(0, DotProduct(shadevector, normal1), 1);
		}

		//if (colour[0] > 1)	colour[0] = 1;
		//if (colour[1] > 1)	colour[1] = 1;
		//if (colour[2] > 1)	colour[2] = 1;

		//		if ((!shell) && r_vertexshading.value)
		//			VectorScale(colour, (1 - (r_vertexshading.value / 10)) + 0.3f, colour);

		//if ((!shell)&&(!cell)) glColor3f (colour[0], colour[1], colour[2]);
		//		if ((!shell)&&(!cell)) glColor4f (colour[0], colour[1], colour[2], alpha);

		if (backwards == true)
			glDrawArrays (GL_TRIANGLE_FAN, 0, count);
		else
			glDrawArrays (GL_TRIANGLE_STRIP, 0, count);
	}
}

void GL_DrawAliasFrameNew (aliashdr_t *paliashdr, int posenum, qboolean shell)
{
	//float 	l, colour[3];
	//trivertx_t	*verts;
	//int		*order;
	//int		count;

	float	*texcoords;
	byte	*vertices;
	int		*indecies;


	lastposenum = posenum;

	texcoords = (float *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies +4);
	vertices = (byte *)((byte *)(paliashdr + paliashdr->posedata + posenum * paliashdr->poseverts));

	glVertexPointer(3, GL_BYTE, 1, vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,paliashdr->numtris*3,GL_UNSIGNED_INT,indecies);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	/*
	glBegin (GL_TRIANGLES);
	do
	{

		// normals and vertexes come from the frame list
		//qmb :alias coloured lighting
		l = shadedots[verts->lightnormalindex];
		VectorScale(lightcolor, l, colour);
		//if (colour[0] > 1)	colour[0] = 1;
		//if (colour[1] > 1)	colour[1] = 1;
		//if (colour[2] > 1)	colour[2] = 1;

		if ((!shell)&&(!cell)) glColor3f (colour[0], colour[1], colour[2]);

		glNormal3fv(&r_avertexnormals[verts->lightnormalindex][0]);

		if (!shell){
			// texture coordinates come from the draw list
			glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
		}else{
			glTexCoord2f (((float *)order)[0] + realtime*2, ((float *)order)[1] + realtime*2);
			glVertex3f (r_avertexnormals[verts->lightnormalindex][0] * 5 + verts->v[0],
						r_avertexnormals[verts->lightnormalindex][1] * 5 + verts->v[1],
						r_avertexnormals[verts->lightnormalindex][2] * 5 + verts->v[2]);
		}
		order += 2;
		verts++;
	} while (--count);

	glEnd ();*/
}

/*
=============
GL_DrawAliasBlendedFrame

fenix@io.com: model animation interpolation
=============
*/
void GL_DrawAliasBlendedFrame (aliashdr_t *paliashdr, int pose1, int pose2, float blend, qboolean shell, float setcolors[4] )
{
	float       L2 = 9999;
	trivertx_t* verts1;
	trivertx_t* verts2;
	int*        order;
	int         count, backwards, i;
	vec3_t      colour;

	//shell and new blending
	vec3_t		iblendshell;
	vec3_t		blendshell;
	float		iblend;

	lastposenum0 = pose1;
	lastposenum  = pose2;

	verts1  = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2  = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	//LH: shell blending
	iblend = 1.0 - blend;
	if (shell)
	{
		iblendshell[0] = iblend * 5;
		iblendshell[1] = iblend * 5;
		iblendshell[2] = iblend * 5;
		blendshell[0] = blend * 5;
		blendshell[1] = blend * 5;
		blendshell[2] = blend * 5;
	}

	for (;;)
	{
		float		shade;

		count = *order++;
		if (!count)
			break;
		
		if (count < 0)
		{
			count = -count;
			backwards = true;
		}
		else
			backwards = false;

		for (i=0; i < count; i++)
		{
			vec3_t	normal1;

			vert[i*3] = verts1->v[0] * iblend + verts2->v[0] * blend;
			vert[1 + (i*3)] = verts1->v[1] * iblend + verts2->v[1] * blend;
			vert[2 + (i*3)] = verts1->v[2] * iblend + verts2->v[2] * blend;

			normal[i*3] = (r_avertexnormals[verts1->lightnormalindex][0] * iblend + r_avertexnormals[verts2->lightnormalindex][0] * blend);
			normal[1 + (i*3)] = (r_avertexnormals[verts1->lightnormalindex][1] * iblend + r_avertexnormals[verts2->lightnormalindex][1] * blend);
			normal[2 + (i*3)] = (r_avertexnormals[verts1->lightnormalindex][2] * iblend + r_avertexnormals[verts2->lightnormalindex][2] * blend);

			normal1[0] = normal[0 + (i*3)];
			normal1[1] = normal[1 + (i*3)];
			normal1[2] = normal[2 + (i*3)];

			if (!shell)
			{
				if (r_dirlighting.value && vertshade)
				{
					shade = 0;
					if (extra_light)
					{
						shade = DotProduct (lightVector, normal1);
						if (shade < 0)
							shade = 0;
					}
					shade += DotProduct(lightRegular, normal1);
					if (shade < 0)
						shade = 0;
					colour[0] = colour[1] = colour[2] = shade * (r_vertexshading.value/10) + (1 - (r_vertexshading.value/10));
				}
				else
					colour[0] = colour[1] = colour[2] = shadedots[verts1->lightnormalindex] * iblend + shadedots[verts2->lightnormalindex] * blend;
				colour[0] *= setcolors[0];
				colour[1] *= setcolors[1];
				colour[2] *= setcolors[2];
				color[(i*4)] = colour[0];
				color[1 + (i*4)] = colour[1];
				color[2 + (i*4)] = colour[2];
				color[3 + (i*4)] = setcolors[3];
			}
			else
			{
				color[(i*4)] = 1;
				color[1 + (i*4)] = 1;
				color[2 + (i*4)] = 1;
				color[3 + (i*4)] = 1;

				vert[i*3] += normal[i*3]*2;
				vert[1 + (i*3)] += normal[1+(i*3)]*2;
				vert[2 + (i*3)] += normal[2+(i*3)]*2;
			}

			VectorScale(colour, (1 - (r_vertexshading.value / 10)) + 0.3f, colour);
			color2[(i*4)] = colour[0];
			color2[1 + (i*4)] = colour[1];
			color2[2 + (i*4)] = colour[2];
			color2[3 + (i*4)] = setcolors[3];
			verts1++;
			verts2++;

			if (!shell)
			{
				texture[i*2] = ((float *)order)[0];
				texture[1+(i*2)] = ((float *)order)[1];
			}
			else
			{
				texture[i*2] = ((float *)order)[0] + realtime*slowmo.value*2;
				texture[1+(i*2)] = ((float *)order)[1] + realtime*slowmo.value*2;
			}
			order += 2;

			if (r_celshading.value && vertshade)
				texture2[i] = bound(0, DotProduct(shadevector, normal1), 1);
		}

		//if (colour[0] > 1)	colour[0] = 1;
		//if (colour[1] > 1)	colour[1] = 1;
		//if (colour[2] > 1)	colour[2] = 1;

//		if ((!shell) && r_vertexshading.value)
//			VectorScale(colour, (1 - (r_vertexshading.value / 10)) + 0.3f, colour);

		//if ((!shell)&&(!cell)) glColor3f (colour[0], colour[1], colour[2]);
//		if ((!shell)&&(!cell)) glColor4f (colour[0], colour[1], colour[2], alpha);

		if (backwards == true)
			glDrawArrays (GL_TRIANGLE_FAN, 0, count);
		else
			glDrawArrays (GL_TRIANGLE_STRIP, 0, count);
	}
}

__inline qboolean IsSet( unsigned flags, unsigned mask ) {
	return (flags & mask) != 0;
}

void GL_BeginAliasFrame (qboolean renderShell, qboolean renderOutline, qboolean additiveRendering, qboolean lightPass, float setcolors[4]) {
	//cell shading
	extern	unsigned int	celtexture;
	extern	unsigned int	vertextexture;

	if ( setcolors[3] < 1.0f ) 
	{ 
		glEnable (GL_BLEND); 
		glDepthMask (GL_FALSE); 
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} 
	else if ( renderShell )
	{
		glEnable (GL_BLEND); 
		glDepthMask (GL_FALSE); 
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		if (gl_combine)
		{
			// Entar : let's turn up the colors on the whole thing
			// and make it more noticeable. This makes for a really great effect.
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2);
		} 
		else
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	}
	else 
	{ 
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND); 
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.9f);
		// TODO: check whether we need this at all 
		glDepthMask (GL_TRUE); // was false, now fixed - LH
	} 

	//(effects & EF_ADDITIVE)
	if ( additiveRendering )
	{
		glEnable( GL_BLEND );
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);		
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vert);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texture);

	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, 0, normal);

	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, color);

	if( renderOutline )
	{
		glCullFace (GL_BACK);
		glEnable(GL_BLEND);
		glPolygonMode (GL_FRONT, GL_LINE);
		glLineWidth (2.0f);
		glEnable (GL_LINE_SMOOTH);
	}

	if( r_celshading.value && !lightPass && multitex_go && vertshade && !renderShell ) // cel shading
	{
		//setup for shading
		GL_SelectTexture(GL_TEXTURE1_ARB);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_1D);
		glBindTexture (GL_TEXTURE_1D, celtexture);

		qglClientActiveTexture(GL_TEXTURE1_ARB);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glTexCoordPointer(1, GL_FLOAT, 0, texture2);

		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_FLOAT, 0, color2);
	}
}

void GL_EndAliasFrame (qboolean renderShell, qboolean renderOutline, qboolean additiveRendering, qboolean lightPass, float setcolors[4]) {
	if (renderOutline) {
		glPolygonMode (GL_FRONT, GL_FILL);
		glDisable (GL_BLEND);
		glCullFace (GL_FRONT);
		glDisable(GL_LINE_SMOOTH);
	}

	if (multitex_go && !lightPass && vertshade && !renderShell && r_celshading.value)
	{
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		qglClientActiveTexture(GL_TEXTURE0_ARB);

		glDisable(GL_TEXTURE_1D);
		glEnable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0_ARB);		
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if (setcolors[3] < 1)
	{ 
		glDisable (GL_BLEND); 
		glDepthMask (GL_TRUE); // was false, now fixed - LH
	} 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glAlphaFunc(GL_GREATER, 0.666f);
	glDisable(GL_ALPHA_TEST);
}

/*
=================
R_SetupAliasBlendedFrame

fenix@io.com: model animation interpolation
=================
*/
void R_SetupAliasBlendedFrame (int frame, aliashdr_t *paliashdr, entity_t* e, unsigned flags)
{
	qboolean renderShell = IsSet( flags, R_ALIAS_SHELL );
	qboolean renderOutline = IsSet( flags, R_ALIAS_OUTLINE );
	qboolean additiveRendering = IsSet( flags, R_ALIAS_ADDITIVE );

	int   pose;
	int   numposes;
	float blend, setcolors[4];

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / e->frame_interval) % numposes;
	} else {
		/* One tenth of a second is a good for most Quake animations.
		If the nextthink is longer then the animation is usually meant to pause
		(e.g. check out the shambler magic animation in shambler.qc).  If its
		shorter then things will still be smoothed partly, and the jumps will be
		less noticable because of the shorter time.  So, this is probably a good
		assumption. */
		e->frame_interval = 0.1f;
	}

	if (e->pose2 != pose)
	{
		e->frame_start_time = realtime;
		e->pose1 = e->pose2;
		e->pose2 = pose;
		blend = 0;
	} else {
		blend = ((realtime - e->frame_start_time)*slowmo.value) / e->frame_interval;
	}

	//	if (gl_fogglobal.value)
	//		glDisable(GL_FOG);

	VectorCopy(e->baseline.colormod, setcolors);
	setcolors[3] = e->alpha;
	
	// this is really only valid when world lights and model draw are enabled
	if (r_shadow_realtime_draw_models.value && r_shadow_realtime_world.value && !r_fullbright.value)
		VectorScale(setcolors, r_shadow_realtime_world_lightmaps.value, setcolors);
	VectorMultiply (lightcolor, setcolors, setcolors);

	GL_BeginAliasFrame( renderShell, renderOutline, additiveRendering, false, setcolors );

	// weird things start happening if blend passes 1
	if (r_interpolate_model_a.value == 0 || cl.paused || blend > 1)
	{
		GL_DrawAliasFrame (paliashdr, e->pose2, renderShell, setcolors );
		blend = 1;
	} 
	else 
	{
		GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend, renderShell, setcolors);
	}

	GL_EndAliasFrame( renderShell, renderOutline, additiveRendering, false, setcolors );

	if (r_shadow_realtime_draw_models.value && r_shadow_realtime_world.value)
		VectorCopy(e->baseline.colormod, setcolors); // put it back to normal

	if(r_shadow_realtime_draw_models.value && R_Shader_CanRenderLights() && !renderShell) // the shell doesn't need good lighting
	{
		int i;
		// TODO: pass no setcolors
		GL_BeginAliasFrame( false, renderOutline, false, true, setcolors );
		R_Shader_StartLightRendering();
		for(i = r_shadow_realtime_world.value ? 0 : R_MIN_SHADER_DLIGHTS ; i < R_MAX_SHADER_LIGHTS ; i++ ) {
			if( !R_Shader_IsLightInScopeByPoint( i, e->origin ) ) {
				continue;
			}
			R_Shader_StartLightPass( i );

			// weird things start happening if blend passes 1
			if (cl.paused || blend > 1)
			{
				GL_DrawAliasFrame (paliashdr, e->pose2, false, setcolors);
				blend = 1;
			} else 
			{
				GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend, false, setcolors);
			}

			R_Shader_FinishLightPass();
		}
		R_Shader_FinishLightRendering();

		GL_EndAliasFrame( false, renderOutline, false, true, setcolors );
	}
	//	if (gl_fogglobal.value)
	//		glEnable(GL_FOG);
}

/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupQ3AliasFrame (int frame, aliashdr_t *paliashdr, int shell)
{
	int				pose, numposes;
	float			interval;
	//md3 stuff
	float			*texcoos, *vertices;
	int				*indecies;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	glDisable(GL_DEPTH);
	texcoos = (float *)((byte *)paliashdr + paliashdr->texcoords);
	indecies = (int *)((byte *)paliashdr + paliashdr->indecies);
	vertices = (float *)((byte *)(paliashdr + paliashdr->posedata + pose * paliashdr->poseverts));
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoos);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawElements(GL_TRIANGLES,paliashdr->numtris*3,GL_UNSIGNED_INT,indecies);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_DEPTH);
}


/*
=============
GL_DrawQ2AliasFrame
=============
*/
void GL_DrawQ2AliasFrame (entity_t *e, md2_t *pheader, int lastpose, int pose, float lerp, int shell)
{
	float	ilerp;
	int		*order, count;
	md2trivertx_t	*verts1, *verts2;
	vec3_t	scale1, translate1, scale2, translate2, d;
	md2frame_t *frame1, *frame2;

    if (e->alpha < 1 || shell) 
    { 
		glEnable (GL_BLEND); 
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//		glDepthMask (GL_TRUE); 
		glDepthMask (GL_FALSE); 
    } 
    else 
	{ 
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND); 
//		glEnable (GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.9f);
		glDepthMask (GL_TRUE); // was false, now fixed - LH
	} 

	ilerp = 1.0 - lerp;

	//new version by muff - fixes bug, easier to read, faster (well slightly)
	frame1 = (md2frame_t *)((int) pheader + pheader->ofs_frames + (pheader->framesize * lastpose)); 
	frame2 = (md2frame_t *)((int) pheader + pheader->ofs_frames + (pheader->framesize * pose)); 

	VectorCopy(frame1->scale, scale1);
	VectorCopy(frame1->translate, translate1);
	VectorCopy(frame2->scale, scale2);
	VectorCopy(frame2->translate, translate2);
	verts1 = &frame1->verts[0];
	verts2 = &frame2->verts[0];
	order = (int *)((int)pheader + pheader->ofs_glcmds);

	if (shell)
		glColor4f (1,1,1,0.75*e->alpha); // offset :D
	else
		glColor4f (1,1,1,e->alpha);

	if (shell)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glEnable(GL_BLEND);
	}

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			if (shell)
				glColor3f (1,1,1);
			else
			{
				// normals and vertexes come from the frame list
				// blend the light intensity from the two frames together    
				d[0] = d[1] = d[2] = shadedots[verts1->lightnormalindex] * ilerp + shadedots[verts2->lightnormalindex] * lerp;
				//d[0] = (shadedots[verts2->lightnormalindex] + shadedots[verts1->lightnormalindex])/2;
				d[0] *= lightcolor[0];
				d[1] *= lightcolor[1];
				d[2] *= lightcolor[2];
				glColor3f (d[0], d[1], d[2]);
			}

			if (shell)
				glTexCoord2f(((float *)order)[0] + realtime*slowmo.value*2, ((float *)order)[1] + realtime*slowmo.value*2);
			else
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
			glVertex3f((verts1[order[2]].v[0]*scale1[0]+translate1[0])*ilerp+(verts2[order[2]].v[0]*scale2[0]+translate2[0])*lerp,
					   (verts1[order[2]].v[1]*scale1[1]+translate1[1])*ilerp+(verts2[order[2]].v[1]*scale2[1]+translate2[1])*lerp,
					   (verts1[order[2]].v[2]*scale1[2]+translate1[2])*ilerp+(verts2[order[2]].v[2]*scale2[2]+translate2[2])*lerp);
				
			order+=3;
		} while (--count);

		glEnd ();
	}

	if (e->alpha < 1 || shell)
	{ 
		glDisable (GL_BLEND); 
		glDepthMask (GL_TRUE); // was false, now fixed - LH
	} 
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glAlphaFunc(GL_GREATER, 0.666f);
	glDisable(GL_ALPHA_TEST);
}

/*
=================
R_SetupQ2AliasFrame

=================
*/
void R_SetupQ2AliasFrame (entity_t *e, md2_t *pheader, int shell)
{
	int				frame;
	float			lerp;//, lerpscale;

	frame = e->frame;

    glPushMatrix ();
	R_RotateForEntity (e);

	if ((frame >= pheader->num_frames) || (frame < 0))
	{
		Con_DPrintf ("R_SetupQ2AliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (e->draw_lastmodel == e->model)
	{
		if (frame != e->draw_pose)
		{
			e->draw_lastpose = e->draw_pose;
			e->draw_pose = frame;
			e->draw_lerpstart = cl.time;
			lerp = 0;
		}
		else
			lerp = (cl.time - e->draw_lerpstart) * 20.0;
	}
	else // uninitialized
	{
		e->draw_lastmodel = e->model;
		e->draw_lastpose = e->draw_pose = frame;
		e->draw_lerpstart = cl.time;
		lerp = 0;
	}
	if (lerp > 1) lerp = 1;

	GL_DrawQ2AliasFrame (e, pheader, e->draw_lastpose, frame, lerp, shell);
	glPopMatrix();
}

int quadtexture;

extern	vec3_t	avelocities[NUMVERTEXNORMALS];
extern	int		compareValue (ls_t *script, char *section, char *key, char *value);
extern	void	R_ParticleScript(char *section, vec3_t org);
extern	cvar_t	r_part_scripts;

extern cvar_t r_bloom;
extern int flareglow_tex;

void R_DrawAliasModel (entity_t *e)
{
	extern	cvar_t	r_part_flame;
	extern	void AddFire(vec3_t org, float size);
	extern	void AddSmoke(vec3_t org, float size);
	extern	void DefineFlare(vec3_t origin, int radius, int mode, int alfa);
	extern	vec3_t	zerodir, zero;

	// scripts
	char		*trailsection;

    int         lnum, golight, i;
    vec3_t      dist, delta, forward, up, right;
    float       add, l, L2 = 9999, L3;
    model_t     *clmodel;
    vec3_t      mins, maxs;
    aliashdr_t  *paliashdr;
    //trivertx_t  *verts, *v;
    //int         index;
    float       an;//s, t, 
    int         anim;
	md2_t		*pheader; // LH / muff
	int			shell = false; //QMB :model shells

	int flags;

	float		blend;
	md3header_mem_t *header;  // Reckless - Quake3 Model shadows
	//vec3_t      mins;

	//not implemented yet
	//	byte		c, *color;	//QMB :color map

	if (e == &cl.viewent && g_drawing_refl)
		return;

	if (modelScript == NULL) 
		loadmodelScript(MODELSCRIPTFILENAME); 

//set get the model from the e
    clmodel = e->model;

//work out its max and mins
    VectorAdd (e->origin, clmodel->mins, mins);
    VectorAdd (e->origin, clmodel->maxs, maxs);
//make sure its in screen

	if (!(e->effects & EF_NODEPTHTEST))
	{
		if (R_CullBox (mins, maxs) && e != &cl.viewent)
			return;
	}
	else
		glDisable(GL_DEPTH_TEST);

	if ((cl.items & IT_QUAD && (e == &cl.viewent || e == &cl_entities[cl.viewentity])))
		shell = true;

	if (cl.paused || !strcmp (clmodel->name, "progs/player.mdl")) // not when paused or on player models, prevents cheating
		goto doneflame;

//does the model need a shell?
	//if (cl.items & IT_QUAD && e == &cl.viewent)
	if (model_checkValue(EF_QUADSHELLFLAG, clmodel->name))
		shell = true;

	if (hasSection(partscript, clmodel->name) != -1) // particle scripts
	{
		if (hasKey(partscript, clmodel->name, "trailscript") != -1) 
		{
			trailsection = getValue(partscript, clmodel->name, "trailscript", 0);
			if (trailsection) // can't go ahead without a particle effect to use
			{
				if ((hasKey(partscript, clmodel->name, "trailmode") != -1) && compareValue(partscript, clmodel->name, "trailmode", "spiral"))
				{
					vec3_t	forward, realorg; // FIXME: check for bugs
					float		sp, sy, cp, cy;
					float		angle;
					int			areaspread1 = 16, areaspreadvert1 = 16, i, movespeed = 100;

					if (hasKey(partscript, clmodel->name, "areaspread") != -1)
						areaspread1 = atoi(getValue(partscript, clmodel->name, "areaspread", 0));
					if (hasKey(partscript, clmodel->name, "areaspreadvert") != -1)
						areaspreadvert1 = atoi(getValue(partscript, clmodel->name, "areaspreadvert", 0));
					if (hasKey(partscript, clmodel->name, "movespeed") != -1)
						movespeed = atoi(getValue(partscript, clmodel->name, "movespeed", 0));

					if (!avelocities[0][0])
					{
						for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
							avelocities[0][i] = (rand()&255) * 0.01;
					}	

					//angle = cl.time * avelocities[0][0];
					angle = (cl.time * movespeed) * avelocities[0][0];
					sy = sin(angle);
					cy = cos(angle);
					//angle = cl.time * avelocities[0][1];
					angle = (cl.time * movespeed) * avelocities[0][1];
					sp = sin(angle);
					cp = cos(angle);

					// Entar : FIXME : relatively hacky bugfix
					cp = cp - 0.5f;
					cy = cy - 0.5f;
					sy = sy - 0.5f;
					
					forward[0] = cp*cy;
					forward[1] = cp*sy;
					forward[2] = -sp;

					realorg[0] = e->origin[0] + r_avertexnormals[0][0]*areaspread1 + forward[0]*areaspreadvert1;
					realorg[1] = e->origin[1] + r_avertexnormals[0][1]*areaspread1 + forward[1]*areaspreadvert1;
					realorg[2] = e->origin[2] + r_avertexnormals[0][2]*areaspread1 + forward[2]*areaspreadvert1;

					R_ParticleScript(trailsection, realorg);
				}
				else
					R_ParticleScript(trailsection, e->origin);
			}
			goto doneflame;
		}
	}

//QMB: FIXME
//should use a particle emitter linked to the model for its org
//needs to be linked when the model is created and destroyed when
//the entity is distroyed
//check if its a fire and add the particle effect
	if (!strcmp (clmodel->name, "progs/flame.mdl"))
	{
		// Entar : just making use of other vectors, instead of making new ones
		VectorCopy(e->origin, lightVector);
		lightVector[2] += 3;
//		DefineFlare(lightVector, 45, 0, 22);
		if (r_part_scripts.value && (hasSection(partscript, "Torch") != -1))
			R_ParticleScript("Torch", e->origin);
		else
			AddFire(e->origin, 9); // original value == 4

		//heh.
		//R_ParticleScript("TE_SPIKE", e->origin);
		//R_ParticleScript("trail_2001", e->origin);

		goto doneflame;
	}
	if (!strcmp (clmodel->name, "progs/flame2.mdl") && r_part_flame.value)
	{
//		DefineFlareColor(e->origin, 60, 0, 20, 1, 0.98f, 0.98f);
		VectorCopy(e->origin, lightVector);
		lightVector[2] += 3;
//		DefineFlare(lightVector, 60, 0, 25);

		if (r_part_scripts.value && (hasSection(partscript, "Torch2") != -1))
			R_ParticleScript("Torch2", e->origin);
		else
		{
			AddFire(e->origin, 14); // original value == 10
			AddSmoke(e->origin, 8);
		}
		return; //do not draw the big fire model, its just a place holder for the particles
	}

	if (model_checkValue(EF_FIREFLAG, e->model->name) || (e->effects & EF_FLAME))
	{
		AddFire(e->origin, 18);
	}

doneflame:
    //
    // get lighting information
    //
//QMB: FIXME
//SHOULD CHANGE TO A PASSED VAR
//get vertex normals (for lighting and shells)
    shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

//make a default lighting direction
    an = e->angles[1]/180*M_PI;
    shadevector[0] = cos(-an);
    shadevector[1] = sin(-an);
    shadevector[2] = 1;//e->angles[0];
    VectorNormalize (shadevector);

//get the light for the model
	R_LightPoint(e->origin); // LordHavoc: lightcolor is all that matters from this

//work out lighting from the dynamic lights
	golight = false;
    for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
    {
	//if the light is alive
        if (cl_dlights[lnum].die >= cl.time)
        {
		//work out the distance to the light
            VectorSubtract (e->origin, cl_dlights[lnum].origin, dist);
            add = cl_dlights[lnum].radius - Length(dist);
		//if its close enough add light from it
			if (add > -3)
			{
				if (add > 0)
				{
					lightcolor[0] += add * cl_dlights[lnum].colour[0];
					lightcolor[1] += add * cl_dlights[lnum].colour[1];
					lightcolor[2] += add * cl_dlights[lnum].colour[2];
				}
				VectorSubtract(cl_dlights[lnum].origin, e->origin, delta);
				l = Length(delta)/4;
				if (l < L2)
				{
					L2 = l;
					VectorCopy(cl_dlights[lnum].origin, lightVector);
					L3 = add;
					VectorSubtract(cl_dlights[lnum].origin, e->origin, lightVector);
					golight = true;
				}
			}
        }
    }

	if ((e->effects & EF_FULLBRIGHT) || r_fullbright.value)
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;

//scale lighting to floating point
	VectorScale(lightcolor, 1.0f / 255.0f, lightcolor); 
	
	if (r_dirlighting.value)
	{
		vec3_t vector;
		if (e == &cl.viewent) // Entar : for correct dynamic lighting on the gun too
		{
			AngleVectors(r_refdef.viewangles, forward, right, up);
		}
		else
			AngleVectors(e->angles, forward, right, up);
	
		// we need to make sure that a dlight is getting to it at all
		if (golight == true)
		{
			extra_light = true;

			VectorCopy (lightVector, vector);
			lightVector[0] = DotProduct(right, vector);
			lightVector[1] = DotProduct(forward, vector);
			lightVector[2] = DotProduct(up, vector);
	//		lightVector[0] = -lightVector[0];
			lightVector[1] = -lightVector[1];
		}
		else
		{
			extra_light = false;
		}

		vector[0] = 0;
		vector[1] = 1;
		vector[2] = 2;

		lightRegular[0] = DotProduct(right, vector);
		lightRegular[1] = DotProduct(forward, vector);
		lightRegular[2] = DotProduct(up, vector);
		lightRegular[1] = -lightRegular[1];

		if (e == &cl.viewent)
			lightRegular[2] += 2; // to make sure the gun looks decent

		VectorNormalize (lightRegular);
		VectorNormalize (lightVector);
	}

	if (model_checkValue(EF_NOSHADEFLAG, e->model->name))
		vertshade=false;
	else
		vertshade=true;
	//
	// locate the proper data
	//
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_n_patches && gl_npatches.value)
	{
		glEnable(GL_PN_TRIANGLES_ATI);
		glPNTrianglesiATI( GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_npatches.value);

	   if (true)
		  glPNTrianglesiATI( GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
	   else
		  glPNTrianglesiATI( GL_PN_TRIANGLES_POINT_MODE_ATI, GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI);

	   if (true)
		  glPNTrianglesiATI( GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
	   else
		  glPNTrianglesiATI( GL_PN_TRIANGLES_NORMAL_MODE_ATI, GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI);
	}

#ifdef Q3MODELS
	if (clmodel->aliastype == MD3IDHEADER)
	{
		float setcolors[4] = {e->baseline.colormod[0], e->baseline.colormod[1], e->baseline.colormod[2], e->alpha};
		//do nothing for testing
		if (!r_modeltexture.value){	GL_DisableTMU(GL_TEXTURE0_ARB); }//disable texture if needed

		glPushMatrix();

		//interpolate unless its the viewmodel
		if (e == &cl.viewent)
			R_RotateForEntity (e);
		else
			R_BlendedRotateForEntity (e);

		//md3 interpolation
		e->frame_interval = 0.1f;
		if (e->pose2 != e->frame)
		{
			e->frame_start_time = realtime;
			e->pose1 = e->pose2;
			e->pose2 = e->frame;
			blend = 0;
		} else {
			blend = (realtime - e->frame_start_time) / e->frame_interval;
			if (blend > 1)
				blend = 1;
		}

		// this is really only valid when world lights and model draw are enabled
		if (r_shadow_realtime_draw_models.value && r_shadow_realtime_world.value && !r_fullbright.value)
			VectorScale(lightcolor, r_shadow_realtime_world_lightmaps.value, lightcolor);
		VectorMultiply(lightcolor, e->baseline.colormod, lightcolor);

		if (gl_ammoflash.value && (e->model->flags & EF_MODELFLASH)){
			lightcolor[0] += sin(2 * cl.time * M_PI)/4;
			lightcolor[1] += sin(2 * cl.time * M_PI)/4;
			lightcolor[2] += sin(2 * cl.time * M_PI)/4;
			//Con_Printf("Model flags: %x\n", model->flags);
		}

		GL_BeginAliasFrame(false, false, e->flags & EF_ADDITIVE ? true : false, false, setcolors);
		c_alias_polys += R_DrawQ3Model (e, false, false, blend);
		GL_EndAliasFrame(false, false, e->flags & EF_ADDITIVE ? true : false, false, setcolors);

		if (r_shadow_realtime_draw_models.value && r_shadow_realtime_world.value)
			VectorCopy(e->baseline.colormod, lightcolor);

		if(r_shadow_realtime_draw_models.value && R_Shader_CanRenderLights())
		{
			int i;
			// TODO: pass no setcolors
			GL_BeginAliasFrame(false, false, e->flags & EF_ADDITIVE ? true : false, true, setcolors);
			R_Shader_StartLightRendering();
			for(i = r_shadow_realtime_world.value ? 0 : R_MIN_SHADER_DLIGHTS ; i < R_MAX_SHADER_LIGHTS ; i++ ) {
				if( !R_Shader_IsLightInScopeByPoint( i, e->origin ) ) {
					continue;
				}
				R_Shader_StartLightPass( i );
				R_DrawQ3Model(e, false, false, blend);
				R_Shader_FinishLightPass();
			}
			R_Shader_FinishLightRendering();
			GL_EndAliasFrame(false, false, e->flags & EF_ADDITIVE ? true : false, true, setcolors);
		}

		if (!r_modeltexture.value){	GL_EnableTMU(GL_TEXTURE0_ARB); }//enable texture if needed

		if (r_celshading.value > 1 || r_outline.value)
		{
			glCullFace (GL_BACK);
			glEnable(GL_BLEND);
			glPolygonMode (GL_FRONT, GL_LINE);
			
			if (e == &cl.viewent){
				glLineWidth (1.0f);
			}else{
				glLineWidth (5.0f);
			}

			glEnable (GL_LINE_SMOOTH);

			GL_DisableTMU(GL_TEXTURE0_ARB);

			glColor3f(0.0,0.0,0.0);
			GL_BeginAliasFrame(false, true, e->flags & EF_ADDITIVE, false, setcolors);
			R_DrawQ3Model (e, false, true, blend);
			GL_EndAliasFrame(false, true, e->flags & EF_ADDITIVE, false, setcolors);
			glColor3f(1.0,1.0,1.0);

			GL_EnableTMU(GL_TEXTURE0_ARB);

			glPolygonMode (GL_FRONT, GL_FILL);
			glDisable (GL_BLEND);
			glCullFace (GL_FRONT);
		}

		if (r_quadshell.value && shell)
		{
			if (quadtexture)
			{
				glBindTexture(GL_TEXTURE_2D,quadtexture);
				glColor4f(1.0f,1.0f,1.0f,0.5f);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, flareglow_tex);
				glColor4f(0.5f,0.5f,1.0f,0.5f);
			}
			glEnable(GL_BLEND);
			GL_BeginAliasFrame(true, false, e->flags & EF_ADDITIVE, false, setcolors);
			R_DrawQ3Model (e, true, false, blend);
			GL_EndAliasFrame(true, false, e->flags & EF_ADDITIVE, false, setcolors);
			glDisable(GL_BLEND);
			glColor3f(1.0,1.0,1.0);
			if (!quadtexture)
			{
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
			}
		}
		glPopMatrix();
	} else
#endif 
	if (clmodel->aliastype != ALIASTYPE_MD2)
	{
		paliashdr = (aliashdr_t *)Mod_Extradata (e->model);
		c_alias_polys += paliashdr->numtris;

		glPushMatrix ();

		//interpolate unless its the viewmodel
		if (e != &cl.viewent)
			R_BlendedRotateForEntity (e);
		else
			R_RotateForEntity (e);

		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

		anim = (int)(cl.time*10) & 3;
	    glBindTexture(GL_TEXTURE_2D,paliashdr->gl_texturenum[e->skinnum][anim]);
		if (e->colormap != vid.colormap && !gl_nocolors.value) 
		{ 
			i = e - cl_entities;
			// OH, HACK! We ought to be able to have translations on other models...
			if (i >= 1 && i<=cl.maxclients && !strcmp (e->model->name, "progs/player.mdl")) 
				glBindTexture(GL_TEXTURE_2D, playertextures - 1 + i); 
		}

    //
    // draw all the triangles
    //
		if (!r_modeltexture.value){
			GL_DisableTMU(GL_TEXTURE0_ARB);
		}else {
			//highlighting test code
			if (0 && gl_textureunits>2){
				GL_EnableTMU(GL_TEXTURE1_ARB);

				glBindTexture(GL_TEXTURE_2D,highlighttexture);

				glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
				glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
				glEnable(GL_TEXTURE_GEN_S);
				glEnable(GL_TEXTURE_GEN_T);
				//need correct mode
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
				glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
				glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0);

				//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			}
		}

		if ((e->effects & EF_FULLBRIGHT) || r_fullbright.value)
			glColor3f(1, 1, 1);
		else
			glColor3fv(lightcolor);
		
		if (e->effects & EF_ADDITIVE) {
			flags = R_ALIAS_ADDITIVE;
		} else {
			flags = 0;
		}

		R_SetupAliasBlendedFrame (e->frame, paliashdr, e, flags);
		glDisable(GL_TEXTURE_1D);

		if (r_celshading.value > 1 || r_outline.value)
		{
			glColor3f(0.0,0.0,0.0);
			R_SetupAliasBlendedFrame (e->frame, paliashdr, e, flags | R_ALIAS_OUTLINE );
			glColor3f(1.0,1.0,1.0);
		}

		if (!r_modeltexture.value){
			GL_EnableTMU(GL_TEXTURE0_ARB);
		}else {
			if (0 && gl_textureunits>2){
				//highlighting test code
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
				GL_DisableTMU(GL_TEXTURE1_ARB);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		}
		GL_SelectTexture(GL_TEXTURE0_ARB);

//colour map code... not working yet...
/*		if (e->colormap != vid.colormap && !gl_nocolors.value)
		{
			if (paliashdr->gl_texturenumColorMap&&paliashdr->gl_texturenumColorMap){
				glBindTexture(GL_TEXTURE_2D,paliashdr->gl_texturenumColorMap);
				c = (byte)e->colormap & 0xF0;
				c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
				color = (byte *) (&d_8to24table[c]);
				//glColor3fv(color);
				glColor3f(1.0,1.0,1.0);
			}
		}*/
		if (r_quadshell.value && shell)
		{
			if (quadtexture)
			{
				glBindTexture(GL_TEXTURE_2D,quadtexture);
				glColor4f(1.0f,1.0f,1.0f,0.5f);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, flareglow_tex);
				glColor4f(0.5f,0.5f,1.0f,0.5f);
			}
			glEnable(GL_BLEND);
			R_SetupAliasBlendedFrame (e->frame, paliashdr, e, flags | R_ALIAS_SHELL );
			glDisable(GL_BLEND);
			glColor3f(1.0,1.0,1.0);
		}
		glPopMatrix ();
	}  
	else
	{
		pheader = (md2_t *)Mod_Extradata (e->model);
		c_alias_polys += pheader->num_tris;

		glBindTexture(GL_TEXTURE_2D,pheader->gl_texturenum[e->skinnum]);
		R_SetupQ2AliasFrame (e, pheader, false);
		if (r_quadshell.value && shell)
		{
			glBindTexture(GL_TEXTURE_2D,quadtexture);
			glColor4f(1.0,1.0,1.0,0.5);
			glEnable(GL_BLEND);
			R_SetupQ2AliasFrame (e, pheader, true);
			glDisable(GL_BLEND);
			glColor3f(1.0,1.0,1.0);
		}
	}
 
	if (gl_n_patches && gl_npatches.value)
	{
		glDisable(GL_PN_TRIANGLES_ATI);
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);


	blend = (realtime - e->frame_start_time) / e->frame_interval * slowmo.value; 
 	if (blend > 1)
		blend = 1;  

	if (r_shadows.value)
	{ 
		//header = (md3header_mem_t *)Mod_Extradata (currententity->model);
		header = (md3header_mem_t *)Mod_Extradata (e->model);

		VectorAdd (e->origin, e->model->mins, mins); 

		glPushMatrix ();
		R_RotateForEntity (e);

		glDisable (GL_TEXTURE_2D); 
		glDepthMask(GL_FALSE); // disable zbuffer updates 

		//GL_DrawQ3AliasShadow (e, header, e->draw_lastpose, frame, blend);
		if (clmodel->aliastype == MD3IDHEADER)
		{
			if (modelScript == NULL) 
			{ 
				loadmodelScript(MODELSCRIPTFILENAME); 
			} 
			if (!model_checkValue(EF_NOMD3SHADOWFLAG, e->model->name))
			{
				GL_DrawQ3AliasShadow (e, header, e->draw_lastpose, e->frame, blend);
			}
		}

		glDepthMask(GL_TRUE); // enable zbuffer updates 
		glEnable (GL_TEXTURE_2D); 
		glColor4f (1.0f, 1.0f, 1.0f, 1.0f); 
		glPopMatrix (); 
   }
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);

}

//==================================================================================