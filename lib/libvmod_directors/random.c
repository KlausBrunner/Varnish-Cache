/*-
 * Copyright (c) 2013 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>

#include "cache/cache.h"
#include "cache/cache_backend.h"

#include "vrt.h"
#include "vbm.h"

#include "vdir.h"

#include "vcc_if.h"

struct vmod_directors_random {
	unsigned				magic;
#define VMOD_DIRECTORS_RANDOM_MAGIC		0x4732d092
	struct vdir				*vd;
	unsigned				nloops;
	struct vbitmap				*vbm;
};

static unsigned __match_proto__(vdi_healthy)
vmod_rr_healthy(const struct director *dir, const struct req *req)
{
	struct vmod_directors_random *rr;

	CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_DIRECTORS_RANDOM_MAGIC);
	return (vdir_any_healthy(rr->vd, req));
}

static struct vbc * __match_proto__(vdi_getfd_f)
vmod_rr_getfd(const struct director *dir, struct req *req)
{
	struct vmod_directors_random *rr;
	VCL_BACKEND be;
	double r;

	CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_DIRECTORS_RANDOM_MAGIC);
	r = scalbn(random(), -31);
	be = vdir_pick_be(rr->vd, req, r, rr->nloops);
	if (be == NULL)
		return (NULL);
	return (be->getfd(be, req));
}

VCL_VOID __match_proto__()
vmod_random__init(struct req *req, struct vmod_directors_random **rrp,
    const char *vcl_name)
{
	struct vmod_directors_random *rr;

	AZ(req);
	AN(rrp);
	AZ(*rrp);
	ALLOC_OBJ(rr, VMOD_DIRECTORS_RANDOM_MAGIC);
	AN(rr);
	rr->vbm = vbit_init(8);
	AN(rr->vbm);
	rr->nloops = 3; //
	*rrp = rr;
	vdir_new(&rr->vd, vcl_name, vmod_rr_healthy, vmod_rr_getfd, rr);
}

VCL_VOID __match_proto__()
vmod_random__fini(struct req *req, struct vmod_directors_random **rrp)
{
	struct vmod_directors_random *rr;

	AZ(req);
	rr = *rrp;
	*rrp = NULL;
	CHECK_OBJ_NOTNULL(rr, VMOD_DIRECTORS_RANDOM_MAGIC);
	vdir_delete(&rr->vd);
	vbit_destroy(rr->vbm);
	FREE_OBJ(rr);
}

VCL_VOID __match_proto__()
vmod_random_add_backend(struct req *req,
    struct vmod_directors_random *rr, VCL_BACKEND be, double w)
{

	(void)req;
	CHECK_OBJ_NOTNULL(rr, VMOD_DIRECTORS_RANDOM_MAGIC);
	(void)vdir_add_backend(rr->vd, be, w);
}

VCL_BACKEND __match_proto__()
vmod_random_backend(struct req *req, struct vmod_directors_random *rr)
{
	(void)req;
	CHECK_OBJ_NOTNULL(rr, VMOD_DIRECTORS_RANDOM_MAGIC);
	return (rr->vd->dir);
}
