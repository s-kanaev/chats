#ifndef _P2P_MU_CRC16_CCITT_H_
# define _P2P_MU_CRC16_CCITT_H_

# include <stddef.h>

/*
 * Name  : CRC-16 CCITT
 * Poly  : 0x1021    x^16 + x^12 + x^5 + 1
 * Init  : 0xFFFF
 * Revert: false
 * XorOut: 0x0000
 * Check : 0x29B1 ("123456789")
 */
uint16_t crc16_ccitt(const uint8_t *ptr, size_t len);

#endif /* _P2P_MU_CRC16_CCITT_H_ */
