/*
 * CFE boot loader OS Abstraction Layer.
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id$
 */

#ifndef _cfe_osl_h_
#define _cfe_osl_h_

#include <lib_types.h>
#include <lib_string.h>
#include <lib_printf.h>
#include <lib_malloc.h>
#include <cpu_config.h>
#include <cfe_timer.h>
#include <cfe_iocb.h>
#include <cfe_devfuncs.h>
#include <addrspace.h>

#include <typedefs.h>

/* dump string */
extern int (*xprinthook)(const char *str);
#define puts(str) do { if (xprinthook) xprinthook(str); } while (0)

/* assert and panic */
#define	ASSERT(exp)		do {} while (0)

/* PCMCIA attribute space access macros */
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	bzero(buf, size)
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	do {} while (0)

/* PCI configuration space access macros */
#define	OSL_PCI_READ_CONFIG(loc, offset, size) \
	(offset == 8 ? 0 : 0xffffffff)
#define	OSL_PCI_WRITE_CONFIG(loc, offset, size, val) \
	do {} while (0)

/* register access macros */
#define wreg32(r, v)		(*(volatile uint32*)(r) = (uint32)(v))
#define rreg32(r)		(*(volatile uint32*)(r))
#ifdef IL_BIGENDIAN
#define wreg16(r, v)		(*(volatile uint16*)((ulong)(r)^2) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16*)((ulong)(r)^2))
#define wreg8(r, v)		(*(volatile uint8*)((ulong)(r)^3) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8*)((ulong)(r)^3))
#else
#define wreg16(r, v)		(*(volatile uint16*)(r) = (uint16)(v))
#define rreg16(r)		(*(volatile uint16*)(r))
#define wreg8(r, v)		(*(volatile uint8*)(r) = (uint8)(v))
#define rreg8(r)		(*(volatile uint8*)(r))
#endif
#define R_REG(r) ({ \
	__typeof(*(r)) __osl_v; \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	__osl_v = rreg8((r)); break; \
	case sizeof(uint16):	__osl_v = rreg16((r)); break; \
	case sizeof(uint32):	__osl_v = rreg32((r)); break; \
	} \
	__osl_v; \
})
#define W_REG(r, v) do { \
	switch (sizeof(*(r))) { \
	case sizeof(uint8):	wreg8((r), (v)); break; \
	case sizeof(uint16):	wreg16((r), (v)); break; \
	case sizeof(uint32):	wreg32((r), (v)); break; \
	} \
} while (0)
#define	AND_REG(r, v)		W_REG((r), R_REG(r) & (v))
#define	OR_REG(r, v)		W_REG((r), R_REG(r) | (v))

/* bcopy, bcmp, and bzero */
#define bcmp(b1, b2, len)	lib_memcmp((b1), (b2), (len))

#define osl_attach(pdev)	(pdev)
#define osl_detach(osh)		

/* general purpose memory allocation */
#define	MALLOC(osh, size)	KMALLOC((size),0)
#define	MFREE(osh, addr, size)	KFREE((addr))
#define	MALLOCED(osh)		(0)
#define	MALLOC_DUMP(osh, buf, sz)
#define	MALLOC_FAILED(osh)	(0)

/* uncached virtual address */
#define	OSL_UNCACHED(va)	((void*)UNCADDR((ulong)(va)))

/* host/bus architecture-specific address byte swap */
#define BUS_SWAP32(v)		(v)

/* get processor cycle count */
#define OSL_GETCYCLES(x)	((x) = 0)

/* microsecond delay */
#define	OSL_DELAY(usec)		cfe_usleep((cfe_cpu_speed/CPUCFG_CYCLESPERCPUTICK/1000000*(usec)))

/* map/unmap physical to virtual I/O */
#define	REG_MAP(pa, size)	((void*)UNCADDR((ulong)(pa)))
#define	REG_UNMAP(va)		do {} while (0)

/* dereference an address that may cause a bus exception */
#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (uint32)(addr))
extern int osl_busprobe(uint32 *val, uint32 addr);

/* allocate/free shared (dma-able) consistent (uncached) memory */
#define	DMA_CONSISTENT_ALIGN	4096
#define	DMA_ALLOC_CONSISTENT(osh, size, pap) \
	osl_dma_alloc_consistent((size), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa) \
	osl_dma_free_consistent((void*)(va))
extern void *osl_dma_alloc_consistent(uint size, ulong *pap);
extern void osl_dma_free_consistent(void *va);

/* map/unmap direction */
#define	DMA_TX			1
#define	DMA_RX			2

/* map/unmap shared (dma-able) memory */
#define	DMA_MAP(osh, va, size, direction, lb) ({ \
	cfe_flushcache(CFE_CACHE_FLUSH_D); \
	PHYSADDR((ulong)(va)); \
})
#define	DMA_UNMAP(osh, pa, size, direction, p) \
	do {} while (0)

/* shared (dma-able) memory access macros */
#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	lib_memset((r), '\0', (len))

/* generic packet structure */
#define LBUFSZ		4096
#define LBDATASZ	(LBUFSZ - sizeof(struct lbuf))
struct lbuf {	
	struct lbuf	*next;		/* pointer to next lbuf if in a chain */
	struct lbuf	*link;		/* pointer to next lbuf if in a list */
	uchar		*head;		/* start of buffer */
	uchar		*end;		/* end of buffer */
	uchar		*data;		/* start of data */
	uchar		*tail;		/* end of data */
	uint		len;		/* nbytes of data */
	void		*cookie;	/* generic cookie */
};

/* the largest reasonable packet buffer driver uses for ethernet MTU in bytes */
#define	PKTBUFSZ	2048

/* packet primitives */
#define	PKTGET(drv, len, send)		((void*)osl_pktget((len)))
#define	PKTFREE(drv, lb, send)		osl_pktfree((struct lbuf*)(lb))
#define	PKTDATA(drv, lb)		(((struct lbuf*)(lb))->data)
#define	PKTLEN(drv, lb)			(((struct lbuf*)(lb))->len)
#define PKTHEADROOM(drv, lb)		(PKTDATA(drv,lb)-(((struct lbuf*)(lb))->head))
#define PKTTAILROOM(drv, lb)		((((struct lbuf*)(lb))->end)-(((struct lbuf*)(lb))->tail))
#define	PKTNEXT(drv, lb)		(((struct lbuf*)(lb))->next)
#define	PKTSETNEXT(lb, x)		(((struct lbuf*)(lb))->next = (struct lbuf*)(x))
#define	PKTSETLEN(drv, lb, len)		osl_pktsetlen((struct lbuf*)(lb), (len))
#define	PKTPUSH(drv, lb, bytes)		osl_pktpush((struct lbuf*)(lb), (bytes))
#define	PKTPULL(drv, lb, bytes)		osl_pktpull((struct lbuf*)(lb), (bytes))
#define	PKTDUP(drv, lb)			osl_pktdup((struct lbuf*)(lb))
#define	PKTCOOKIE(lb)			(((struct lbuf*)(lb))->cookie)
#define	PKTSETCOOKIE(lb, x)		(((struct lbuf*)(lb))->cookie = (void*)(x))
#define	PKTLINK(lb)			(((struct lbuf*)(lb))->link)
#define	PKTSETLINK(lb, x)		(((struct lbuf*)(lb))->link = (struct lbuf*)(x))
#define	PKTPRIO(lb)			(0)
#define	PKTSETPRIO(lb, x)		do {} while (0)
extern struct lbuf *osl_pktget(uint len);
extern void osl_pktfree(struct lbuf *lb);
extern void osl_pktsetlen(struct lbuf *lb, uint len);
extern uchar *osl_pktpush(struct lbuf *lb, uint bytes);
extern uchar *osl_pktpull(struct lbuf *lb, uint bytes);
extern struct lbuf *osl_pktdup(struct lbuf *lb);

#endif	/* _cfe_osl_h_ */
