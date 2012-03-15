/*******************************************************
	gl_refl.c

	by:
		Matt Ownby	- original code
		dukey		- rewrote most of the above :)
		spike		- helped me with the matrix maths 
		jitspoe		- shaders


	adds reflective water to the Quake2 engine

*******************************************************/


#include "quakedef.h"
#include "gl_refl.h"

//================================global and local variables==============================
unsigned int REFL_TEXW;				// width 
unsigned int REFL_TEXH;				//  and height of the texture we are gonna use to capture our reflection

unsigned int g_reflTexW;			// dynamic size of reflective texture
unsigned int g_reflTexH;

int			g_num_refl		= 0;	// how many reflections we need to generate
int			g_active_refl	= 0;	// which reflection is being rendered at the moment

float		*g_refl_X;
float		*g_refl_Y;
float		*g_refl_Z;				// the Z (vertical) value of each reflection
float		*g_waterDistance;		// the rough distance from player to water vertice .. we want to render the closest water surface.
float		*g_waterDistance2;		// the distance from the player to water plane, different from above (player can be miles away from water but close to water plane)
vec3_t		*waterNormals;			// water normal
int			*g_tex_num;				// corresponding texture numbers for each reflection
int			maxReflections;			// maximum number of reflections

entity_t	*playerEntity = NULL;	// used to create a player reflection (hopefully)

qboolean	g_drawing_refl = false;
qboolean	g_refl_enabled = true;	// whether reflections should be drawn at all
qboolean	brightenTexture= true;	// needed to stop glUpload32 method brightening fragment textures.. screws them up dirty hack tbh

float		g_last_known_fov = 90.0;

unsigned int gWaterProgramId;

//================================global and local variables==============================




/*
================
R_init_refl

sets everything up 
================
*/
void R_init_refl( int maxNoReflections ) {

	//===========================
	int				power;
	int				maxSize;
	int				i;
	unsigned char	*buf = NULL;
	//===========================

	R_setupArrays( maxNoReflections );	// setup number of reflections

	//okay we want to set REFL_TEXH etc to be less than the resolution 
	//otherwise white boarders are left .. we dont want that.
	//if waves function is turned off we can set reflection size to resolution.
	//however if it is turned on in game white marks round the sides will be left again
	//so maybe its best to leave this alone.

	for(power = 2; power < vid.height; power*=2) {
		
		REFL_TEXW = power;	
		REFL_TEXH = power;  
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);		//get max supported texture size

	if(REFL_TEXW > maxSize) {

		for(power = 2; power < maxSize; power*=2) {
		
			REFL_TEXW = power;	
			REFL_TEXH = power;  
		}
	}

	//g_reflTexW = REFL_TEXW;
	//g_reflTexH = REFL_TEXH;

	g_reflTexW = 256; //dunno why it keeps getting reduced to 256 .. entar can fix this ;)
	g_reflTexH = 256;
	REFL_TEXW  = 256;
	REFL_TEXH  = 256;

	for (i = 0; i < maxReflections; i++) {

		buf = (unsigned char *) malloc(REFL_TEXW * REFL_TEXH * 3);	// create empty buffer for texture

		if (buf) {

			memset(buf, 255, (REFL_TEXW * REFL_TEXH * 3));	// fill it with white color so we can easily see where our tex border is
			g_tex_num[i] = txm_genTexObject(buf, REFL_TEXW, REFL_TEXH, GL_RGB,false,true);	// make this texture
			free(buf);	// once we've made texture memory, we don't need the sys ram anymore
		}

		else {
			fprintf(stderr, "Malloc failed?\n");
			exit(1);	// unsafe exit, but we don't ever expect malloc to fail anyway
		}
	}

	// if screen dimensions are smaller than texture size, we have to use screen dimensions instead (doh!)
	g_reflTexW = (vid.width < REFL_TEXW) ? vid.width :  REFL_TEXW;	//keeping these in for now ..
	g_reflTexH = (vid.height< REFL_TEXH) ? vid.height : REFL_TEXH;

	if (extra_info)
		Con_Printf("...reflective texture size set at %d\n",g_reflTexH);

}


/*
================
R_setupArrays

creates the actual arrays
to hold the reflections in
================
*/
void R_setupArrays(int maxNoReflections) {

	//R_shutdown_refl ();

	g_refl_X		= (float *) malloc ( sizeof(float) * maxNoReflections );
	g_refl_Y		= (float *) malloc ( sizeof(float) * maxNoReflections );
	g_refl_Z		= (float *) malloc ( sizeof(float) * maxNoReflections );
	g_waterDistance	= (float *)	malloc ( sizeof(float) * maxNoReflections );
	g_waterDistance2= (float *)	malloc ( sizeof(float) * maxNoReflections );
	g_tex_num		= (int   *) malloc ( sizeof(int)   * maxNoReflections );
	waterNormals	= (vec3_t*) malloc ( sizeof(vec3_t)* maxNoReflections );

	maxReflections	= maxNoReflections;
}

/*
================
R_clear_refl

clears the relfection array
================
*/
void R_clear_refl() {

	g_num_refl = 0;
}

/*
================
calculateDistance

private method to work out distance 
from water to player ..
================
*/
float calculateDistance(float x, float y, float z) {
	
	//====================
	vec3_t	distanceVector;
	vec3_t  v;
	float	distance;
	//====================

	v[0] = x;
	v[1] = y;
	v[2] = z;
		
	VectorSubtract( v, r_refdef.vieworg, distanceVector );
	distance = VectorLength	( distanceVector ) ;

	return distance;
}


/*
================
R_add_refl

creates an array of reflections
================
*/
void R_add_refl(float x, float y, float z, float normalX, float normalY, float normalZ, float distance2) {
	
	//===============
	float	distance;
	int		i;
	//===============

	if(!maxReflections) return;		//safety check.

	if(4 != maxReflections) {
		R_init_refl( 4 );
	}

	//check for duplicates ..
	for (i=0; i < g_num_refl; i++) {

		if( normalX==waterNormals[i][0] &&
			normalY==waterNormals[i][1] &&
			normalZ==waterNormals[i][2]	&&
			distance2==g_waterDistance2[i])
			return;

		//same water normal and same distance from plane
	}

	distance = calculateDistance(x,y,z);	//used to calc closest water surface

	// make sure we have room to add
	if (g_num_refl < maxReflections) {

		g_refl_X[g_num_refl]			= x;		//needed to set pvs
		g_refl_Y[g_num_refl]			= y;		//needed to set pvs
		g_refl_Z[g_num_refl]			= z;		//needed to set pvs
		g_waterDistance[ g_num_refl ]	= distance;	//needed to find closest water surface
		g_waterDistance2[ g_num_refl ]	= distance2;//needed for reflection transform
		waterNormals[g_num_refl][0]		= normalX;
		waterNormals[g_num_refl][1]		= normalY;
		waterNormals[g_num_refl][2]		= normalZ;
		g_num_refl++;
	}

	else {
		// we want to use the closest surface
		// not just any random surface
		// good for when 1 reflection enabled.
		
		for (i=0; i < g_num_refl; i++) {
			
			if ( distance < g_waterDistance[i] ) {
				
				g_refl_X[i]			= x;		//needed to set pvs
				g_refl_Y[i]			= y;		//needed to set pvs
				g_refl_Z[i]			= z;		//needed to set pvs
				g_waterDistance[i]	= distance;	//needed to find closest water surface
				g_waterDistance2[i]	= distance2;//needed for reflection transform
				waterNormals[i][0]	= normalX;
				waterNormals[i][1]	= normalY;
				waterNormals[i][2]	= normalZ;
				
				return;	//lets go
			}
		}

	}// else


}



static int txm_genTexObject(unsigned char *texData, int w, int h,
								int format, qboolean repeat, qboolean mipmap)
{
	unsigned int texNum;

	texNum = texture_extension_number++;

	repeat = false;
	mipmap = false;

	if (texData) {

		glBindTexture(GL_TEXTURE_2D, texNum);
		//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Set the tiling mode */
		if (repeat) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		else {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
		}

		/* Set the filtering */
		if (mipmap) {
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			gluBuild2DMipmaps(GL_TEXTURE_2D, format, w, h, format, 
				GL_UNSIGNED_BYTE, texData);

		}
		else {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, 
				GL_UNSIGNED_BYTE, texData);
		}
	}
	return texNum;
}

void R_RecursiveFindRefl (mnode_t *node, float *modelorg)
{
	extern	void getCentreOfSurf(msurface_t *surf, vec3_t centre);
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

	if (!r_waterrefl.value)
		return;

	//make sure we are still inside the world
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	//is this node visable
	if (node->visframe != r_visframecount)
		return;

	//i think this checks if its on the screen and not behind the viewer
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;
	
// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveFindRefl (node->children[side], modelorg);

// recurse down the back side
	if (r_outline.value)
		R_RecursiveFindRefl (node->children[!side], modelorg);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				if (surf->flags & SURF_DRAWTURB) {
					vec3_t centerPoint;

					getCentreOfSurf(surf, centerPoint);

					R_add_refl(centerPoint[0], centerPoint[1], centerPoint[2],
						surf->plane->normal[0], surf->plane->normal[1], surf->plane->normal[2],
							plane->dist );
 
		

				}//if
			}//for
		}
	}//if

// recurse down the back side
	if (!r_outline.value)
		R_RecursiveFindRefl (node->children[!side], modelorg);
}



/*
================
R_DrawDebugReflTexture

draws debug texture in game
so you can see whats going on
================
*/
void R_DrawDebugReflTexture() {

	glBindTexture(GL_TEXTURE_2D, g_tex_num[0]);	// do the first texture
	glBegin(GL_QUADS);
	glTexCoord2f(1, 1); glVertex3f(0, 0, 0);
	glTexCoord2f(0, 1); glVertex3f(200, 0, 0);
	glTexCoord2f(0, 0); glVertex3f(200, 200, 0);
	glTexCoord2f(1, 0); glVertex3f(0, 200, 0);
	glEnd();
}

/*
================
R_UpdateReflTex

this method renders the reflection
into the right texture (slow)
we have to draw everything a 2nd time
================
*/
void R_UpdateReflTex()
{
	if(!g_num_refl || !r_waterrefl.value)	return;	// nothing to do here

	g_drawing_refl = true;	// begin drawing reflection

	g_last_known_fov = r_refdef.fov_y;;
	
	// go through each reflection and render it
	//for (g_active_refl = 0; g_active_refl < g_num_refl; g_active_refl++) {

	for (g_active_refl = 0; g_active_refl < g_num_refl; g_active_refl++) {

		glClearColor(0, 0, 0, 1);								//clear screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		R_RenderView ();	// draw the scene here!

		glBindTexture(GL_TEXTURE_2D, g_tex_num[g_active_refl]);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0,
			(REFL_TEXW - g_reflTexW) >> 1,
			(REFL_TEXH - g_reflTexH) >> 1,
			0, 0, g_reflTexW, g_reflTexH);		
		
		r_framecount--;	//hack to stop dynamic lighting screwing up

	} //for

	g_drawing_refl = false;	// done drawing refl
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//clear stuff now cause we want to render scene
}

/*
================
mirrorMatrix

creates a mirror matrix from the reflection plane
this matrix is then multiplied by the current
world matrix to invert all the geometry
thanks spike :>
================
*/
void mirrorMatrix(float normalX, float normalY, float normalZ, float distance) {

	//==================
	float mirror[16]; 
	float nx = normalX;
	float ny = normalY;
	float nz = normalZ;
	float k  = distance;
	//==================

	mirror[0] = 1-2*nx*nx; 
	mirror[1] = -2*nx*ny; 
	mirror[2] = -2*nx*nz; 
	mirror[3] = 0; 

	mirror[4] = -2*ny*nx; 
	mirror[5] = 1-2*ny*ny; 
	mirror[6] = -2*ny*nz; 
	mirror[7] = 0; 

	mirror[8] = -2*nz*nx; 
	mirror[9] = -2*nz*ny; 
	mirror[10] = 1-2*nz*nz; 
	mirror[11] = 0; 

	mirror[12] = -2*nx*k; 
	mirror[13] = -2*ny*k; 
	mirror[14] = -2*nz*k; 
	mirror[15] = 1; 

	glMultMatrixf(mirror); 
}														

/*
================
R_LoadReflMatrix()

alters texture matrix to handle our reflection
================
*/
void R_LoadReflMatrix() {
	float aspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	int farclip;
	extern cvar_t gl_farclip;

	glMatrixMode	(GL_TEXTURE);
	glLoadIdentity	( );

	glTranslatef	(0.5, 0.5, 0);				/* Center texture */

	glScalef(
		0.5f * (float)g_reflTexW / REFL_TEXW,
		0.5f * (float)g_reflTexH / REFL_TEXH,
		1.0
	);								/* Scale and bias */
	if (gl_farclip.value) // sanity check
		farclip = max((int) gl_farclip.value, 4096);
	else
		farclip = 4096;
	gluPerspective(g_last_known_fov, aspect, 4, farclip);

	glRotatef (-90, 1, 0, 0);			// put Z going up
	glRotatef ( 90, 0, 0, 1);			// put Z going up

 	R_DoReflTransform	( false );		// do transform
	glTranslatef		( 0, 0, 0 );
	glMatrixMode		( GL_MODELVIEW );
}


/*
================
R_ClearReflMatrix()

Load identity into texture matrix
================
*/
void R_ClearReflMatrix() {

	glMatrixMode	( GL_TEXTURE	);
	glLoadIdentity	(				);
	glMatrixMode	( GL_MODELVIEW	);
}





/*
================
R_DoReflTransform

sets modelview to reflection
instead of normal view
================
*/
void R_DoReflTransform(qboolean update) {

	//====================
	float	a,b,c,d;		//to define a plane ..
	vec3_t	mirrorNormal;
	float	worldMatrix[16];
	//====================

	mirrorNormal[0]	= waterNormals[g_active_refl][0];
	mirrorNormal[1]	= waterNormals[g_active_refl][1];
	mirrorNormal[2]	= waterNormals[g_active_refl][2];

	a	= mirrorNormal[0] * g_refl_X[g_active_refl];
	b	= mirrorNormal[1] * g_refl_Y[g_active_refl];
	c	= mirrorNormal[2] * g_refl_Z[g_active_refl];
	d	= a + b + c;
	d	= d*-1;
	
	glRotatef	(-r_refdef.viewangles[2],  1, 0, 0);	// MPO : this handles rolling (ie when we strafe side to side we roll slightly)
	glRotatef	(-r_refdef.viewangles[0],  0, 1, 0);	// MPO : this handles up/down rotation
	glRotatef	(-r_refdef.viewangles[1],  0, 0, 1);	// MPO : this handles left/right rotation
	glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);
	
	mirrorMatrix(mirrorNormal[0],mirrorNormal[1],mirrorNormal[2],d);

	if(!update)	return;

	glGetFloatv (GL_MODELVIEW_MATRIX, worldMatrix);

	vright[0]	=  worldMatrix[0];		//setup these so setup frustrum works right ..
	vup[0]		=  worldMatrix[1];
	vpn[0]		= -worldMatrix[2];
	 
	vright[1]	=  worldMatrix[4];
	vup[1]		=  worldMatrix[5];
	vpn[1]		= -worldMatrix[6];
	 
	vright[2]	=  worldMatrix[8]; 
	vup[2]		=  worldMatrix[9];
	vpn[2]		= -worldMatrix[10];
}


/*
// - needs to be fixed to work in quake1
   - q1 doesn't have the underwater flag
   - entar can fix this too :)


void setupClippingPlanes() {

	//=============================
	double	clipPlane[] = {0,0,0,0};
	float	normalX;
	float	normalY;
	float	normalZ;
	float	distance;
	//=============================

	if (!g_drawing_refl)	return;
	
	//setup variables

	normalX		= waterNormals[g_active_refl][0];
	normalY		= waterNormals[g_active_refl][1];
	normalZ		= waterNormals[g_active_refl][2];
	distance	= g_waterDistance2[g_active_refl];
	
	if(r_refdef.rdflags & RDF_UNDERWATER) {
			
		clipPlane[0] = -normalX;
		clipPlane[1] = -normalY;
		clipPlane[2] = -normalZ;
		clipPlane[3] = distance;
	}

	else {
		
		clipPlane[0] = normalX;
		clipPlane[1] = normalY;
		clipPlane[2] = normalZ;
		clipPlane[3] = -distance;
	}

	glEnable(GL_CLIP_PLANE0);
	glClipPlane(GL_CLIP_PLANE0, clipPlane);
}

*/


