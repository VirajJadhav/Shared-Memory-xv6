#define	SHM_RDONLY	010000	/* read-only access */
#define	SHM_RND		020000	/* round attach address to SHMLBA boundary */
#define	SHM_REMAP	040000	/* take-over region on attach */
#define	SHM_EXEC	0100000	/* execution access */

#define	SHMLBA	(1 * PGSIZE)

struct shmid_ds {
  uint shm_segsz; // size of segment in bytes
  int shm_nattch; // current attaches
};