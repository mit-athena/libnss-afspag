/*
 * nss_afspag.c
 * nsswitch module to assign group names to AFS PAG groups.
 *
 * Copyright © 2007 Anders Kaseorg <andersk@mit.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <grp.h>
#include <nss.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static enum nss_status
getfakegr_r(gid_t gid, struct group *grp, char *buffer, size_t buflen, int *errnop)
{
    int n;
    
    n = snprintf(buffer, buflen, "afspag-%d", gid);
    if (n < 0 || n >= buflen)
	goto erange;
    grp->gr_name = buffer;
    buffer += n + 1;
    buflen -= n + 1;

    n = snprintf(buffer, buflen, "x");
    if (n < 0 || n >= buflen)
	goto erange;
    grp->gr_passwd = buffer;
    buffer += n + 1;
    buflen -= n + 1;

    grp->gr_gid = gid;

    if (buflen < sizeof((char *)0))
	goto erange;
    grp->gr_mem = (char **)buffer;
    *grp->gr_mem = NULL;
    buffer += sizeof((char *)0);
    buflen -= sizeof((char *)0);

    return NSS_STATUS_SUCCESS;
 erange:
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
}

enum nss_status
_nss_afspag_getgrgid_r(gid_t gid, struct group *grp,
		       char *buffer, size_t buflen, int *errnop)
{
    gid_t *gids = NULL, *pgid;
    int ngids = getgroups(0, gids);
    if (ngids == -1) {
	*errnop = EINVAL;
	return NSS_STATUS_TRYAGAIN;
    }
    gids = malloc(ngids * sizeof(gid_t));
    if (gids == NULL) {
	*errnop = ENOMEM;
	return NSS_STATUS_TRYAGAIN;
    }
    ngids = getgroups(ngids, gids);
    if (ngids == -1) {
	free(gids);
	*errnop = EINVAL;
	return NSS_STATUS_TRYAGAIN;
    }

    /* See openafs/src/afs/afs_osi_pag.c. */
    for (pgid = gids; pgid < gids + ngids; pgid++)
	if (*pgid == gid && ((gid >> 24) & 0xff) == 'A') {
	    free(gids);
	    return getfakegr_r(gid, grp, buffer, buflen, errnop);
	}

    if ((gid == gids[0] || gid == gids[1])) {
	uint32_t g0 = gids[0], g1 = gids[1];
	free(gids);
	g0 -= 0x3f00;
	g1 -= 0x3f00;
	if (g0 < 0xc000 && g1 < 0xc000) {
	    uint32_t l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	    uint32_t h = (g1 >> 14) + 3*(g0 >> 14);
	    uint32_t ret = ((h << 28) | l);
	    if (((ret >> 24) & 0xff) == 'A')
		return getfakegr_r(gid, grp, buffer, buflen, errnop);
	}
    } else {
	free(gids);
    }

    return NSS_STATUS_NOTFOUND;
}

enum nss_status
_nss_afspag_getgrnam_r(const char *name, struct group *grp,
		       char *buffer, size_t buflen, int *errnop)
{
    unsigned int gid;
    int n;
    if (strncmp(name, "afspag-", 7) == 0 &&
	sscanf(name + 7, "%u%n", &gid, &n) >= 1 &&
	n == strlen(name + 7))
	return _nss_afspag_getgrgid_r((gid_t)gid, grp, buffer, buflen, errnop);
    return NSS_STATUS_NOTFOUND;
}
