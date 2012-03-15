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

// refresh.h -- public interface to refresh functions

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;


typedef struct entity_s
{
	qboolean				forcelink;		// model changed

	int						updated;

	entity_state_t			baseline;		// to fill in defaults in updates

	double					msgtime;		// time of last update
	vec3_t					msg_origins[2];	// last two updates (0 is newest)	
	vec3_t					origin;
	vec3_t					msg_angles[2];	// last two updates (0 is newest)
	vec3_t					angles;	
	struct model_s			*model;			// NULL = no model
	struct efrag_s			*efrag;			// linked list of efrags
	int						frame;
	float					syncbase;		// for client-side animations
	byte					*colormap;
	int						effects;		// light, particals, etc
	int						skinnum;		// for Alias models
	int						visframe;		// last frame this entity was
											//  found in an active leaf
											
	int						dlightframe;	// dynamic lighting
	int						dlightbits;
	
// FIXME: could turn these into a union
	int						trivial_accept;
	struct mnode_s			*topnode;		// for bmodels, first world node
											//  that splits bmodel, or NULL if
											//  not split
	//qmb :model interpolation
	// fenix@io.com: model animation interpolation
	float                   frame_start_time;
	float                   frame_interval;
	int                     pose1; 
	int                     pose2;
	
	// fenix@io.com: model transform interpolation
	float                   translate_start_time;
	vec3_t                  origin1;
	vec3_t                  origin2;

	float                   rotate_start_time;
	vec3_t                  angles1;
	vec3_t                  angles2;
	//qmb :end

	//qmb :add model flags
	int						flags;

	// LordHavoc: added support for Q2 interpolation
	int				draw_lastpose, draw_pose; 	// for interpolation
	float				draw_lerpstart; 		// for interpolation
	struct model_s			*draw_lastmodel; 		// for interpolation

	float				   alpha; // new for alpha code
} entity_t;

/*
#define R_LIGHTSHADERPERMUTATION_SPECULAR 1
#define R_LIGHTSHADERPERMUTATION_FOG 2
#define R_LIGHTSHADERPERMUTATION_CUBEFILTER 4
#define R_LIGHTSHADERPERMUTATION_OFFSETMAPPING 8
#define R_LIGHTSHADERPERMUTATION_LIMIT 16
// maximum possible permutations (used for array sizes)
#define R_SHADERPERMUTATION_LIMIT 64
*/

#define R_MAX_SHADER_LIGHTS 128
#define R_DROPLIGHT_MAX_LIGHTS 3
#define R_MIN_SHADER_DLIGHTS 120 // where the dynamic light range starts

typedef struct RenderState_LightShader_s
{
	// locations of uniform variables in the GLSL program object
	// (-1 means no such uniform)

	// vertex shader uniforms
	int lightPosition;
	//int eyePosition;

	// view->light matrix
	int viewToLightMatrix;

	// fragment shader uniforms
	int diffuseSampler;
	int cubemapSampler;
	// int normalSampler;
	int specularSampler;

	int lightColor;
	int lightMaxDistance;

	/*int loc_LightColor;
	int loc_OffsetMapping_Scale;
	int loc_OffsetMapping_Bias;
	int loc_SpecularPower;
	int loc_FogRangeRecip;
	int loc_AmbientScale;
	int loc_DiffuseScale;
	int loc_SpecularScale;
	int loc_Texture_Normal;
	int loc_Texture_Color;
	int loc_Texture_Gloss;
	int loc_Texture_Cube;
	int loc_Texture_FogMask;*/

	// GLSL program object
	unsigned int programObject;

	// current view matrix
	// TODO: move this into r_refdef and change the engine to use it?
	matrix4x4_t worldToViewMatrix;
}
RenderState_LightShader;

typedef struct R_ShaderLight_Definition {
	// world-space coordinates
	vec3_t origin;
	// color of the light
	vec3_t color;
	// intensity of the light (also the max visible distance of it)
	float maxDistance;
	// angles
	vec3_t angles;
	// whether light should render shadows
	int shadow;
	// light filter
	char cubemapname[64];
	// lightstyle (flickering, etc)
	int style;
	// intensity of corona to render
	float corona;
	// radius scale of corona to render (1.0 means same as light radius)
	float coronasizescale;
	// ambient intensity to render
	float ambientscale;
	// diffuse intensity to render
	float diffusescale;
	// specular intensity to render
	float specularscale;
	// LIGHTFLAG_* flags
	int flags;
} R_ShaderLight_Definition;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably allways be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;
	
	float		fov_x, fov_y;

	float fovscale_x, fovscale_y; // for DP water warping

	int			ambientlight;

	// shaders
	//RenderState_ShaderPermutation shader_standard[R_LIGHTSHADERPERMUTATION_LIMIT];
	RenderState_LightShader lightShader;
} refdef_t;

//
// refresh
//
extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

void R_Init (void);
void R_InitTextures (void);
void R_InitEfrags (void);
void R_RenderView (void);		// must set r_refdef first
								// called whenever r_refdef or vid change
void R_InitSky (struct texture_s *mt);	// called at level load
int R_LoadSky (char *newname);
void R_CurrentCoord_f (void);
extern int	R_Skybox;

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (void);


void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end, int type);

#ifdef QUAKE2
void R_DarkFieldParticles (entity_t *ent);
#endif
void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

void R_PushDlights (void);

// shader handling
void R_Shader_Init();
void R_Shader_Reset();
void R_Shader_Quit();
int R_LoadWorldLights(void);
void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void);

qboolean R_Shader_CanRenderLights();

void R_Shader_SetLight( unsigned int lightIndex, const R_ShaderLight_Definition *light );
// returns -1 if the light couldnt be added
int R_Shader_AddLight( const R_ShaderLight_Definition *light );
void R_Shader_ResetLight( unsigned int lightIndex );
qboolean R_Shader_IsLightIndexUsed( unsigned int lightIndex );

qboolean R_Shader_IsLightInScopeByPoint( unsigned int lightIndex, const vec3_t renderOrigin );

typedef struct mleaf_s mleaf_t;
qboolean R_Shader_IsLightInScopeByLeaf( unsigned int lightIndex, mleaf_t *leaf );

/*
image unit 1 contains the diffuse map
image unit 2 contains the cube map
image unit 2 contains the normal map (not yet)
image unit 3 contains the specular map (not yet)

texture coordinate unit 1 contains the texture coordinates
*/
void R_Shader_StartLightRendering();
qboolean R_Shader_StartLightPass( unsigned int lightIndex );
void R_Shader_FinishLightPass();
void R_Shader_FinishLightRendering();