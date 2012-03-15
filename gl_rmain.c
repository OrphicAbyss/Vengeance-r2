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
// r_main.c

#include "quakedef.h"

entity_t	r_worldentity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

//int			particletexture;	// little dot for particles
int			shinetex_glass, shinetex_chrome, underwatertexture, highlighttexture, screen_pptexture0, screen_blurtexture, sceneblur_texture;
int			crosshair_tex[32];

int			playertextures;		// up to 16 color translated skins

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

//qmb :texture units
int		gl_textureunits = 0;

void R_MarkLeaves (void);

cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_shadows = {"r_shadows","0.8", true}; // shadows - Entar : currently only on md3 models
cvar_t	r_wateralpha = {"r_wateralpha","0.45", true};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0", true};
cvar_t	r_depthsort = {"r_depthsort","1", true};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","1", true};
cvar_t	gl_24bitmaptex = {"gl_24bitmaptex","1",true};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_polyblend = {"gl_polyblend","0", true};
cvar_t	gl_flashblend = {"gl_flashblend","0", true};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","1"}; // Entar : changed to 1 as default

//qmb :extra cvars
cvar_t	gl_detail = {"gl_detail","0", true};
cvar_t	gl_shiny = {"gl_shiny","0", true};
cvar_t	gl_caustics = {"gl_caustics","1", true};
cvar_t	gl_dualwater = {"gl_dualwater","1", true};
cvar_t	gl_ammoflash = {"gl_ammoflash","1", true};

//Entar :extra cvars
cvar_t	v_gunmove = {"v_gunmove","2", true};
cvar_t	slowmo = {"slowmo", "1"};
cvar_t	v_hurtblur = {"v_hurtblur", "20"};
cvar_t	r_test = {"r_test", "0", true};
cvar_t	chase_anglesfix = {"chase_anglesfix", "1", true};
#if 0 // old bloom
cvar_t	gl_bloom = {"gl_bloom", "0", true};
cvar_t	gl_bloom_darken = {"gl_bloom_darken", "2", true};
cvar_t	gl_bloom_change = {"gl_bloom_change", "1"};
cvar_t	gl_bloom_showonly = {"gl_bloom_showonly", "0", true};
cvar_t	gl_bloom_intensity = {"gl_bloom_intensity", "1.25", true};
#endif

// fenix@io.com: model interpolation
cvar_t  r_interpolate_model_a = { "r_interpolate_model_a", "1", true };
cvar_t  r_interpolate_model_t = { "r_interpolate_model_t", "1", true };
cvar_t  r_wave = {"r_wave", "1", true};
cvar_t  gl_fog = {"gl_fog", "1", true};
cvar_t  gl_fogglobal = {"gl_fogglobal", "0", true};
cvar_t  gl_fogred = {"gl_fogred", "0.7", true};
cvar_t  gl_foggreen = {"gl_foggreen", "0.7", true};
cvar_t  gl_fogblue = {"gl_fogblue", "0.7", true};
cvar_t  gl_fogstart = {"gl_fogstart", "160", true};
cvar_t  gl_fogend = {"gl_fogend", "3100", true};
cvar_t  sv_fastswitch = {"sv_fastswitch", "0", true};

cvar_t  gl_conalpha = {"gl_conalpha", "0.5", true};
cvar_t	gl_checkleak = {"gl_checkleak","0", true};
cvar_t	r_skydetail = {"r_skydetail","1", true};
cvar_t	r_sky_x = {"r_sky_x","0", true};
cvar_t	r_sky_y = {"r_sky_y","0", true};
cvar_t	r_sky_z = {"r_sky_z","0", true};

cvar_t	r_errors = {"r_errors","1", true};
cvar_t	r_fullbright = {"r_fullbright", "0"};

//cel shading & vertex shading in model
cvar_t	r_modeltexture = {"r_modeltexture","1", true};
cvar_t	r_celshading = {"r_celshading","0", true};
cvar_t	r_outline = {"r_outline","0", true};
cvar_t	r_vertexshading = {"r_vertexshading","5", true};

//ati truform
cvar_t	gl_npatches = {"gl_npatches","1", true};

//anisotropy filtering
cvar_t	gl_anisotropic = {"gl_anisotropic","2", true};

//temp var for internal testing of new features
cvar_t  gl_test = {"gl_temp", "1", true};
//qmb :end

extern	cvar_t		v_gamma; // muff

vec3_t	temp;	//for debug can remove l8er

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

void R_RotateForEntity (entity_t *e)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
	glRotatef (-e->angles[0],  0, 1, 0);
	glRotatef (e->angles[2],  1, 0, 0);
}


/*
=============
R_BlendedRotateForEntity

fenix@io.com: model transform interpolation
=============
*/
void R_BlendedRotateForEntity (entity_t *e) // Entar : added slowmo.value calculations
{
	float timepassed; //JHL:FIX; for checking passed time
	float blend;
	vec3_t d;
	int i;

	timepassed = realtime - e->translate_start_time;

	// positional interpolation
	if (e->translate_start_time == 0 || timepassed > 0.5)
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin, e->origin1);
		VectorCopy (e->origin, e->origin2);
	}

	if (!VectorCompare (e->origin, e->origin2))
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin2, e->origin1);
		VectorCopy (e->origin,  e->origin2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.1 * slowmo.value;

		if (cl.paused || blend > 1)
			blend = 1;
	}

	VectorSubtract (e->origin2, e->origin1, d);
	glTranslatef (e->origin1[0] + (blend * d[0]), e->origin1[1] + (blend * d[1]), e->origin1[2] + (blend * d[2]));

	// orientation interpolation (Euler angles, yuck!)
	timepassed = realtime - e->rotate_start_time;

	if (e->rotate_start_time == 0 || timepassed > 0.5)
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles, e->angles1);
		VectorCopy (e->angles, e->angles2);
	}

	if (!VectorCompare (e->angles, e->angles2))
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles2, e->angles1);
		VectorCopy (e->angles,  e->angles2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.1 * slowmo.value;

		if (cl.paused || blend > 1)
			blend = 1;
	}

	VectorSubtract (e->angles2, e->angles1, d);

	// always interpolate along the shortest path
	for (i = 0; i < 3; i++)
	{
		if (d[i] > 180)
			d[i] -= 360;
		else if (d[i] < -180)
			d[i] += 360;
	}

	glRotatef ( e->angles1[1] + ( blend * d[1]),  0, 0, 1);
	glRotatef (-e->angles1[0] + (-blend * d[0]),  0, 1, 0);
	glRotatef ( e->angles1[2] + ( blend * d[2]),  1, 0, 0);
}



/*
=============
R_DrawEntitiesOnList
=============
*/

//QMB: will fix this function to go through for the alias models and for the brushes so optimize for rendering
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// Entar : why go through the loop 3 times?

	// draw sprites seperately, because of alpha blending
	// draw brushes
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->model->type == mod_brush)
		{
			R_DrawBrushModel (cl_visedicts[i]);
		}
//	}

	// draw models
//	for (i=0 ; i<cl_numvisedicts ; i++)
//	{
		// Entar : chasecam player model fix
		if (cl_visedicts[i] == &cl_entities[cl.viewentity]) 
			if (chase_anglesfix.value)
				cl_visedicts[i]->angles[0] = 0.0f; // it just looks darned weird to have the player model tilting that way
			else
				cl_visedicts[i]->angles[0] *= 0.3f;
		if (cl_visedicts[i]->model->type == mod_alias)
		{
			R_DrawAliasModel (cl_visedicts[i]);
		}
//	}

	// draw sprites
//	for (i=0 ; i<cl_numvisedicts ; i++)
//	{
		
		if (cl_visedicts[i]->model->type == mod_sprite)
		{
			R_DrawSpriteModel (cl_visedicts[i]);
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	entity_t	*e;

    float old_interpolate_model_transform;
 
    if (!r_drawviewmodel.value)
        return;
 
    if (chase_active.value) //make sure we are in 1st person view
        return;
 
    if (!r_drawentities.value) //make sure we are drawing entities
        return;
 
    if (cl.items & IT_INVISIBILITY) //make sure we aren't invisable
        return;
 
    if (cl.stats[STAT_HEALTH] <= 0)	//make sure we aren't dead
        return;
 
    e = &cl.viewent;
    if (!e->model)	//make sure we have a model to draw
        return;
 
    // hack the depth range to prevent view model from poking into walls
    glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	//qmb :interpolation
	// fenix@io.com: model transform interpolation
	old_interpolate_model_transform = r_interpolate_model_t.value;
	Cvar_SetValueQuick(&r_interpolate_model_t, 0);
	R_DrawAliasModel (e);
	Cvar_SetValueQuick(&r_interpolate_model_t, old_interpolate_model_transform);
	//qmb :end
    glDepthRange (gldepthmin, gldepthmax);
}

/*
============
R_PolyBlend
============
*/
//qmb :gamma
//some code from LordHavoc early DP version
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;

	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	glDisable (GL_BLEND); // muff
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_ALPHA_TEST); //muff
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);	

	if (!g_drawing_refl)
	{
		// build the transformation matrix for the given view angles
		AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	}

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	// start MPO
	// we want to look from the mirrored origin's perspective when drawing reflections
	if (g_drawing_refl) {
		
		//===============
		float	distance;
		vec3_t	correctedNormal;
		vec3_t	viewVector;
		double	dot;
		//===============

		distance = DotProduct(r_origin, waterNormals[g_active_refl]) - g_waterDistance2[g_active_refl]; 
		
		VectorMA(r_refdef.vieworg, (distance*-2), waterNormals[g_active_refl], r_origin);

		temp[0] = g_refl_X[g_active_refl];	// sets PVS over x
		temp[1] = g_refl_Y[g_active_refl];	//  and y coordinate of water surface
		temp[2]	= g_refl_Z[g_active_refl];

		VectorSubtract(r_origin, temp, viewVector);
		dot = DotProduct (viewVector, waterNormals[g_active_refl]);

		VectorCopy(waterNormals[g_active_refl], correctedNormal);

		if (dot > 0) VectorInverse(correctedNormal);

		VectorMA(temp, 10, correctedNormal, temp); //scale temp position by correct water normal

		r_viewleaf = Mod_PointInLeaf (temp, cl.worldmodel);

	}
	// stop MPO

	// removed, caused flashes underwater bug
//	V_SetContentsColor (r_viewleaf->contents);

	if ((r_viewleaf->contents = CONTENTS_EMPTY) || (r_viewleaf->contents = CONTENTS_SOLID))
	{
		r_refdef.fovscale_x = 1;
		r_refdef.fovscale_y = 1;
	}

	V_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;

}

extern cvar_t	gl_farclip;

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h, farclip;

	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	
    screenaspect = ((float)r_refdef.vrect.width * r_refdef.fovscale_x) / (r_refdef.vrect.height * r_refdef.fovscale_y);
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;

	if (!g_drawing_refl) {
		glViewport (glx + x, gly + y2, w, h);;	// MPO : note this happens every frame interestingly enough
	}
	else
	{
		glViewport(0, 0, g_reflTexW, g_reflTexH);	// width/height of texture size, not screen size
	}

	if (gl_farclip.value) // sanity check
		farclip = max((int) gl_farclip.value, 4096);
	else
		farclip = 4096;
//	gluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);
	gluPerspective (r_refdef.fov_y,  screenaspect,  4,  farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up

	if (!g_drawing_refl)
	{
		glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
		glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
		glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
		glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);
	}
	else if (r_waterrefl.value)
	{
		R_DoReflTransform(true);
	}

	//
	// set drawing parms
	//
	if ((gl_cull.value) && (!g_drawing_refl))
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

void GL_Scissor (int x, int y, int width, int height)
{
	glScissor(x, vid.realheight - (y + height),width,height);
}

void GL_ScissorTest(int state)
{
	if (state == true)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	extern void DrawFlares(void);
	extern void R_GatherLights(void), R_Shader_ShowLights(void);
	GLenum error;

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009eh, what the:&r %s\n", gluErrorString(error));

	if(!g_drawing_refl && r_waterrefl.value)
	{
		R_clear_refl();								//clear our reflections found in last frame
		R_RecursiveFindRefl(cl.worldmodel->nodes, r_refdef.vieworg);	//find reflections for this frame
		R_UpdateReflTex();					//render reflections to textures
	}
	//else
		//R_clear_refl();

	
	R_SetupGL ();    //dukey moved here ..

	if (r_letterbox.value)
	{
		int y = vid.realheight / (100 / r_letterbox.value);
		y /= 2;
		GL_Scissor(0, y, vid.realwidth, vid.realheight - y*2);
		GL_ScissorTest(true);
	}

	R_SetupFrame ();

	R_SetFrustum ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009glsetup:&r %s\n", gluErrorString(error));

	R_MarkLeaves ();	// done here so we know if we're in water

	R_GatherLights();

	R_DrawWorld ();		// adds static entities to the list

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009map render:&r %s\n", gluErrorString(error));


	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009entities:&r %s\n", gluErrorString(error));

	R_RenderDlights ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009aw dang:&r %s\n", gluErrorString(error));

	R_DrawParticles ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009particles:&r %s\n", gluErrorString(error));

	DrawFlares();

	R_DrawViewModel ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: &c009viewmodel:&r %s\n", gluErrorString(error));

	if (r_depthsort.value)
		RQ_RenderDistAndClear(); // Entar: depth sorting

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: depth sort:&r %s\n", gluErrorString(error));

	if (r_letterbox.value)
	{
		GL_Scissor(0, 0, vid.realwidth, vid.realheight);
		GL_ScissorTest(false);
	}

	if (r_editlights.value)
		R_Shader_ShowLights();

//	R_DrawDebugReflTexture();
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	//qmb :map leak check
	if (gl_checkleak.value){
		gl_clear.value = 1;
		glClearColor (1,0,0,1);
	}else
		glClearColor (0,0,0,0);

	if (gl_stencil){
		glClearStencil(1);

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
		else
			glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
	}else {
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
		else
			glClear (GL_DEPTH_BUFFER_BIT);	//stencil bit ignored when no stencil buffer
	}

	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

#if 0 // old bloom
void GL_Set2D_Special (void)
{
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	//glOrtho  (0, vid.width, 0, vid.height, -99999, 99999);
	glOrtho  (0, vid.realwidth, 0, vid.realheight, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

void R_PostProcess (void)
{
	float texwidth;
	float vw, vh;

	float vs;
	float vt;

	if (!gl_bloom.value || !gl_bloom_intensity.value)
		return;

	// texture size is the smallest power of two which can copy the whole screen
	// that way we don't lose any detail

//	Con_Printf ("realheight is %i.\n", vid.realheight);
//	Con_Printf ("realwidth is %i.\n", vid.realwidth);
	vw = vid.realwidth;
	vh = vid.realheight;

	texwidth = 1;
	while (texwidth < vid.width)
	{
		texwidth *= 2;
	}

	vs = vw / texwidth;
	vt = vh / texwidth;

	// Then switch to 2d to rerender these textures
	// Special version uses the opengl coordinate system, with 0 at the bottom.
	// This makes all the screen grabbing and stuff far easier.
	GL_Set2D_Special ();

	//glColor4f(1, 1, 1, 1);
	glColor4f(1 - (gl_bloom_darken.value / 10), 1 - (gl_bloom_darken.value / 10), 1 - (gl_bloom_darken.value / 10), 1);

	if (gl_bloom.value)
	{
#if 1
		// full screen size
		float htex = texwidth;
		float hvh = vh;
		float hvw = vw;
		float hvs = vs;
		float hvt = vt;
#endif
#if 0
		// half screen size
		float htex = texwidth * 0.5;
		float hvh = vh * 0.5;
		float hvw = vw * 0.5;
		float hvs = vs;
		float hvt = vt;
#endif
#if 0
		// nearest power of 2 that fits in screen width
		float htex = texwidth * 0.5;
		float hvw = htex;
		float hvh = (htex / vw) * vh;
		float hvs = 1;
		float hvt = vh / vw;
#endif
		float bs = 1 / htex;
		float bs2 = bs * 2;

		// copy the screen
		glBindTexture(GL_TEXTURE_2D, screen_pptexture0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texwidth, texwidth, 0);

		// Grab it smaller into the blur texture
		glBindTexture(GL_TEXTURE_2D, screen_blurtexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texwidth, texwidth, 0);

		// Copy the smaller one
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, htex, htex, 0);

		// Grab the darkened one
//		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, htex, htex, 0);

		// Blur the darkened one
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		
		if (gl_bloom.value == 1)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else if (gl_bloom.value == 2)
			glBlendFunc (GL_SRC_COLOR, GL_ONE);
		else if (gl_bloom.value == 3)
			glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR);
		else if (gl_bloom.value == 4)
			glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);

		// tested + NO (second parameter with GL_SRC_ALPHA): GL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA,
		// GL_ONE_MINUS_DST_ALPHA, GL_ZERO
		// tested + NO (first parameter with GL_ONE): GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA
		//glColor4f(1, 1, 1, 0.01 + (gl_bloom_intensity.value / 10));
		glColor4f(1 - (gl_bloom_darken.value / 10), 1 - (gl_bloom_darken.value / 10), 1 - (gl_bloom_darken.value / 10), 0.035 + (gl_bloom_intensity.value / 10));
		
		// Horizontally...

		glBegin(GL_QUADS);

		glTexCoord2f(0 + bs, 0);
		glVertex2f(0, 0);

		glTexCoord2f(hvs + bs, 0);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs + bs, hvt);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0 + bs, hvt);
		glVertex2f(0, hvh);



		glTexCoord2f(0 + bs2, 0);
		glVertex2f(0, 0);

		glTexCoord2f(hvs + bs2, 0);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs + bs2, hvt);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0 + bs2, hvt);
		glVertex2f(0, hvh);



		glTexCoord2f(0 - bs, 0);
		glVertex2f(0, 0);

		glTexCoord2f(hvs - bs, 0);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs - bs, hvt);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0 - bs, hvt);
		glVertex2f(0, hvh);



		glTexCoord2f(0 - bs2, 0);
		glVertex2f(0, 0);

		glTexCoord2f(hvs - bs2, 0);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs - bs2, hvt);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0 - bs2, hvt);
		glVertex2f(0, hvh);

		glEnd();


		// Vertically
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, htex, htex, 0);

		glBegin(GL_QUADS);

		glTexCoord2f(0, 0 + bs);
		glVertex2f(0, 0);

		glTexCoord2f(hvs, 0 + bs);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs, hvt + bs);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0, hvt + bs);
		glVertex2f(0, hvh);



		glTexCoord2f(0, 0 + bs2);
		glVertex2f(0, 0);

		glTexCoord2f(hvs, 0 + bs2);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs, hvt + bs2);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0, hvt + bs2);
		glVertex2f(0, hvh);



		glTexCoord2f(0, 0 - bs);
		glVertex2f(0, 0);

		glTexCoord2f(hvs, 0 - bs);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs, hvt - bs);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0, hvt - bs);
		glVertex2f(0, hvh);



		glTexCoord2f(0, 0 - bs2);
		glVertex2f(0, 0);

		glTexCoord2f(hvs, 0 - bs2);
		glVertex2f(hvw, 0);

		glTexCoord2f(hvs, hvt - bs2);
		glVertex2f(hvw, hvh);

		glTexCoord2f(0, hvt - bs2);
		glVertex2f(0, hvh);

		glEnd();

		glDisable(GL_BLEND);

/*		// copy fully blurred
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, htex, htex, 0);

		// draw the original screen again
		glBindTexture(GL_TEXTURE_2D, screen_pptexture0);

		// now add the blurred over the top
		if (gl_bloom_showonly.value)
		{
			glDisable(GL_BLEND);
		}
		else
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		}

		glBindTexture(GL_TEXTURE_2D, screen_blurtexture);

		glColor3f(gl_bloom.value, gl_bloom.value, gl_bloom.value);*/

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glColor4f(1, 1, 1, 1);
	}

	// grab the screen again
//	glBindTexture(GL_TEXTURE_2D, screen_pptexture0);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texwidth, texwidth, 0);

	// draw it with gamma this time
//	if (v_gamma.value > 0.2 && v_gamma.value < 1)
//		Cvar_SetValue("gamma", 1);
//	else if (v_gamma.value < 1)
//		Cvar_SetValue("gamma", 1);

	GL_Set2D();
}
#endif

extern cvar_t r_bloom;

extern double	time1;

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	extern	void R_BloomBlend (int bloom);
	GLenum error;
	float colors[4] = {0.0, 0.0, 0.0, 1.0};

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error:&c009Finish&r %s\n", gluErrorString(error));

	R_Clear ();

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error:&c009Clear&r %s\n", gluErrorString(error));

	// render normal view

	if ((gl_fogglobal.value))//&&(CONTENTS_EMPTY==r_viewleaf->contents))
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
			colors[0] = gl_fogred.value;
			colors[1] = gl_foggreen.value;
			colors[2] = gl_fogblue.value; 
		glFogfv(GL_FOG_COLOR, colors); 
		glFogf(GL_FOG_START, gl_fogstart.value); 
		glFogf(GL_FOG_END, gl_fogend.value); 
		glFogf(GL_FOG_DENSITY, 0.2f);
		glEnable(GL_FOG);

		if (r_errors.value && developer.value)
			while ( (error = glGetError()) != GL_NO_ERROR )
				Con_DPrintf ("&c900Error:&c009Fog&r %s\n", gluErrorString(error));
	}

	R_RenderScene ();

	if (gl_fogglobal.value)
		glDisable(GL_FOG);

	if ((gamemode == GAME_RO && (v_hurtblur.value && !g_drawing_refl)) ||
		(gamemode != GAME_RO && (v_hurtblur.value && !g_drawing_refl && cl.stats[STAT_HEALTH] < v_hurtblur.value)))
	{ 
		int vwidth = 1, vheight = 1;  // motion blur code from Spike
		float vs, vt;				  // edited by Entar
		float	blur;


		if (gamemode == GAME_RO)
			blur = v_hurtblur.value;
		else
			blur = (v_hurtblur.value - cl.stats[STAT_HEALTH]) / 40.00;

		if (blur > 0.88)
			blur = 0.88f;

		while (vwidth < glwidth) 
		{ 
			vwidth *= 2; 
		} 
		while (vheight < glheight) 
		{ 
			vheight *= 2; 
		} 
		//Con_Printf ("%f\n", blur);
		glViewport (glx, gly, vid.realwidth, vid.realheight); 

		//GL_Bind(sceneblur_texture); 
		glBindTexture(GL_TEXTURE_2D, sceneblur_texture);

		// go 2d 
		glMatrixMode(GL_PROJECTION); 
		glPushMatrix(); 
		glLoadIdentity (); 
//		glOrtho  (0, glwidth, 0, glheight, -99999, 99999); 
		glOrtho  (0, vid.realwidth, 0, vid.realheight, -99999, 99999);
		glMatrixMode(GL_MODELVIEW); 
		glPushMatrix(); 
		glLoadIdentity (); 

		//blend the last frame onto the scene 
		//the maths is because our texture is over-sized (must be power of two) 
		vs = (float)glwidth / vwidth; 
		vt = (float)glheight / vheight; 
		glDisable (GL_DEPTH_TEST); 
		glDisable (GL_CULL_FACE); 
		glDisable (GL_ALPHA_TEST); 
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glColor4f(1, 1, 1, 0.8); 
		glColor4f(1, 1, 1, blur);
		glBegin(GL_QUADS); 
		glTexCoord2f(0, 0); 
		glVertex2f(0, 0); 
		glTexCoord2f(vs, 0); 
		glVertex2f(glwidth, 0); 
		glTexCoord2f(vs, vt);
		glVertex2f(glwidth, glheight);
		glTexCoord2f(0, vt); 
		glVertex2f(0, glheight);
		glEnd(); 

		glMatrixMode(GL_PROJECTION); 
		glPopMatrix(); 
		glMatrixMode(GL_MODELVIEW); 
		glPopMatrix(); 

		//copy the image into the texture so that we can play with it next frame too! 
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, glx/2, gly/2, vwidth, vheight, 0); 
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
	} 
 
	if (!g_drawing_refl)
		R_PolyBlend ();

	if (!g_drawing_refl) // this doesn't need to be done twice
		R_BloomBlend(true);//BLOOMS
//		R_PostProcess (); // light blooms

	if (r_errors.value && developer.value)
		while ( (error = glGetError()) != GL_NO_ERROR )
			Con_DPrintf ("&c900Error: AWW DANG &r %s\n", gluErrorString(error));
}
