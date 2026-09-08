/** Captive DNS: internal to hk_portal. */
#ifndef HK_PORTAL_DNS_H
#define HK_PORTAL_DNS_H

#include <stdint.h>

#include "esp_err.h"

/** Answer every A query with `address` (network byte order) until stopped. */
esp_err_t hk_portal_dns_start(uint32_t address);
void hk_portal_dns_stop(void);

#endif /* HK_PORTAL_DNS_H */
