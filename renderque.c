//this is to render transparent things in a distance oriented order

#include "quakedef.h"
#include "gl_rpart.h"
extern void DrawParticles_Specific(particle_t *p, particle_tree_t *pt);

#define NUMGRADUATIONS 0x400
static renderque_t *freerque;
static renderque_t *activerque;
static renderque_t *initialque;

static renderque_t *distlastarque[NUMGRADUATIONS];
static renderque_t *distrque[NUMGRADUATIONS];

int rqmaxgrad, rqmingrad;

#define MIN_RQUESIZE 0x1000
int rquesize = 0x2000;

void RQ_AddDistReorder(void (*render) (void *, void *), void *data1, void *data2, float *pos)
{
	int dist;
	vec3_t delta;
	renderque_t *rq;
	//particle_tree_t	*last_pt = NULL;

	if (!freerque)
	{
		//HACK:: particles send the rendering function as NULL, and assume a direct call, slightly faster
		if (render == NULL)
		{
			Part_OpenGLSetup(data2);	//setup opengl state
			DrawParticles_Specific(data1, data2);
			Part_OpenGLReset();	//reset opengl state
		}else {
			render(data1, data2);
		}
		return;
	}

	VectorSubtract(pos, r_refdef.vieworg, delta);
	dist = Length(delta)/4; // what the?!
//	dist = VectorDistance(r_refdef.vieworg, pos);

	if (dist > rqmaxgrad)
	{
		if (dist >= NUMGRADUATIONS)
			dist = NUMGRADUATIONS-1;
		rqmaxgrad = dist;
	}
	if (dist < rqmingrad)
	{
		if (dist < 0)	//hmm... value wrapped? shouldn't happen
			dist = 0;
		rqmingrad = dist;
	}

	rq = freerque;
	freerque = freerque->next;
	rq->next = NULL;
	if (distlastarque[dist])
		distlastarque[dist]->next = rq;
	distlastarque[dist] = rq;

	rq->render = render;
	rq->data1 = data1;
	rq->data2 = data2;

	if (!distrque[dist])
		distrque[dist] = rq;
}

void RQ_RenderDistAndClear(void)
{
	int i;
	
	renderque_t *rq;

	particle_tree_t	*last_pt = NULL;

	for (i = rqmaxgrad; i>=rqmingrad; i--)
//	for (i = rqmingrad; i<=rqmaxgrad; i++)
	{
		for (rq = distrque[i]; rq; rq=rq->next)	
		{
			//HACK:: particles send the rendering function as NULL, and assume a direct call, slightly faster
			if (rq->render == NULL){
				if ((!last_pt) || rq->data2 != last_pt)//((((particle_tree_t *)rq->data2)->id != last_pt->id) || (((particle_tree_t *)rq->data2)->custom_id != last_pt->custom_id)))
				{
					Part_OpenGLSetup(rq->data2);	//setup opengl state
				}
				DrawParticles_Specific(rq->data1, rq->data2);
				last_pt = rq->data2;	//set the last particle tree to be the one just rendered
			}else {
				if (last_pt){			//check if a particle was rendered last and if so reset the states that particles change
					Part_OpenGLReset();	//reset opengl state
					last_pt = NULL;
				}

				rq->render(rq->data1, rq->data2);
			}
		}
		if (distlastarque[i])
		{
			distlastarque[i]->next = freerque;
			freerque = distrque[i];
			distrque[i] = NULL;
			distlastarque[i] = NULL;
		}
	}

	Part_OpenGLReset();			//reset opengl state
	rqmaxgrad=0;
	rqmingrad = NUMGRADUATIONS-1;
}

void RQ_Init(void)
{
	int		i;

//	if (initialque)
//		return;

	i = COM_CheckParm ("-rquesize");
	if (i)
	{
		rquesize = (int)(Q_atoi(com_argv[i+1]));
		if (rquesize < MIN_RQUESIZE)
			rquesize = MIN_RQUESIZE;	// can't have less than set min
	}
	
	initialque = (renderque_t *) Hunk_AllocName (rquesize * sizeof(renderque_t), "renderque");
			
	
	freerque = &initialque[0];
	activerque = NULL;

	for (i=0 ;i<rquesize-1 ; i++)
		initialque[i].next = &initialque[i+1];
	initialque[rquesize-1].next = NULL;
}
