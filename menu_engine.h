#define	FONTSIZE		10
#define	MAX_FIELDS		32

#define	LABEL_NORMAL	0
#define	LABEL_LINK		1

#define	LINK_QUIT		1
#define LINK_SP			2
#define LINK_MP			3
#define LINK_OPTIONS	4
#define LINK_VIDEO		5
#define LINK_HELP		6

typedef struct {
	POINT		pos;
	POINT		size;
	char		*name;
} mwin_t;

typedef struct {
	float	r;
	float	g;
	float	b;
	float	a;
} mrgba_t;

typedef struct {
	float		color[3];
	int			flagg;
	float		alpha;
} mlabel_class_t;

typedef struct {
	float		color[3];
	float		alpha;
} mtextbox_class_t;

typedef struct {
	float	fore_color[3];
	mrgba_t	back_color;
	float	alpha;
	int		type;
} mclass_t;

typedef struct {
	POINT			pos;
	char			*value;
	int				target;
	qboolean		enabled;
	mlabel_class_t	clsval;
} mlabel_t;

typedef struct {
	POINT				pos;
	int					size;
	qboolean			enabled;
	char				*name;
	char				*value;
	mtextbox_class_t	clsval;
} mtextbox_t;

typedef struct {
	POINT				pos;
	qboolean			enabled;
	char				*value;
	int					target;
} mbutton_t;

typedef struct {
	POINT		size;
	int			image;
	qboolean	isloaded;
} mpic_t;

typedef struct {
	POINT		pos;
	int			flagg;
	int			target;
	mpic_t		picdata;
	mclass_t	gclass;
	qboolean	enabled;
} mpicture_t;

typedef struct {
	POINT		pos;
	qboolean	value;
	qboolean	enabled;
} mcheckbox_t;

typedef struct {
	POINT		pos;
	POINT		size;
	char		*name;
	mlabel_t	label[MAX_FIELDS];
	mpicture_t	picture[MAX_FIELDS];
	mcheckbox_t	checkbox[MAX_FIELDS];
	mtextbox_t	textbox[MAX_FIELDS];
	mbutton_t	button[MAX_FIELDS];
	mclass_t	gclass;
	qboolean	enabled;
} mform_t;

typedef struct {
	int	val;
	int	id;
} msort_t;

//externals
void PrintWhite (int cx, int cy, char *str);

//internals
void M_Func_PrintColor (int cx, int cy, char *str, float color[3], float alpha);
int make_topmost (int id);
void M_Func_DrawIMG (int formid, mpicture_t pic);
qboolean isinlabel (int formid, int labelid, int x, int y);
qboolean isincheckbox (int formid, int chkid, int x, int y);
qboolean check_windowcollision (int id, int x, int y);
void M_Draw_TMPwindow (int x, int y, int width, int height);
void M_Func_DrawGBG (int formid);
qboolean isingrabbfield (int id, int x, int y);
void M_Func_Draw3D (int x, int y, int width, int height, int style);
int M_Func_GetFreeFormID (void);
mbutton_t M_Func_Button (int x, int y, int target, char *val);
int M_Func_Form (char *name, int x, int y, int w, int h);
mrgba_t M_Func_RGBA (float red, float green, float blue, float alpha);
void M_DrawIMG (int pic, int x, int y, int width, int height);

msort_t form_sort[MAX_FIELDS];
int show_int, show_int2;

char	string_temp[128];

mwin_t	windows[MAX_FIELDS];

mform_t	forms[MAX_FIELDS];

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist, m_vid_options, m_credits, m_serveroptions} m_state;

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

extern POINT current_pos;
extern int omousex, omousey, old_mouse_x, old_mouse_y;
extern int form_focus;

int	m_main_cursor;

int m_gfx_cursor;
int m_gfx_background;
int m_gfx_form;
int m_gfx_aform;
int m_ico[32];
int m_gfx_checkbox[2];

extern int	char_texture;
void Sys_Strtime(char *buf);