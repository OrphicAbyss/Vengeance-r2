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
// disable data conversion warnings

#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA
  
#ifdef _WIN32
#include <windows.h>
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#include <GL/gl.h>
#include <GL/glu.h>

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

// wgl uses APIENTRY
#ifndef APIENTRY
#define APIENTRY
#endif
// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

// 1-byte character string
typedef char GLcharARB;
// 4-byte integer handle to a shader object or program object
typedef unsigned int GLhandleARB;

extern	int		texture_extension_number;
extern	int		texture_mode;
extern	int		gl_textureunits;	//qmb :multitexture stuff
extern	float	gldepthmin, gldepthmax;

extern	float	r_world_matrix[16];

void	GL_Upload32		(GLenum target, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha);
int		GL_Upload8		(GLenum target, byte *data, int width, int height,  qboolean mipmap, qboolean alpha);
int		GL_LoadTexture	(char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel);
int		GL_LoadCubemapTexture (char *identifier, int size, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel);
int		GL_LoadTexImage (char *filename, qboolean complain, qboolean mipmap);
int		GL_LoadCubeTexImage (char* basename, qboolean complain, qboolean mipmap);

int		GL_FindTexture	(char *identifier);

extern	int glx, gly, glwidth, glheight;

#define BACKFACE_EPSILON	0.01

extern GLenum TEXTURE0;
extern GLenum TEXTURE1;
extern GLenum TEXTURE2;
extern GLenum TEXTURE3;

void R_TimeRefresh_f (void);
void R_LoadSky_f (void);		//for loading a new skybox during play
texture_t *R_TextureAnimation (texture_t *base);

//====================================================

extern	entity_t	r_worldentity;

extern	int			r_visframecount;	//used in pvs poly culling
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys; //poly counts for brush and alias models

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	texture_t	*r_notexture_mip;
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int shinetex_glass, shinetex_chrome, underwatertexture, highlighttexture, screen_pptexture0, screen_blurtexture;

extern	int	playertextures;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_shadows;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;

extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_keeptjunctions;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_nocolors;

//qmb :extra cvars
extern  cvar_t  gl_detail;
extern  cvar_t  gl_shiny;
extern  cvar_t  gl_caustics;
extern  cvar_t  gl_dualwater;
extern  cvar_t  gl_ammoflash;

//Entar :extra cvars
extern	cvar_t	v_gunmove;
extern	cvar_t	gl_bloom;
extern	cvar_t	gl_bloom_darken;
extern	cvar_t	gl_bloom_showonly;
extern	cvar_t	gl_bloom_change;
extern	cvar_t	gl_bloom_intensity;
extern	cvar_t	slowmo;
extern	cvar_t	v_hurtblur;
extern	cvar_t	r_depthsort;
extern	cvar_t	r_waterrefl;
extern	cvar_t	r_test;
extern	cvar_t	r_dirlighting;
extern  cvar_t  r_shadow_realtime_dlight, r_shadow_realtime_world, r_shadow_realtime_draw_world, r_shadow_realtime_draw_models, r_shadow_realtime_world_lightmaps, r_shadow_lightintensityscale;
extern	cvar_t	r_editlights, r_editlights_quakelightsizescale;

// fenix@io.com: model interpolation
//extern  cvar_t  r_interpolate_model_animation;
//extern  cvar_t  r_interpolate_model_transform;
extern  cvar_t  r_interpolate_model_a;
extern  cvar_t  r_interpolate_model_t;
extern	cvar_t	r_wave;
extern	cvar_t	gl_fog;
extern	cvar_t	gl_fogglobal;
extern	cvar_t	gl_fogred;
extern	cvar_t	gl_foggreen;
extern	cvar_t	gl_fogblue;
extern	cvar_t	gl_fogstart;
extern	cvar_t	gl_fogend;
extern	cvar_t	gl_test;
extern	cvar_t	gl_conalpha;
extern	cvar_t	gl_checkleak;
extern	cvar_t	r_skydetail;
extern	cvar_t	r_sky_x;
extern	cvar_t	r_sky_y;
extern	cvar_t	r_sky_z;

extern	cvar_t	r_errors;
extern	cvar_t	r_fullbright;

extern	cvar_t	r_modeltexture;
extern	cvar_t	r_celshading;
extern	cvar_t	r_vertexshading;

extern	cvar_t	gl_npatches;

extern	cvar_t	gl_anisotropic;

extern	cvar_t	r_outline;
extern	cvar_t	gl_24bitmaptex;

extern	cvar_t	sv_stepheight;
extern	cvar_t	sv_jumpstep;
//qmb :end


extern	int		gl_lightmap_format;
extern	int		gl_solid_format;
extern	int		gl_alpha_format;

extern	cvar_t	gl_max_size;

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);

// Multitexture
//QMB :arb multitexture
//with extra texture units :)
#define    GL_TEXTURE0_ARB					0x84C0
#define    GL_TEXTURE1_ARB					0x84C1
#define    GL_TEXTURE2_ARB					0x84C2
#define    GL_TEXTURE3_ARB					0x84C3
#define    GL_TEXTURE4_ARB					0x84C4
#define    GL_TEXTURE5_ARB					0x84C5

#define GL_ACTIVE_TEXTURE_ARB				0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB		0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB			0x84E2

#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

//QMB :overbright stuff
#define GL_COMBINE_EXT						0x8570
#define GL_COMBINE_RGB_EXT					0x8571
#define GL_COMBINE_ALPHA_EXT					0x8572
#define GL_RGB_SCALE_EXT					0x8573
#define GL_ADD_SIGNED						0x8574
#define GL_SUBTRACT							0x84E7
//QMB :end

//QMB :texture shaders
#define GL_DSDT8_NV							0x8709
#define GL_DSDT_NV							0x86F5
#define GL_TEXTURE_SHADER_NV				0x86DE
#define GL_SHADER_OPERATION_NV				0x86DF
#define GL_OFFSET_TEXTURE_2D_NV				0x86E8
#define GL_PREVIOUS_TEXTURE_INPUT_NV		0x86E4
#define GL_OFFSET_TEXTURE_MATRIX_NV			0x86E1
//QMB :end

//QMB :texture compression
#ifndef GL_ARB_texture_compression
#define GL_COMPRESSED_ALPHA_ARB				0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB			0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB	0x84EB
#define GL_COMPRESSED_INTENSITY_ARB			0x84EC
#define GL_COMPRESSED_RGB_ARB				0x84ED
#define GL_COMPRESSED_RGBA_ARB				0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB		0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB			0x86A0
#define GL_TEXTURE_COMPRESSED_ARB			0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A3
#endif
//QMB :end

//QMB :SGIS_generate_mipmap
#define GL_GENERATE_MIPMAP_SGIS				0x8191
//QMB :end

//QMB :NV_point_sprite
#define GL_POINT_SPRITE_NV					0x8861
#define GL_COORD_REPLACE_NV					0x8862
#define GL_POINT_SPRITE_R_MODE_NV			0x8863
//QMB :end

//QMB :points
#ifndef GL_SGIS_point_parameters
#define GL_POINT_SIZE_MIN_EXT				0x8126
#define GL_POINT_SIZE_MIN_SGIS				0x8126
#define GL_POINT_SIZE_MAX_EXT				0x8127
#define GL_POINT_SIZE_MAX_SGIS				0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT	0x8128
#define GL_POINT_FADE_THRESHOLD_SIZE_SGIS	0x8128
#define GL_DISTANCE_ATTENUATION_EXT			0x8129
#define GL_DISTANCE_ATTENUATION_SGIS		0x8129
#endif
//QMB :end


#define GL_OBJECT_COMPILE_STATUS_ARB			0x8B81
#define GL_OBJECT_LINK_STATUS_ARB				0x8B82

//QMB :npatches
#ifndef GL_ATI_pn_triangles
#define GL_ATI_pn_triangles 1

#define GL_PN_TRIANGLES_ATI							0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI	0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI				0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI				0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI		0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI		0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI		0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI		0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI   0x87F8
#endif
//QMB :end

#ifndef _WIN32
#define APIENTRY /* */
#endif

#define GL_COMBINE							0x8570
#define GL_COMBINE_RGB						0x8571

#define GL_SOURCE0_RGB                    0x8580
#define GL_SOURCE1_RGB                    0x8581

#define GL_OPERAND0_RGB                   0x8590
#define GL_OPERAND1_RGB                   0x8591


#define GL_CONSTANT                       0x8576
#define GL_PRIMARY_COLOR                  0x8577


typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lp1DMTexFUNC) (GLenum, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);
//QMB :NV_point_sprite
typedef void (APIENTRY *pointPramFUNCv) (GLenum pname, const GLfloat *params);
typedef void (APIENTRY *pointPramFUNC) (GLenum pname, const GLfloat params);
//QMB :npatches
typedef void (APIENTRY *pnTrianglesIatiPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *pnTrianglesFaitPROC)(GLenum pname, GLfloat param);

typedef void (APIENTRY *qglClientActiveTexturePROC) (GLenum num);

extern lpMTexFUNC qglMTexCoord2fARB;
extern lp1DMTexFUNC qglMTexCoord1fARB;
extern lpSelTexFUNC qglSelectTextureARB;
extern pointPramFUNCv qglPointParameterfvEXT;
extern pointPramFUNC qglPointParameterfEXT;
extern pnTrianglesIatiPROC glPNTrianglesiATI;

extern qglClientActiveTexturePROC qglClientActiveTexture; 

extern qboolean gl_support_GLSL_shaders;
extern qboolean gl_support_cubemaps;

extern void (GLAPIENTRY *qglDeleteObjectARB)(GLhandleARB obj);
extern GLhandleARB (GLAPIENTRY *qglGetHandleARB)(GLenum pname);
extern void (GLAPIENTRY *qglDetachObjectARB)(GLhandleARB containerObj, GLhandleARB attachedObj);
extern GLhandleARB (GLAPIENTRY *qglCreateShaderObjectARB)(GLenum shaderType);
extern void (GLAPIENTRY *qglShaderSourceARB)(GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length);
extern void (GLAPIENTRY *qglCompileShaderARB)(GLhandleARB shaderObj);
extern GLhandleARB (GLAPIENTRY *qglCreateProgramObjectARB)(void);
extern void (GLAPIENTRY *qglAttachObjectARB)(GLhandleARB containerObj, GLhandleARB obj);
extern void (GLAPIENTRY *qglLinkProgramARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglUseProgramObjectARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglValidateProgramARB)(GLhandleARB programObj);
extern void (GLAPIENTRY *qglUniform1fARB)(GLint location, GLfloat v0);
extern void (GLAPIENTRY *qglUniform2fARB)(GLint location, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglUniform3fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglUniform4fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglUniform1iARB)(GLint location, GLint v0);
extern void (GLAPIENTRY *qglUniform2iARB)(GLint location, GLint v0, GLint v1);
extern void (GLAPIENTRY *qglUniform3iARB)(GLint location, GLint v0, GLint v1, GLint v2);
extern void (GLAPIENTRY *qglUniform4iARB)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern void (GLAPIENTRY *qglUniform1fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform2fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform3fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform4fvARB)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform1ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform2ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform3ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform4ivARB)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniformMatrix2fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix3fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix4fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglGetObjectParameterfvARB)(GLhandleARB obj, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetObjectParameterivARB)(GLhandleARB obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetInfoLogARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
extern void (GLAPIENTRY *qglGetAttachedObjectsARB)(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
extern GLint (GLAPIENTRY *qglGetUniformLocationARB)(GLhandleARB programObj, const GLcharARB *name);
extern void (GLAPIENTRY *qglGetActiveUniformARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
extern void (GLAPIENTRY *qglGetUniformfvARB)(GLhandleARB programObj, GLint location, GLfloat *params);
extern void (GLAPIENTRY *qglGetUniformivARB)(GLhandleARB programObj, GLint location, GLint *params);
extern void (GLAPIENTRY *qglGetShaderSourceARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);

#ifndef GL_ARB_vertex_shader
#define GL_VERTEX_SHADER_ARB              0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB 0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB         0x8B4B
#define GL_MAX_VERTEX_ATTRIBS_ARB         0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
#define GL_MAX_TEXTURE_COORDS_ARB         0x8871
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB  0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB    0x8643
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB   0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB   0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB   0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB 0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB      0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB 0x8645
#define GL_FLOAT                          0x1406
#define GL_FLOAT_VEC2_ARB                 0x8B50
#define GL_FLOAT_VEC3_ARB                 0x8B51
#define GL_FLOAT_VEC4_ARB                 0x8B52
#define GL_FLOAT_MAT2_ARB                 0x8B5A
#define GL_FLOAT_MAT3_ARB                 0x8B5B
#define GL_FLOAT_MAT4_ARB                 0x8B5C
#endif

#ifndef GL_ARB_fragment_shader
#define GL_OBJECT_COMPILE_STATUS_ARB                0x8B81
#define GL_OBJECT_LINK_STATUS_ARB                   0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB               0x8B83
#define GL_FRAGMENT_SHADER_ARB            0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB 0x8B49
#define GL_MAX_TEXTURE_COORDS_ARB         0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB 0x8B8B
#endif

#ifndef GL_ARB_shading_language_100
#define GL_SHADING_LANGUAGE_VERSION_ARB   0x8B8C
#endif

#ifndef GL_ARB_texture_cube_map
#	define GL_ARB_texture_cube_map

#	define GL_NORMAL_MAP_ARB			    0x8511
#	define GL_REFLECTION_MAP_ARB		    0x8512

#	define GL_TEXTURE_CUBE_MAP_ARB		    0x8513

#	define GL_TEXTURE_BINDING_CUBE_MAP_ARB	    0x8514

#	define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB     0x8515
#	define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB     0x8516
#	define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB     0x8517
#	define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB     0x8518
#	define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB     0x8519
#	define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB     0x851A

#	define GL_PROXY_TEXTURE_CUBE_MAP_ARB	    0x851B

#	define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB	    0x851C
#endif

extern qboolean gl_combine;
extern qboolean gl_stencil;
extern qboolean gl_shader;
extern qboolean gl_sgis_mipmap;
extern qboolean gl_texture_non_power_of_two;
extern qboolean gl_point_sprite;
extern qboolean gl_n_patches;

extern void GL_EnableTMU(int tmu);
extern void GL_DisableTMU(int tmu);
extern void GL_SelectTexture (GLenum target);

#define Q3MODELS 1      //Allow Q3 models in the same way as Q1 models. --> JTR

void   Mod_LoadMd3Model (model_t *mod, void *buffer); 
