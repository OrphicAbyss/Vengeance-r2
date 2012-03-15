/*
Copyright (C) 2006 Andreas Kirsch

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
#include <assert.h>
#include "quakedef.h"
#include "mathlib.h"

// flags for rtlight rendering
#define LIGHTFLAG_NORMALMODE 1
#define LIGHTFLAG_REALTIMEMODE 2

// keep in sync with R_ShaderLight_Definition
typedef struct R_ShaderLight {
	qboolean active;
	// world-space coordinates
	vec3_t origin;
	// color of the light
	vec3_t color;
	// angle
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

	// maxDistance of the light (also the max visible distance)
	float maxDistance;
	// per-level VIS/PVS cache
	byte visCache[ MAX_MAP_LEAFS ];
} R_ShaderLight;

static R_ShaderLight r_shader_lights[ R_MAX_SHADER_LIGHTS ];

cvar_t r_shadow_realtime_dlight = {"r_shadow_realtime_dlight", "1", true};
cvar_t r_shadow_realtime_world = {"r_shadow_realtime_world", "0", true};
cvar_t r_shadow_realtime_draw_world = {"r_shadow_realtime_draw_world", "1", true};
cvar_t r_shadow_realtime_draw_models = {"r_shadow_realtime_draw_models", "1", true};
cvar_t r_shadow_realtime_world_lightmaps = {"r_shadow_realtime_world_lightmaps", "0.5", true};
cvar_t r_shadow_lightintensityscale = {"r_shadow_lightintensityscale", "1"};

cvar_t r_editlights = {"r_editlights", "0"};
cvar_t r_editlights_quakelightsizescale = {"r_editlights_quakelightsizescale", "1", true};

GLhandleARB R_CompileGLSLShader( GLhandleARB programObject, GLenum shaderType, unsigned int sourceStringCount, const char *sourceStrings[] );
unsigned int R_CompileGLSLProgram( unsigned int vertexstrings_count, const char **vertexstrings_list, unsigned int fragmentstrings_count, const char **fragmentstrings_list );
R_ShaderLight* GetLightFromIndex( unsigned int lightIndex );
matrix4x4_t GetWorldToViewMatrix();
qboolean R_Shader_IsLightVisibleFromLeaf( const R_ShaderLight *light, mleaf_t *leaf );
void R_Shader_UpdateLightVISCache( R_ShaderLight *light );
qboolean R_Shader_IsLightIndexUsed( unsigned int lightIndex );

static matrix4x4_t GetWorldToLightMatrix( const R_ShaderLight *light ) {
	void ML_ModelViewMatrix(float *modelview, const vec3_t viewangles, const vec3_t vieworg, qboolean zup);
	matrix4x4_t worldToLightMatrixT, worldToLightMatrix;
	ML_ModelViewMatrix( &worldToLightMatrixT.m[0][0], light->angles, light->origin, false);
	// ML is OGL matrix style, here something sane is needed
	Matrix4x4_Transpose( &worldToLightMatrix, &worldToLightMatrixT );
	return worldToLightMatrix;
}

static matrix4x4_t GetWorldToViewMatrix() {
	void ML_ModelViewMatrix(float *modelview, const vec3_t viewangles, const vec3_t vieworg, qboolean zup);
	matrix4x4_t worldToViewMatrixT, worldToViewMatrix;
	ML_ModelViewMatrix( &worldToViewMatrixT.m[0][0], r_refdef.viewangles, r_refdef.vieworg, true);
	// ML is OGL matrix style, here something sane is needed
	Matrix4x4_Transpose( &worldToViewMatrix, &worldToViewMatrixT );
	return worldToViewMatrix;
}

static __inline R_ShaderLight* GetLightFromIndex( unsigned int lightIndex ) {
	assert( lightIndex < R_MAX_SHADER_LIGHTS );
	return &r_shader_lights[ lightIndex ];
}

static GLhandleARB R_CompileGLSLShader( GLhandleARB programObject, GLenum shaderType, unsigned int sourceStringCount, const char *sourceStrings[] ) {
	GLhandleARB shaderObject;
	GLint shaderCompiled;
	const char *shaderName;
	int i;
	char compileLog[ 4096 ];

	if( shaderType == GL_VERTEX_SHADER_ARB ) {
		shaderName = "vertex shader";
	} else {
		shaderName = "fragment shader";
	}

	// run a basic error check
	for (i = 0 ; i < sourceStringCount ; i++)
	{
		if (sourceStrings[i] == 0)
		{
			Con_Printf("R_CompileGLSLShader: string #%i of %s is NULL!\n", i, shaderName);
			return 0;
		}
	}

	// R_CheckError();
	shaderObject = qglCreateShaderObjectARB( shaderType );
	if( shaderObject == 0 )
	{
		// R_CheckError();
		return 0;
	}

	if( sourceStringCount != 0 ) {
		qglShaderSourceARB( shaderObject, sourceStringCount, sourceStrings, NULL);
		qglCompileShaderARB( shaderObject );
		// R_CheckError();
		qglGetObjectParameterivARB( shaderObject, GL_OBJECT_COMPILE_STATUS_ARB, &shaderCompiled );
		qglGetInfoLogARB( shaderObject, sizeof(compileLog), NULL, compileLog);
		if( *compileLog ) {
			Con_Printf("%s compile log:\n%s\n", shaderName, compileLog);
		}
		if( !shaderCompiled ) {
			qglDeleteObjectARB( shaderObject );
			// R_CheckError();
			return 0;
		}
	}
	// TODO: check whether an empty shader object can be compiled!
	qglAttachObjectARB( programObject, shaderObject);
	qglDeleteObjectARB( shaderObject );
	//R_CheckError();

	return shaderObject;
}

static unsigned int R_CompileGLSLProgram(unsigned int vertexstrings_count, const char **vertexstrings_list, unsigned int fragmentstrings_count, const char **fragmentstrings_list)
{
	GLint programLinked;
	GLhandleARB programObject = 0;
	char compileLog[4096];
	//R_CheckError();

	//	if (!R.ext.ARB_fragment_shader)
	if (!gl_support_GLSL_shaders)
		return 0;

	programObject = qglCreateProgramObjectARB();
	//R_CheckError();
	if( programObject == 0 ) {
		return 0;
	}

	if( R_CompileGLSLShader( programObject, GL_VERTEX_SHADER_ARB, vertexstrings_count, vertexstrings_list ) == 0 ||
		R_CompileGLSLShader( programObject, GL_FRAGMENT_SHADER_ARB, fragmentstrings_count, fragmentstrings_list ) == 0 ) {
			qglDeleteObjectARB( programObject );
			// R_CheckError();
			return 0;
		}

		qglLinkProgramARB( programObject );
		//	R_CheckError();
		qglGetObjectParameterivARB( programObject, GL_OBJECT_LINK_STATUS_ARB, &programLinked );
		qglGetInfoLogARB( programObject, sizeof( compileLog ), NULL, compileLog );
		if( *compileLog ) {
			Con_Printf("program link log:\n%s\n", compileLog);
			// software vertex shader is ok but software fragment shader is WAY
			// too slow, fail program if so.
			// NOTE: this string might be ATI specific, but that's ok because the
			// ATI R300 chip (Radeon 9500-9800/X300) is the most likely to use a
			// software fragment shader due to low instruction and dependent
			// texture limits.
			if (strstr( compileLog, "fragment shader will run in software" ))
				programLinked = false;
		}
		//R_CheckError();
		if( !programLinked ) {
			qglDeleteObjectARB( programObject );
			return 0;
		}
		//R_CheckError();
		return (unsigned int) programObject;
}

void R_Shader_Quit(void) 
{	
	unsigned int * const programObject = &r_refdef.lightShader.programObject;
	
	if( *programObject != 0 ) {
		qglDeleteObjectARB( *programObject );
		*programObject = 0;
	}
}

int shaderlight = R_DROPLIGHT_MAX_LIGHTS;

// From DP, edited by Entar
int R_LoadWorldLights(void)
{
	int n, a, style, shadow, flags;
	char tempchar, *lightsstring, *s, *t, name[MAX_QPATH], cubemapname[MAX_QPATH];
	float origin[3], radius, color[3], angles[3], corona, coronasizescale, ambientscale, diffusescale, specularscale;
	R_ShaderLight_Definition light;

	shaderlight = R_DROPLIGHT_MAX_LIGHTS;

	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return false;
	}
	COM_StripExtension (cl.worldmodel->name, name);
	strlcat (name, ".rtlights", sizeof (name));
//	lightsstring = (char *)FS_LoadFile(name, tempmempool, false, NULL);
	lightsstring = (char *)COM_LoadTempFile(name);
	if (lightsstring)
	{
		s = lightsstring;
		n = 0;
		while (*s)
		{
			t = s;
			/*
			shadow = true;
			for (;COM_Parse(t, true) && strcmp(
			if (COM_Parse(t, true))
			{
				if (com_token[0] == '!')
				{
					shadow = false;
					origin[0] = atof(com_token+1);
				}
				else
					origin[0] = atof(com_token);
				if (Com_Parse(t
			}
			*/
			t = s;
			while (*s && *s != '\n' && *s != '\r')
				s++;
			if (!*s)
				break;
			tempchar = *s;
			shadow = true;
			// check for modifier flags
			if (*t == '!')
			{
				shadow = false;
				t++;
			}
			*s = 0;
			a = sscanf(t, "%f %f %f %f %f %f %f %d %s %f %f %f %f %f %f %f %f %i", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname, &corona, &angles[0], &angles[1], &angles[2], &coronasizescale, &ambientscale, &diffusescale, &specularscale, &flags);
			*s = tempchar;
//			if (a < 18)
//				flags = LIGHTFLAG_REALTIMEMODE;
			if (a < 17)
				specularscale = 1;
			if (a < 16)
				diffusescale = 1;
			if (a < 15)
				ambientscale = 0;
			if (a < 14)
				coronasizescale = 0.25f;
			if (a < 13)
				VectorClear(angles);
			if (a < 10)
				corona = 0;
			if (a < 9 || !strcmp(cubemapname, "\"\""))
				cubemapname[0] = 0;
			// remove quotes on cubemapname
			if (cubemapname[0] == '"' && cubemapname[strlen(cubemapname) - 1] == '"')
			{
				size_t namelen;
				namelen = strlen(cubemapname) - 2;
				memmove(cubemapname, cubemapname + 1, namelen);
				cubemapname[namelen] = '\0';
			}
			if (a < 8)
			{
				Con_Printf("found %d parameters on line %i, should be 8 or more parameters (origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style \"cubemapname\" corona angles[0] angles[1] angles[2] coronasizescale ambientscale diffusescale specularscale flags)\n", a, n + 1);
				break;
			}
			// check if we've hit our limit
			if (shaderlight < R_MIN_SHADER_DLIGHTS)
			{
//				R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, corona, style, shadow, cubemapname, coronasizescale, ambientscale, diffusescale, specularscale, flags);
				VectorCopy (color, light.color);
				light.maxDistance = radius;
				VectorCopy(origin, light.origin);
				VectorCopy(angles, light.angles);
				light.corona = corona;
				light.style = style;
				light.shadow = shadow;
				sprintf (light.cubemapname, cubemapname);
				light.coronasizescale = coronasizescale;
				light.ambientscale = ambientscale;
				light.diffusescale = diffusescale;
				light.specularscale = specularscale;
				light.flags = flags;
				R_Shader_SetLight( shaderlight, &light );
				shaderlight++;
			}
			if (*s == '\r')
				s++;
			if (*s == '\n')
				s++;
			n++;
		}
		if (*s)
			Con_Printf("invalid rtlights file \"%s\"\n", name);
//		free(lightsstring);
		return true;
	}
	return false;
}

void R_Shadow_SaveWorldLights(void)
{
	int i;
	R_ShaderLight light;
	size_t bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[16384];

	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	COM_StripExtension (cl.worldmodel->name, name);
	strlcat (name, ".rtlights", sizeof (name));
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (i=R_DROPLIGHT_MAX_LIGHTS, light = r_shader_lights[i]; i < R_MIN_SHADER_DLIGHTS; i++, light = r_shader_lights[i])
	{
		if (!r_shader_lights[i].active)
			continue;
		if (light.coronasizescale != 0.25f || light.ambientscale != 0 || light.diffusescale != 1 || light.specularscale != 1 || light.flags != LIGHTFLAG_REALTIMEMODE)
			sprintf(line, "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f %f %f %f %f %i\n", light.shadow ? "" : "!", light.origin[0], light.origin[1], light.origin[2], light.maxDistance, light.color[0], light.color[1], light.color[2], light.style, light.cubemapname, light.corona, light.angles[0], light.angles[1], light.angles[2], light.coronasizescale, light.ambientscale, light.diffusescale, light.specularscale, light.flags);
		else if (light.cubemapname[0] || light.corona || light.angles[0] || light.angles[1] || light.angles[2])
			sprintf(line, "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f\n", light.shadow ? "" : "!", light.origin[0], light.origin[1], light.origin[2], light.maxDistance, light.color[0], light.color[1], light.color[2], light.style, light.cubemapname, light.corona, light.angles[0], light.angles[1], light.angles[2]);
		else
			sprintf(line, "%s%f %f %f %f %f %f %f %d\n", light.shadow ? "" : "!", light.origin[0], light.origin[1], light.origin[2], light.maxDistance, light.color[0], light.color[1], light.color[2], light.style);
		if (bufchars + strlen(line) > bufmaxchars)
		{
			bufmaxchars = bufchars + strlen(line) + 2048;
			oldbuf = buf;
			buf = (char *)malloc(bufmaxchars);
			if (oldbuf)
			{
				if (bufchars)
					memcpy(buf, oldbuf, bufchars);
				free(oldbuf);
			}
		}
		if (strlen(line))
		{
			memcpy(buf + bufchars, line, strlen(line));
			bufchars += strlen(line);
		}
	}
	if (bufchars)
		FS_WriteFile(name, buf, (long)bufchars, FS_GAMEONLY);
	if (buf)
		free(buf);
}

typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void)
{
	int entnum, style, islight, skin, pflags, effects, type, n;
	char *entfiledata;
	const char *data;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], vec[4];
	char key[256], value[16384];
	R_ShaderLight_Definition s_light;

	shaderlight = R_DROPLIGHT_MAX_LIGHTS;

	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	// try to load a .ent file first
	COM_StripExtension (cl.worldmodel->name, key);
	strlcat (key, ".ent", sizeof (key));
	data = entfiledata = (char *)COM_LoadTempFile(key);
	// and if that is not found, fall back to the bsp file entity string
	if (!data)
		data = cl.worldmodel->entities;
	if (!data)
		return;
	for (entnum = 0;COM_ParseTokenConsole(&data) && com_token[0] == '{';entnum++)
	{
		type = LIGHTTYPE_MINUSX;
		origin[0] = origin[1] = origin[2] = 0;
		originhack[0] = originhack[1] = originhack[2] = 0;
		angles[0] = angles[1] = angles[2] = 0;
		color[0] = color[1] = color[2] = 1;
		light[0] = light[1] = light[2] = 1;light[3] = 300;
		overridecolor[0] = overridecolor[1] = overridecolor[2] = 1;
		fadescale = 1;
		lightscale = 1;
		style = 0;
		skin = 0;
		pflags = 0;
		effects = 0;
		islight = false;
		while (1)
		{
			if (!COM_ParseTokenConsole(&data))
				break; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '_')
				strlcpy(key, com_token + 1, sizeof(key));
			else
				strlcpy(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseTokenConsole(&data))
				break; // error
			strlcpy(value, com_token, sizeof(value));

			// now that we have the key pair worked out...
			if (!strcmp("light", key))
			{
				n = sscanf(value, "%f %f %f %f", &vec[0], &vec[1], &vec[2], &vec[3]);
				if (n == 1)
				{
					// quake
					light[0] = vec[0] * (1.0f / 256.0f);
					light[1] = vec[0] * (1.0f / 256.0f);
					light[2] = vec[0] * (1.0f / 256.0f);
					light[3] = vec[0];
				}
				else if (n == 4)
				{
					// halflife
					light[0] = vec[0] * (1.0f / 255.0f);
					light[1] = vec[1] * (1.0f / 255.0f);
					light[2] = vec[2] * (1.0f / 255.0f);
					light[3] = vec[3];
				}
			}
			else if (!strcmp("delay", key))
				type = atoi(value);
			else if (!strcmp("origin", key))
				sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp("angle", key))
				angles[0] = 0, angles[1] = atof(value), angles[2] = 0;
			else if (!strcmp("angles", key))
				sscanf(value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
			else if (!strcmp("color", key))
				sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
			else if (!strcmp("wait", key))
				fadescale = atof(value);
			else if (!strcmp("classname", key))
			{
				if (!strncmp(value, "light", 5))
				{
					islight = true;
					if (!strcmp(value, "light_fluoro"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_fluorospark"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_globe"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1.0f;
						overridecolor[1] = 0.8f;
						overridecolor[2] = 0.4f;
					}
					if (!strcmp(value, "light_flame_large_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1.0f;
						overridecolor[1] = 0.5f;
						overridecolor[2] = 0.1f;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5f;
						overridecolor[2] = 0.1f;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5f;
						overridecolor[2] = 0.1f;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5f;
						overridecolor[2] = 0.1f;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
			else if (!strcmp("skin", key))
				skin = (int)atof(value);
			else if (!strcmp("pflags", key))
				pflags = (int)atof(value);
			else if (!strcmp("effects", key))
				effects = (int)atof(value);
/*			else if (cl.worldmodel->type == mod_brushq3)
			{
				if (!strcmp("scale", key))
					lightscale = atof(value);
				if (!strcmp("fade", key))
					fadescale = atof(value);
			}*/
		}
		if (!islight)
			continue;
		if (lightscale <= 0)
			lightscale = 1;
		if (fadescale <= 0)
			fadescale = 1;
		if (color[0] == color[1] && color[0] == color[2])
		{
			color[0] *= overridecolor[0];
			color[1] *= overridecolor[1];
			color[2] *= overridecolor[2];
		}
		radius = light[3] * r_editlights_quakelightsizescale.value * lightscale / fadescale;
		color[0] = color[0] * light[0];
		color[1] = color[1] * light[1];
		color[2] = color[2] * light[2];
		switch (type)
		{
		case LIGHTTYPE_MINUSX:
			break;
		case LIGHTTYPE_RECIPX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		case LIGHTTYPE_RECIPXX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		default:
		case LIGHTTYPE_NONE:
			break;
		case LIGHTTYPE_SUN:
			break;
		case LIGHTTYPE_MINUSXX:
			break;
		}
		VectorAdd(origin, originhack, origin);
		if (radius >= 1 && shaderlight < R_MIN_SHADER_DLIGHTS)
		{
//			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, (pflags & PFLAGS_CORONA) != 0, style, (pflags & PFLAGS_NOSHADOW) == 0, skin >= 16 ? va("cubemaps/%i", skin) : NULL, 0.25, 0, 1, 1, LIGHTFLAG_REALTIMEMODE);
			VectorCopy (color, s_light.color);
			s_light.maxDistance = radius;
			VectorCopy(origin, s_light.origin);
			VectorCopy(angles, s_light.angles);
			s_light.corona = 0;
			s_light.style = style;
			s_light.shadow = 1;
			sprintf (s_light.cubemapname, skin >= 16 ? va("cubemaps/%i", skin) : "");
			s_light.coronasizescale = 1;
			s_light.ambientscale = 1;
			s_light.diffusescale = 1;
			s_light.specularscale = 1;
			s_light.flags = 0;
			R_Shader_SetLight( shaderlight, &s_light );
			shaderlight++;
		}
	}
//	if (entfiledata)
//		free(entfiledata);
}

void R_GatherLights (void)
{
	extern void DefineFlare(vec3_t origin, int radius, int mode, int alfa);
	extern void DefineFlareColor(vec3_t origin, int radius, int mode, int alfa, float red, float green, float blue);
	extern cvar_t r_coronas;
	int i=0;

	// coronas
	for (i=0; i < R_MIN_SHADER_DLIGHTS; i++)
	{
		// cleanup/optimize?
		if (r_shader_lights[i].corona)
			DefineFlareColor(r_shader_lights[i].origin, r_shader_lights[i].maxDistance*r_shader_lights[i].coronasizescale, 0, 10, r_shader_lights[i].color[0]*r_shader_lights[i].corona, r_shader_lights[i].color[1]*r_shader_lights[i].corona, r_shader_lights[i].color[2]*r_shader_lights[i].corona);
	}


	// reset all our old dlights
	for( i = R_MIN_SHADER_DLIGHTS ; i < R_MAX_SHADER_LIGHTS ; i++ ) {
		R_Shader_ResetLight( i );
	}

	if (!r_shadow_realtime_dlight.value)
		return;

	shaderlight = R_MIN_SHADER_DLIGHTS;

	for (i=0; i<MAX_DLIGHTS; i++)
	{
		R_ShaderLight_Definition light;
		dlight_t *dlight = &cl_dlights[ i ];
		float scale;
		
		// check if we've hit our limit
		if (shaderlight >= R_MAX_SHADER_LIGHTS)
			break;

		// check if dlight is active
		if (dlight->die && dlight->die < cl.time)
			continue;
		
		// TODO: replace this with an epsilon constant
        if( dlight->radius < 0.1f ) {
			continue;
		}

		VectorCopy (dlight->colour, light.color);
		scale = 1/(light.color[0] > light.color[1] ? light.color[0] > light.color[2] ? light.color[0] : light.color[2] : light.color[1] > light.color[2] ? light.color[1] : light.color[2]);
		VectorScale(light.color, scale, light.color);

		// without the /scale, it's closer to correct (Entar)
		light.maxDistance = dlight->radius; // /scale;
		VectorCopy(dlight->origin, light.origin);
		light.style = 0;
		light.angles[0] = light.angles[1] = light.angles[2] = 0;
		light.corona = 0;
		light.shadow = 1;
		sprintf (light.cubemapname, "");
		light.coronasizescale = 1;
		light.ambientscale = 1;
		light.diffusescale = 1;
		light.specularscale = 1;
		light.flags = 0;
		R_Shader_SetLight( shaderlight, &light );
		shaderlight++;
	}
}

void R_Shadow_EditLights_Clear_f (void)
{
	int i;
	for (i=0; i < R_MAX_SHADER_LIGHTS; i++) // reset the old ones
		R_Shader_ResetLight(i);
}

void R_Shadow_EditLights_Reload_f(void)
{
	if (!cl.worldmodel)
		return;

	R_Shadow_EditLights_Clear_f();
	if (!R_LoadWorldLights ())
		R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
}

void R_Shadow_EditLights_Spawn_f(void)
{
	R_ShaderLight_Definition s_light;
	int shaderlight=R_DROPLIGHT_MAX_LIGHTS;
	
	if (!r_editlights.value)
	{
		Con_Print("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (Cmd_Argc() != 1)
	{
		Con_Print("r_editlights_spawn does not take parameters\n");
		return;
	}
	while (R_Shader_IsLightIndexUsed(shaderlight) && shaderlight < R_MIN_SHADER_DLIGHTS)
		shaderlight++;
	if (shaderlight == R_MIN_SHADER_DLIGHTS)
	{
		Con_Print("Cannot spawn new light, list is full.\n");
		return;
	}
	s_light.color[0] = s_light.color[1] = s_light.color[2] = 1;
	s_light.maxDistance = 200;
	VectorCopy( r_refdef.vieworg, s_light.origin );
	s_light.angles[0] = s_light.angles[1] = s_light.angles[2] = 0;
	s_light.corona = 0;
	s_light.style = 0;
	s_light.shadow = 1;
	sprintf (s_light.cubemapname, "");
	s_light.coronasizescale = 1;
	s_light.ambientscale = 1;
	s_light.diffusescale = 1;
	s_light.specularscale = 1;
	s_light.flags = 0;
	R_Shader_SetLight( shaderlight, &s_light );
	Con_Printf("Created realtime light at %f, %f, %f\n", s_light.origin[0], s_light.origin[1], s_light.origin[2]);
//	R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), r_editlights_cursorlocation, vec3_origin, color, 200, 0, 0, true, NULL, 0.25, 0, 1, 1, LIGHTFLAG_REALTIMEMODE);
}

void _DropLight_f(void)
{
	static int lightIndex = -1;
	R_ShaderLight_Definition light;
	memset( &light, 0, sizeof( light ) );
	VectorSet( light.color, 1.0, 1.0, 1.0 );
	light.maxDistance = 100.0;
	VectorCopy( r_refdef.vieworg, light.origin );
	light.style = 0;
	
	lightIndex = (lightIndex + 1) % R_DROPLIGHT_MAX_LIGHTS;
	if( lightIndex == 0 ) {
		int i;
		Con_Printf ("Shader lights full, clearing list.\n");
		for ( i=0; i < R_DROPLIGHT_MAX_LIGHTS ; i++)
			R_Shader_ResetLight( i );		
	}
	R_Shader_SetLight( lightIndex, &light );
}

void _ReloadShader_f(void) {
	R_Shader_Quit();
	R_Shader_Init();
}

void R_Shader_Reset(void) {
	int i;
	
	for( i = 0 ; i < R_MAX_SHADER_LIGHTS ; i++ ) {
		R_ShaderLight *light = &r_shader_lights[ i ];
		light->active = false;
	}
}

static const char *builtinfragshader =
"uniform sampler2D diffuseSampler;\n"
"uniform samplerCube cubemapSampler;\n"
"\n"
"varying vec2 texCoords;\n"
"varying vec3 cubemapCoords;\n"
"\n"
"uniform vec3 lightColor;\n"
"uniform float lightMaxDistance;\n"
"varying vec3 lightDirection;\n"
"\n"
"varying vec3 viewerDirection;\n"
"\n"
"varying vec3 surfaceNormal;\n"
"\n"
"struct Light {\n"
"	vec3 direction;\n"
"	vec3 color;\n"
"	float maxDistance;\n"
"	float specularPower;\n"
"};\n"
"\n"
"struct Surface {\n"
"	vec3 normal;\n"
"	vec4 diffuseFactor;\n"
"	vec3 specularFactor;\n"
"};\n"
"\n"
"vec4 getColor( in Surface surface, in Light light, in vec3 viewerDirection ) {\n"
"	vec3 normLightDirection = normalize( light.direction );\n"
"	vec3 normViewerDirection = normalize( viewerDirection );\n"
"\n"	
"	vec3 diffuseColor = surface.diffuseFactor.rgb * max( 0.0, dot( normLightDirection, surface.normal ) );\n"
"\n"
"	#if 1\n"
"		vec3 specularColor = surface.specularFactor * pow( max( 0.0, dot( normalize( reflect(-normLightDirection, surface.normal ) ), normViewerDirection ) ), light.specularPower );\n"
"	#else\n"
"		vec3 specularColor = vec3( 0.0 );\n"
"	#endif\n"
"\n"	
"	float lightDistance = length( light.direction );\n"
"	float lightFallOff = lightDistance / light.maxDistance;\n"
"	float attenuation = 1.0 - lightFallOff * lightFallOff;\n"
"\n"
"	const vec3 cubemapModulation = textureCube( cubemapSampler, cubemapCoords ).rgb;\n"
"	vec3 finalColor = cubemapModulation * light.color * attenuation * (diffuseColor + specularColor);\n"
"\n"
"	return vec4( finalColor, surface.diffuseFactor.a );\n"
"}\n"
"\n"
"void main() {\n"
"	Light light = Light( lightDirection, lightColor, lightMaxDistance, 32.0 );\n"
"\n"
"	vec4 diffuseFactor = texture2D( diffuseSampler, texCoords );\n"
"	Surface surface = Surface( normalize( surfaceNormal ), diffuseFactor, min( vec3( diffuseFactor ), vec3( 0.5 ) ) );\n"
"\n"
"	gl_FragColor = getColor( surface, light, viewerDirection ) * gl_Color.rgba;\n"
"}\n"
"\n"
;

static const char *builtinvertshader =
"uniform vec3 lightPosition;\n"
"uniform mat4 viewToLightMatrix;\n"
"\n"
"varying vec3 lightDirection;\n"
"varying vec3 viewerDirection;\n"
"varying vec3 surfaceNormal;\n"
"varying vec2 texCoords;\n"
"varying vec3 cubemapCoords;\n"
"\n"
"void main() {\n"
"	float temp;\n"
"	gl_Position = ftransform();\n"
"\n"
"	texCoords = vec2( gl_MultiTexCoord0 );\n"
"\n"
"	const vec4 eyeVector = gl_ModelViewMatrix * gl_Vertex;\n"
"	const vec4 lightVector = viewToLightMatrix * eyeVector;\n"
"	cubemapCoords = vec3( lightVector );\n"
"\n"	
"	lightDirection = lightPosition - vec3( eyeVector );\n"
"	viewerDirection = vec3( 0.0 ) - vec3( eyeVector );\n"
"	surfaceNormal = gl_NormalMatrix * gl_Normal;\n"
"	gl_FrontColor = gl_Color;\n"
"}\n"
"\n"
;

void R_Shader_Init(void)
{
	//RenderState_ShaderPermutation *s;
	char *vertstring, *fragstring;
	//unsigned int vertstrings_count, fragstrings_count;
	//const char *vertstrings_list[R_SHADERPERMUTATION_LIMIT];
	//const char *fragstrings_list[R_SHADERPERMUTATION_LIMIT];

	// set up the lighting shaders
	memset(&r_refdef.lightShader, 0, sizeof(r_refdef.lightShader));
	//	if (!R.ext.ARB_fragment_shader)
	if (!gl_support_GLSL_shaders)
		return;
	//	vertstring = File_LoadFile("shaders/light.vert", NULL);
	//	fragstring = File_LoadFile("shaders/light.frag", NULL);
#define VERTEX_SHADER_FILENAME	"shaders/standard.vert"
#define FRAGMENT_SHADER_FILENAME "shaders/standard.frag"
	vertstring = COM_LoadMallocFile( VERTEX_SHADER_FILENAME );
	fragstring = COM_LoadMallocFile( FRAGMENT_SHADER_FILENAME );

	if (!vertstring || !fragstring)
		r_refdef.lightShader.programObject = R_CompileGLSLProgram (1, &builtinvertshader, 1, &builtinfragshader);
	else
		r_refdef.lightShader.programObject = R_CompileGLSLProgram( 1, &vertstring, 1, &fragstring );
	if( r_refdef.lightShader.programObject == 0 ) {
		Con_Printf( "Couldn't load light shaders '" VERTEX_SHADER_FILENAME "' and '" FRAGMENT_SHADER_FILENAME "'!\n" );
	}

	// free the source strings
	if (fragstring)
		Z_Free( fragstring );
	if (vertstring)
		Z_Free( vertstring );

	// determine the uniform locations
#define UNIFORM( name )		r_refdef.lightShader.##name = qglGetUniformLocationARB( r_refdef.lightShader.programObject, #name );
	UNIFORM( lightPosition );
	//UNIFORM( eyePosition );
	UNIFORM( viewToLightMatrix );

	UNIFORM( diffuseSampler );
	//UNIFORM( normalSampler );
	//UNIFORM( specularSampler );
	
	UNIFORM( lightColor );
	UNIFORM( lightMaxDistance );


	/*
	Black: According to the specs, these two caps don't exist so comment them out for a beginning.
	*/
	/*
	// ?
	glEnable(GL_FRAGMENT_SHADER_ARB);
	glEnable(GL_VERTEX_SHADER_ARB);
	*/
	/*
	for (i = 0, s = r_refdef.shader_standard;i < R_LIGHTSHADERPERMUTATION_LIMIT;i++, s++)
	{
	// build the source list for this shader permutation
	vertstrings_count = 0;
	fragstrings_count = 0;
	if (i & R_LIGHTSHADERPERMUTATION_SPECULAR)
	{
	vertstrings_list[vertstrings_count++] = "#define USESPECULAR\n";
	fragstrings_list[fragstrings_count++] = "#define USESPECULAR\n";
	}
	if (i & R_LIGHTSHADERPERMUTATION_FOG)
	{
	vertstrings_list[vertstrings_count++] = "#define USEFOG\n";
	fragstrings_list[fragstrings_count++] = "#define USEFOG\n";
	}
	if (i & R_LIGHTSHADERPERMUTATION_CUBEFILTER)
	{
	vertstrings_list[vertstrings_count++] = "#define USECUBEFILTER\n";
	fragstrings_list[fragstrings_count++] = "#define USECUBEFILTER\n";
	}
	if (i & R_LIGHTSHADERPERMUTATION_OFFSETMAPPING)
	{
	vertstrings_list[vertstrings_count++] = "#define USEOFFSETMAPPING\n";
	fragstrings_list[fragstrings_count++] = "#define USEOFFSETMAPPING\n";
	}
	vertstrings_list[vertstrings_count++] = vertstring;
	fragstrings_list[fragstrings_count++] = fragstring;
	//		vertstrings_list[vertstrings_count++] = vertstring ? vertstring : builtinshader_standard_vert;
	//		fragstrings_list[fragstrings_count++] = fragstring ? fragstring : builtinshader_standard_frag;

	// compile this shader permutation
	s->programmObject = R_CompileGLSL(vertstrings_count, vertstrings_list, fragstrings_count, fragstrings_list);
	if (!s->programmObject)
	{
	//			Console_Printf("permutation %s %s %s %s failed for lighting shader (glsl/light.frag and .vert), some features may not work properly!\n", i & 1 ? "specular" : "", i & 1 ? "fog" : "", i & 1 ? "cubefilter" : "", i & 1 ? "offsetmapping" : "");
	Con_Printf("permutation %s %s %s %s failed for lighting shader (shaders/standard.frag and .vert), some features may not work properly!\n", i & 1 ? "specular" : "", i & 1 ? "fog" : "", i & 1 ? "cubefilter" : "", i & 1 ? "offsetmapping" : "");
	continue;
	}

	//		R_CheckError();

	// switch to the new program object
	qglUseProgramObjectARB(s->programmObject);

	// fetch all the uniform locations
	s->loc_LightPosition = qglGetUniformLocationARB(s->programmObject, "LightPosition");
	s->loc_EyePosition = qglGetUniformLocationARB(s->programmObject, "EyePosition");
	s->loc_LightColor = qglGetUniformLocationARB(s->programmObject, "LightColor");
	s->loc_OffsetMapping_Scale = qglGetUniformLocationARB(s->programmObject, "OffsetMapping_Scale");
	s->loc_OffsetMapping_Bias = qglGetUniformLocationARB(s->programmObject, "OffsetMapping_Bias");
	s->loc_SpecularPower = qglGetUniformLocationARB(s->programmObject, "SpecularPower");
	s->loc_FogRangeRecip = qglGetUniformLocationARB(s->programmObject, "FogRangeRecip");
	s->loc_AmbientScale = qglGetUniformLocationARB(s->programmObject, "AmbientScale");
	s->loc_DiffuseScale = qglGetUniformLocationARB(s->programmObject, "DiffuseScale");
	s->loc_SpecularScale = qglGetUniformLocationARB(s->programmObject, "SpecularScale");
	s->loc_Texture_Normal = qglGetUniformLocationARB(s->programmObject, "Texture_Normal");
	s->loc_Texture_Color = qglGetUniformLocationARB(s->programmObject, "Texture_Color");
	s->loc_Texture_Gloss = qglGetUniformLocationARB(s->programmObject, "Texture_Gloss");
	s->loc_Texture_Cube = qglGetUniformLocationARB(s->programmObject, "Texture_Cube");
	s->loc_Texture_FogMask = qglGetUniformLocationARB(s->programmObject, "Texture_FogMask");

	// set static uniforms
	if (s->loc_Texture_Normal)
	qglUniform1iARB(s->loc_Texture_Normal, 0);
	if (s->loc_Texture_Color)
	qglUniform1iARB(s->loc_Texture_Color, 1);
	if (s->loc_Texture_Gloss)
	qglUniform1iARB(s->loc_Texture_Gloss, 2);
	if (s->loc_Texture_Cube)
	qglUniform1iARB(s->loc_Texture_Cube, 3);
	if (s->loc_Texture_FogMask)
	qglUniform1iARB(s->loc_Texture_FogMask, 4);

	//		R_CheckError();
	}

	// free the source strings
	if (fragstring)
	//		Mem_Free(&fragstring);
	free(&fragstring);
	if (vertstring)
	//		Mem_Free(&vertstring);
	free(&vertstring);
	// switch back to fixed function program
	qglUseProgramObjectARB( 0 );
	*/
}

qboolean R_Shader_CanRenderLights() {
	return r_refdef.lightShader.programObject != 0 && (r_shadow_realtime_dlight.value || r_shadow_realtime_world.value) && (r_shadow_realtime_draw_models.value || r_shadow_realtime_draw_world.value) && (deathmatch.value || !r_fullbright.value);
}

static void R_Shader_UpdateLightVISCache( R_ShaderLight *light ) {
	unsigned int pvsSize = (cl.worldmodel->numleafs + 7) / 8;
	mleaf_t *leaf = Mod_PointInLeaf( (float*)light->origin, cl.worldmodel );
	if( leaf != NULL ) {
		byte *pvs = Mod_LeafPVS( leaf, cl.worldmodel );
		memcpy( light->visCache, pvs, pvsSize );
	} else {
		// set all leaf flags to draw everything
		memset( light->visCache, 0xff, pvsSize );
	}
}

void R_Shader_SetLight( unsigned int lightIndex, const R_ShaderLight_Definition *light ) {
	R_ShaderLight *r_light = GetLightFromIndex( lightIndex );
	r_light->active = true;
	VectorCopy(light->origin, r_light->origin );
	r_light->maxDistance = light->maxDistance;
	VectorCopy(light->color, r_light->color );
	r_light->style = light->style;
	VectorCopy(light->angles, r_light->angles);
	r_light->corona = light->corona;
	r_light->style = light->style;
	r_light->shadow = light->shadow;
	sprintf (r_light->cubemapname, light->cubemapname);
	// FIXME: clean this up
    {
		int tex = 0;
		if( r_light->cubemapname[0] ) {
			tex = GL_LoadCubeTexImage( r_light->cubemapname, true, false );
		}
		if( tex == 0 ) {
			strncpy( r_light->cubemapname, "defaultcubemap", sizeof( r_light->cubemapname ) - 1 );
		}
	}
	r_light->coronasizescale = light->coronasizescale;
	r_light->ambientscale = light->ambientscale;
	r_light->diffusescale = light->diffusescale;
	r_light->specularscale = light->specularscale;
	r_light->flags = light->flags;
	
	// update the PVS/VIS cache
	R_Shader_UpdateLightVISCache( r_light );			
}

int R_Shader_AddLight( const R_ShaderLight_Definition *light ) {
	int i;
	for( i = 0 ; i < R_MAX_SHADER_LIGHTS ; i++ ) {
		R_ShaderLight *r_light = &r_shader_lights[ i ];
		if( r_light->active == false ) {
			R_Shader_SetLight( i, light );
			return i;
		}
	}
	return -1;
}

void R_Shader_ResetLight( unsigned int lightIndex ) {
	R_ShaderLight *light = GetLightFromIndex( lightIndex );
	light->active = false;
}

qboolean R_Shader_IsLightIndexUsed( unsigned int lightIndex ) {
	R_ShaderLight *light = GetLightFromIndex( lightIndex );
	return light->active;
}

static qboolean R_Shader_IsLightVisibleFromLeaf( const R_ShaderLight *light, mleaf_t *leaf ) {
	unsigned int leafNum;
	assert( leaf != NULL );
	leafNum = leaf - cl.worldmodel->leafs - 1;
	// FIXME: why does it sometimes return solid content leaves which break this assert?
	//assert( leafNum < cl.worldmodel->numleafs );
	if( leafNum >= cl.worldmodel->numleafs ) {
		return true;
	}
	if( (light->visCache[ leafNum / 8 ] & 1<<(leafNum & 7)) > 0 ) {
		return true;
	}
	return false;
}

qboolean R_Shader_IsLightInScopeByPoint( unsigned int lightIndex, const vec3_t renderOrigin ) {
	const R_ShaderLight *light = GetLightFromIndex( lightIndex );
	mleaf_t *leaf;
	if( !light->active ) {
		return false;
	}

	if( VectorDistance( light->origin, renderOrigin ) > light->maxDistance ) {
		return false;
	}

	leaf = Mod_PointInLeaf( (float*)renderOrigin, cl.worldmodel );
	if( leaf == NULL ) {
		return true;
	}

	return R_Shader_IsLightVisibleFromLeaf( light, leaf );
}

qboolean R_Shader_IsLightInScopeByLeaf( unsigned int lightIndex, mleaf_t *leaf ) {
	const R_ShaderLight *light = GetLightFromIndex( lightIndex );

	if( !light->active ) {
		return false;
	}

	if( !R_Shader_IsLightVisibleFromLeaf( light, leaf ) ) {
		return false;
	}

	// bounding sphere check
	{		
		vec3_t midPoint;
		float boundingSphereRadius;
		
		VectorLerp( &leaf->minmaxs[ 0 ], 0.5f, &leaf->minmaxs[ 3 ], midPoint );
		boundingSphereRadius = VectorDistance( &leaf->minmaxs[ 0 ], midPoint );
		if( VectorDistance( light->origin, midPoint ) > light->maxDistance + boundingSphereRadius ) {
			return false;
		}
	}
	return true;
}

/*
image unit 1 contains the diffuse map
image unit 2 contains the cube map
image unit 3 contains the normal map (not yet)
image unit 4 contains the specular map (not yet)

texture coordinate unit 1 contains the texture coordinates
*/
void R_Shader_StartLightRendering() { 
    assert( R_Shader_CanRenderLights() );
	/*
	what it needs to do:
		-set the blend mode
		-set uniforms (lightDirection and viewerDirection) and samplers
	*/	
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	qglUseProgramObjectARB( r_refdef.lightShader.programObject );

	// TODO: move the sampler initialization into Init!
	// sample initialization
	qglUniform1iARB( r_refdef.lightShader.diffuseSampler, 0 );
	qglUniform1iARB( r_refdef.lightShader.cubemapSampler, 1 );
	//glUniform1iARB( r_refdef.lightShader.normalSampler, 2 );
	//glUniform1iARB( r_refdef.lightShader.specularSampler, 3 );
    
	// setup the light position (view-space)
	r_refdef.lightShader.worldToViewMatrix = GetWorldToViewMatrix();
}

qboolean R_Shader_StartLightPass( unsigned int lightIndex ) {
	GLint valid;
	R_ShaderLight *light = GetLightFromIndex( lightIndex );
	matrix4x4_t *worldToViewMatrix = &r_refdef.lightShader.worldToViewMatrix;
	vec3_t lightPosition, newcolor;
	float f;
	
	assert( light->active  == true );

	// setup cubemap texture generation
	if( gl_support_cubemaps ) {
		matrix4x4_t worldToLightMatrix;
		matrix4x4_t viewToWorldMatrix;
		matrix4x4_t viewToLightMatrix;

		// setup the cubemap
		qglSelectTextureARB( GL_TEXTURE1_ARB );
		glEnable( GL_TEXTURE_CUBE_MAP_ARB );
		glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, GL_LoadCubeTexImage( light->cubemapname, false, true ) ); 
		qglSelectTextureARB( GL_TEXTURE0_ARB );
		
		// invert worldToViewMatrix
		worldToLightMatrix = GetWorldToLightMatrix( light );
		Matrix4x4_Invert_Simple( &viewToWorldMatrix, worldToViewMatrix );
		Matrix4x4_Concat( &viewToLightMatrix, &worldToLightMatrix, &viewToWorldMatrix );

		qglUniformMatrix4fvARB( r_refdef.lightShader.viewToLightMatrix, 1, true, (float *)&viewToLightMatrix.m );
	}

	Matrix4x4_Transform( worldToViewMatrix, light->origin, lightPosition );
	//Con_Printf( "Light distance to origin: %f (vs %f)\n", VectorDistance( light->origin, r_refdef.vieworg ), VectorLength( lightPosition ) );
    
	qglUniform3fvARB( r_refdef.lightShader.lightPosition, 1, lightPosition );
	f = (light->style >= 0 ? d_lightstylevalue[light->style] : 128) * (1.0f / 256.0f) * r_shadow_lightintensityscale.value;
	VectorScale(light->color, f, newcolor);
	qglUniform3fvARB( r_refdef.lightShader.lightColor, 1, newcolor );
	qglUniform1fARB( r_refdef.lightShader.lightMaxDistance, light->maxDistance );

	qglValidateProgramARB( r_refdef.lightShader.programObject );
	qglGetObjectParameterivARB( r_refdef.lightShader.programObject, GL_OBJECT_VALIDATE_STATUS_ARB, &valid );
	return valid == true;
}

void R_Shader_FinishLightPass() {
	// reset this here because otherwise Vr2 has problems in e1m1 not idea
	// what causes the texture corruption bug though
	if( gl_support_cubemaps ) {
		qglSelectTextureARB( GL_TEXTURE1_ARB );
		glDisable( GL_TEXTURE_CUBE_MAP_ARB );
		glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, 0 );

		qglSelectTextureARB( GL_TEXTURE0_ARB );
	}
}

void R_Shader_FinishLightRendering() {
	glDisable( GL_BLEND );
	qglUseProgramObjectARB( 0 );
}

void R_Shader_ShowLights()
{
	int i;
	float f;
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDepthMask(GL_FALSE);
	glPointSize(9);
	for (i=0; i < R_MAX_SHADER_LIGHTS; i++)
	{
		if (r_shader_lights[i].active)
		{
			f = (r_shader_lights[i].style >= 0 ? d_lightstylevalue[r_shader_lights[i].style] : 128) * (1.0f / 256.0f);
			glColor4f(r_shader_lights[i].color[0]*f,r_shader_lights[i].color[1]*f,r_shader_lights[i].color[2]*f,1);
			glBegin(GL_POINTS);
			glVertex3fv(r_shader_lights[i].origin);
			glEnd();
		}
	}
	glDepthMask(GL_TRUE);
	glEnable(GL_TEXTURE_2D);
}