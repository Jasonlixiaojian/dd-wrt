#ifndef _ASM_PPC_PARAM_H
#define _ASM_PPC_PARAM_H

#ifndef HZ
#define HZ 100
#ifdef __KERNEL__
#define hz_to_std(a) (a)
#endif
#endif

#define EXEC_PAGESIZE	4096

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
# define CLOCKS_PER_SEC	HZ	/* frequency at which times() counts */
#endif /* __KERNEL__ */

#endif
