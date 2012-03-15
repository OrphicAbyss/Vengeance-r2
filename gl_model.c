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
// models.c -- model loading and caching
// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"
#include "gl_md3.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// Entar : can you guess what this qboolean is for?
qboolean hl_map;

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};
cvar_t halflifebsp = {"halflifebsp", "0"};

cvar_t ed_printtextures = {"ed_printtextures", "0", true};

//QMB :md2
void Mod_LoadQ2AliasModel (model_t *mod, void *buffer);

extern char *shaderScript;

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	Cvar_RegisterVariable (&halflifebsp);
	Cvar_RegisterVariable (&gl_subdivide_size);
	Cvar_RegisterVariable (&ed_printtextures);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_Init
===============
*/
void Mod_Shutdown (void)
{
	Mod_ClearAll();

	mod_numknown = 0;
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}
/*
byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}
*/
// mh - auto water trans begin 
byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model) 
{ 
   // leaf 0 is the solid leaf 
   if (leaf == model->leafs) return mod_novis; 

   // no visdata 
   if (!leaf->decompressed_vis) return mod_novis; 

   // return the decompressed vis 
   return leaf->decompressed_vis; 
}
// mh - auto water trans end 

// mh - auto water trans begin 
void Mod_AddLeaftoPVS (model_t *mod, mleaf_t *targleaf, mleaf_t *addleaf) 
{ 
   int i; 

   if (!targleaf->decompressed_vis) return; 
   if (!addleaf->decompressed_vis) return; 

   for (i = 0; i < mod->vissize; i++) 
   { 
      targleaf->decompressed_vis[i] |= addleaf->decompressed_vis[i]; 
   } 
} 


qboolean Mod_CompareSurfs (msurface_t *surf1, msurface_t *surf2) 
{ 
   glpoly_t   *p1; 
   glpoly_t   *p2; 

   // don't forget this!!! 
   if (surf1->surfNum == surf2->surfNum) return false; 

   // already matched (redundant check) 
   if (surf1->match) return false; 
   if (surf2->match) return false; 

   // it's always either one before or 1 after (qbsp quirk) 
   // both searches are going forward, so the logic here is sound.  the first check of surf2 
   // will always be the one before surf1, the second check the one after. 
   if (surf1->surfNum > surf2->surfNum + 1) return false; 
   if (surf2->surfNum > surf1->surfNum + 1) return false; 

   // same texture (checked e2m3, it's OK) 
   if (surf1->texinfo->texture->gl_texturenum != surf2->texinfo->texture->gl_texturenum) return false; 

   // both must be turbulent (redundant check) 
   if (!((surf1->flags & SURF_DRAWTURB) && (surf2->flags & SURF_DRAWTURB))) return false; 

   // same number of polys 
   if (surf1->numPolys != surf2->numPolys) return false; 

   // we only need to check if each poly has the same number of verts here 
   // SURF_DRAWTURB surfs are already subdivided from the map load, so this is safe to do 
   for (p1 = surf1->polys, p2 = surf2->polys; p1; p1 = p1->next, p2 = p2->next) 
      if (p1->numverts != p1->numverts) return false; 

   return true; 
} 


void Mod_BruteForceSurfMatch (model_t *mod) 
{ 
   msurface_t *surf1; 
   msurface_t *surf2; 

   int i; 
   int j; 

   for (i = 0, surf1 = mod->surfaces; i < mod->numsurfaces; i++, surf1++) 
   { 
      // not liquid 
      if (!(surf1->flags & SURF_DRAWTURB)) continue; 

      // already matched 
      if (surf1->match) continue; 

      for (j = 0, surf2 = mod->surfaces; j < mod->numsurfaces; j++, surf2++) 
      { 
         // not liquid 
         if (!(surf2->flags & SURF_DRAWTURB)) continue; 

         // already matched 
         if (surf2->match) continue; 

         // try to match them 
         if (Mod_CompareSurfs (surf1, surf2)) 
         { 
            // match in both directions 
            surf1->match = surf2; 
            surf2->match = surf1; 
         } 
      } 
   } 


   for (i = 0, surf1 = mod->surfaces; i < mod->numsurfaces; i++, surf1++) 
   { 
      if (!(surf1->flags & SURF_DRAWTURB)) continue; 
      if (surf1->match) continue; 

      Con_DPrintf ("Liquid surf %i with no match\n", i); 
   } 
} 
// mh - auto water trans end 

// mh - auto water trans part 3 begin 
void Mod_AddTransPVSForSurf (model_t *mod, msurface_t *msurf, mleaf_t *mleaf) 
{ 
   int i; 
   int j; 
   mleaf_t *leaf; 
   msurface_t **surf; 

   // now we need to find all leafs in the map that contain the matched surf 
   for (i = 1; i < mod->numleafs + 1; i++) 
   { 
      // get the leaf 
      leaf = &mod->leafs[i]; 

      // not interested in these leafs 
      if (leaf->contents > CONTENTS_EMPTY) continue; 
      if (leaf->contents == CONTENTS_SOLID) continue; 
      if (leaf->contents <= CONTENTS_SKY) continue; //SKY and ORIGIN and CLIP
      //if (leaf->contents == CONTENTS_ORIGIN) continue; 
      //if (leaf->contents == CONTENTS_CLIP) continue; 

      // must be different contents to the original leaf 
      if (leaf->contents == mleaf->contents) continue; 

      // no visibility 
      if (!leaf->decompressed_vis) continue; 

      // check marksurfaces for the match 
      surf = leaf->firstmarksurface; 

      for (j = 0; j < leaf->nummarksurfaces; j++, surf++) 
      { 
         // bad surf/texinfo/texture (some old maps have this from a bad qbsp) 
         if (!surf) continue; 
         if (!(*surf)) continue; 
         if (!(*surf)->texinfo) continue; 
         if (!(*surf)->texinfo->texture) continue; 

         // liquid only 
         if ((*surf)->texinfo->texture->name[0] != '*') continue; 

         // no reciprocal match 
         if (!(*surf)->match) continue; 

         // see do they match 
         if ((*surf) == msurf) 
         { 
            goto LeafOK; 
         } 
      } 

      // didn't find a match 
      continue; 

LeafOK:; 
      // found a match.  add this leaf's PVS to the original leaf.  we have to continue 
      // checking leafs however as this surf may be shared between more than 1 leaf. 
      Mod_AddLeaftoPVS (mod, mleaf, leaf); 

      // also add the original leaf to this leaf 
      Mod_AddLeaftoPVS (mod, leaf, mleaf); 

      // set the visframe of both to -2 to indicate that they're already merged 
      leaf->visframe = -2; 
      mleaf->visframe = -2; 
   } 
} 


void Mod_SetAWT (model_t *mod) 
{ 
   int i; 
   int j; 
   int k; 
   mleaf_t *leaf; 
   byte *leafpvs = malloc ((mod->numleafs + 7) >> 3); 
   msurface_t **surf; 

   // use negative values so as not to conflict with runtime visibility checking 
   // -1 = initial value (because 0 is valid at runtime) 
   // -2 = leaf already merged 
   // -3 or lower = active visframes 
   r_visframecount = -3; 

   // go through all the leafs, ignoring leaf 0 
   for (i = 1; i < mod->numleafs + 1; i++) 
   { 
      // get the leaf 
      leaf = &mod->leafs[i]; 

      // not interested in these leafs 
      if (leaf->contents > CONTENTS_EMPTY) continue; 
      if (leaf->contents == CONTENTS_SOLID) continue; 
      if (leaf->contents <= CONTENTS_SKY) continue; 
      //if (leaf->contents == CONTENTS_ORIGIN) continue; 
      //if (leaf->contents == CONTENTS_CLIP) continue; 

      // no visibility 
      if (!leaf->decompressed_vis) continue; 

      // get the leafs PVS.  here we do a memcpy rather than a straight pointer assignment 
      // because we'll be modifying the original PVS, so we want a seperate copy to work on 
      memcpy (leafpvs, leaf->decompressed_vis, (mod->numleafs + 7) >> 3); 

      // already merged 
      if (leaf->visframe == -2) continue; 

      // see has the leaf pvs got any liquid in it 
      // this should be done as part of the original pvs check for whether the map already has 
      // translucent water in Mod_DetectWaterTrans, but i wanted to keep them seperate for the 
      // purposes of this tutorial. 
      for (j = 0; j < mod->numleafs; j++) 
      { 
         // in the PVS 
         if (leafpvs[j >> 3] & (1 << (j & 7))) 
         { 
            // get the leaf 
            mleaf_t *visleaf = &mod->leafs[j + 1]; 

            surf = visleaf->firstmarksurface; 

            for (k = 0; k < visleaf->nummarksurfaces; k++, surf++) 
            { 
               // bad surf/texinfo/texture (some old maps have this from a bad qbsp) 
               if (!surf) continue; 
               if (!(*surf)) continue; 
               if (!(*surf)->texinfo) continue; 
               if (!(*surf)->texinfo->texture) continue; 

               // no match 
               if (!(*surf)->match) continue; 

               // texture isn't liquid 
               if ((*surf)->texinfo->texture->name[0] != '*') continue; 

               // already checked this pass (necessary because more than 1 
               // leaf may have this surf) 
               if ((*surf)->visframe == r_visframecount) continue; 

               // set surface visframe so it won't get checked again 
               (*surf)->visframe = r_visframecount; 

               // do the water trans bit.  we're adding PVS to the original leaf 
               // so we must use that.  pass in the matched surf 
               Mod_AddTransPVSForSurf (mod, (*surf)->match, leaf); 
            } 
         } 
      } 

      // decrement visframe count 
      r_visframecount--; 
   } 

   // release our memory 
   free (leafpvs); 
} 


// mh 17th july 2006 - merges the pvs for 2 leafs.
// this might duplicate code from elsewhere, i dunno.  here it is again anyway...
void Mod_MergeLeafPVS (model_t *mod, mleaf_t *src, mleaf_t *dst)
{
	int i;

	// no pvs to merge!
	if (!src->decompressed_vis) return;
	if (!dst->decompressed_vis) return;

	for (i = 0; i < (mod->numleafs >> 3); i++)
	{
		dst->decompressed_vis[i] |= src->decompressed_vis[i];
	}
}


// mh 17th july 2006 - pick up orphaned leafs
void Mod_FinishAWT (model_t *mod)
{
	int i;
	int j;
	mleaf_t *l;
//	mleaf_t *l2;

	for (i = 0; i < mod->numleafs; i++)
	{
		l = &mod->leafs[i];

		if (!l || !l->decompressed_vis)
			continue;

		// deal with underwater leafs
		if (l->contents == CONTENTS_WATER || l->contents == CONTENTS_SLIME || l->contents == CONTENTS_LAVA)
		{
			// we're underwater now, find all other underwater leafs in our current pvs and merge their pvs into the original
			for (j = 0; j < mod->numleafs; j++) 
				if ((l->decompressed_vis[j >> 3] & (1 << (j & 7))) && 
					(mod->leafs[j].contents == CONTENTS_WATER || mod->leafs[j].contents == CONTENTS_SLIME 
					|| mod->leafs[j].contents == CONTENTS_LAVA))
					Mod_MergeLeafPVS (mod, &mod->leafs[j], l);

			continue;
		}
		else if (l->contents == CONTENTS_EMPTY)
		{
		}

		// not interested in any other contents type
	}
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;
	
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	
	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");
		
//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;
			
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");

		memset(mod, 0, sizeof(*mod));
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	void	*d;
	unsigned *buf;
	byte	stackbuf[1024];		// avoid dirtying the cache heap
	char	strip[128];
	char	md2name[128];
	char	md3name[128];

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod;		// not cached at all
	}
	
#ifdef Q3MODELS
#if Q3MODELS && MULTIMDL //If a directory is passed in stead of a path, open it as a Q3Player.
	c = COM_FileExtension(mod->name);
	if(!c[0])
	{
		mod->needload = false;
		Mod_LoadQ3MultiModel(mod);
		return mod;
	}
#endif
#endif 

//
// load the file
//
	COM_StripExtension(mod->name, &strip[0]);
	sprintf (&md2name[0], "%s.md2", &strip[0]);
	sprintf (&md3name[0], "%s.md3", &strip[0]);

	buf = (unsigned *)COM_LoadStackFile (md3name, stackbuf, sizeof(stackbuf));
	if (!buf){
		buf = (unsigned *)COM_LoadStackFile (md2name, stackbuf, sizeof(stackbuf));
		if (!buf){
			buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf));
			if (!buf)
			{
				if (crash)
					Sys_Error ("Mod_NumForName: %s not found", mod->name);
				return NULL;
			}
		}
	}
	
//
// allocate a new model
//
	COM_FileBase (mod->name, loadname);
	
	loadmodel = mod;

//
// fill it in
//

// call the apropriate loader
	mod->needload = false;
	
	switch (LittleLong(*(unsigned *)buf))
	{
#ifdef Q3MODELS
   case MD3IDHEADER:
      Mod_LoadQ3Model (mod, buf);
      break;
#endif 

	case MD2IDALIASHEADER:
		Mod_LoadQ2AliasModel (mod, buf);
		break;

	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;
	
	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;

qboolean Img_HasFullbrights (byte *pixels, int size) {
    int i;

    for (i = 0; i < size; i++)
        if (pixels[i] >= 224)
            return true;

    return false;
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	extern cvar_t gl_24bitmaptex;
	int		i, j, pixels, num, max, altmax;
	
	char	*c;
	char	pathName[64], mapname[MAX_QPATH];

	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;
	
// Tomaz || TGA Begin
	char		texname[64],texnamefb[64],texnamefbluma[64];
// Tomaz || TGA End

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	
	m->nummiptex = LittleLong (m->nummiptex);
	
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , loadname);

	c = loadmodel->name;
	if (!strncasecmp(c, "maps/", 5))
		c += 5;
	COM_StripExtension(c, &mapname);
	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);
		
		if ( (mt->width & 15) || (mt->height & 15) )
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width*mt->height/64*85;
		tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));

		if (ed_printtextures.value)
			Con_Printf("%s\n", tx->name);

		tx->width = mt->width;
		tx->height = mt->height;

		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		if (!hl_map) // Entar : HACK
			memcpy ( tx+1, mt+1, pixels);
		

		if (!Q_strncmp(mt->name,"sky",3) || !Q_strncmp(mt->name,"SKY",3))	
		{
			R_InitSky (tx);
		}
		else
		{ // Fixed :)
			texture_mode = GL_LINEAR_MIPMAP_LINEAR;
			
			if (gl_24bitmaptex.value)
			{
				sprintf (texname, "textures/%s/%s", mapname, mt->name);
				tx->gl_texturenum = GL_LoadTexImage (texname, false, true);
				if (tx->gl_texturenum == 0)
				{
					sprintf (texname, "textures/%s", mt->name);
					tx->gl_texturenum = GL_LoadTexImage (texname, false, true);
					sprintf (pathName, "textures");
				}
				else // worked the first time
					sprintf (pathName, "textures/%s", mapname);

				sprintf (texnamefb, "%s/%s_glow", pathName, mt->name);

				//if there is a 24bit texture, check for a fullbright
				if (tx->gl_texturenum != 0)
				{
					sprintf (texnamefbluma, "%s/%s_luma", pathName, mt->name);
					tx->gl_fullbright = GL_LoadTexImage (texnamefbluma, false, true);
					sprintf (texnamefbluma, "%s/%s_gloss", pathName, mt->name);
					tx->gl_gloss = GL_LoadTexImage (texnamefbluma, false, true);
					
					/*if (tx->gl_fullbright == 0){ //no texture _glow
						sprintf (texnamefb, "textures/%s_glow", mt->name);
						tx->gl_fullbright = GL_LoadTexImage (texnamefb, false, true);
					}*/
				}else{
					tx->gl_fullbright = 0;
				}
			}

			if (tx->gl_texturenum == 0) // No Matching Texture
			{
				tx->gl_texturenum = GL_LoadTexture (mt->name, tx->width, tx->height, (byte *)(tx+1), true, false ,1);
			
				//check if there is a fullbright for the defualt
				//no need for if, only get here if no 24bits were loaded
				//if (tx->gl_fullbright == 0){ // No Matching Fullbright Texture
					if (Img_HasFullbrights((byte *)(tx+1), tx->width*tx->height)){
						tx->gl_fullbright = GL_LoadTexture (texnamefb, tx->width, tx->height, (byte *)(tx+1), true, 2 ,1);
					}else{
						tx->gl_fullbright = 0;
					}
				//}
			}

			texture_mode = GL_LINEAR;
		}
	}

//
// sequence the animations
//
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error ("Bad animating texture %s", tx->name);

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture %s", tx->name);
		}
		
#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

extern int		image_width;
extern int		image_height;

/*
=================
Mod_LoadHLTextures
=================
*/
// disabled as of yet - doesn't work properly.
void Mod_LoadHLTextures (lump_t *l)
{/*
	int				i, j, num, max, altmax;
	miptex_t		*mt;
	texture_t		*tx, *tx2;
	texture_t		*anims[10];
	texture_t		*altanims[10];
	dmiptexlump_t	*m;
	byte			*data;
	int				*dofs;
	char			texname[64];
	qboolean		fullbrights = false;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , loadname);

	dofs = m->dataofs;
	for (i=0 ; i<m->nummiptex ; i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1)
			continue;
		mt = (miptex_t *)((byte *)m + dofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);
		
		if ( (mt->width & 15) || (mt->height & 15) )
			Host_Error ("Texture %s is not 16 aligned", mt->name);

		tx = Hunk_AllocName (sizeof(texture_t), loadname );
		loadmodel->textures[i] = tx;

		for (j = 0;mt->name[j] && j < 15;j++)
		{
			if (mt->name[j] >= 'A' && mt->name[j] <= 'Z')
				tx->name[j] = mt->name[j] + ('a' - 'A');
			else
				tx->name[j] = mt->name[j];
		}
		for (;j < 16;j++)
			tx->name[j] = 0;

		tx->width = mt->width;
		tx->height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = 0;

		//tx->rs			= GetRSForName(mt->name);

		sprintf (texname, "textures/%s", tx->name);

	// NOTE: uncommenting this will let quake print all textures used in a map to the console
	//	Con_Printf("%s\n", texname);

		tx->transparent	= false;
		//tx->fullbrights = -1;
		tx->gl_fullbright = -1;

		data = loadimagepixels(tx->name, false);

		if (data)
		{
			tx->width	= mt->width;
			tx->height	= mt->height;

			for (j = 0;j < image_width*image_height;j++)
			{
				if (data[j*4+3] < 255)
				{
					tx->transparent = true;
					break;
				}
			}
			tx->gl_texturenum = GL_LoadTexture (tx->name, image_width, image_height, data, true, tx->transparent, 4);

			free(data);
		}
		else
		{
			if (tx->name[0] == '{')
				tx->transparent = true;

			if (mt->offsets[0])
			{
				data				= W_ConvertWAD3Texture(mt);
				if (data)
				{
					tx->width			= mt->width;
					tx->height			= mt->height;
					tx->gl_texturenum	= GL_LoadTexture (tx->name, mt->width, mt->height, data, true, tx->transparent, 4);
					free(data);
				}
			}
			if (!data)
			{
				data				= W_GetTexture(mt->name);
				if (data)
				{
					tx->width			= image_width;
					tx->height			= image_height;
					tx->gl_texturenum	= GL_LoadTexture (tx->name, image_width, image_height, data, true, tx->transparent, 4);
					free(data);
				}
			}
			if (!data)
			{
				tx->width			= r_notexture_mip->width;
				tx->height			= r_notexture_mip->height;
				tx->gl_texturenum	= GL_LoadTexture ("notexture", tx->width, tx->height, (byte *)((int) r_notexture_mip + r_notexture_mip->offsets[0]), true, false, 1);
			}
		}
	}

//
// sequence the animations
//
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error ("Bad animating texture %s", tx->name);

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture %s", tx->name);
		}
		
#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}*/
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	int i;
	byte *in, *out, *data;
	byte d;
	char litfilename[1024];

	loadmodel->lightdata = NULL;
	// LordHavoc: check for a .lit file
	strcpy(litfilename, loadmodel->name);
	COM_StripExtension(litfilename, litfilename);
	strcat(litfilename, ".lit");

	data = (byte*) COM_LoadHunkFile (litfilename);//, false);
	if (data)
	{
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
		{
			i = LittleLong(((int *)data)[1]);
			if (i == 1)
			{
				Con_DPrintf("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			}
			else
				Con_Printf("Unknown .lit file version (%d)\n", i);
		}
		else
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
	}
	// LordHavoc: no .lit found, expand the white lighting data to color

	//check if there is lump data
	if (!l->filelen)
		return;

	loadmodel->lightdata = Hunk_AllocName ( l->filelen*3, litfilename);
	in = loadmodel->lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0;i < l->filelen;i++)
	{
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
	// LordHavoc: .lit support end
}

/*
==================
Mod_LoadHLLighting
==================
*/
void Mod_LoadHLLighting (lump_t *l)
{
	loadmodel->lightdata = NULL;

	loadmodel->lightdata = Hunk_AllocName ( l->filelen, va("%s lightmaps", loadname));
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( l->filelen, loadname);	
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	byte *data;
	char entfilename[1024];

	loadmodel->entities = NULL;

	strcpy(entfilename, loadmodel->name);
	COM_StripExtension(entfilename, entfilename);
	strcat(entfilename, ".ent");

	data = (byte*) COM_LoadHunkFile (entfilename);//, false);
	if (data)
	{
		loadmodel->entities = data;
		return;
	}
	//no .ent found

	//check if there is lump data
	if (!l->filelen){
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = Hunk_AllocName ( l->filelen, loadname);	
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Sys_Error ("Bad surface extents");
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	extern	void	loadShaderScript(char *filename);
	extern	qboolean checkValue (char *key, char *value);
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->surfNum = surfnum;
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

        // mh - auto water trans part 3 begin 
        // set visframe for water translucency 
        out->visframe = -1; 

        // initialize to no match 
        out->match = NULL; 
        // mh - auto water trans part 3 end 

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
		{
			// original code
			//out->samples = loadmodel->lightdata + i;
			
			if (!hl_map) // Entar : little hack so you can see in HLBSP
				out->samples = loadmodel->lightdata + (i * 3); // LordHavoc
			else
				out->samples = loadmodel->lightdata + i;
		}

     
		
	// set the drawing flags flag
		
		if (!Q_strncmp(out->texinfo->texture->name,"sky",3) || !Q_strncmp(out->texinfo->texture->name,"SKY",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			continue;
		}
		
		if (!Q_strncmp(out->texinfo->texture->name,"*",1) || (hl_map && !Q_strncmp(out->texinfo->texture->name, "!", 1)))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}

		//qmb :reflections
		//JHL:ADD; Make thee shine like a glass!
/*
		if ((!Q_strncmp(out->texinfo->texture->name,"window",6)
			 && Q_strncmp(out->texinfo->texture->name,"window03",8))
			|| !Q_strncmp(out->texinfo->texture->name,"afloor3_1",9)
			|| !Q_strncmp(out->texinfo->texture->name,"wizwin",6) )
			out->flags |= SURF_SHINY_GLASS;
		else if (out->flags & SURF_SHINY_GLASS)
			out->flags = out->flags - SURF_SHINY_GLASS;

		//JHL:ADD; Make thee shine like the mighty steel!
		if ( (!Q_strncmp(out->texinfo->texture->name,"metal", 5)		// iron / steel
			&& Q_strncmp(out->texinfo->texture->name,"metal4", 6)
			&& Q_strncmp(out->texinfo->texture->name,"metal5", 6)
			&& Q_strncmp(out->texinfo->texture->name,"metal1_6", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_1", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_2", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_3", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_4", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_5", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_7", 8)
			&& Q_strncmp(out->texinfo->texture->name,"metal2_8", 8))
		    || (!Q_strncmp(out->texinfo->texture->name,"cop",3)			// copper
			&& Q_strncmp(out->texinfo->texture->name,"cop3_1", 8))
		    || !Q_strncmp(out->texinfo->texture->name,"rune",4)			// runes
		    || !Q_strncmp(out->texinfo->texture->name,"met_",4)
		    || !Q_strncmp(out->texinfo->texture->name,"met5",4))		// misc. metal
			out->flags |= SURF_SHINY_METAL;
		else if (out->flags & SURF_SHINY_METAL)
			out->flags = out->flags - SURF_SHINY_METAL;*/

		if (shaderScript == NULL)
			loadShaderScript(SCRIPTFILENAME);
		if (checkValue(SK_SHINYGLASS, out->texinfo->texture->name))
			out->flags |= SURF_SHINY_GLASS;
		else if (out->flags & SURF_SHINY_GLASS)
			out->flags = out->flags - SURF_SHINY_GLASS;

		if (checkValue(SK_SHINYMETAL, out->texinfo->texture->name))
			out->flags |= SURF_SHINY_METAL;
		else if (out->flags & SURF_SHINY_METAL)
			out->flags = out->flags - SURF_SHINY_METAL;

	}

}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		
		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
//void Mod_LoadLeafs (lump_t *l)
// mh - auto water trans 
void Mod_LoadLeafs (lump_t *l, lump_t *v)
{
	extern	qboolean checkValue (char *key, char *value);
	extern	void   loadShaderScript(char *filename);
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
    out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

   // mh - auto water trans begin 
   // Mod_LoadVisibility stuff 
   // we know how many leafs there are in the map, so we can set up the visdata now. 
	if (v->filelen) 
	{ 
      // add 7 to prevent round down errors with int division by 8. 
		loadmodel->vissize = (loadmodel->numleafs + 7) / 8; 

		loadmodel->visdata = Hunk_AllocName (loadmodel->numleafs * loadmodel->vissize, loadname); 

      // set to all solid initially 
		memset (loadmodel->visdata, 0xff, loadmodel->numleafs * loadmodel->vissize); 
	} 
	else 
	{
		loadmodel->vissize = 0; 
		loadmodel->visdata = NULL; 
	} 
	// mh - auto water trans end 

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		
		out->visframe = -1; // mh - atw #3

/*		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;*/
      // mh - auto water trans begin 
      p = LittleLong (in->visofs); 

      // if the leaf has a valid visoffset and the model has visdata and the 
      // leaf is one of the valid types for having visdata, calculate visdata 
      // this is likely multiple redundant checking... 
      if (p != -1 && loadmodel->visdata && (out->contents == CONTENTS_EMPTY || 
         out->contents == CONTENTS_WATER || out->contents == CONTENTS_LAVA || 
         out->contents == CONTENTS_SLIME || out->contents == CONTENTS_SKY)) 
      { 
         // paranoid about brackets!!! 
         out->decompressed_vis = loadmodel->visdata + (i * loadmodel->vissize); 

         // copy it in 
         memcpy (out->decompressed_vis, 
            Mod_DecompressVis (mod_base + v->fileofs + p, loadmodel), 
            loadmodel->vissize); 
      } 
      else out->decompressed_vis = NULL; 
      // mh - auto water trans end 

		out->efrags = NULL;
		
		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}

		if (shaderScript == NULL)
			loadShaderScript(SCRIPTFILENAME);

		// Entar : marking textures for special caustics
		for (j=0 ; j<out->nummarksurfaces ; j++)
		{
			if (out->firstmarksurface[j]->flags & SURF_UNDERWATER)
			{
				// do nothing
			}
			else if (checkValue(SK_USECAUSTICS, out->firstmarksurface[j]->texinfo->texture->name) && !checkValue(SK_NOCAUSTICS, out->firstmarksurface[j]->texinfo->texture->name))
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}	
}


// mh 17th july 2006 - new version handles loading both at the same time, more robust
void Mod_LoadVisibilityAndLeafs (lump_t *l, lump_t *v)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;
	byte		*vis;
	int			visptr;

	extern	qboolean checkValue (char *key, char *value);
	extern	void   loadShaderScript(char *filename);

	in = (void *) (mod_base + l->fileofs);

	if (l->filelen % sizeof (*in)) Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof (*in);

	// allocate vis first cos leafs and nodes should be in contiguous RAM so that the last leaf + 1 maps to the first node
	// when we do our pointer cast.
	// take 1 extra byte to allow for numbers of leafs that don't divide evenly by 8.
	// we don;t bother checking for un-vised maps.  This will be wasteful of memory in that event, but an un-vised map
	// will always be suboptimal to begin with so it's no big deal.
	loadmodel->visdata = Hunk_AllocName ((count + 1) * (count + 1), loadname);
	visptr = 0;

	// now allocate leafs
	out = Hunk_AllocName (count * sizeof (*out), loadname);	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		
		p = LittleLong (in->visofs);

		if (p == -1)
			out->decompressed_vis = NULL;
		else
		{
			// set up decompressed visibility
			out->decompressed_vis = &loadmodel->visdata[visptr];
			visptr += (count + 1);

			// decompress and copy across.
			// a typical ID1 map will require about 2.5MB for decompressed visdata, compared to < 40K for compressed
			// it's an ouch, but RAM isn't really at a premium these days so it's acceptable enough.
			vis = Mod_DecompressVis (mod_base + v->fileofs + p, loadmodel);
			Q_memcpy (out->decompressed_vis, vis, count + 1);
		}

		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		// changed this to check for liquid content types.
		if (out->contents == CONTENTS_WATER || out->contents == CONTENTS_SLIME || out->contents == CONTENTS_LAVA)
		{
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}

		// entar added stuff follows
		if (shaderScript == NULL)
			loadShaderScript(SCRIPTFILENAME);

		// Entar : marking textures for special caustics
		for (j=0 ; j<out->nummarksurfaces ; j++)
		{
			if (out->firstmarksurface[j]->flags & SURF_UNDERWATER)
			{
				// do nothing
			}
			else if (checkValue(SK_USECAUSTICS, out->firstmarksurface[j]->texinfo->texture->name) && !checkValue(SK_NOCAUSTICS, out->firstmarksurface[j]->texinfo->texture->name))
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}	
}




/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -12;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 16;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
=================
Mod_LoadHLClipnodes
=================
*/
void Mod_LoadHLClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count	= l->filelen / sizeof(*in);
	out		= Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->clipnodes	= out;
	loadmodel->numclipnodes = count;

	hull				= &loadmodel->hulls[1];
	hull->clipnodes		= out;
	hull->firstclipnode = 0;
	hull->lastclipnode	= count-1;
	hull->planes		= loadmodel->planes;
	hull->clip_mins[0]	= -16;
	hull->clip_mins[1]	= -16;
	hull->clip_mins[2]	= -36;
	hull->clip_maxs[0]	= 16;
	hull->clip_maxs[1]	= 16;
	hull->clip_maxs[2]	= 36;

	hull				= &loadmodel->hulls[2];
	hull->clipnodes		= out;
	hull->firstclipnode = 0;
	hull->lastclipnode	= count-1;
	hull->planes		= loadmodel->planes;
	hull->clip_mins[0]	= -32;
	hull->clip_mins[1]	= -32;
	hull->clip_mins[2]	= -32;
	hull->clip_maxs[0]	= 32;
	hull->clip_maxs[1]	= 32;
	hull->clip_maxs[2]	= 32;

	hull				= &loadmodel->hulls[3];
	hull->clipnodes		= out;
	hull->firstclipnode = 0;
	hull->lastclipnode	= count-1;
	hull->planes		= loadmodel->planes;
	hull->clip_mins[0]	= -16;
	hull->clip_mins[1]	= -16;
	hull->clip_mins[2]	= -18;
	hull->clip_maxs[0]	= 16;
	hull->clip_maxs[1]	= 16;
	hull->clip_maxs[2]	= 18;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*2*sizeof(*out), loadname);	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
		}
		if (out->normal[0] < 0)
			bits |= 1;
		if (out->normal[1] < 0)
			bits |= 2;
		if (out->normal[2] < 0)
			bits |= 4;

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

// mh - auto water trans begin 
// detect if a model has been vised for translucent water 
// mh 17th July 2006 - renamed
qboolean Mod_DetectWaterTransBROKEN (model_t *mod) 
{ 
   int i; 

   // no visdata 
   if (!mod->visdata) return true; 

   for (i = 0; i < mod->numleafs; i++) 
   { 
      // leaf 0 is the solid leaf, leafs go to numleafs + 1 
      mleaf_t *leaf = &mod->leafs[i + 1]; 
      byte *vis; 
      msurface_t **surf; 
      int j; 

      // not interested in these leafs 
      if (leaf->contents >= CONTENTS_EMPTY) continue; 
      if (leaf->contents == CONTENTS_SOLID) continue; 
      if (leaf->contents == CONTENTS_SKY) continue; 
      if (leaf->contents == CONTENTS_ORIGIN) continue; 
      if (leaf->contents == CONTENTS_CLIP) continue; 

      // check marksurfaces for a water texture 
      surf = leaf->firstmarksurface; 

      for (j = 0; j < leaf->nummarksurfaces; j++, surf++) 
      { 
         // bad surf/texinfo/texture (some old maps have this from a bad qbsp) 
         if (!surf) continue; 
         if (!(*surf)) continue; 
         if (!(*surf)->texinfo) continue; 
         if (!(*surf)->texinfo->texture) continue; 

         // not interested in teleports 
         if ((*surf)->texinfo->texture->name[0] == '*' && 
            (*surf)->texinfo->texture->name[1] != 't' && 
            (*surf)->texinfo->texture->name[2] != 'e' && 
            (*surf)->texinfo->texture->name[3] != 'l' && 
            (*surf)->texinfo->texture->name[4] != 'e') 
         { 
            goto LeafOK; 
         } 
      } 

      // no water/etc textures here 
      continue; 

LeafOK:; 
      // get the decompressed vis 
      //vis = Mod_DecompressVis (leaf->compressed_vis, mod); 
		vis = leaf->decompressed_vis; 
		if (!vis) continue; 


      // check the other leafs 
      for (j = 0; j < mod->numleafs; j++) 
      { 
         // in the PVS 
         if (vis[j >> 3] & (1 << (j & 7))) 
         { 
            mleaf_t *visleaf = &mod->leafs[j + 1]; 

            // the leaf we hit originally was under water/slime/lava, and a 
            // leaf in it's pvs is above water/slime/lava. 
            if (visleaf->contents == CONTENTS_EMPTY) return true; 
         } 
      } 
   } 

   // found nothing 
   return false; 
} 
// mh - auto water trans end 

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}


// mh 17th July 2006 - new version works properly
qboolean Mod_DetectTransWater666 (model_t *mod)
{
	int i;
	int j;
	int c;
	mleaf_t *leaf;
	mleaf_t *vleaf;
	msurface_t **mark;
	int watersurfleafs = 0;
	int watertransleafs = 0;

	for (i = 0; i < mod->numtextures; i++)
	{
		// not a texture
		if (!mod->textures[i])
			continue;

		// ignore teleports
		if (!strnicmp (mod->textures[i]->name, "*tele", 5))
			continue;

		// water texture
		if (mod->textures[i]->name[0] == '*') break;
	}

	// no water
	if (i == mod->numtextures)
	{
		Con_DPrintf ("Map %s has no water\n", mod->name);
		return false;
	}

	for (i = 0, leaf = mod->leafs; i < mod->numleafs; i++, leaf++)
	{
		// wrong contents
		if (leaf->contents != CONTENTS_EMPTY)
			continue;

		// no vis data
		if (!leaf->decompressed_vis)
			continue;

		// check surfs
		mark = leaf->firstmarksurface;
		c = leaf->nummarksurfaces;

		// look for water
		if (c)
		{
			do
			{
				// break if it's liquid but not teleport
				// (include teleports here...)
				if ((*mark)->texinfo->texture->name[0] == '*') // && !((*mark)->flags & SURF_DRAWTELE))
					break;

				mark++;
			} while (--c);
		}

		// we're only interested in leafs that have liquid surfs
		if (!c) continue;

		// increment number of water surf leafs
		watersurfleafs++;

		// check visibility
		for (j = 0; j < mod->numleafs; j++)
		{
			if (leaf->decompressed_vis[j >> 3] & (1 << (j & 7)))
			{
				vleaf = &mod->leafs[j + 1];

				// is it a water leaf?
				// note - we don't ignore teleports here cos they are valid for a translucent water vis.
				if (vleaf->contents == CONTENTS_WATER || vleaf->contents == CONTENTS_SLIME || vleaf->contents == CONTENTS_LAVA)
				{
					// increment counter and break, cos we need a 1 for 1 match in the counters
					watertransleafs++;
					break;
				}
			}
		}

		// these should match each other 1 for 1 every step of the way
		// we can break out early if they ever don't
		if (watertransleafs != watersurfleafs) break;
	}

	if (watertransleafs == watersurfleafs)
	{
		Con_DPrintf ("Map %s has translucent water visibility!!!\n", mod->name);
		return true;
	}


	// no trans
	Con_DPrintf ("Map %s is not vis'ed for translucent water: transleafs: %i  surfleafs: %i\n", mod->name, watertransleafs, watersurfleafs);
	return false;
}



/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	
	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);

	// Tomaz - Fixing Wrong BSP Version Error Begin
	if ((i != Q1BSP) && (i != HLBSP))	// Tomaz - HL Maps
	{
		Con_Printf("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Q1) or %i (HL))", mod->name, i, Q1BSP, HLBSP);
		mod->numvertexes=-1;	// HACK - incorrect BSP version is no longer fatal
		return;
	}
	// Tomaz - Fixing Wrong BSP Version Error End

	if (i == HLBSP)
		hl_map = true;
	else
		hl_map = false;

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	if (hl_map == true)
		Cvar_SetValue("halflifebsp", 1);
	else
		Cvar_SetValue("halflifebsp", 0);

// load into heap

	Mod_LoadEntities	(&header->lumps[LUMP_ENTITIES]);	
	Mod_LoadVertexes	(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges		(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges	(&header->lumps[LUMP_SURFEDGES]);

//	if (!hl_map)
		Mod_LoadTextures	(&header->lumps[LUMP_TEXTURES]);
//	else
//		Mod_LoadHLTextures	(&header->lumps[LUMP_TEXTURES]);

	if (!hl_map)
		Mod_LoadLighting	(&header->lumps[LUMP_LIGHTING]);
	else
		Mod_LoadHLLighting	(&header->lumps[LUMP_LIGHTING]);

	Mod_LoadPlanes		(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo		(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces		(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);

	if (hl_map) // water vis thing (disabled on Q1 maps for mh stuff)
		Mod_LoadVisibility	(&header->lumps[LUMP_VISIBILITY]);
//	Mod_LoadLeafs		(&header->lumps[LUMP_LEAFS]);
	// mh - auto water trans begin 
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], &header->lumps[LUMP_VISIBILITY]); 

	// mh 17th july 2006 - new loader
//	Mod_LoadVisibilityAndLeafs (&header->lumps[LUMP_LEAFS], &header->lumps[LUMP_VISIBILITY]);

	Con_DPrintf ("Vis Memory Overhead: %i [was %i]\n", 
		mod->numleafs * mod->vissize, header->lumps[LUMP_VISIBILITY].filelen); 
	// mh - auto water trans end 
	
	Mod_LoadNodes		(&header->lumps[LUMP_NODES]);

	if (!hl_map)
		Mod_LoadClipnodes	(&header->lumps[LUMP_CLIPNODES]);
	else
		Mod_LoadHLClipnodes	(&header->lumps[LUMP_CLIPNODES]);

	Mod_LoadSubmodels	(&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();
	
	//mod->watertrans = Mod_DetectWaterTrans (mod); // mh - auto water trans
	// mh 17th July 2006 - new version works properly
	mod->watertrans = Mod_DetectTransWater666 (mod);

	// mh - auto water trans begin 
	// if there's no water in mod, this won't get called 
	// mh 17th July 2006 - this works properly now
	if (!mod->watertrans) 
	{ 
		Mod_BruteForceSurfMatch (mod);
		Mod_SetAWT (mod); // mh - atw #3

		// mh 17th July 2006 - check any left-over leafs
		Mod_FinishAWT (mod);
	} 
	// mh - auto water trans end 

	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		
		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;
int	aliasbboxmins[3], aliasbboxmaxs[3]; //qmb :bounding box fix
    
/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	int				i;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about
		// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]); //qmb :bounding box
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]); //qmb :bounding box
	}
   

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]); //qmb :bounding box
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]); //qmb :bounding box
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================


//=**===================================================**=
//Lord havoc's colour maps for skins
//=========================================================

int GL_SkinSplitShirt(byte *in, int width, int height, int bits, char *name)
{
	byte	data[512*512];
	byte	*out = data;
	int i, pixels, passed, texnum;
	byte pixeltest[16];

	if (width>512 || height>512)
		return 0;

	for (i = 0;i < 16;i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while(pixels--)
	{
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255)
		{
			passed++;
			// turn to white while copying
			if (*in >= 128 && *in < 224) // backwards ranges
				*out = (*in & 15) ^ 15;
			else
				*out = *in & 15;
		}
		else
			*out = 0;
		in++;
		out++;
	}

	if (passed){
		texnum = GL_LoadTexture (name, width, height, out, true, true, 1);
//		free(out);
		return texnum;
	}else{
//		free(out);
		return 0;
	}
}

int GL_SkinSplit(byte *in, int width, int height, int bits, char *name)
{
	byte	data[512*512];
	byte	*out = data;
	int i, pixels, passed, texnum;
	byte pixeltest[16];

	for (i = 0;i < 16;i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while(pixels--)
	{
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255)
		{
			passed++;
			*out = *in;
		}
		else
			*out = 0;
		*in++;
		*out++;
	}

	if (passed){
		texnum = GL_LoadTexture (name, width, height, out, true, true, 1);
		free(out);
		return texnum;
	}else{
		free(out);
		return 0;
	}

}
//end
 
//=**===================================================**=

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int		i, j, k;
	int		s;
	byte	*skin;
	byte	*texels;
	daliasskingroup_t		*pinskingroup;
	int		groupskins;
	daliasskininterval_t	*pinskinintervals;
	
	char	name[64], model[64], model2[64], model3[64];//TGA
	//qmb :model3 is for lordhavoc's replacement skin naming

	
	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	for (i=0 ; i<numskins ; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			//TGA: begin
			sprintf (model3, "%s_%i", loadmodel->name, i); //qmb :loardhavoc's skin naming
			COM_StripExtension(loadmodel->name, model);
			sprintf (model2, "%s_%i", model, i);

			// save 8 bit texels for the player model to remap
			if (!strcmp(loadmodel->name,"progs/player.mdl")) {
				texels = Hunk_AllocName(s, loadname);
				pheader->texels[i] = texels - (byte *)pheader;		
				memcpy (texels, (byte *)(pskintype + 1), s);
				GL_SkinSplitShirt(texels, pheader->skinwidth, pheader->skinheight, 0x0040, model);
			}

			//qmb :tomaz skin naming for blah.mdl skin 0
			//the name is blah_0.tga
			pheader->gl_texturenum[i][0] =
			pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] =
			pheader->gl_texturenum[i][3] =
			GL_LoadTexImage (model2, false, true);
			if (pheader->gl_texturenum[i][0] == 0)// did not find a matching TGA...		
			{
				//qmb :lordhavoc skin naming for blah.mdl skin 0
				//the name is blah.mdl_0.tga
				pheader->gl_texturenum[i][0] =
				pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] =
				pheader->gl_texturenum[i][3] =
				GL_LoadTexImage (model3, false, true);
				
				if (pheader->gl_texturenum[i][0] == 0)// did not find a matching TGA...		
				{
					sprintf (name, "%s_%i", loadmodel->name, i);
					pheader->gl_texturenum[i][0] =
					pheader->gl_texturenum[i][1] =
					pheader->gl_texturenum[i][2] =
					pheader->gl_texturenum[i][3] =
					GL_LoadTexture (name, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), true, false,1);
				}
			}
			//TGA: end

			pskintype = (daliasskintype_t *)((byte *)(pskintype+1) + s);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for (j=0 ; j<groupskins ; j++)
			{
					Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
					if (j == 0) {
						texels = Hunk_AllocName(s, loadname);
						pheader->texels[i] = texels - (byte *)pheader;
						memcpy (texels, (byte *)(pskintype), s);
					}
					sprintf (name, "%s_%i_%i", loadmodel->name, i,j);
					pheader->gl_texturenum[i][j&3] = GL_LoadTexture (name, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), true, false, 1);
					pskintype = (daliasskintype_t *)((byte *)(pskintype) + s);
			}
			k = j;
			for (/* */; j < 4; j++)
				pheader->gl_texturenum[i][j&3] = 
				pheader->gl_texturenum[i][j - k]; 
		}
	}

	return (void *)pskintype;
}

//=========================================================================

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, end, total;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) 
			+ (LittleLong (pinmodel->numframes) - 1) *
			sizeof (pheader->frames[0]);
	pheader = Hunk_AllocName (size, loadname);
	
	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size);
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *)pskintype;

	for (i=0 ; i<pheader->numverts ; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];
	pheader->indecies = (byte *)&pinstverts[pheader->numverts] - (byte *)pheader;

	for (i=0 ; i<pheader->numtris ; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			triangles[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] =  99999;
	aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = -99999;   

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
		}
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

// FIXME: do this right :done right
	//qmb :bounding box
	for (i = 0; i < 3; i++)
	{
		mod->mins[i] = min(-16,aliasbboxmins[i] * pheader->scale[i] + pheader->scale_origin[i]);
		mod->maxs[i] = max(16,aliasbboxmaxs[i] * pheader->scale[i] + pheader->scale_origin[i]);
	}

	//
	// build the draw lists
	//
	GL_MakeAliasModelDisplayLists (mod, pheader);

//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

/*
=================
Mod_LoadQ2AliasModel
=================
*/
void Mod_LoadQ2AliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, size, *pinglcmd, *poutglcmd, start, end, total;
	md2_t				*pinmodel, *pheader;
	md2triangle_t		*pintriangles, *pouttriangles;
	md2frame_t			*pinframe, *poutframe;
	char				*pinskins;

	char	model[64], model2[64], model3[64];//TGA

	start = Hunk_LowMark ();

	pinmodel = (md2_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2ALIAS_VERSION);

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MD2;

// LordHavoc: see pheader ofs adjustment code below for why this is bigger
	size = LittleLong(pinmodel->ofs_end) + sizeof(md2_t);

	if (size <= 0 || size >= MD2MAX_SIZE)
		Sys_Error ("%s is not a valid model", mod->name);
	pheader = Hunk_AllocName (size, loadname);
	
	mod->flags = 0; // there are no MD2 flags

// endian-adjust and copy the data, starting with the alias model header
	for (i = 0;i < 17;i++) // LordHavoc: err... FIXME or something...
		((int*)pheader)[i] = LittleLong(((int *)pinmodel)[i]);
	mod->numframes = numframes = pheader->num_frames;
	mod->synctype = ST_RAND;

	if (pheader->ofs_skins <= 0 || pheader->ofs_skins >= pheader->ofs_end)
		Sys_Error ("%s is not a valid model", mod->name);
	if (pheader->ofs_st <= 0 || pheader->ofs_st >= pheader->ofs_end)
		Sys_Error ("%s is not a valid model", mod->name);
	if (pheader->ofs_tris <= 0 || pheader->ofs_tris >= pheader->ofs_end)
		Sys_Error ("%s is not a valid model", mod->name);
	if (pheader->ofs_frames <= 0 || pheader->ofs_frames >= pheader->ofs_end)
		Sys_Error ("%s is not a valid model", mod->name);
	if (pheader->ofs_glcmds <= 0 || pheader->ofs_glcmds >= pheader->ofs_end)
		Sys_Error ("%s is not a valid model", mod->name);

	if (pheader->num_tris < 1 || pheader->num_tris > MD2MAX_TRIANGLES)
		Sys_Error ("%s has invalid number of triangles: %i", mod->name, pheader->num_tris);
	if (pheader->num_xyz < 1 || pheader->num_xyz > MD2MAX_VERTS)
		Sys_Error ("%s has invalid number of vertices: %i", mod->name, pheader->num_xyz);
	if (pheader->num_frames < 1 || pheader->num_frames > MD2MAX_FRAMES)
		Sys_Error ("%s has invalid number of frames: %i", mod->name, pheader->num_frames);
	if (pheader->num_skins < 0 || pheader->num_skins > MD2MAX_SKINS)
		Sys_Error ("%s has invalid number of skins: %i", mod->name, pheader->num_skins);

// LordHavoc: adjust offsets in new model to give us some room for the bigger header
// cheap offsetting trick, just offset it all by the pheader size...mildly wasteful
for (i = 0;i < 7;i++)
        ((int*)&pheader->ofs_skins)[i] += sizeof(pheader);

	if (pheader->num_skins == 0)
		pheader->num_skins++;
// load the skins
	if (pheader->num_skins)
	{
		pinskins = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_skins));
		for (i = 0;i < pheader->num_skins;i++)
		{

			//TGA: begin
			sprintf (model3, "%s_%i", mod->name, i); //qmb :loardhavoc's skin naming
			COM_StripExtension(mod->name, model);
			sprintf (model2, "%s_%i", model, i);

			//qmb :tomaz skin naming for blah.mdl skin 0
			//the name is blah_0.tga
			pheader->gl_texturenum[i] =	GL_LoadTexImage (model2, false, true);
			if (pheader->gl_texturenum[i] == 0)// did not find a matching TGA...		
			{
				//qmb :lordhavoc skin naming for blah.mdl skin 0
				//the name is blah.mdl_0.tga
				pheader->gl_texturenum[i] = GL_LoadTexImage (model3, false, true);
				
				if (pheader->gl_texturenum[i] == 0)// did not find a matching TGA...		
				{
					pheader->gl_texturenum[i] = GL_LoadTexImage (pinskins, false, true);
				}
			}
			//TGA: end

			pinskins += MD2MAX_SKINNAME;
		}
	}

// load triangles
	pintriangles = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_tris));
	pouttriangles = (void*)((int) pheader + pheader->ofs_tris);
	// swap the triangle list
	for (i=0 ; i < pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			pouttriangles->index_xyz[j] = LittleShort (pintriangles->index_xyz[j]);
			pouttriangles->index_st[j] = LittleShort (pintriangles->index_st[j]);
			if (pouttriangles->index_xyz[j] >= pheader->num_xyz)
				Sys_Error ("%s has invalid vertex indices", mod->name);
			if (pouttriangles->index_st[j] >= pheader->num_st)
				Sys_Error ("%s has invalid vertex indices", mod->name);
		}
		pintriangles++;
		pouttriangles++;
	}

//
// load the frames
//
	pinframe = (void*) ((int) pinmodel + LittleLong(pinmodel->ofs_frames));
	poutframe = (void*) ((int) pheader + pheader->ofs_frames);
	for (i=0 ; i < numframes ; i++)
	{
		for (j = 0;j < 3;j++)
		{
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		for (j = 0;j < 17;j++)
			poutframe->name[j] = pinframe->name[j];

		for (j = 0;j < pheader->num_xyz;j++)
		{
			poutframe->verts[j].v[0] = pinframe->verts[j].v[0];
			poutframe->verts[j].v[1] = pinframe->verts[j].v[1];
			poutframe->verts[j].v[2] = pinframe->verts[j].v[2];
			poutframe->verts[j].lightnormalindex = pinframe->verts[j].lightnormalindex;
		}


		pinframe = (void*) &pinframe->verts[j].v[0];
		poutframe = (void*) &poutframe->verts[j].v[0];
	}

	// LordHavoc: I may fix this at some point
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -64;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 64;

	// load the draw list
	pinglcmd = (void*) ((int) pinmodel + LittleLong(pinmodel->ofs_glcmds));
	poutglcmd = (void*) ((int) pheader + pheader->ofs_glcmds);
	for (i = 0;i < pheader->num_glcmds;i++)
		*poutglcmd++ = LittleLong(*pinglcmd++);

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

//=============================================================================

void Mod_Sprite_StripExtension(char *in, char *out)
{
	char *end;
	end = in + strlen(in);
	if ((end - in) >= 6)
		if (strcmp(end - 6, ".spr32") == 0)
			end -= 6;
	if ((end - in) >= 4)
		if (strcmp(end - 4, ".spr") == 0)
			end -= 4;
	while (in < end)
		*out++ = *in++;
	*out++ = 0;
}

int Mod_Sprite_Bits(char *in)
{
	char *end;
	end = in + strlen(in);
	if ((end - in) >= 6)
		if (strcmp(end - 6, ".spr32") == 0)
			return 4;
	return 1;
}

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum, int bytesperpixel)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					i, width, height, size, origin[2];
	char				name[256], tempname[256];
	byte				*pixbuf, *pixel, *inpixel;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height * bytesperpixel;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t),loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	Mod_Sprite_StripExtension(loadmodel->name, tempname);
	sprintf (name, "%s_%i", tempname, framenum);
//	pspriteframe->gl_texturenum = loadtextureimagewithmask(name, 0, 0, false, true);
	pspriteframe->gl_texturenum = GL_LoadTexImage(name, false, true);
//	pspriteframe->gl_fogtexturenum = image_masktexnum;
	if (pspriteframe->gl_texturenum == 0)
	{
		pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), true, true, bytesperpixel);
		// make fog version (just alpha)
//		pixbuf = pixel = qmalloc(width*height*4);
		pixbuf = pixel = malloc(width*height*4);
		inpixel = (byte *)(pinframe + 1);
		if (bytesperpixel == 1)
		{
			for (i = 0;i < width*height;i++)
			{
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = 255;
				if (*inpixel++ != 255)
					*pixel++ = 255;
				else
					*pixel++ = 0;
			}
		}
		else
		{
			inpixel+=3;
			for (i = 0;i < width*height;i++)
			{
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = *inpixel;
				inpixel+=4;
			}
		}
//		sprintf (name, "%s_%ifog", loadmodel->name, framenum);
//		pspriteframe->gl_fogtexturenum = GL_LoadTexture (name, width, height, pixbuf, true, true, 4);
//		qfree(pixbuf);
		free(pixbuf);
	}

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum, int bytesperpixel)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i, bytesperpixel);

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	// LordHavoc: 32bit textures
	int		bytesperpixel;

	mod->flags = EF_FULLBRIGHT;
	// LordHavoc: hack to allow sprites to be non-fullbright
	for (i = 0;i < MAX_QPATH && mod->name[i];i++)
	{
		if (mod->name[i] == '!')
		{
			mod->flags &= ~EF_FULLBRIGHT;
			break;
		}
	}

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version == 2)
	{
		version = 32;
		Con_Printf("warning: %s is a version 2 sprite (RGBA), supported for now, please hex edit to version 32 incase HalfLife sprites might be supported at some point.\n", mod->name);
	}
	// LordHavoc: 32bit textures
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION)
		Host_Error ("%s has wrong version number "
				 "(%i should be %i or %i)", mod->name, version, SPRITE_VERSION, SPRITE32_VERSION);
	bytesperpixel = 1;
	if (version == SPRITE32_VERSION)
		bytesperpixel = 4;

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;
	
//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i, bytesperpixel);
		else
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i, bytesperpixel);
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}



