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
// r_misc.c

#include "quakedef.h"

extern entity_t *currententity;

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

unsigned int celtexture = 0;
unsigned int vertextexture = 0;

char	crossfound[32][64];
int		found = 0;
int Crosshair_Function(char *filename, int size, void *cookie)
{
	strcpy(crossfound[found++], filename);
	return true;
}

void R_InitOtherTextures (void)
{
//FIXME - remove ifdef
#ifdef MENUENGINE
	extern	void M_Engine_Load (void);
#endif
	extern	void R_InitBloomTextures( void );
	float	cellData[32] = {0.2f,0.2f,0.2f,0.2f,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
	float	cellFull[32][3];
	float	vertexFull[32][3];
	int		i;

	extern int crosshair_tex[32], sceneblur_texture;

//FIXME - remove ifdef
#ifdef MENUENGINE
	M_Engine_Load ();
#endif

	shinetex_glass = GL_LoadTexImage ("textures/shine_glass", false, true);
	shinetex_chrome = GL_LoadTexImage ("textures/shine_chrome", false, true);
	underwatertexture = GL_LoadTexImage ("textures/water_caustic", false, true);
	highlighttexture = GL_LoadTexImage ("gfx/highlight", false, true);
	sceneblur_texture = texture_extension_number;
	texture_extension_number++;

	R_InitBloomTextures();//BLOOMS

//	found = COM_MultipleSearch("textures/crosshairs/*", crossfound, 32);
	COM_EnumerateFiles ("textures/crosshairs/*", Crosshair_Function, NULL);

	if (extra_info)
		Con_Printf("\nFound %i crosshairs.\n", found);

	for (i=0;i<found && i<32;i++)
	{
		Con_DPrintf("%i. %s\n",i,crossfound[i]);
		crosshair_tex[i] = GL_LoadTexImage (crossfound[i], false, false);
//		Z_Free(crossfound[i]);
	}
	Con_Printf("\n");

	for (i=0;i<32;i++)
		cellFull[i][0] = cellFull[i][1] = cellFull[i][2] = cellData[i];

	for (i=0;i<32;i++)
	{
		vertexFull[i][0] = vertexFull[i][1] = vertexFull[i][2] = ((i*2)/24.0f);
	}

	//cell shading stuff...
//	glGenTextures (1, &celtexture);			// Get A Free Texture ID
	celtexture = texture_extension_number;
	texture_extension_number++;
	glBindTexture (GL_TEXTURE_1D, celtexture);	// Bind This Texture. From Now On It Will Be 1D
	// For Crying Out Loud Don't Let OpenGL Use Bi/Trilinear Filtering!
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// Upload
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, cellFull);

	//vertex shading stuff...
//	glGenTextures (1, &vertextexture);			// Get A Free Texture ID
	vertextexture = texture_extension_number;
	texture_extension_number++;
	glBindTexture (GL_TEXTURE_1D, vertextexture);	// Bind This Texture. From Now On It Will Be 1D
	// For Crying Out Loud Don't Let OpenGL Use Bi/Trilinear Filtering!
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// Upload
	glTexImage1D (GL_TEXTURE_1D, 0, GL_RGB, 32, 0, GL_RGB , GL_FLOAT, vertexFull);

	// create the default cubemap (white)
	{
		unsigned cubemap[6] = 
		{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	        
		// TODO: do something if GL_LoadCubemapTexture returns 0 (error)
		GL_LoadCubemapTexture( "defaultcubemap", 1, (byte*) cubemap, true, true, 4 );
	}

}

/*
===============
R_Init
===============
*/
extern void R_Init_Shaders(void);

void R_Init (void)
{	
	extern	void RQ_Init(void);
	extern	byte *hunk_base;

	R_InitParticles ();
	//qmb :no longer used
	//R_InitParticleTexture ();
	R_InitTextures ();
	R_InitOtherTextures ();
	RQ_Init(); // renderque

	R_init_refl(4);	// MPO : init reflections

//	R_Init_Shaders ();

	playertextures = texture_extension_number;
	texture_extension_number += MAX_SCOREBOARD;
}

void R_Init_Register (void)
{
	extern	cvar_t gl_finish, r_waterrefl, r_dirlighting, r_quadshell, gl_farclip, r_coronas, r_coronas_mode, scr_centerfade,
		r_bloom, r_bloom_darken, r_bloom_alpha, r_bloom_diamond_size, r_bloom_intensity, r_bloom_sample_size, r_bloom_fast_sample,
		chase_anglesfix, cl_viewangles_down, cl_viewangles_up, show_stats, r_skyfactor, cl_gunx, cl_guny, cl_gunz;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("currentcoord", R_CurrentCoord_f);

	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_speeds);

	Cvar_RegisterVariable (&r_depthsort);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);

	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_nocolors);

	Cvar_RegisterVariable (&gl_keeptjunctions);

	//qmb :extra cvars
	Cvar_RegisterVariable (&gl_detail);
	Cvar_RegisterVariable (&gl_shiny);
	Cvar_RegisterVariable (&gl_caustics);
	Cvar_RegisterVariable (&gl_dualwater);
	Cvar_RegisterVariable (&gl_ammoflash);

	//Entar :extra cvars
	Cvar_RegisterVariable (&v_gunmove);
	Cvar_RegisterVariable (&r_skyfactor);
	Cvar_RegisterVariable (&r_bloom);
	Cvar_RegisterVariable (&r_bloom_darken);
	Cvar_RegisterVariable (&r_bloom_alpha);
	Cvar_RegisterVariable (&r_bloom_diamond_size);
	Cvar_RegisterVariable (&r_bloom_intensity);
	Cvar_RegisterVariable (&r_bloom_sample_size);
	Cvar_RegisterVariable (&r_bloom_fast_sample);

	Cvar_RegisterVariable (&r_shadow_realtime_dlight);
	Cvar_RegisterVariable (&r_shadow_realtime_world);
	Cvar_RegisterVariable (&r_shadow_realtime_draw_world);
	Cvar_RegisterVariable (&r_shadow_realtime_draw_models);
	Cvar_RegisterVariable (&r_shadow_realtime_world_lightmaps);
	Cvar_RegisterVariable (&r_shadow_lightintensityscale);
	Cvar_RegisterVariable (&r_editlights);
	Cvar_RegisterVariable (&r_editlights_quakelightsizescale);

	Cvar_RegisterVariable (&slowmo);
	Cvar_RegisterVariable (&v_hurtblur);
	if (gamemode == GAME_RO)
		Cvar_Set("v_hurtblur", "0");
	Cvar_RegisterVariable (&r_waterrefl);
	Cvar_RegisterVariable (&r_test);
	Cvar_RegisterVariable (&r_dirlighting);
	Cvar_RegisterVariable (&r_quadshell);
	Cvar_RegisterVariable (&gl_farclip);
	Cvar_RegisterVariable (&r_coronas);
	Cvar_RegisterVariable (&r_coronas_mode);
	Cvar_RegisterVariable (&scr_centerfade);
	Cvar_RegisterVariable (&chase_anglesfix);
	Cvar_RegisterVariable (&cl_viewangles_down);
	Cvar_RegisterVariable (&cl_viewangles_up);
	Cvar_RegisterVariable (&cl_gunx);
	Cvar_RegisterVariable (&cl_guny);
	Cvar_RegisterVariable (&cl_gunz);
	

	// fenix@io.com: register new cvars for model interpolation
	Cvar_RegisterVariable (&r_interpolate_model_a);
	Cvar_RegisterVariable (&r_interpolate_model_t);
	Cvar_RegisterVariable (&r_wave);
	Cvar_RegisterVariable (&gl_fog);
	Cvar_RegisterVariable (&gl_fogglobal);
	Cvar_RegisterVariable (&gl_fogred);
	Cvar_RegisterVariable (&gl_foggreen);
	Cvar_RegisterVariable (&gl_fogblue);
	Cvar_RegisterVariable (&gl_fogstart);
	Cvar_RegisterVariable (&gl_fogend);
	Cvar_RegisterVariable (&gl_conalpha);
	Cvar_RegisterVariable (&gl_test);
	Cvar_RegisterVariable (&gl_checkleak);
	Cvar_RegisterVariable (&r_skydetail);
	Cvar_RegisterVariable (&r_sky_x);
	Cvar_RegisterVariable (&r_sky_y);
	Cvar_RegisterVariable (&r_sky_z);

	Cvar_RegisterVariable (&r_errors);
	Cvar_RegisterVariable (&r_fullbright);

	Cvar_RegisterVariable (&r_modeltexture);
	Cvar_RegisterVariable (&r_celshading);
	Cvar_RegisterVariable (&r_outline);
	Cvar_RegisterVariable (&r_vertexshading);
	Cvar_RegisterVariable (&gl_npatches);

	Cvar_RegisterVariable (&gl_anisotropic);

	Cvar_RegisterVariable (&gl_24bitmaptex);
	//qmb :end

	Cvar_RegisterVariable (&show_stats);
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum) // FIXME!!!!!
{
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	unsigned int		i, j, s;//, n;
	model_t	*model;
	aliashdr_t *paliashdr;
	byte	*original;
	unsigned	pixels[512*256], *out;
	unsigned	scaled_width, scaled_height;
	int			inwidth, inheight;
	byte		*inrow;
	unsigned	frac, fracstep;

	GL_SelectTexture(GL_TEXTURE0_ARB);

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors &15)<<4;

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;
				
		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}

	//
	// locate the original skin pixels
	//
	currententity = &cl_entities[1+playernum];
	model = currententity->model;
	if (!model)
		return;		// player doesn't have a model yet
	if (model->type != mod_alias)
		return; // only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	s = paliashdr->skinwidth * paliashdr->skinheight;
	if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins) {
		Con_Printf("(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	} else
		original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
	if (s & 3)
		Sys_Error ("R_TranslatePlayerSkin: s&3");

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
	GL_Bind(playertextures + playernum);
//	glBindTexture(GL_TEXTURE_2D, paliashdr->gl_texturenum[currententity->skinnum][n]);

	scaled_width = gl_max_size.value < 512 ? gl_max_size.value : 512;
	scaled_height = gl_max_size.value < 256 ? gl_max_size.value : 256;

	// allow users to crunch sizes down even more if they want
//	scaled_width >>= (int)gl_playermip.value;
//	scaled_height >>= (int)gl_playermip.value;

	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table[translate[i]];

	out = pixels;
	fracstep = inwidth*0x10000/scaled_width;
	for (i=0 ; i<scaled_height ; i++, out += scaled_width)
	{
		inrow = original + inwidth*(i*inheight/scaled_height);
		frac = fracstep >> 1;
		for (j=0 ; j<scaled_width ; j+=4)
		{
			out[j] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+1] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+2] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+3] = translate32[inrow[frac>>16]];
			frac += fracstep;
		}
	}
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// ported from DP, edited by Entar
void CL_ParseEntityLump(void)
{
	const char *data;
	char key[128], value[16384];

	data = cl.worldmodel->entities;
	if (!data)
		return;
	if (!COM_ParseTokenConsole(&data))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseTokenConsole(&data))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strlcpy (key, com_token + 1, sizeof (key));
		else
			strlcpy (key, com_token, sizeof (key));
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseTokenConsole(&data))
			return; // error
		strlcpy (value, com_token, sizeof (value));
		if (!strcmp("sky", key))
			R_LoadSky(value);
		else if (!strcmp("skyname", key)) // non-standard, introduced by QuakeForge... sigh.
			R_LoadSky(value);
		else if (!strcmp("qlsky", key)) // non-standard, introduced by QuakeLives (EEK)
			R_LoadSky(value);
/*		else if (!strcmp("fog", key))
			sscanf(value, "%f %f %f %f", &r_refdef.fog_density, &r_refdef.fog_red, &r_refdef.fog_green, &r_refdef.fog_blue);
		else if (!strcmp("fog_density", key))
			r_refdef.fog_density = atof(value);
		else if (!strcmp("fog_red", key))
			r_refdef.fog_red = atof(value);
		else if (!strcmp("fog_green", key))
			r_refdef.fog_green = atof(value);
		else if (!strcmp("fog_blue", key))
			r_refdef.fog_blue = atof(value);*/
	}
}

/*
===============
R_NewMap
===============
*/
extern char lastscript[64];
void R_NewMap (void)
{
	int		i;
	
	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;

//	R_ClearParticles ();
	LoadParticleScript(lastscript);
	R_ClearBeams ();
	Clear_Flares();

	R_Shader_Reset();

	GL_BuildLightmaps ();

	// mh - auto water trans begin 
	// set r_wateralpha here if you want... 
	if (cl.worldmodel->watertrans) 
		Con_Printf ("Map vis'ed for translucent water\n"); 
	else 
		Con_Printf ("Map NOT vis'ed for translucent water\n"); 
	// mh - auto water trans end 

	CL_ParseEntityLump();

	R_Shader_Init();

	if (!R_LoadWorldLights())
		R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_FloatTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_FloatTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer  (GL_BACK);
	GL_EndRendering ();
}