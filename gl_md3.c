/* gl_md3.c
 * Based on code from the Aftershock 3D rendering engine
 * Copyright (C) 1999 Stephen C. Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "quakedef.h"
#ifdef Q3MODELS
#include "gl_md3.h"

#define TEMPHACK 1
#ifdef TEMPHACK
extern cvar_t temp1;
#endif

extern vec3_t lightspot;
/* 
================= 
GL_DrawQ3AliasShadow 
================= 
*/ 
void GL_DrawQ3AliasShadow (entity_t *e, md3header_mem_t *header, int lastpose, int pose, float blend) 
{ 
   int               i, j, k; 
   int               frame; 
   int               lastframe; 
   int               vertframeoffset; 
   int               lastvertframeoffset; 
   float            iblend, height, lheight; 
   float            s1, c1; 
   vec3_t            point;
   vec3_t            downmove;
   trace_t            downtrace;
   md3surface_mem_t   *surf; 
   md3tri_mem_t      *tris; 
   md3vert_mem_t      *verts, *vertslast; 

   lheight = e->origin[2] - lightspot[2]; 
   height = 0; 
   iblend = 1.0 - blend; 

   VectorCopy (e->origin, downmove); 
   downmove[2] = downmove[2] - 4096; 
   memset (&downtrace, 0, sizeof(downtrace)); 
   if (cl.worldmodel) { 
      SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, e->origin, downmove, &downtrace); 
   } else { 
      SV_RecursiveHullCheck (sv.worldmodel->hulls, 0, 0, 1, e->origin, downmove, &downtrace); 
   } 
   s1 = sin( e->angles[1]/180*M_PI); 
   c1 = cos( e->angles[1]/180*M_PI); 

   // if its not an md3 model crap out 
   if (*(long *)header->id != MD3IDHEADER){ 
      Con_Printf("MD3 bad model for: %s\n", header->filename); 
      return; 
   } 
   surf = (md3surface_mem_t *)((byte *)header + header->offs_surfaces); 

   // if the surface is incorrect do the same 
   for (i = 0; i < header->num_surfaces; i++) { 
      if (*(long *)surf->id != MD3IDHEADER) { 
         Con_Printf("MD3 bad surface for: %s\n", header->filename); 
      } 

      // YUCK!!! 
      if (surf->num_surf_frames == 0){ 
         surf = (md3surface_mem_t *)((byte *)surf + surf->offs_end); 
         continue;   //shouldn't ever do this, each surface should have at least one frame 
      } 
      frame = e->pose2%surf->num_surf_frames;   //cap the frame inside the list of frames in the model 
      vertframeoffset = frame*surf->num_surf_verts * sizeof(md3vert_mem_t); 
      lastframe = e->pose1%surf->num_surf_frames; 
      lastvertframeoffset = lastframe*surf->num_surf_verts * sizeof(md3vert_mem_t); 

      tris = (md3tri_mem_t *)((byte *)surf + surf->offs_tris); 
      verts = (md3vert_mem_t *)((byte *)surf + surf->offs_verts + vertframeoffset); 
      vertslast = (md3vert_mem_t *)((byte *)surf + surf->offs_verts + lastvertframeoffset); 

      height = -lheight + 1.0; 

      if (gl_stencil == true) { 
         glEnable(GL_STENCIL_TEST); 
         glStencilFunc(GL_EQUAL, 1, 2); 
         glStencilOp(GL_KEEP, GL_KEEP, GL_INCR); 
      } 

	  // Entar : adds transparency to the shadows
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	  glColor4f(0, 0, 0, r_shadows.value);
	  
	  glRotatef (e->angles[0],  0, 1, 0); // offset that nastyness from the rotation, we don't need that axis here
	  glRotatef (-e->angles[2],  1, 0, 0);

      glBegin(GL_TRIANGLES); 

      for (j = 0 ; j < surf->num_surf_tris ; j++) { 
         for (k = 0 ; k < 3 ; k++) { 
            point[0] = verts[tris[j].tri[k]].vec[0] * blend + vertslast[tris[j].tri[k]].vec[0] * iblend; 
            point[1] = verts[tris[j].tri[k]].vec[1] * blend + vertslast[tris[j].tri[k]].vec[1] * iblend; 
            //point[2] = verts[tris[j].tri[k]].vec[2] * blend + vertslast[tris[j].tri[k]].vec[2] * iblend; // this doesnt seem to matter even if its not used 

            point[2] = - (e->origin[2] - downtrace.endpos[2]); 

            point[2] += ((point[1] * (s1 * downtrace.plane.normal[0])) - 
                       (point[0] * (c1 * downtrace.plane.normal[0])) - 
                       (point[0] * (s1 * downtrace.plane.normal[1])) - 
                       (point[1] * (c1 * downtrace.plane.normal[1]))) +  
                      ((1.0 - downtrace.plane.normal[2])*20) + 0.2; 
            glVertex3fv (point); 
         } 
      } 
      glEnd(); 
   } 
   surf = (md3surface_mem_t *)((byte *)surf + surf->offs_end); 

   if (gl_stencil == true) { 
      glDisable(GL_STENCIL_TEST); 
   } 
}

void R_MD3TagRotate (entity_t *e, model_t *tagmodel, char *tagname)
{
	int i;
	md3tag_t *tag = NULL;
	md3header_t *model = Mod_Extradata(tagmodel);
	float m[16];

	for (i=0; i<model->num_tags; i++)
	{
		md3tag_t *tags = (md3tag_t *)((byte *)model + model->tag_offs);
		if(Q_strcmp(tags[i].name, tagname)==0)
#if TEMPHACK
			if(temp1.value > model->num_frames)
				tag = &tags[(model->num_frames-1) * model->num_tags + i];
			else
				tag = &tags[(int)temp1.value * model->num_tags + i];
#else
			if(e->frame > model->num_frames)
				tag = &tags[(model->num_frames-1) * model->num_tags + i];
			else
				tag = &tags[e->frame * model->num_tags + i];
#endif
	}

	if(!tag)
	{
		Con_Printf("Tag not found in %s : %s\n", tagmodel->name, tagname);
		return;
	}

	m[0] = tag->rot[0][0];	m[4] = tag->rot[1][0];	m[8] = tag->rot[2][0];	m[12] = tag->pos[0];
	m[1] = tag->rot[0][1];	m[5] = tag->rot[1][1];	m[9] = tag->rot[2][1];	m[13] = tag->pos[1];
	m[2] = tag->rot[0][2];	m[6] = tag->rot[1][2];	m[10]= tag->rot[2][2];	m[14] = tag->pos[2];
	m[3] = 0;				m[7] = 0;				m[11]= 0;				m[15] = 1;

	glMultMatrixf(m);
}

extern int		multitex_go;

extern GLfloat	vert[MAXALIASVERTS*3];
extern GLfloat	texture[MAXALIASVERTS*2];
extern GLfloat	normal[MAXALIASVERTS*3];
extern GLfloat	color[MAXALIASVERTS*4];

// returns number of polys drawn
int R_DrawQ3Model(entity_t *e, int shell, int outline, float blend)
{
	md3header_mem_t *model;
	int i, j, k, offs;
	int polys=0;
	int frame;
	int lastframe;
	int vertframeoffset;
	int lastvertframeoffset;
	//int *tris;
	md3surface_mem_t *surf;
	md3shader_mem_t *shader;
	md3st_mem_t *tc;
	md3tri_mem_t *tris;
	md3vert_mem_t *verts, *vertslast;

	int	usevertexarray = true;

	extern vec3_t lightcolor;
	extern vec3_t shadevector;
	extern	unsigned int	celtexture;
	extern	unsigned int	vertextexture;
	extern int	vertshade;

	//md3 interpolation
	float iblend;

	model = Mod_Extradata (e->model);

	if (*(long *)model->id != MD3IDHEADER){
		Con_Printf("MD3 bad model for: %s\n", model->filename);
		return 0;
	}

	iblend = 1.0 - blend;

	surf = (md3surface_mem_t *)((byte *)model + model->offs_surfaces);
	for (i = 0, offs=0; i < model->num_surfaces; i++)
	{
		if (*(long *)surf->id != MD3IDHEADER){
			Con_Printf("MD3 bad surface for: %s\n", model->filename);
			surf = (md3surface_mem_t *)((byte *)surf + surf->offs_end);
			continue;
		}

		frame = e->frame;
		if (surf->num_surf_frames == 0){
			surf = (md3surface_mem_t *)((byte *)surf + surf->offs_end);
			continue;	//shouldn't ever do this, each surface should have at least one frame
		}

		frame = frame % surf->num_surf_frames;	//cap the frame inside the list of frames in the model
		vertframeoffset = frame * surf->num_surf_verts * sizeof(md3vert_mem_t);

		lastframe = e->pose1 % surf->num_surf_frames;
		lastvertframeoffset = lastframe * surf->num_surf_verts * sizeof(md3vert_mem_t);

		//get pointer to shaders
		shader = (md3shader_mem_t *)((byte *)surf + surf->offs_shaders);
		tc = (md3st_mem_t *)((byte *)surf + surf->offs_tc);
		tris = (md3tri_mem_t *)((byte *)surf + surf->offs_tris);
		verts = (md3vert_mem_t *)((byte *)surf + surf->offs_verts + vertframeoffset);
		vertslast = (md3vert_mem_t *)((byte *)surf + surf->offs_verts + lastvertframeoffset);

		if (!shell){
			if (surf->num_surf_shaders!=0)
				glBindTexture(GL_TEXTURE_2D, shader[(e->skinnum%surf->num_surf_shaders)].texnum);
			else
				glBindTexture(GL_TEXTURE_2D, 0);
		}

		//for each triangle
		for (j = 0; j < surf->num_surf_tris; j++)
		{
			//draw the poly
			for (k=0; k < 3; k++)
			{
				//interpolated
				vert[0+offs*3] = verts[tris[j].tri[k]].vec[0] * blend + vertslast[tris[j].tri[k]].vec[0] * iblend;
				vert[1+offs*3] = verts[tris[j].tri[k]].vec[1] * blend + vertslast[tris[j].tri[k]].vec[1] * iblend;
				vert[2+offs*3] = verts[tris[j].tri[k]].vec[2] * blend + vertslast[tris[j].tri[k]].vec[2] * iblend;
				normal[0+offs*3] = verts[tris[j].tri[k]].normal[0] * blend + vertslast[tris[j].tri[k]].normal[0] * iblend;
				normal[1+offs*3] = verts[tris[j].tri[k]].normal[1] * blend + vertslast[tris[j].tri[k]].normal[1] * iblend;
				normal[2+offs*3] = verts[tris[j].tri[k]].normal[2] * blend + vertslast[tris[j].tri[k]].normal[2] * iblend;
				color[0+offs*4] = lightcolor[0];
				color[1+offs*4] = lightcolor[1];
				color[2+offs*4] = lightcolor[2];
				color[3+offs*4] = e->alpha;
				polys++;

				if (shell)
				{
					vert[offs*3] += normal[offs*3];
					vert[1+offs*3] += normal[1+offs*3];
					vert[2+offs*3] += normal[2+offs*3];
				}

				if (!shell)
				{
					texture[offs*2] = tc[tris[j].tri[k]].s;
					texture[1+offs*2] = tc[tris[j].tri[k]].t;
				}else{
					texture[offs*2] = tc[tris[j].tri[k]].s + realtime*2;
					texture[1+offs*2] = tc[tris[j].tri[k]].t + realtime*slowmo.value*2;
				}
				offs++;
			}
		}
		glDrawArrays(GL_TRIANGLES, 0, surf->num_surf_tris*3);

		surf = (md3surface_mem_t *)((byte *)surf + surf->offs_end);
	}
	return polys;
}

extern char loadname[32];

void Mod_LoadQ3Model(model_t *mod, void *buffer)
{
	md3header_t *header;
	md3header_mem_t *mem_head;
	md3surface_t *surf;
	md3surface_mem_t *currentsurf;
	int i, j;//, size, skinnamelen;
	int posn;
	int surfstartposn;
	char name[128], name2[128];

	//we want to work out the size of the model in memory
	
	//size of surfaces = surface size + num_shaders * shader_mem size + 
	//		num_triangles * tri size + num_verts * textcoord size +
	//		num_verts * vert_mem size
	int surf_size = 0;
	int mem_size = 0;

	//pointer to header
	header = (md3header_t *)buffer;
	//pointer to the surface list
	surf = (md3surface_t*)((byte *)buffer + header->surface_offs);

	surf_size = 0;
	for (i = 0; i < header->num_surfaces; i++)
	{
		surf_size += sizeof(md3surface_mem_t);
		surf_size += surf->num_surf_shaders * sizeof(md3shader_mem_t);
		surf_size += surf->num_surf_tris * sizeof(md3tri_mem_t);
		surf_size += surf->num_surf_verts * sizeof(md3st_mem_t);	//space for actual texture coords
		surf_size += surf->num_surf_verts * sizeof(md3stshade_mem_t);	//space for shading texture coords
		surf_size += surf->num_surf_verts * surf->num_surf_frames * sizeof(md3vert_mem_t);

		//goto next surf
		surf = (md3surface_t*)((byte *)surf + surf->end_offs);
	}

	//total size =	header size + num_frames * frame size + num_tags * tag size +
	//		size of surfaces
	mem_size = sizeof(md3header_mem_t);
	mem_size += header->num_frames * sizeof(md3frame_mem_t);
	mem_size += header->num_tags * sizeof(md3tag_mem_t);
	mem_size += surf_size;

	Con_DPrintf("Loading md3 model...%s (%s)\n", header->filename, mod->name);

	mem_head = (md3header_mem_t *)Cache_Alloc (&mod->cache, mem_size, mod->name);
	if (!mod->cache.data){
		return;	//cache alloc failed
	}

	//setup more mem stuff
	mod->type = mod_alias;
	mod->aliastype = MD3IDHEADER;
	mod->numframes = header->num_frames;

	//copy stuff across from disk buffer to memory
	posn = 0; //posn in new buffer

	//copy header
	Q_memcpy(mem_head, header, sizeof(md3header_t));
	posn += sizeof(md3header_mem_t);

	//posn of frames
	mem_head->offs_frames = posn;

	//copy frames
	Q_memcpy((byte *)mem_head + mem_head->offs_frames, (byte *)header + header->frame_offs, sizeof(md3frame_t)*header->num_frames);
	posn += sizeof(md3frame_mem_t)*header->num_frames;

	//posn of tags
	mem_head->offs_tags = posn;

	//copy tags
	Q_memcpy((byte *)mem_head + mem_head->offs_tags, (byte *)header + header->tag_offs, sizeof(md3tag_t)*header->num_tags);
	posn += sizeof(md3tag_mem_t)*header->num_tags;

	//posn of surfaces
	mem_head->offs_surfaces = posn;

	//copy surfaces, one at a time
	//get pointer to surface in file
	surf = (md3surface_t *)((byte *)header + header->surface_offs);
	//get pointer to surface in memory
	currentsurf = (md3surface_mem_t *)((byte *)mem_head + posn);
	surfstartposn = posn;

	for (i=0; i < header->num_surfaces; i++)
	{
		//copy size of surface
		Q_memcpy((byte *)mem_head + posn, (byte *)header + header->surface_offs, sizeof(md3surface_t));
		posn += sizeof(md3surface_mem_t);

		//posn of shaders for this surface
		currentsurf->offs_shaders = posn - surfstartposn;
		
		for (j=0; j < surf->num_surf_shaders; j++){
			//copy jth shader accross
			Q_memcpy((byte *)mem_head + posn, (byte *)surf + surf->shader_offs + j * sizeof(md3shader_t), sizeof(md3shader_t));
			posn += sizeof(md3shader_mem_t); //copyed non-mem into mem one
		}
		//posn of tris for this surface
		currentsurf->offs_tris = posn - surfstartposn;

		//copy tri
		Q_memcpy((byte *)mem_head + posn, (byte *)surf + surf->tris_offs, sizeof(md3tri_t) * surf->num_surf_tris);
		posn += sizeof(md3tri_mem_t) * surf->num_surf_tris;

		//posn of tex coords in this surface
		currentsurf->offs_tc = posn - surfstartposn;

		//copy st
		Q_memcpy((byte *)mem_head + posn, (byte *)surf + surf->tc_offs, sizeof(md3st_t) * surf->num_surf_verts);
		posn += sizeof(md3st_mem_t) * surf->num_surf_verts;

		//insert space for shading texture coords
		currentsurf->offs_shadetc = posn - surfstartposn;
		posn += sizeof(md3stshade_mem_t) * surf->num_surf_verts;

		//posn points to surface->verts
		currentsurf->offs_verts = posn - surfstartposn;

		//next to have to be redone
		for (j=0; j < surf->num_surf_verts * surf->num_surf_frames; j++){
			float lat;
			float lng;

			//convert verts from shorts to floats
			md3vert_mem_t *mem_vert = (md3vert_mem_t *)((byte *)mem_head + posn);
			md3vert_t *disk_vert = (md3vert_t *)((byte *)surf + surf->vert_offs + j * sizeof(md3vert_t));
			mem_vert->vec[0] = (float)disk_vert->vec[0] / 64.0f;
			mem_vert->vec[1] = (float)disk_vert->vec[1] / 64.0f;
			mem_vert->vec[2] = (float)disk_vert->vec[2] / 64.0f;

			
			//work out normals
//			lat = (disk_vert->normal + 255) * (2 * 3.141592654f) / 256.0f;
			lat = (disk_vert->normal & 255) * (2 * 3.141592654f) / 256.0f;
			lng = ((disk_vert->normal >> 8) & 255) * (2 * 3.141592654f) / 256.0f;
			mem_vert->normal[0] = /*-*/(float)(sin (lat) * cos (lng));
			mem_vert->normal[1] =  (float)(sin (lat) * sin (lng));
			mem_vert->normal[2] =  (float)(cos (lat) * 1);

			posn += sizeof(md3vert_mem_t); //copyed non-mem into mem one
		}

		//point to next surf (or eof)
		surf = (md3surface_t*)((byte *)surf + surf->end_offs);

		//posn points to the end of this surface
		currentsurf->offs_end = posn;
		//next start of surf (if there is one)
		surfstartposn = posn;
	}
	//posn should now equal mem_size
	if (posn != mem_size){
		Con_Printf("Copied diffrent ammount to what was worked out, copied: %i worked out: %i\n",posn, mem_size);
	}

	VectorCopy(((md3frame_mem_t *)((byte *)mem_head + mem_head->offs_frames))->mins, mod->mins);
	VectorCopy(((md3frame_mem_t *)((byte *)mem_head + mem_head->offs_frames))->maxs, mod->maxs);
	mod->flags = mem_head->flags;

	//get pointer to first surface
	currentsurf = (md3surface_mem_t *)((byte *)mem_head + mem_head->offs_surfaces);
	for (i=0; i<mem_head->num_surfaces; i++)
	{
		if (*(long *)currentsurf->id != MD3IDHEADER)
		{
			Con_Printf("MD3 bad surface for: %s\n", mem_head->filename);
		}
		else
		{
			md3shader_mem_t *shader = (md3shader_mem_t *)((byte *)currentsurf + currentsurf->offs_shaders);
			
			for (j=0; j<currentsurf->num_surf_shaders; j++)
			{
				//try loading several different texture names here
				sprintf(&name[0],"progs/%s",shader[j].name);
				
				shader[j].texnum = GL_LoadTexImage(&name[0], false, true);
				if (shader[j].texnum == 0)
				{
					sprintf(&name[0], "progs/%s", mem_head->filename);
					shader[j].texnum = GL_LoadTexImage(&name[0], false, true);
					
					if (shader[j].texnum == 0)
					{
						sprintf (&name[0], "progs/%s_%i", shader[j].name, j); // name_# for multiple skins
						shader[j].texnum = GL_LoadTexImage(&name[0], false, true);
						if (shader[j].texnum == 0)
						{
							sprintf (&name[0], "progs/%s_%i", mem_head->filename, j); // name_# for multiple skins
							shader[j].texnum = GL_LoadTexImage(&name[0], false, true);
							if (shader[j].texnum == 0)
							{
								Con_Printf("Model: %s  Texture missing: %s\n", mod->name, shader[j].name);
								shader[j].texnum = 0;//GL_LoadTexture ("notexture", r_notexture_mip->width, r_notexture_mip->height, (byte *)((int) r_notexture_mip + r_notexture_mip->offsets[0]), false, false, 1);
							}
						}
					}
				}
				if (shader[j].texnum != 0)
				{
					sprintf (&name2[0], "%s_luma", name);
					shader[j].texnum_fullbright = GL_LoadTexImage(&name2[0], false, true);
					sprintf (&name2[0], "%s_gloss", name);
					shader[j].texnum_gloss = GL_LoadTexImage(&name2[0], false, true);
				}
			}
		}
		currentsurf = (md3surface_mem_t *)((byte *)currentsurf + currentsurf->offs_end);
	}
}

#if 0
int debug = 0;
multimodel_t *Mod_AddMultiModel (entity_t *entity, model_t *mod);
void Mod_LoadQ3MultiModel (model_t *mod)
{
	multimodel_t *head, *upper, *lower;
	int handle;
	char path[MAX_QPATH];
	mod->type = mod_null;

	sprintf(path, "%sanimation.cfg", mod->name);
	COM_OpenFile(path, &handle);

	if(handle) //Q3Player
	{
		COM_CloseFile(handle);
		//Load player
		lower = Mod_AddMultiModel(NULL, mod);//Allocate a mmodel
		lower->model = Z_Malloc(sizeof(model_t));
		Q_strcpy(lower->model->name, va("%slower.md3", mod->name));
		lower->model->needload = TRUE;
		Mod_LoadModel(lower->model, 1);
		Q_strcpy(lower->identifier, "lower");

		lower->linktype = MULTIMODEL_LINK_STANDARD;


		upper = Mod_AddMultiModel(NULL, mod);//Allocate a mmodel
		upper->model = Z_Malloc(sizeof(model_t));
		Q_strcpy(upper->model->name, va("%supper.md3", mod->name));
		upper->model->needload = TRUE;
		Mod_LoadModel(upper->model, 1);
		Q_strcpy(upper->identifier, "upper");

		upper->linktype = MULTIMODEL_LINK_TAG;
		upper->linkedmodel = lower;
		Q_strcpy(upper->tagname, "tag_torso");


		head = Mod_AddMultiModel(NULL, mod);//Allocate a mmodel
		head->model = Z_Malloc(sizeof(model_t));
		Q_strcpy(head->model->name, va("%shead.md3", mod->name));
		head->model->needload = TRUE;
		Mod_LoadModel(head->model, 1);
		Q_strcpy(head->identifier, "head");

		head->linktype = MULTIMODEL_LINK_TAG;
		head->linkedmodel = upper;
		Q_strcpy(head->tagname, "tag_head");
		
		return;
	}

	sprintf(path, "%s", mod->name);
	Q_strcat(path, Q_strrchr(path, '/'));
	path[Q_strlen(path)-1] = '\0';
	Q_strcat(path, "_hand.md3");
	Sys_Error("Trying to find weaponmodel %s", path);
	COM_OpenFile(path, &handle);
	if(handle) //W_Weapon
	{
		COM_CloseFile(handle);
	}
}
#endif

#endif
