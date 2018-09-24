// SPDX-License-Identifier: Beerware
/*
 * 2018 by Marek Behun <marek.behun@nic.cz>
 */

#include <stdlib.h>
#include <string.h>
#include <openssl/ecdsa.h>
#include <endian.h>
#include "tim.h"
#include "utils.h"
#include "wtptp.h"
#include "key.h"
#include "bn.h"
#include "images.h"

static reshdr_t *reserved_area(timhdr_t *timhdr)
{
	return ((void *) timhdr) + sizeof(timhdr_t) +
		le32toh(timhdr->numimages) * sizeof(imginfo_t) +
		le32toh(timhdr->numkeys) * sizeof(keyinfo_t);
}

static respkg_t *nextpkg(timhdr_t *timhdr, respkg_t *pkg)
{
	reshdr_t *reshdr;
	respkg_t *next, *end;

	reshdr = reserved_area(timhdr);

	next = ((void *) pkg) + le32toh(pkg->size);
	end = ((void *) reshdr) + le32toh(timhdr->sizeofreserved);

	if (next > end)
		die("Reserved area broken");
	else if (next == end)
		return NULL;
	else
		return next;
}

static respkg_t *firstpkg(timhdr_t *timhdr)
{
	reshdr_t *reshdr;
	respkg_t *first, *pkg;
	u32 i, id, pkgs, size;

	size = le32toh(timhdr->sizeofreserved);
	if (!size)
		return NULL;

	if (size < sizeof(reshdr_t))
		die("Size of reserved area (%u bytes) too small", size);

	reshdr = reserved_area(timhdr);

	id = le32toh(reshdr->id);
	pkgs = le32toh(reshdr->pkgs);

	if (id != RES_ID)
		die("Incorrect reserved area ID %s", id2name(id));

	if (size < sizeof(reshdr_t) + pkgs * sizeof(respkg_t))
		die("Size of reserved area (%u bytes) too small for "
		    "%u packages", size, pkgs);

	first = (respkg_t *) (reshdr + 1);

	i = 0;
	for (pkg = first; pkg; pkg = nextpkg(timhdr, pkg))
		++i;

	if (i != pkgs)
		die("Reserved area broken (expected %u packages, found %u)",
		    pkgs, i);

	return first;
}

static u32 getsizetohash(timhdr_t *timhdr)
{
	void *res;

	res = (void *) timhdr + sizeof(timhdr_t) +
	      le32toh(timhdr->numimages) * sizeof(imginfo_t) +
	      le32toh(timhdr->sizeofreserved) +
	      le32toh(timhdr->numkeys) * sizeof(keyinfo_t);

	if (timhdr->trusted) {
		platds_t *platds = res;

		res += (void *) &platds->ECDSA.sig - res;
	}

	return res - (void *) timhdr;
}

static imginfo_t *tim_find_image(image_t *tim, u32 id)
{
	timhdr_t *timhdr;
	imginfo_t *img;
	int i;

	timhdr = (void *) tim->data;

	for (i = 0; i < tim_nimages(timhdr); ++i) {
		img = tim_image(timhdr, i);

		if (le32toh(img->id) == id)
			return img;
	}

	return NULL;
}

void tim_remove_image(image_t *tim, u32 id)
{
	timhdr_t *timhdr;
	imginfo_t *img;
	void *imgend;
	int i;

	timhdr = (void *) tim->data;

	img = tim_find_image(tim, id);
	if (!img)
		return;

	if (i > 0)
		(img - 1)->nextid = img->nextid;

	imgend = img + 1;
	memmove(img, imgend, (void *) tim->data + tim->size - imgend);

	timhdr->numimages = htole32(tim_nimages(timhdr) - 1);
	tim->size -= sizeof(imginfo_t);

	img = tim_find_image(tim, tim->id);
	if (img) {
		img->size = htole32(tim->size);
		img->sizetohash = htole32(tim->size);
	}
}

static void tim_remove_pkg(image_t *tim, u32 id)
{
	timhdr_t *timhdr;
	reshdr_t *reshdr;
	respkg_t *pkg;
	u32 pkgsize;
	void *pkgend;

	timhdr = (void *) tim->data;
	reshdr = reserved_area(timhdr);

	for (pkg = firstpkg(timhdr); pkg; pkg = nextpkg(timhdr, pkg))
		if (le32toh(pkg->id) == id)
			break;

	if (!pkg)
		return;

	pkgsize = le32toh(pkg->size);
	pkgend = (void *) pkg + pkgsize;
	memmove(pkg, pkgend, (void *) tim->data + tim->size - pkgend);

	reshdr = reserved_area(timhdr);
	reshdr->pkgs = htole32(le32toh(reshdr->pkgs) - 1);
	timhdr->sizeofreserved = htole32(le32toh(timhdr->sizeofreserved) - pkgsize);
	tim->size -= pkgsize;
}

char minimal_secure_tim[] =
	"\x00\x06\x03\x00\x48\x4d\x49\x54\x00\x00\x00\x00\x18\x20\x09\x24"
	"\x43\x4e\x5a\x43\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\x0a\x49\x50\x53\x01\x00\x00\x00"
	"\x00\x00\x00\x00\x30\x00\x00\x00\x48\x4d\x49\x54\xff\xff\xff\xff"
	"\x00\x00\x00\x00\x00\x60\x00\x20\x00\x04\x00\x00\x88\x01\x00\x00"
	"\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x48\x54\x50\x4f\x02\x00\x00\x00\x50\x41\x4d\x49"
	"\x20\x00\x00\x00\x01\x00\x00\x00\x54\x4b\x53\x43\x00\x00\x00\x00"
	"\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x6d\x72\x65\x54"
	"\x08\x00\x00\x00"
	;

const size_t minimal_secure_tim_size = 212;

char minimal_secure_timn[] =
	"\x00\x06\x03\x00\x4e\x4d\x49\x54\x00\x00\x00\x00\x18\x20\x09\x24"
	"\x43\x4e\x5a\x43\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\x0a\x49\x50\x53\x01\x00\x00\x00"
	"\x00\x00\x00\x00\xc0\x08\x00\x00\x4e\x4d\x49\x54\xff\xff\xff\xff"
	"\x00\x10\x00\x00\x00\x30\x00\x20\x64\x09\x00\x00\x18\x0a\x00\x00"
	"\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x48\x54\x50\x4f\x06\x00\x00\x00\x32\x56\x52\x43"
	"\x14\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x00\x00\xff\x1f"
	"\x50\x44\x49\x43\x20\x00\x00\x00\x01\x00\x00\x00\x49\x52\x42\x54"
	"\x03\x00\x00\x00\x31\x50\x50\x47\x32\x50\x50\x47\x33\x52\x44\x44"
	"\x31\x50\x50\x47\xa0\x00\x00\x00\x01\x00\x00\x00\x0c\x00\x00\x00"
	"\x06\x00\x00\x00\x00\x00\x00\x00\x19\x00\x00\x00\x0c\xd0\x00\xc0"
	"\x00\x00\x00\x80\x00\x00\x00\x00\x01\x00\x00\x00\x42\x50\x53\x31"
	"\x06\x00\x00\x00\x0c\xd0\x00\xc0\xff\xff\xff\x7f\x01\x00\x00\x00"
	"\x40\x38\x01\xc0\x1e\x1d\x00\x00\x18\x00\x00\x00\x42\x50\x53\x30"
	"\x00\x00\x00\x00\x1b\x00\x00\x00\x42\x50\x53\x30\x18\x00\x00\x00"
	"\x42\x50\x53\x31\x06\x00\x00\x00\x00\x56\x01\xc0\xff\xfe\xff\xff"
	"\x07\x00\x00\x00\x00\x56\x01\xc0\x00\x06\x00\x00\x07\x00\x00\x00"
	"\x00\xa4\x01\xc0\x40\x00\x00\x00\x07\x00\x00\x00\x50\x34\x00\x40"
	"\x00\x00\x00\xc0\x07\x00\x00\x00\x54\x34\x00\x40\xbf\x01\x00\x00"
	"\x32\x50\x50\x47\xc8\x01\x00\x00\x01\x00\x00\x00\x22\x00\x00\x00"
	"\x06\x00\x00\x00\x00\x00\x00\x00\x19\x00\x00\x00\x30\x83\x00\xc0"
	"\x02\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x43\x4e\x54\x33"
	"\x06\x00\x00\x00\x30\x83\x00\xc0\xfe\xff\xff\xff\x07\x00\x00\x00"
	"\x30\x83\x00\xc0\x01\x00\x00\x00\x18\x00\x00\x00\x43\x4e\x54\x33"
	"\x19\x00\x00\x00\x4c\x40\x01\xc0\x01\x00\x00\x00\x01\x00\x00\x00"
	"\x01\x00\x00\x00\x4d\x42\x31\x00\x01\x00\x00\x00\x00\x04\x00\x64"
	"\x00\x00\x00\x00\x18\x00\x00\x00\x4d\x42\x31\x00\x19\x00\x00\x00"
	"\x08\x02\x00\xc0\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00"
	"\x43\x53\x31\x00\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xc0"
	"\x0d\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x1b\x00\x00\x00"
	"\x43\x4f\x4e\x46\x18\x00\x00\x00\x43\x53\x31\x00\x0c\x00\x00\x00"
	"\x00\x00\x00\x00\x08\x02\x00\xc0\x0f\x00\x00\x00\x01\x00\x00\x00"
	"\x00\x00\x00\x00\x12\x00\x00\x00\x01\x00\x00\x00\x00\x00\x80\xff"
	"\x10\x00\x00\x00\x01\x00\x00\x00\x10\x00\x00\x00\x18\x00\x00\x00"
	"\x43\x4f\x4e\x46\x10\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00"
	"\x12\x00\x00\x00\x00\x00\x00\x00\x1f\x00\x00\x00\x0d\x00\x00\x00"
	"\x02\x00\x00\x00\x01\x00\x00\x00\x18\x00\x00\x00\x43\x4f\x4e\x00"
	"\x1a\x00\x00\x00\x00\x00\x00\x00\x1f\x00\x00\x00\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x4f\x55\x54\x00\x11\x00\x00\x00\x02\x00\x00\x00"
	"\x01\x00\x00\x00\x1f\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x1b\x00\x00\x00\x43\x4f\x4e\x00\x18\x00\x00\x00\x4f\x55\x54\x00"
	"\x1f\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x1e\x00\x00\x00"
	"\x02\x00\x00\x00\x01\x00\x00\x00\x1a\x00\x00\x00\x02\x00\x00\x00"
	"\x00\x80\xff\x00\x00\x00\x00\x00\x01\x00\x00\x00\x44\x52\x41\x4d"
	"\x0d\x00\x00\x00\x02\x00\x00\x00\xff\x7f\x00\x00\x18\x00\x00\x00"
	"\x44\x52\x41\x4d\x06\x00\x00\x00\x00\xcf\x00\xc0\xfe\xff\xff\xff"
	"\x0e\x00\x00\x00\x02\x00\x00\x00\x04\xcf\x00\xc0\x07\x00\x00\x00"
	"\x00\xcf\x00\xc0\x01\x00\x00\x00\x33\x52\x44\x44\x14\x06\x00\x00"
	"\x01\x00\x00\x00\x80\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00"
	"\x01\x00\x00\x00\x08\x40\x01\xc0\x00\x45\x40\x00\x01\x00\x00\x00"
	"\x00\x20\x00\xc0\x00\x00\x01\x00\x01\x00\x00\x00\x04\x20\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x40\x03\x00\xc0\xef\x0f\x0f\x0f"
	"\x01\x00\x00\x00\x44\x03\x00\xc0\xaa\x00\x00\x10\x01\x00\x00\x00"
	"\x10\x03\x00\xc0\x00\x00\x20\x00\x01\x00\x00\x00\x04\x03\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x08\x03\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x00\x02\x00\xc0\x01\x00\x0e\x00\x01\x00\x00\x00"
	"\x04\x02\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\x20\x02\x00\xc0"
	"\x32\x06\x02\x13\x01\x00\x00\x00\x44\x00\x00\xc0\x00\x02\x03\x00"
	"\x01\x00\x00\x00\xc0\x02\x00\xc0\x00\x60\x00\x00\x01\x00\x00\x00"
	"\xc4\x02\x00\xc0\x20\x00\x10\x00\x01\x00\x00\x00\x58\x00\x00\xc0"
	"\x3f\x14\x00\x00\x01\x00\x00\x00\x48\x00\x00\xc0\x01\x00\x00\x00"
	"\x01\x00\x00\x00\x80\x01\x00\xc0\x00\x02\x01\x00\x01\x00\x00\x00"
	"\x50\x00\x00\xc0\xff\x01\x00\x00\x01\x00\x00\x00\x4c\x00\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x54\x00\x00\xc0\x80\x04\x00\x00"
	"\x01\x00\x00\x00\x00\x03\x00\xc0\x0b\x08\x00\x00\x01\x00\x00\x00"
	"\x80\x03\x00\xc0\x20\xa1\x07\x00\x01\x00\x00\x00\x84\x03\x00\xc0"
	"\x40\x0d\x03\x00\x01\x00\x00\x00\x88\x03\x00\xc0\x6b\x00\x60\x09"
	"\x01\x00\x00\x00\x8c\x03\x00\xc0\x00\x02\x00\x00\x01\x00\x00\x00"
	"\x90\x03\x00\xc0\x00\x01\x40\x00\x01\x00\x00\x00\x94\x03\x00\xc0"
	"\xcf\x03\xf0\x00\x01\x00\x00\x00\x98\x03\x00\xc0\x00\x02\xf8\x00"
	"\x01\x00\x00\x00\x9c\x03\x00\xc0\x08\x08\x00\x00\x01\x00\x00\x00"
	"\xa0\x03\x00\xc0\x14\x06\x04\x00\x01\x00\x00\x00\xa4\x03\x00\xc0"
	"\x01\x00\x00\x00\x01\x00\x00\x00\xa8\x03\x00\xc0\x04\x0c\x00\x00"
	"\x01\x00\x00\x00\xac\x03\x00\xc0\x1f\x0c\x2a\x20\x01\x00\x00\x00"
	"\xb0\x03\x00\xc0\x0c\x06\x0c\x0c\x01\x00\x00\x00\xb4\x03\x00\xc0"
	"\x00\x06\x00\x04\x01\x00\x00\x00\xb8\x03\x00\xc0\x00\x08\x00\x00"
	"\x01\x00\x00\x00\xbc\x03\x00\xc0\x04\x04\x02\x02\x01\x00\x00\x00"
	"\xc0\x03\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\xc4\x03\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\xdc\x03\x00\xc0\x39\x12\x08\x00"
	"\x01\x00\x00\x00\xc8\x02\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x64\x00\x00\xc0\x06\x00\x00\x00\x01\x00\x00\x00\x44\x00\x00\xc0"
	"\x00\x02\x03\x00\x01\x00\x00\x00\x04\x10\x00\xc0\x89\x78\x47\xc4"
	"\x01\x00\x00\x00\x08\x10\x00\xc0\xfa\x0f\x50\x05\x01\x00\x00\x00"
	"\x0c\x10\x00\xc0\x77\xdf\x21\x05\x01\x00\x00\x00\x10\x10\x00\xc0"
	"\x08\x01\x30\x00\x01\x00\x00\x00\x28\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x30\x10\x00\xc0\x00\x00\x80\x03\x01\x00\x00\x00"
	"\x34\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\x40\x10\x00\xc0"
	"\x00\x04\x00\x00\x01\x00\x00\x00\xc0\x10\x00\xc0\x01\x00\x00\x80"
	"\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\xe0\x10\x00\xc0\xf0\x1f\x01\x00\x01\x00\x00\x00\x90\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x94\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x98\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x9c\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\xa0\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\xa4\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\xa8\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\xac\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\xb0\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x10\x00\xc0\x41\x40\x04\x00"
	"\x01\x00\x00\x00\x14\x10\x00\xc0\x00\x02\x08\x00\x01\x00\x00\x00"
	"\x38\x10\x00\xc0\x02\x00\x00\x00\x01\x00\x00\x00\x3c\x10\x00\xc0"
	"\x10\x00\x00\x00\x01\x00\x00\x00\x80\x11\x00\xc0\x0a\x02\x00\x00"
	"\x01\x00\x00\x00\x84\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00"
	"\x88\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00\x8c\x11\x00\xc0"
	"\x0a\x02\x00\x00\x01\x00\x00\x00\x90\x11\x00\xc0\x0a\x02\x00\x00"
	"\x01\x00\x00\x00\x94\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00"
	"\x98\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00\x9c\x11\x00\xc0"
	"\x0a\x02\x00\x00\x01\x00\x00\x00\xa0\x11\x00\xc0\x0a\x02\x00\x00"
	"\x01\x00\x00\x00\x50\x10\x00\xc0\x00\x00\x13\x1f\x01\x00\x00\x00"
	"\x54\x10\x00\xc0\x00\x00\x19\x20\x01\x00\x00\x00\x74\x10\x00\xc0"
	"\x00\x00\x20\x20\x01\x00\x00\x00\x58\x10\x00\xc0\x00\x00\x08\x08"
	"\x01\x00\x00\x00\x5c\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00"
	"\x60\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00\x64\x10\x00\xc0"
	"\x00\x00\x08\x08\x01\x00\x00\x00\x68\x10\x00\xc0\x00\x00\x08\x08"
	"\x01\x00\x00\x00\x6c\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00"
	"\x70\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00\x00\x10\x00\xc0"
	"\x41\x40\x04\x00\x01\x00\x00\x00\xbc\x03\x00\xc0\x04\x04\x02\x02"
	"\x01\x00\x00\x00\x14\x10\x00\xc0\x00\x02\x08\x00\x01\x00\x00\x00"
	"\x38\x10\x00\xc0\x03\x00\x00\x00\x01\x00\x00\x00\x3c\x10\x00\xc0"
	"\x20\x00\x00\x00\x01\x00\x00\x00\x20\x10\x00\xc0\x00\x00\x00\x80"
	"\x01\x00\x00\x00\x20\x10\x00\xc0\x00\x00\x00\x20\x01\x00\x00\x00"
	"\x20\x10\x00\xc0\x00\x00\x00\x40\x03\x00\x00\x00\x0a\x00\x00\x00"
	"\x01\x00\x00\x00\x20\x10\x00\xc0\x00\x00\x00\x80\x03\x00\x00\x00"
	"\x0a\x00\x00\x00\x07\x00\x00\x00\x30\x10\x00\xc0\x00\x00\x80\x03"
	"\x07\x00\x00\x00\x34\x10\x00\xc0\x00\x00\x00\x00\x07\x00\x00\x00"
	"\x90\x10\x00\xc0\x00\x00\x00\x00\x07\x00\x00\x00\x94\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\xe0\x10\x00\xc0\x01\x00\x01\x00"
	"\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x03\x0c\x00\x01\x00\x00\x00"
	"\xd4\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xd8\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xdc\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xe0\x10\x00\xc0\x02\x00\x01\x00\x01\x00\x00\x00"
	"\xd0\x10\x00\xc0\x00\x03\x0c\x00\x01\x00\x00\x00\xd4\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xd8\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xdc\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xe0\x10\x00\xc0\x04\x00\x01\x00\x01\x00\x00\x00\xd0\x10\x00\xc0"
	"\x00\x03\x0c\x00\x01\x00\x00\x00\xd4\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xd8\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xdc\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xe0\x10\x00\xc0"
	"\x08\x00\x01\x00\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x03\x0c\x00"
	"\x01\x00\x00\x00\xd4\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xd8\x10\x00\xc0\x42\x08\x31\x00\x01\x00\x00\x00\xdc\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xe0\x10\x00\xc0\x10\x00\x01\x00"
	"\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x03\x0c\x00\x01\x00\x00\x00"
	"\xd4\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xd8\x10\x00\xc0"
	"\x42\x08\x31\x00\x01\x00\x00\x00\xdc\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\x20\x00\x00\xc0\x01\x00\x00\x11\x04\x00\x00\x00"
	"\x08\x00\x00\xc0\x01\x00\x00\x00\x00\x10\x00\x00\x6d\x72\x65\x54"
	"\x08\x00\x00\x00"
	;

const size_t minimal_secure_timn_size = 2404;

char minimal_tim[] =
	"\x00\x06\x03\x00\x48\x4d\x49\x54\x00\x00\x00\x00\x18\x20\x09\x24"
	"\x4c\x56\x52\x4d\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\x23\x52\x41\x55\x01\x00\x00\x00"
	"\x00\x00\x00\x00\xa8\x08\x00\x00\x48\x4d\x49\x54\xff\xff\xff\xff"
	"\x00\x00\x00\x00\x00\x60\x00\x20\x4c\x09\x00\x00\x4c\x09\x00\x00"
	"\x40\x00\x00\x00\x3e\x99\x84\x39\xa9\x91\x6c\xfb\x48\x4f\x7f\x28"
	"\x1e\x12\x88\xb1\xd2\xc0\x74\x7c\xa7\xfc\x9c\x84\xc2\xe1\x90\x3f"
	"\xab\xda\x23\xcb\xd6\xaf\xf6\x30\x85\x79\x73\x1b\xab\x65\xaa\x3c"
	"\x65\x55\xc5\x21\x6d\xd9\x32\x3e\x7f\xc9\x51\xa0\x58\x5e\x17\xcd"
	"\x23\xa7\x00\xa9\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x48\x54\x50\x4f\x06\x00\x00\x00\x32\x56\x52\x43"
	"\x14\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x00\x00\xff\x1f"
	"\x50\x44\x49\x43\x20\x00\x00\x00\x01\x00\x00\x00\x49\x52\x42\x54"
	"\x03\x00\x00\x00\x31\x50\x50\x47\x32\x50\x50\x47\x33\x52\x44\x44"
	"\x31\x50\x50\x47\x88\x00\x00\x00\x01\x00\x00\x00\x0a\x00\x00\x00"
	"\x06\x00\x00\x00\x00\x00\x00\x00\x19\x00\x00\x00\x0c\xd0\x00\xc0"
	"\x00\x00\x00\x80\x00\x00\x00\x00\x01\x00\x00\x00\x42\x50\x53\x31"
	"\x06\x00\x00\x00\x0c\xd0\x00\xc0\xff\xff\xff\x7f\x01\x00\x00\x00"
	"\x40\x38\x01\xc0\x1e\x1d\x00\x00\x18\x00\x00\x00\x42\x50\x53\x30"
	"\x00\x00\x00\x00\x1b\x00\x00\x00\x42\x50\x53\x30\x18\x00\x00\x00"
	"\x42\x50\x53\x31\x06\x00\x00\x00\x00\x56\x01\xc0\xff\xfe\xff\xff"
	"\x07\x00\x00\x00\x00\x56\x01\xc0\x00\x06\x00\x00\x07\x00\x00\x00"
	"\x00\xa4\x01\xc0\x40\x00\x00\x00\x32\x50\x50\x47\xc8\x01\x00\x00"
	"\x01\x00\x00\x00\x22\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\x00"
	"\x19\x00\x00\x00\x30\x83\x00\xc0\x02\x00\x00\x00\x02\x00\x00\x00"
	"\x01\x00\x00\x00\x43\x4e\x54\x33\x06\x00\x00\x00\x30\x83\x00\xc0"
	"\xfe\xff\xff\xff\x07\x00\x00\x00\x30\x83\x00\xc0\x01\x00\x00\x00"
	"\x18\x00\x00\x00\x43\x4e\x54\x33\x19\x00\x00\x00\x4c\x40\x01\xc0"
	"\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x4d\x42\x31\x00"
	"\x01\x00\x00\x00\x00\x04\x00\x64\x00\x00\x00\x00\x18\x00\x00\x00"
	"\x4d\x42\x31\x00\x19\x00\x00\x00\x08\x02\x00\xc0\x01\x00\x00\x00"
	"\x01\x00\x00\x00\x01\x00\x00\x00\x43\x53\x31\x00\x0c\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x02\x00\xc0\x0d\x00\x00\x00\x01\x00\x00\x00"
	"\x00\x00\x00\x00\x1b\x00\x00\x00\x43\x4f\x4e\x46\x18\x00\x00\x00"
	"\x43\x53\x31\x00\x0c\x00\x00\x00\x00\x00\x00\x00\x08\x02\x00\xc0"
	"\x0f\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x12\x00\x00\x00"
	"\x01\x00\x00\x00\x00\x00\x80\xff\x10\x00\x00\x00\x01\x00\x00\x00"
	"\x10\x00\x00\x00\x18\x00\x00\x00\x43\x4f\x4e\x46\x10\x00\x00\x00"
	"\x00\x00\x00\x00\x10\x00\x00\x00\x12\x00\x00\x00\x00\x00\x00\x00"
	"\x1f\x00\x00\x00\x0d\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00"
	"\x18\x00\x00\x00\x43\x4f\x4e\x00\x1a\x00\x00\x00\x00\x00\x00\x00"
	"\x1f\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x4f\x55\x54\x00"
	"\x11\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x1f\x00\x00\x00"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x1b\x00\x00\x00\x43\x4f\x4e\x00"
	"\x18\x00\x00\x00\x4f\x55\x54\x00\x1f\x00\x00\x00\x02\x00\x00\x00"
	"\x01\x00\x00\x00\x1e\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00"
	"\x1a\x00\x00\x00\x02\x00\x00\x00\x00\x80\xff\x00\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x44\x52\x41\x4d\x0d\x00\x00\x00\x02\x00\x00\x00"
	"\xff\x7f\x00\x00\x18\x00\x00\x00\x44\x52\x41\x4d\x06\x00\x00\x00"
	"\x00\xcf\x00\xc0\xfe\xff\xff\xff\x0e\x00\x00\x00\x02\x00\x00\x00"
	"\x04\xcf\x00\xc0\x07\x00\x00\x00\x00\xcf\x00\xc0\x01\x00\x00\x00"
	"\x33\x52\x44\x44\x14\x06\x00\x00\x01\x00\x00\x00\x80\x00\x00\x00"
	"\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x08\x40\x01\xc0"
	"\x00\x45\x40\x00\x01\x00\x00\x00\x00\x20\x00\xc0\x00\x00\x01\x00"
	"\x01\x00\x00\x00\x04\x20\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x40\x03\x00\xc0\xef\x0f\x0f\x0f\x01\x00\x00\x00\x44\x03\x00\xc0"
	"\xaa\x00\x00\x10\x01\x00\x00\x00\x10\x03\x00\xc0\x00\x00\x20\x00"
	"\x01\x00\x00\x00\x04\x03\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x08\x03\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\x00\x02\x00\xc0"
	"\x01\x00\x0e\x00\x01\x00\x00\x00\x04\x02\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x20\x02\x00\xc0\x32\x06\x02\x13\x01\x00\x00\x00"
	"\x44\x00\x00\xc0\x00\x02\x03\x00\x01\x00\x00\x00\xc0\x02\x00\xc0"
	"\x00\x60\x00\x00\x01\x00\x00\x00\xc4\x02\x00\xc0\x20\x00\x10\x00"
	"\x01\x00\x00\x00\x58\x00\x00\xc0\x3f\x14\x00\x00\x01\x00\x00\x00"
	"\x48\x00\x00\xc0\x01\x00\x00\x00\x01\x00\x00\x00\x80\x01\x00\xc0"
	"\x00\x02\x01\x00\x01\x00\x00\x00\x50\x00\x00\xc0\xff\x01\x00\x00"
	"\x01\x00\x00\x00\x4c\x00\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x54\x00\x00\xc0\x80\x04\x00\x00\x01\x00\x00\x00\x00\x03\x00\xc0"
	"\x0b\x08\x00\x00\x01\x00\x00\x00\x80\x03\x00\xc0\x20\xa1\x07\x00"
	"\x01\x00\x00\x00\x84\x03\x00\xc0\x40\x0d\x03\x00\x01\x00\x00\x00"
	"\x88\x03\x00\xc0\x6b\x00\x60\x09\x01\x00\x00\x00\x8c\x03\x00\xc0"
	"\x00\x02\x00\x00\x01\x00\x00\x00\x90\x03\x00\xc0\x00\x01\x40\x00"
	"\x01\x00\x00\x00\x94\x03\x00\xc0\xcf\x03\xf0\x00\x01\x00\x00\x00"
	"\x98\x03\x00\xc0\x00\x02\xf8\x00\x01\x00\x00\x00\x9c\x03\x00\xc0"
	"\x08\x08\x00\x00\x01\x00\x00\x00\xa0\x03\x00\xc0\x14\x06\x04\x00"
	"\x01\x00\x00\x00\xa4\x03\x00\xc0\x01\x00\x00\x00\x01\x00\x00\x00"
	"\xa8\x03\x00\xc0\x04\x0c\x00\x00\x01\x00\x00\x00\xac\x03\x00\xc0"
	"\x1f\x0c\x2a\x20\x01\x00\x00\x00\xb0\x03\x00\xc0\x0c\x06\x0c\x0c"
	"\x01\x00\x00\x00\xb4\x03\x00\xc0\x00\x06\x00\x04\x01\x00\x00\x00"
	"\xb8\x03\x00\xc0\x00\x08\x00\x00\x01\x00\x00\x00\xbc\x03\x00\xc0"
	"\x04\x04\x02\x02\x01\x00\x00\x00\xc0\x03\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\xc4\x03\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\xdc\x03\x00\xc0\x39\x12\x08\x00\x01\x00\x00\x00\xc8\x02\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x64\x00\x00\xc0\x06\x00\x00\x00"
	"\x01\x00\x00\x00\x44\x00\x00\xc0\x00\x02\x03\x00\x01\x00\x00\x00"
	"\x04\x10\x00\xc0\x89\x78\x47\xc4\x01\x00\x00\x00\x08\x10\x00\xc0"
	"\xfa\x0f\x50\x05\x01\x00\x00\x00\x0c\x10\x00\xc0\x77\xdf\x21\x05"
	"\x01\x00\x00\x00\x10\x10\x00\xc0\x08\x01\x30\x00\x01\x00\x00\x00"
	"\x28\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\x30\x10\x00\xc0"
	"\x00\x00\x80\x03\x01\x00\x00\x00\x34\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\x40\x10\x00\xc0\x00\x04\x00\x00\x01\x00\x00\x00"
	"\xc0\x10\x00\xc0\x01\x00\x00\x80\x01\x00\x00\x00\xd0\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\xe0\x10\x00\xc0\xf0\x1f\x01\x00"
	"\x01\x00\x00\x00\x90\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x94\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\x98\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\x9c\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\xa0\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\xa4\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00\xa8\x10\x00\xc0"
	"\x00\x00\x00\x00\x01\x00\x00\x00\xac\x10\x00\xc0\x00\x00\x00\x00"
	"\x01\x00\x00\x00\xb0\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\x00\x10\x00\xc0\x41\x40\x04\x00\x01\x00\x00\x00\x14\x10\x00\xc0"
	"\x00\x02\x08\x00\x01\x00\x00\x00\x38\x10\x00\xc0\x02\x00\x00\x00"
	"\x01\x00\x00\x00\x3c\x10\x00\xc0\x10\x00\x00\x00\x01\x00\x00\x00"
	"\x80\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00\x84\x11\x00\xc0"
	"\x0a\x02\x00\x00\x01\x00\x00\x00\x88\x11\x00\xc0\x0a\x02\x00\x00"
	"\x01\x00\x00\x00\x8c\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00"
	"\x90\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00\x94\x11\x00\xc0"
	"\x0a\x02\x00\x00\x01\x00\x00\x00\x98\x11\x00\xc0\x0a\x02\x00\x00"
	"\x01\x00\x00\x00\x9c\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00"
	"\xa0\x11\x00\xc0\x0a\x02\x00\x00\x01\x00\x00\x00\x50\x10\x00\xc0"
	"\x00\x00\x13\x1f\x01\x00\x00\x00\x54\x10\x00\xc0\x00\x00\x19\x20"
	"\x01\x00\x00\x00\x74\x10\x00\xc0\x00\x00\x20\x20\x01\x00\x00\x00"
	"\x58\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00\x5c\x10\x00\xc0"
	"\x00\x00\x08\x08\x01\x00\x00\x00\x60\x10\x00\xc0\x00\x00\x08\x08"
	"\x01\x00\x00\x00\x64\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00"
	"\x68\x10\x00\xc0\x00\x00\x08\x08\x01\x00\x00\x00\x6c\x10\x00\xc0"
	"\x00\x00\x08\x08\x01\x00\x00\x00\x70\x10\x00\xc0\x00\x00\x08\x08"
	"\x01\x00\x00\x00\x00\x10\x00\xc0\x41\x40\x04\x00\x01\x00\x00\x00"
	"\xbc\x03\x00\xc0\x04\x04\x02\x02\x01\x00\x00\x00\x14\x10\x00\xc0"
	"\x00\x02\x08\x00\x01\x00\x00\x00\x38\x10\x00\xc0\x03\x00\x00\x00"
	"\x01\x00\x00\x00\x3c\x10\x00\xc0\x20\x00\x00\x00\x01\x00\x00\x00"
	"\x20\x10\x00\xc0\x00\x00\x00\x80\x01\x00\x00\x00\x20\x10\x00\xc0"
	"\x00\x00\x00\x20\x01\x00\x00\x00\x20\x10\x00\xc0\x00\x00\x00\x40"
	"\x03\x00\x00\x00\x0a\x00\x00\x00\x01\x00\x00\x00\x20\x10\x00\xc0"
	"\x00\x00\x00\x80\x03\x00\x00\x00\x0a\x00\x00\x00\x07\x00\x00\x00"
	"\x30\x10\x00\xc0\x00\x00\x80\x03\x07\x00\x00\x00\x34\x10\x00\xc0"
	"\x00\x00\x00\x00\x07\x00\x00\x00\x90\x10\x00\xc0\x00\x00\x00\x00"
	"\x07\x00\x00\x00\x94\x10\x00\xc0\x00\x00\x00\x00\x01\x00\x00\x00"
	"\xe0\x10\x00\xc0\x01\x00\x01\x00\x01\x00\x00\x00\xd0\x10\x00\xc0"
	"\x00\x03\x0c\x00\x01\x00\x00\x00\xd4\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xd8\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xdc\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xe0\x10\x00\xc0"
	"\x02\x00\x01\x00\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x03\x0c\x00"
	"\x01\x00\x00\x00\xd4\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xd8\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xdc\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xe0\x10\x00\xc0\x04\x00\x01\x00"
	"\x01\x00\x00\x00\xd0\x10\x00\xc0\x00\x03\x0c\x00\x01\x00\x00\x00"
	"\xd4\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\xd8\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xdc\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xe0\x10\x00\xc0\x08\x00\x01\x00\x01\x00\x00\x00"
	"\xd0\x10\x00\xc0\x00\x03\x0c\x00\x01\x00\x00\x00\xd4\x10\x00\xc0"
	"\x03\x0c\x30\x00\x01\x00\x00\x00\xd8\x10\x00\xc0\x42\x08\x31\x00"
	"\x01\x00\x00\x00\xdc\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00"
	"\xe0\x10\x00\xc0\x10\x00\x01\x00\x01\x00\x00\x00\xd0\x10\x00\xc0"
	"\x00\x03\x0c\x00\x01\x00\x00\x00\xd4\x10\x00\xc0\x03\x0c\x30\x00"
	"\x01\x00\x00\x00\xd8\x10\x00\xc0\x42\x08\x31\x00\x01\x00\x00\x00"
	"\xdc\x10\x00\xc0\x03\x0c\x30\x00\x01\x00\x00\x00\x20\x00\x00\xc0"
	"\x01\x00\x00\x11\x04\x00\x00\x00\x08\x00\x00\xc0\x01\x00\x00\x00"
	"\x00\x10\x00\x00\x6d\x72\x65\x54\x08\x00\x00\x00";

const size_t minimal_tim_size = 2380;

void tim_minimal_image(image_t *tim, int secure)
{
	void *data, *from;
	u32 size;

	if (secure == 2) {
		from = minimal_secure_timn;
		size = minimal_secure_timn_size;
	} else if (secure == 1) {
		from = minimal_secure_tim;
		size = minimal_secure_tim_size;
	} else {
		from = minimal_tim;
		size = minimal_tim_size;
	}

	data = xmalloc(size);
	memcpy(data, from, size);

	if (tim->data)
		free(tim->data);

	tim->id = le32toh(*(u32 *) (from + 4));
	tim->data = data;
	tim->size = size;
}

void tim_parse(image_t *tim, int *numimagesp)
{
	static const u32 zerohash[16];
	timhdr_t *timhdr;
	imginfo_t *i, *start, *end;
	respkg_t *pkg;
	platds_t *platds;
	u32 version, date, numimages, numkeys, bootfs, sizeofreserved;

	if (tim->size < sizeof(timhdr_t))
		die("TIMH length too small (%u, should be at least %zu)",
		    tim->size, sizeof(timhdr_t));

	timhdr = (timhdr_t *) tim->data;

	version = le32toh(timhdr->version);
	date = le32toh(timhdr->issuedate);
	numimages = le32toh(timhdr->numimages);
	numkeys = le32toh(timhdr->numkeys);
	bootfs = le32toh(timhdr->bootflashsign);
	sizeofreserved = le32toh(timhdr->sizeofreserved);

	printf("TIM version %u.%u.%u, issue date %02x.%02x.%04x, %s, %u images,"
	       " %u keys, boot flash sign %s\n",
	       version >> 16, (version >> 8) & 0xff, version & 0xff, date >> 24,
	       (date >> 16) & 0xff, date & 0xffff,
	       timhdr->trusted ? "trusted" : "non-trusted", numimages, numkeys,
	       bootfs2name(bootfs));

	printf("Reserved area packages:");
	for (pkg = firstpkg(timhdr); pkg; pkg = nextpkg(timhdr, pkg))
		printf(" %s", id2name(pkg->id));
	printf("\n");

	if (!timhdr->trusted && numkeys)
		die("Keys present in non-trusted TIM");

	if (tim->size != tim_size(timhdr))
		die("Invalid TIM length (%u, expected %u)", tim->size,
		    tim_size(timhdr));

	platds = (void *) reserved_area(timhdr) + sizeofreserved;
	if (timhdr->trusted)
		printf("Platform digital signature algorithm %s, key size %u "
		       "bits, hash %s\n", dsalg2name(le32toh(platds->dsalg)),
		       le32toh(platds->keysize),
		       hash2name(le32toh(platds->hashalg)));

	start = (imginfo_t *) (timhdr + 1);
	end = start + numimages;
	for (i = start; i < end; ++i) {
		image_t *img;
		int nohash;
		u32 id, size, nextid, hashid, hashalg, sizetohash;
		u32 hash[16];

		id = le32toh(i->id);
		size = le32toh(i->size);
		hashalg = le32toh(i->hashalg);
		sizetohash = le32toh(i->sizetohash);

		if (i->nextid != 0xffffffff &&
		    (i + 1 == end || (i + 1)->id != i->nextid))
			die("Next image ID check failed");

		img = image_find(id);
		if (img->size != size)
			die("Wrong length of %s image (%u, expected %u)",
			    id2name(id), img->size, size);

		nohash = !memcmp(i->hash, zerohash, sizeof(zerohash));

		printf("Found %s, hash %s%s, encryption %s, size %u, load 0x%08x, flash 0x%08x\n",
		       id2name(id), hash2name(i->hashalg),
		       nohash ? " (hash zeroed)" : "",
		       enc2name(le32toh(i->encalg)),
		       le32toh(i->size), le32toh(i->loadaddr),
		       le32toh(i->flashentryaddr));

		if (nohash)
			continue;

		if (id == tim->id && sizetohash > getsizetohash(timhdr))
			sizetohash = getsizetohash(timhdr);
		else if (sizetohash > size)
			sizetohash = size;

		image_hash(i->hashalg, img->data, sizetohash, hash,
			   id == tim->id ? (u8 *) &i->hash[0] - tim->data : -1);

		if (memcmp(hash, i->hash, sizeof(hash)))
			die("Hash check failed for %s", id2name(id));
	}

	if (numimagesp)
		*numimagesp = numimages;
}

static void tim_grow(image_t *tim, u32 growby)
{
	image_t oldtim;

	oldtim = *tim;
	tim->data = xmalloc(oldtim.size + growby);
	memcpy(tim->data, oldtim.data, oldtim.size);
	tim->size += growby;

	free(oldtim.data);
}

void tim_enable_hash(image_t *tim, u32 id, int enable)
{
	imginfo_t *img;

	img = tim_find_image(tim, id);
	if (img) {
		if (enable) {
			img->sizetohash = img->size ? img->size : 1;
		} else {
			img->sizetohash = 0;
			memset(img->hash, 0, sizeof(img->hash));
		}
	}
}

void tim_rehash(image_t *tim)
{
	timhdr_t *timhdr;
	int i;
	imginfo_t *img;
	u32 sizetohash, id;

	timhdr = (void *) tim->data;
	sizetohash = getsizetohash(timhdr);

	for (i = 0; i < tim_nimages(timhdr); ++i) {
		image_t *image;

		img = tim_image(timhdr, i);

		id = le32toh(img->id);
		if (id == tim->id)
			continue;

		image = image_find(id);

		if (!img->sizetohash) {
			memset(img->hash, 0, sizeof(img->hash));
		} else {
			img->sizetohash = htole32(image->size);
			image_hash(le32toh(img->hashalg), image->data,
				   image->size, img->hash, -1);
		}
	}

	img = tim_find_image(tim, tim->id);
	if (img) {
		img->size = htole32(tim->size);
		img->sizetohash = htole32(sizetohash);

		if (timhdr->trusted)
			memset(img->hash, 0, sizeof(img->hash));
		else
			image_hash(le32toh(img->hashalg), tim->data, sizetohash,
				   img->hash, (u8 *) &img->hash[0] - tim->data);
	}
}

void tim_set_boot(image_t *tim, u32 boot)
{
	timhdr_t *timhdr = (void *) tim->data;

	timhdr->bootflashsign = htole32(boot);
	tim_rehash(tim);
}

void tim_add_image(image_t *tim, image_t *image, u32 after, u32 loadaddr,
		   u32 flashaddr, int hash)
{
	timhdr_t *timhdr;
	imginfo_t *timinfo, *prev, *this, *next;
	int i;
	u32 oldtimsize;

	oldtimsize = tim->size;
	tim_grow(tim, sizeof(imginfo_t));

	timinfo = prev = NULL;

	timhdr = (timhdr_t *) tim->data;
	for (i = 0; i < tim_nimages(timhdr); ++i) {
		imginfo_t *img;

		img = tim_image(timhdr, i);

		if (le32toh(img->id) == tim->id)
			timinfo = img;

		if (le32toh(img->id) == after)
			prev = img;
	}

	this = prev + 1;
	next = prev + 2;
	memmove(next, this, oldtimsize - ((void *) this - (void *) timhdr));

	memset(this, 0, sizeof(imginfo_t));
	this->id = htole32(image->id);
	this->nextid = prev->nextid;
	prev->nextid = this->id;
	this->size = htole32(image->size);
	this->flashentryaddr = htole32(flashaddr);
	this->loadaddr = htole32(loadaddr);
	this->hashalg = htole32(HASH_SHA512);
	this->sizetohash = hash ? this->size : 0;

	timhdr->numimages = htole32(tim_nimages(timhdr) + 1);

	if (timinfo) {
		timinfo->size = htole32(tim->size);
		timinfo->sizetohash = timinfo->size;
	}
}

static void tim_add_pkgs(image_t *tim, int npkgs, void *pkgs, size_t size)
{
	static int alloced;
	void *oldend;
	timhdr_t *timhdr;
	reshdr_t *reshdr;
	respkg_t *pkg;
	u32 sizeofreserved, toadd;
	int i;

	timhdr = (timhdr_t *) tim->data;
	reshdr = reserved_area(timhdr);
	sizeofreserved = le32toh(timhdr->sizeofreserved);

	toadd = size;
	if (!sizeofreserved)
		toadd += sizeof(reshdr_t) + sizeof(respkg_t);

	tim_grow(tim, toadd);
	oldend = tim->data + tim->size - toadd;

	timhdr = (timhdr_t *) tim->data;
	reshdr = reserved_area(timhdr);

	if (!sizeofreserved) {
		/* add toadd bytes for reserved area */
		memmove(((void *) reshdr) + toadd, reshdr,
			oldend - (void *) reshdr);

		/* add reserved area header */
		reshdr->id = htole32(RES_ID);
		reshdr->pkgs = htole32(npkgs + 1);

		/* add packages */
		memcpy(reshdr + 1, pkgs, size);

		/* add Term package */
		pkg = ((void *) reshdr) + size;
		pkg->id = htole32(PKG_Term);
		pkg->size = htole32(sizeof(respkg_t));
	} else {
		respkg_t *lastpkg = NULL;
		int has_term = 0;

		/* find last package */
		for (pkg = firstpkg(timhdr); pkg; pkg = nextpkg(timhdr, pkg)) {
			lastpkg = pkg;
			if (le32toh(pkg->id) == PKG_Term) {
				has_term = 1;
				break;
			}
		}

		/* if last package was not found, use end of reserved area */
		if (!lastpkg)
			lastpkg = (void *) (reshdr + 1);

		/* add toadd bytes for reserved area */
		memmove(((void *) lastpkg) + toadd, lastpkg,
			oldend - (void *) lastpkg);

		/* add packages */
		memcpy(lastpkg, pkgs, size);

		/* add Term package if it was there */
		if (has_term) {
			pkg = ((void *) lastpkg) + size;
			pkg->id = htole32(PKG_Term);
			pkg->size = htole32(sizeof(respkg_t));
		}

		/* change reserved area header */
		reshdr->pkgs = htole32(le32toh(reshdr->pkgs) + npkgs);
	}

	timhdr->sizeofreserved = htole32(sizeofreserved + toadd);

	tim_rehash(tim);
}

static void key_hash(u32 alg, u32 *hash, const u32 *x, const u32 *y, int pad)
{
	u32 buf[129];

	memset(buf, 0, sizeof(buf));
	if (alg == HASH_SHA512)
		buf[0] = htole32(SIG_SCHEME_ECDSA_P521_SHA512);
	else
		buf[0] = htole32(SIG_SCHEME_ECDSA_P521_SHA256);
	memcpy(&buf[1], x, 68);
	memcpy(&buf[18], y, 68);

	image_hash(alg, buf, pad ? sizeof(buf) : 140, hash, -1);
}

void tim_add_key(image_t *tim, u32 id, EC_KEY *key)
{
	timhdr_t *timhdr;
	keyinfo_t *keyinfo;
	u32 oldtimsize;

	oldtimsize = tim->size;
	tim_grow(tim, sizeof(keyinfo_t));
	timhdr = (timhdr_t *) tim->data;

	keyinfo = ((void *) (timhdr + 1))
		  + sizeof(imginfo_t) * tim_nimages(timhdr)
		  + sizeof(keyinfo_t) * tim_nkeys(timhdr);

	memmove(keyinfo + 1, keyinfo,
		oldtimsize - ((void *) keyinfo - (void *) timhdr));

	timhdr->numkeys = htole32(le32toh(timhdr->numkeys) + 1);

	memset(keyinfo, 0, sizeof(keyinfo_t));
	keyinfo->id = htole32(id);
	keyinfo->hashalg = htole32(HASH_SHA256);
	keyinfo->size = htole32(521);
	keyinfo->publickeysize = htole32(521);
	keyinfo->encryptalg = htole32(DSALG_ECDSA_521);
	key_get_tim_coords(key, keyinfo->ECDSAcompx, keyinfo->ECDSAcompy);
	key_hash(HASH_SHA256, keyinfo->hash, keyinfo->ECDSAcompx,
		 keyinfo->ECDSAcompy, 0);
}

void tim_get_otp_hash(image_t *tim, u32 *hash)
{
	timhdr_t *timhdr;
	platds_t *platds;

	timhdr = (void *) tim->data;

	if (!timhdr->trusted)
		die("Cannot get OTP hash for non-trusted TIM");

	platds = (void *) tim->data + tim->size - sizeof(platds_t);

	key_hash(HASH_SHA256, hash, platds->ECDSA.pub.x, platds->ECDSA.pub.y,
		 1);
}

void tim_sign(image_t *tim, EC_KEY *key)
{
	ECDSA_SIG *sig;
	timhdr_t *timhdr;
	platds_t *platds;
	u32 hash[16];

	timhdr = (void *) tim->data;
	if (!timhdr->trusted)
		tim_grow(tim, sizeof(platds_t));

	timhdr = (void *) tim->data;
	platds = (void *) tim->data + tim->size - sizeof(platds_t);

	timhdr->trusted = htole32(1);
	tim_rehash(tim);

	memset(platds, 0, sizeof(platds_t));

	platds->dsalg = htole32(DSALG_ECDSA_521);
	platds->hashalg = htole32(HASH_SHA256);
	platds->keysize = htole32(521);

	key_get_tim_coords(key, platds->ECDSA.pub.x, platds->ECDSA.pub.y);

	image_hash(HASH_SHA512, tim->data, (u8 *) &platds->ECDSA.sig - tim->data,
		   hash, -1);

	sig = ECDSA_do_sign((void *) hash, sizeof(hash), key);
	if (!sig)
		die("Could not sign");

	bn2tim(sig->r, platds->ECDSA.sig.r, 17);
	bn2tim(sig->s, platds->ECDSA.sig.s, 17);
}
