
extern qboolean com_file_copyprotected;
extern int com_filesize;
struct cache_user_s;

extern	char	com_gamedir[MAX_OSPATH];
extern char	com_quakedir[MAX_OSPATH];
extern char	com_homedir[MAX_OSPATH];
extern char	com_configdir[MAX_OSPATH];	//dir to put cfg_save configs in
//extern	char	*com_basedir;

void COM_WriteFile (char *filename, void *data, int len);

typedef struct {
	struct searchpath_s	*search;
	int				index;
	char			rawname[MAX_OSPATH];
	int				offset;
	int				len;
} flocation_t;

typedef enum {FSLFRT_IFFOUND, FSLFRT_LENGTH, FSLFRT_DEPTH_OSONLY, FSLFRT_DEPTH_ANYPATH} FSLF_ReturnType_e;
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc);
char *FS_GetPackHashes(char *buffer, int buffersize, qboolean referencedonly);
char *FS_GetPackNames(char *buffer, int buffersize, qboolean referencedonly);

//#ifdef _MSC_VER	//this is enough to annoy me, without conflicting with other (more bizzare) platforms.
//#define fopen dont_use_fopen
//#endif

#define COM_FDepthFile(filename,ignorepacks) FS_FLocateFile(filename,ignorepacks?FSLFRT_DEPTH_OSONLY:FSLFRT_DEPTH_ANYPATH, NULL)
#define COM_FCheckExists(filename) FS_FLocateFile(filename,FSLFRT_IFFOUND, NULL)


typedef struct vfsfile_s {
	int (*ReadBytes) (struct vfsfile_s *file, void *buffer, int bytestoread);
	int (*WriteBytes) (struct vfsfile_s *file, void *buffer, int bytestoread);
	qboolean (*Seek) (struct vfsfile_s *file, unsigned long pos);	//returns false for error
	unsigned long (*Tell) (struct vfsfile_s *file);
	unsigned long (*GetLen) (struct vfsfile_s *file);	//could give some lag
	void (*Close) (struct vfsfile_s *file);
	void (*Flush) (struct vfsfile_s *file);
	qboolean seekingisabadplan;
} vfsfile_t;

#define VFS_CLOSE(vf) (vf->Close(vf))
#define VFS_TELL(vf) (vf->Tell(vf))
#define VFS_GETLEN(vf) (vf->GetLen(vf))
#define VFS_SEEK(vf,pos) (vf->Seek(vf,pos))
#define VFS_READ(vf,buffer,buflen) (vf->ReadBytes(vf,buffer,buflen))
#define VFS_WRITE(vf,buffer,buflen) (vf->WriteBytes(vf,buffer,buflen))
#define VFS_FLUSH(vf) do{if(vf->Flush)vf->Flush(vf);}while(0)
char *VFS_GETS(vfsfile_t *vf, char *buffer, int buflen);
int VFS_GETC(vfsfile_t *vf);

void FS_FlushFSHash(void);
void FS_CreatePath(char *pname, int relativeto);
int FS_Rename(char *oldf, char *newf, int relativeto);	//0 on success, non-0 on error
int FS_Rename2(char *oldf, char *newf, int oldrelativeto, int newrelativeto);
int FS_Remove(char *fname, int relativeto);	//0 on success, non-0 on error
qboolean FS_WriteFile (char *filename, void *data, int len, int relativeto);
vfsfile_t *FS_OpenVFS(char *filename, char *mode, int relativeto);
vfsfile_t *FS_OpenTemp(void);
void FS_UnloadPackFiles(void);
void FS_ReloadPackFiles(void);
char *FS_GenerateClientPacksList(char *buffer, int maxlen, int basechecksum);
enum {
	FS_GAME,
	FS_BASE,
	FS_GAMEONLY,
	FS_CONFIGONLY,
	FS_SKINS
};


qbyte *COM_LoadStackFile (char *path, void *buffer, int bufsize);
qbyte *COM_LoadTempFile (char *path);
qbyte *COM_LoadTempFile2 (char *path);	//allocates a little bit more without freeing old temp
qbyte *COM_LoadHunkFile (char *path);
qbyte *COM_LoadMallocFile (char *path);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu);
void COM_CreatePath (char *path);
qboolean COM_Gamedir (char *dir);
void FS_ForceToPure(char *str, char *crcs, int seed);
char *COM_GetPathInfo (int i, int *crc);
char *COM_NextPath (char *prevpath);
void COM_FlushFSCache(void);	//a file was written using fopen
void COM_RefreshFSCache_f(void);

qboolean COM_LoadMapPackFile(char *name, int offset);
void COM_FlushTempoaryPacks(void);

void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm);

extern	struct cvar_s	registered;

int wildcmp(char *wild, char *string);