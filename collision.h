typedef struct
{
	// the hull we're tracing through
	const hull_t *hull;

	// the trace structure to fill in
	trace_t *trace;

	// start, end, and end - start (in model space)
	double start[3];
	double end[3];
	double dist[3];
}
RecursiveHullCheckTraceInfo_t;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

extern hull_t box_hull;

#define SUPERCONTENTS_SOLID			0x00000001
#define SUPERCONTENTS_WATER			0x00000002
#define SUPERCONTENTS_SLIME			0x00000004
#define SUPERCONTENTS_LAVA			0x00000008
#define SUPERCONTENTS_SKY			0x00000010
#define SUPERCONTENTS_BODY			0x00000020
#define SUPERCONTENTS_CORPSE		0x00000040
#define SUPERCONTENTS_NODROP		0x00000080
#define SUPERCONTENTS_LIQUIDSMASK	(SUPERCONTENTS_LAVA | SUPERCONTENTS_SLIME | SUPERCONTENTS_WATER)