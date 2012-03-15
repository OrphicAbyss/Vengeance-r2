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
// mathlib.h
// Tomaz - Whole File Redone

struct	mplane_s;

typedef float vec3_t[3];
typedef float vec4_t[4];

typedef qbyte byte_vec4_t[4];

#ifndef M_PI
#define M_PI		3.141592653589793
#endif

#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)
#define Length(v) (sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]))
#define DEG2RAD(a) (a * degRadScalar)

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide(emins, emaxs, p))

extern	int		nanmask;
extern	float	degRadScalar;
extern	double	angleModScalar1;
extern	double	angleModScalar2;
extern	vec3_t	vec3_origin;

int		BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
void	Math_Init();
void	AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void	VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
void	RotatePointAroundVector( vec3_t dst, vec3_t dir, const vec3_t point, float degrees );
float	VectorNormalize(vec3_t v);
float	anglemod(float a);

extern __inline void VectorInverse(vec3_t vector);
extern __inline void VectorNegate(const vec3_t in, vec3_t out);
extern __inline void VectorClear(vec3_t vector);
extern __inline void VectorSubtract(const vec3_t inA, const vec3_t inB,vec3_t outC);
extern __inline void VectorAdd(const vec3_t inA, const vec3_t inB,vec3_t outC);
extern __inline void VectorCopy(const vec3_t in, vec3_t out);
extern __inline double DotProduct(const vec3_t inA, const vec3_t inB);
extern __inline void CrossProduct(const vec3_t inA, const vec3_t inB, vec3_t outC);
extern __inline double VectorLength(const vec3_t in);
extern __inline void VectorMA(const vec3_t inA, const float scale, const vec3_t inB, vec3_t outC);
extern __inline void VectorScale(const vec3_t in, const float scale, vec3_t out);
extern __inline void VectorNormalizeDouble(vec3_t v);
extern __inline void VectorNormalize2(const vec3_t in, vec3_t out);
extern __inline double VectorDistance(const vec3_t inA, const vec3_t inB);
extern __inline void VectorMultiply(const vec3_t inA, const vec3_t inB, vec3_t outC);

#define VectorSet(a,b,c,d) ((a)[0]=(b),(a)[1]=(c),(a)[2]=(d))

#define VectorCompare(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2]))

#define VectorRandom(v) {do{(v)[0] = lhrandom(-1, 1);(v)[1] = lhrandom(-1, 1);(v)[2] = lhrandom(-1, 1);}while(DotProduct(v, v) > 1);}
#define VectorLerp(v1,lerp,v2,c) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])

#define VectorAverage2(a) (a[0]+a[1]+a[2])

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)