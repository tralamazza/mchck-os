#include <mchck.h>

void
crc_init(
	uint32_t seed,
	uint32_t poly,
	enum crc_width_t width,
	enum crc_transpose_t totr,
	enum crc_transpose_t tot,
	uint8_t compl_xor)
{
	bf_set(SIM_SCGC6, SIM_SCGC6_CRC, 1);
	CRC_CTRL =
		(compl_xor ? CRC_CTRL_FXOR_MASK : 0) |
		(width ? CRC_CTRL_TCRC_MASK : 0) |
		CRC_CTRL_TOTR(totr) |
		CRC_CTRL_TOT(tot);
	CRC_GPOLY = poly;
	bf_set(CRC_CTRL, CRC_CTRL_WAS, 1);
	CRC_CRC = seed;
	bf_set(CRC_CTRL, CRC_CTRL_WAS, 0);
}

void
crc_update(const void *buf, size_t len)
{
	size_t n = len / 4;
	while (n-- > 0) {
		CRC_CRC = *(uint32_t*)buf;
		buf += 4;
	}
	n = len % 4;
	while (n-- > 0)
		CRC_CRCLL = *(uint8_t*)buf++;
}

uint32_t
crc_value(void)
{
	return CRC_CRC;
}