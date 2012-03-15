/*************/
/***Defines***/
/*************/

//default max # of particles at one time
#define MAX_PARTICLES			32768
#define MAX_PARTICLE_TYPES		64
#define MAX_PARTICLE_EMITTER	128
//no fewer than this no matter what's on the command line
#define ABSOLUTE_MIN_PARTICLES	512

// From FTE
#define frandom() (rand()*(1.0f/RAND_MAX))
#define crandom() (rand()*(2.0f/RAND_MAX)-1.0f)
#define hrandom() (rand()*(1.0f/RAND_MAX)-0.5f)

//types of particles
typedef enum {
	p_sparks, p_smoke, p_fire, p_blood, p_chunks, 
	p_lightning, p_bubble, p_trail, p_decal, p_custom,
	p_rain, p_radius, p_fire2, p_none
} part_type_t;

//custom movement effects
typedef enum {
	pm_static, pm_normal, pm_float, pm_bounce, pm_bounce_fast, pm_shrink, pm_die, pm_grow, pm_nophysics, pm_decal, pm_none // pm_none is for scripts
} part_move_t;

//gravity effects
typedef enum {
	pg_none,
	pg_grav_low, pg_grav_belownormal, pg_grav_normal, pg_grav_abovenormal, pg_grav_high, pg_grav_extreme,
	pg_rise_low, pg_rise, pg_rise_high
} part_grav_t;


//particle struct
typedef struct particle_s
{
	int			alive;
	struct particle_tree_s	*particle_type;

	vec3_t		org;				//particles position (start pos for a trail part)
	vec3_t		org2;				//					 (end pos for a trail part)
	vec3_t		colour;				//float color (stops the need for looking up the 8bit to 24bit everytime
	float		rotation;			//current rotation
	float		rotation_speed;		//speed of rotation
	vec3_t		vel;				//particles current velocity
	vec3_t		acc;				//particles acceleration
	float		ramp;				//sort of diffrent purpose
	float		die;				//time that the particle will die
	float		start;				//start time of the particle
	int			hit;				//if the particle has hit the world (used for physics)
	float		size;				//size of the particle
	part_move_t	type;				//gravity for particle (Used for shrink and grow)
									//FIXME: change shrink/grow to a special particle type
	float		dist;				//distance from viewer

	float		updateafter;		//last time lighting was calculated
	vec3_t		lighting;			//lighting values

	int			decal_texture;

	float		verts[12];			//verts calculated for when the particle hits something and need diffrent verts
	float		update_verts;		//if this is set to one the verts array needs to be updated

	// script stuff
	part_type_t	script_trail;			//Entar : particle's trail
	int			script_trailsize;		//Entar : particle's trail's size
	char		script_trailextra[64];	//Entar	: particle's trail's extra effect
	float		script_growrate;		//Entar : particle's rate of growth (or shrinkth!)

	//spawnmodes:
	//box = even spread within the area
	//circle = around edge of a circle
	//ball = filled sphere
	//spiral = spiral trail
	//tracer = tracer trail
	//telebox = q1-style telebox
	//lavasplash = q1-style lavasplash
	//unicircle = uniform circle
	//field = synced field (brightfield, etc)
} particle_t;

//particle type struct
typedef struct particle_tree_s
{
	struct		particle_tree_s *next;	//next particle_type
	int			SrcBlend;				//source blend mode
	int			DstBlend;				//dest blend mode
	part_move_t	move;					//movement effect
	part_grav_t	grav;					//gravity
	part_type_t	id;						//type of particle (builtin or custom qc)
	int			custom_id;				//custom qc particle type id
	int			texture;				//texture particle uses
	float		startalpha;
	vec3_t		offset;
} particle_tree_t;

//FIXME: should be linked to a model????
typedef struct particle_emitter_s
{
	struct		particle_emitter_s	*next;
	vec3_t		org;				//origin
	int			count;				//per second
	int			type;				//type of particles
	int			custom_type;		//custom type id
	float		size;				//size of particles
	float		time;				//time before dieing
	vec3_t		colour;				//colour of the particles
	vec3_t		dir;				//directon of particles spawned (not used yet)
	vec3_t		vel;				//emitter current velocity
	vec3_t		acc;				//emitter acceleration
} particle_emitter_t;

//****New particle creations functions****

//FIXME: these should also accept a qc custom number....
//Add a trail of any particle type
void AddTrail(vec3_t start, vec3_t end, int type, float size, float time, vec3_t dir);
//Add a trail of any particle type and return the particle (for lightning linking up)
particle_t *AddTrailColor(vec3_t start, vec3_t end, int type, float size, float time, vec3_t colour, vec3_t dir);

//Add a particle or two
void AddParticle(vec3_t org, int count, float size, float time, int type, vec3_t dir);
void AddParticleColor(vec3_t org, vec3_t org2, int count, float size, float time, int type, vec3_t colour, vec3_t dir);

void AddParticleCustom(vec3_t org1, int count, float size, float time, int customid, part_type_t type, vec3_t colour, vec3_t dir, int checkwater, float wait, float gravity, part_type_t trail, int trailsize, float trailtime, char *trailextra, int randvelchange, int randorgchange, float growrate, float speed, int rotation, int rotation_speed, char *spawnmode, float areaspread, float areaspreadvert);

//Add a decal :D
void AddDecalColor(vec3_t org, vec3_t normal, float size, float time, int texture, vec3_t colour);
void CL_SpawnDecalParticleForPoint(vec3_t org, float maxdist, float size, int texture, vec3_t color);

//Add a particle type
void AddParticleType(int src, int dst, part_move_t move, part_grav_t grav, part_type_t id, int custom_id, int texture, float startalpha, vec3_t offset);

//FIXME: change to particle emitters
//Add fire particles
void AddFire(vec3_t org, float size);

//FIXME: add particle emitters here
extern int SV_HullPointContents(hull_t *hull, int num, vec3_t p);

//new functions called in cl_tent.c
void R_ParticleWizSpike (vec3_t org);
void R_ParticleGunshot (vec3_t org);
void R_ParticleSpike (vec3_t org);
void R_ParticleSuperSpike (vec3_t org);

void Part_OpenGLSetup(particle_tree_t *pt);
void Part_OpenGLReset(void);

void Part_RenderAndClearArray();
void Part_AddQuadToArray(float *quad, float *texcoord, float *colour);
