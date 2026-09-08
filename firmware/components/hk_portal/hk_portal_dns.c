/**
 * The half of a captive portal that is not HTTP.
 *
 * A phone decides it is behind a portal by fetching a known URL and not getting
 * the answer it expects. For that fetch to reach us at all, the name in it has
 * to resolve to us -- so every name does, for as long as the setup window is
 * open. Without this the setup network looks like a network with no internet,
 * and nothing opens by itself.
 */
#include "hk_portal_dns.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hk_portal_dns";

/** A query bigger than this is not one we can usefully answer. */
#define HK_DNS_BUFFER 512
#define HK_DNS_PORT 53

/** DNS header, wire order. Fields we do not touch are still named. */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority;
    uint16_t additional;
} hk_dns_header_t;

/** Answer record, appended after the echoed question. */
typedef struct __attribute__((packed)) {
    uint16_t name;      /**< Pointer to offset 12, where the question's name is. */
    uint16_t type;
    uint16_t klass;
    uint32_t ttl;
    uint16_t length;
    uint32_t address;
} hk_dns_answer_t;

static TaskHandle_t s_task;
static int          s_socket = -1;
static uint32_t     s_address;
static volatile bool s_running;

/**
 * Build a reply in place.
 *
 * Returns the reply length, or 0 when the query is one we decline to answer.
 * Only single-question A queries get an address; anything else is answered
 * with an empty NOERROR, which is what stops a phone waiting for an AAAA that
 * will never come.
 */
static size_t build_reply(uint8_t *packet, size_t length)
{
    if (length < sizeof(hk_dns_header_t)) {
        return 0;
    }

    hk_dns_header_t *header = (hk_dns_header_t *)packet;

    /* Bit 15 set means this is already a response; opcode (bits 14-11) other
     * than zero means it is not a standard query. Neither is ours to answer. */
    const uint16_t flags = ntohs(header->flags);
    if ((flags & 0x8000u) != 0u || (flags & 0x7800u) != 0u) {
        return 0;
    }
    if (ntohs(header->questions) != 1u) {
        return 0;
    }

    /* Walk the QNAME's length-prefixed labels to find where the question ends. */
    size_t at = sizeof(hk_dns_header_t);
    while (at < length && packet[at] != 0u) {
        /* A label length with its top bits set is a compression pointer, which
         * has no business in a question. */
        if ((packet[at] & 0xC0u) != 0u) {
            return 0;
        }
        at += (size_t)packet[at] + 1u;
    }
    if (at >= length) {
        return 0;
    }
    at += 1u; /* the zero-length label that ends the name */
    if (at + 4u > length) {
        return 0;
    }

    uint16_t qtype;
    memcpy(&qtype, packet + at, sizeof(qtype));
    at += 4u; /* QTYPE and QCLASS */

    /* Response, authoritative. Recursion is neither available nor offered. */
    header->flags = htons(0x8400u);

    if (ntohs(qtype) != 1u) {
        header->answers = 0u;
        return at;
    }

    if (at + sizeof(hk_dns_answer_t) > HK_DNS_BUFFER) {
        return 0;
    }

    hk_dns_answer_t answer = {
        .name    = htons(0xC00Cu), /* the name at offset 12: the question's own */
        .type    = htons(1u),
        .klass   = htons(1u),
        /* Zero, deliberately. The mapping is a lie that must expire the moment
         * the phone leaves this network, and a cached one outlives the window. */
        .ttl     = 0u,
        .length  = htons(4u),
        .address = s_address,
    };
    memcpy(packet + at, &answer, sizeof(answer));
    header->answers = htons(1u);
    return at + sizeof(answer);
}

static void dns_task(void *arg)
{
    (void)arg;
    uint8_t packet[HK_DNS_BUFFER];

    while (s_running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        const int received = (int)recvfrom(s_socket, packet, sizeof(packet), 0,
                                           (struct sockaddr *)&from, &from_len);
        if (received <= 0) {
            /* The socket is closed on stop, which lands here. */
            if (!s_running) {
                break;
            }
            continue;
        }

        const size_t reply = build_reply(packet, (size_t)received);
        if (reply == 0u) {
            continue;
        }
        (void)sendto(s_socket, packet, reply, 0, (struct sockaddr *)&from, from_len);
    }

    close(s_socket);
    s_socket = -1;
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t hk_portal_dns_start(uint32_t address)
{
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_address = address;
    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_socket < 0) {
        ESP_LOGE(TAG, "no socket for the captive responder");
        return ESP_FAIL;
    }

    struct sockaddr_in bind_address = {
        .sin_family      = AF_INET,
        .sin_port        = htons(HK_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_socket, (struct sockaddr *)&bind_address, sizeof(bind_address)) != 0) {
        ESP_LOGE(TAG, "could not bind port %d", HK_DNS_PORT);
        close(s_socket);
        s_socket = -1;
        return ESP_FAIL;
    }

    s_running = true;
    if (xTaskCreate(dns_task, "hk_portal_dns", 3072, NULL, 4, &s_task) != pdPASS) {
        s_running = false;
        close(s_socket);
        s_socket = -1;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void hk_portal_dns_stop(void)
{
    if (s_task == NULL) {
        return;
    }
    s_running = false;
    /* Shut the socket down rather than signalling the task: it is parked in
     * recvfrom, and this is what wakes it. */
    if (s_socket >= 0) {
        shutdown(s_socket, SHUT_RDWR);
    }
}
