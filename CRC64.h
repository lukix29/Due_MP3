#ifndef __CRC64_H__
#define __CRC64_H__
#include <stddef.h>
#include <stdint.h>

#define POLY UINT64_C(0x42f0e1eba9ea3693)
#define TOP UINT64_C(0x8000000000000000)

/* Return crc updated with buf[0..len-1].  If buf is NULL, return the initial
crc.  So, initialize with crc = crc64_ecma182(0, NULL, 0); and follow with
one or more applications of crc = crc64_ecma182(crc, buf, len); */
int64_t crc64_ecma182(int64_t crc, unsigned char *buf, size_t len)
{
	int k;

	if (buf == NULL)
		return 0;
	while (len--) {
		crc ^= (uint64_t)(*buf++) << 56;
		for (k = 0; k < 8; k++)
			crc = crc & TOP ? (crc << 1) ^ POLY : crc << 1;
	}
	return crc;
}
#endif