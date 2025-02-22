/*
 * utils for pmlogrewrite
 *
 * Copyright (c) 2017 Red Hat.
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <assert.h>
#include "pmapi.h"
#include "libpcp.h"

/*
 * raw read of next log record - largely stolen from __pmLogRead in libpcp
 */
int
_pmLogGet(__pmArchCtl *acp, int vol, __int32_t **pb)
{
    __pmLogCtl	*lcp = acp->ac_log;
    int		head;
    int		tail;
    int		sts;
    long	offset;
    char	*p;
    __int32_t	*lpb;
    __pmFILE	*f;

    if (vol == PM_LOG_VOL_META)
	f = lcp->mdfp;
    else
	f = acp->ac_mfp;

    offset = __pmFtell(f);
    assert(offset >= 0);
    if (pmDebugOptions.log) {
	fprintf(stderr, "_pmLogGet: fd=%d vol=%d posn=%ld ",
	    __pmFileno(f), vol, offset);
    }

again:
    sts = (int)__pmFread(&head, 1, sizeof(head), f);
    if (sts != sizeof(head)) {
	if (sts == 0) {
	    if (pmDebugOptions.log)
		fprintf(stderr, "AFTER end\n");
	    __pmFseek(f, offset, SEEK_SET);
	    if (vol != PM_LOG_VOL_META) {
		if (acp->ac_curvol < lcp->maxvol) {
		    if (__pmLogChangeVol(acp, acp->ac_curvol+1) == 0) {
			f = acp->ac_mfp;
			goto again;
		    }
		}
	    }
	    return PM_ERR_EOL;
	}
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: hdr fread=%d %s\n", sts, osstrerror());
	if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -oserror();
    }

    if ((lpb = (__int32_t *)malloc(ntohl(head))) == NULL) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: _pmLogGet:(%d) %s\n",
		(int)ntohl(head), osstrerror());
	__pmFseek(f, offset, SEEK_SET);
	return -oserror();
    }

    lpb[0] = head;
    if ((sts = (int)__pmFread(&lpb[1], 1, ntohl(head) - sizeof(head), f)) != ntohl(head) - sizeof(head)) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: data fread=%d %s\n", sts, osstrerror());
	free(lpb);
	if (sts == 0) {
	    __pmFseek(f, offset, SEEK_SET);
	    return PM_ERR_EOL;
	}
	else if (sts > 0)
	    return PM_ERR_LOGREC;
	else
	    return -oserror();
    }


    p = (char *)lpb;
    memcpy(&tail, &p[ntohl(head) - sizeof(head)], sizeof(head));
    if (head != tail) {
	if (pmDebugOptions.log)
	    fprintf(stderr, "Error: head-tail mismatch (%d-%d)\n",
		(int)ntohl(head), (int)ntohl(tail));
	free(lpb);
	return PM_ERR_LOGREC;
    }

    if (pmDebugOptions.log) {
	if (vol != PM_LOG_VOL_META ||
	    ntohl(lpb[1]) == TYPE_INDOM || ntohl(lpb[1]) == TYPE_INDOM_V2) {
	    fprintf(stderr, "@");
	    if (sts >= 0) {
		__pmTimestamp	stamp;
		if (vol != PM_LOG_VOL_META)
		    __pmLoadTimeval(&lpb[2], &stamp);
		else if (ntohl(lpb[1]) == TYPE_INDOM)
		    __pmLoadTimestamp(&lpb[1], &stamp);
		else if (ntohl(lpb[1]) == TYPE_INDOM_V2)
		    __pmLoadTimeval(&lpb[1], &stamp);
		__pmPrintTimestamp(stderr, &stamp);
	    }
	    else
		fprintf(stderr, "unknown time");
	}
	fprintf(stderr, " len=%d (incl head+tail)\n", (int)ntohl(head));
    }

    if (pmDebugOptions.pdu) {
	int		i, j;
	__pmTimestamp	stamp;
	fprintf(stderr, "_pmLogGet");
	if (vol != PM_LOG_VOL_META ||
	    ntohl(lpb[1]) == TYPE_INDOM || ntohl(lpb[1]) == TYPE_INDOM_V2) {
	    if (vol != PM_LOG_VOL_META)
		__pmLoadTimeval(&lpb[2], &stamp);
	    else if (ntohl(lpb[1]) == TYPE_INDOM)
		__pmLoadTimestamp(&lpb[1], &stamp);
	    else if (ntohl(lpb[1]) == TYPE_INDOM_V2)
		__pmLoadTimeval(&lpb[1], &stamp);
	    fprintf(stderr, " timestamp=");
	    __pmPrintTimestamp(stderr, &stamp);
	}
	fprintf(stderr, " " PRINTF_P_PFX "%p ... " PRINTF_P_PFX "%p", lpb, &lpb[ntohl(head)/sizeof(__int32_t) - 1]);
	fputc('\n', stderr);
	fprintf(stderr, "%03d: ", 0);
	for (j = 0, i = 0; j < ntohl(head)/sizeof(__int32_t); j++) {
	    if (i == 8) {
		fprintf(stderr, "\n%03d: ", j);
		i = 0;
	    }
	    fprintf(stderr, "0x%x ", lpb[j]);
	    i++;
	}
	fputc('\n', stderr);
    }

    *pb = lpb;
    return 0;
}

pmUnits
ntoh_pmUnits(pmUnits units)
{
    unsigned int x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;
    return units;
}
