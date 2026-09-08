#include "hk_portal.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"

#include "hk_identity.h"
#include "hk_portal_dns.h"

static const char *TAG = "hk_portal";

/** Most APs a scan will offer. Beyond this the list stops being a list. */
#define HK_PORTAL_MAX_NETWORKS 20

static httpd_handle_t      s_server;
static bool                s_dns_running;
static hk_portal_state_t   s_state;
/** Dotted-quad of the setup network's own address, read from the interface. */
static char                s_address[16];

/* ------------------------------------------------------------------------ */
/* The page                                                                  */
/*                                                                           */
/* One file, no external references. A captive portal is shown by a browser  */
/* that has no route to anywhere else, so every byte it needs has to already */
/* be here -- a stylesheet or a font from a CDN is a spinner that never ends.*/
/*                                                                           */
/* It also works with no JavaScript at all: the form posts on its own and the*/
/* result page refreshes itself. iOS shows captive portals in a cut-down web */
/* view, and "the setup page needs a real browser" is not an instruction any */
/* owner should have to follow. JavaScript only fills the network list in.   */
/* ------------------------------------------------------------------------ */
static const char HK_PORTAL_PAGE[] =
"<!doctype html><html lang=tr><head><meta charset=utf-8>"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<title>" HK_PORTAL_TITLE "</title><style>"
"body{font:16px/1.5 system-ui,sans-serif;margin:0;background:#111;color:#eee}"
"main{max-width:26rem;margin:0 auto;padding:1.5rem}"
"h1{font-size:1.3rem;margin:.2rem 0 1.2rem}"
"label{display:block;margin:1rem 0 .3rem;font-size:.9rem;color:#bbb}"
"input,button{width:100%;box-sizing:border-box;font:inherit;padding:.7rem;"
"border-radius:.5rem;border:1px solid #444;background:#1c1c1c;color:#eee}"
"button{margin-top:1.4rem;background:#e8552d;border-color:#e8552d;color:#fff;font-weight:600}"
"p.note{color:#999;font-size:.85rem}"
"</style></head><body><main>"
"<h1>" HK_PORTAL_TITLE "</h1>"
"<p class=note>Bu hoparlörün katılacağı ağı seçin.</p>"
"<form method=post action=/apply>"
"<label for=ssid>Wi-Fi ağı</label>"
"<input id=ssid name=ssid list=nets autocapitalize=none autocorrect=off required>"
"<datalist id=nets></datalist>"
"<label for=password>Parola</label>"
"<input id=password name=password type=password autocapitalize=none autocorrect=off>"
"<button type=submit>Katıl</button>"
"</form>"
"<p class=note>Hoparlör ile telefonunuz aynı ağda olmak zorunda. Misafir ağı "
"çalışmaz: o ağ cihazları bilerek birbirinden ayırır.</p>"
"</main><script>"
"fetch('/networks').then(r=>r.json()).then(function(n){"
"var d=document.getElementById('nets');"
"n.networks.forEach(function(s){var o=document.createElement('option');o.value=s;d.appendChild(o)});"
"}).catch(function(){});"
"</script></body></html>";

/** Shown after the form is posted; it comes back for the verdict on its own. */
static const char HK_PORTAL_TRYING[] =
"<!doctype html><html lang=en><head><meta charset=utf-8>"
"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
"<meta http-equiv=refresh content=\"4;url=/result\">"
"<title>Katılıyor</title><style>"
"body{font:16px/1.5 system-ui,sans-serif;margin:0;background:#111;color:#eee}"
"main{max-width:26rem;margin:0 auto;padding:1.5rem}"
"</style></head><body><main><h1>Katılıyor…</h1>"
"<p>Hoparlör seçtiğiniz ağı deniyor.</p>"
"<p><a href=/result style=color:#e8552d>Şimdi bak</a></p>"
"</main></body></html>";

/* ------------------------------------------------------------------------ */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------ */

static esp_err_t send_html(httpd_req_t *request, const char *body)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    /* The page changes with the device's state, and a captive portal that a
     * phone serves from cache is one that never updates. */
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, body);
}

/**
 * Send every probe URL back to the portal's own address.
 *
 * The phone asked for a page it knows the content of; anything other than that
 * content tells it it is captive. A redirect is what makes it open the sheet
 * rather than merely report "no internet".
 */
static esp_err_t redirect_handler(httpd_req_t *request)
{
    char location[32];
    (void)snprintf(location, sizeof(location), "http://%s/", s_address);

    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", location);
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, NULL, 0);
}

static esp_err_t root_handler(httpd_req_t *request)
{
    return send_html(request, HK_PORTAL_PAGE);
}

/**
 * Is this byte sequence valid UTF-8?
 *
 * An SSID is bytes, not text, and a neighbouring network is free to advertise
 * a name that is not valid UTF-8. Putting one into the JSON below would make
 * the whole list unparseable -- so one broken neighbour would empty the list
 * for everybody. Such a network is left out of the list instead; it can still
 * be typed by hand.
 */
static bool is_utf8(const uint8_t *bytes, size_t length)
{
    size_t i = 0;
    while (i < length) {
        const uint8_t first = bytes[i];
        size_t extra;

        if (first < 0x80u) {
            extra = 0u;
        } else if ((first & 0xE0u) == 0xC0u && first >= 0xC2u) {
            extra = 1u;
        } else if ((first & 0xF0u) == 0xE0u) {
            extra = 2u;
        } else if ((first & 0xF8u) == 0xF0u && first <= 0xF4u) {
            extra = 3u;
        } else {
            return false;
        }

        /* The continuation bytes have to be inside the name. */
        if (i + extra >= length) {
            return false;
        }
        for (size_t k = 1u; k <= extra; k++) {
            if ((bytes[i + k] & 0xC0u) != 0x80u) {
                return false;
            }
        }
        i += extra + 1u;
    }
    return true;
}

/** Append `text` to `out` as a JSON string body, escaping what JSON reserves. */
static size_t json_escape(const char *text, char *out, size_t capacity)
{
    size_t written = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        const unsigned char c = (unsigned char)text[i];
        char escape[8];
        const char *piece = escape;
        size_t piece_len;

        if (c == '"' || c == '\\') {
            escape[0] = '\\';
            escape[1] = (char)c;
            piece_len = 2u;
        } else if (c < 0x20u) {
            piece_len = (size_t)snprintf(escape, sizeof(escape), "\\u%04x", c);
        } else {
            escape[0] = (char)c;
            piece_len = 1u;
        }

        if (written + piece_len >= capacity) {
            break;
        }
        memcpy(out + written, piece, piece_len);
        written += piece_len;
    }
    out[written] = '\0';
    return written;
}

/**
 * The networks in range, as JSON.
 *
 * This scans directly rather than going through the provisioning manager,
 * which keeps its scan results to itself. The manager also scans, for the app
 * path, and the two would collide -- but only if somebody drove both paths at
 * once, which is one person with two phones. A collision surfaces as an empty
 * list and a log line, not as a wrong network.
 */
static esp_err_t networks_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");

    const wifi_scan_config_t scan = { .show_hidden = false };
    const esp_err_t started = esp_wifi_scan_start(&scan, true);
    if (started != ESP_OK) {
        ESP_LOGW(TAG, "scan refused (%s); the list stays empty and the name can "
                      "still be typed", esp_err_to_name(started));
        return httpd_resp_sendstr(request, "{\"networks\":[]}");
    }

    uint16_t found = 0;
    (void)esp_wifi_scan_get_ap_num(&found);
    if (found > HK_PORTAL_MAX_NETWORKS) {
        found = HK_PORTAL_MAX_NETWORKS;
    }

    wifi_ap_record_t *records = NULL;
    if (found > 0u) {
        records = calloc(found, sizeof(*records));
    }
    if (records == NULL) {
        /* Even on failure the results must be released, or the next scan
         * inherits this one's memory. */
        (void)esp_wifi_scan_stop();
        esp_wifi_clear_ap_list();
        return httpd_resp_sendstr(request, "{\"networks\":[]}");
    }
    (void)esp_wifi_scan_get_ap_records(&found, records);

    httpd_resp_sendstr_chunk(request, "{\"networks\":[");

    size_t emitted = 0;
    for (uint16_t i = 0; i < found; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (ssid[0] == '\0' || !is_utf8(records[i].ssid, strlen(ssid))) {
            continue;
        }

        /* The same network is heard once per band and per mesh node. */
        bool already = false;
        for (uint16_t k = 0; k < i && !already; k++) {
            already = strcmp((const char *)records[k].ssid, ssid) == 0;
        }
        if (already) {
            continue;
        }

        char escaped[3 * 32 + 1];
        (void)json_escape(ssid, escaped, sizeof(escaped));

        httpd_resp_sendstr_chunk(request, emitted == 0u ? "\"" : ",\"");
        httpd_resp_sendstr_chunk(request, escaped);
        httpd_resp_sendstr_chunk(request, "\"");
        emitted++;
    }

    free(records);
    httpd_resp_sendstr_chunk(request, "]}");
    return httpd_resp_sendstr_chunk(request, NULL);
}

/**
 * Take what was typed and hand it to the provisioning manager.
 *
 * Deliberately not esp_wifi_set_config() plus esp_wifi_connect(): that would be
 * a second copy of the joining, retrying and window-closing logic, and the day
 * the two copies disagree is a day nobody would spend looking here.
 */
static esp_err_t apply_handler(httpd_req_t *request)
{
    char body[256];

    if (request->content_len >= sizeof(body)) {
        httpd_resp_set_status(request, "413 Payload Too Large");
        return send_html(request, "<!doctype html><meta charset=utf-8><p>Bu sığmadı.</p>");
    }

    size_t received = 0;
    while (received < request->content_len) {
        const int chunk = httpd_req_recv(request, body + received,
                                         request->content_len - received);
        if (chunk <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)chunk;
    }

    hk_portal_credentials_t credentials;
    if (!hk_portal_parse_form(body, received, &credentials)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_html(request,
                         "<!doctype html><meta charset=utf-8><p>Bu formda bir ağ adı "
                         "yoktu. <a href=/>Geri</a>");
    }

    wifi_config_t config = { 0 };
    /* Not strlcpy of a NUL-terminated string into a fixed field: an SSID is 32
     * bytes and need not be terminated, so the length is what is copied. */
    memcpy(config.sta.ssid, credentials.ssid, strlen(credentials.ssid));
    memcpy(config.sta.password, credentials.password, strlen(credentials.password));

    ESP_LOGI(TAG, "setup page supplied credentials; handing them to the manager");
    s_state = HK_PORTAL_APPLYING;

    const esp_err_t configured = wifi_prov_mgr_configure_sta(&config);
    /* The buffer held the user's Wi-Fi password. It goes now rather than at the
     * end of the function, so no later branch can return with it still there. */
    memset(&config, 0, sizeof(config));
    memset(&credentials, 0, sizeof(credentials));
    memset(body, 0, sizeof(body));

    if (configured != ESP_OK) {
        ESP_LOGE(TAG, "the manager refused the credentials: %s",
                 esp_err_to_name(configured));
        s_state = HK_PORTAL_FAILED;
        httpd_resp_set_status(request, "503 Service Unavailable");
        return send_html(request,
                         "<!doctype html><meta charset=utf-8><p>Hoparlör bunları şu an "
                         "kullanamadı. <a href=/>Tekrar dene</a>");
    }

    return send_html(request, HK_PORTAL_TRYING);
}

static esp_err_t result_handler(httpd_req_t *request)
{
    switch (s_state) {
    case HK_PORTAL_SUCCEEDED:
        return send_html(request,
                         "<!doctype html><html lang=en><meta charset=utf-8>"
                         "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                         "<title>Bağlandı</title><body style=\"font:16px/1.5 system-ui,sans-serif;"
                         "background:#111;color:#eee;padding:1.5rem\">"
                         "<h1>Bağlandı</h1><p>Hoparlör ağa katıldı. Bu sayfayı kapatıp "
                         "kendi Wi-Fi ağınıza dönebilirsiniz.</p>");
    case HK_PORTAL_FAILED:
        return send_html(request,
                         "<!doctype html><html lang=en><meta charset=utf-8>"
                         "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                         "<title>Katılamadı</title><body style=\"font:16px/1.5 system-ui,"
                         "sans-serif;background:#111;color:#eee;padding:1.5rem\">"
                         "<h1>Olmadı</h1><p>Parola yanlış olabilir, ya da ağ menzil "
                         "dışında olabilir.</p>"
                         "<p><a href=/ style=color:#e8552d>Tekrar dene</a></p>");
    case HK_PORTAL_APPLYING:
        return send_html(request, HK_PORTAL_TRYING);
    case HK_PORTAL_IDLE:
    default:
        return redirect_handler(request);
    }
}

void hk_portal_set_state(hk_portal_state_t state)
{
    s_state = state;
}

/* ------------------------------------------------------------------------ */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------ */

/*
 * The URLs each platform fetches to decide whether it is behind a portal.
 *
 * Listed one by one rather than caught with a wildcard, because protocomm
 * publishes the app path's endpoints on this same server: a catch-all handler
 * registered first would answer /prov-session with an HTML page and break the
 * path that already works.
 */
static const char *const HK_PORTAL_PROBES[] = {
    "/hotspot-detect.html",     /* iOS, macOS */
    "/library/test/success.html",
    "/generate_204",            /* Android */
    "/gen_204",
    "/connecttest.txt",         /* Windows */
    "/ncsi.txt",
    "/canonical.html",          /* Firefox */
    "/success.txt",
    "/redirect",
};

esp_err_t hk_portal_start(void)
{
    if (s_server != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip = { 0 };
    if (ap == NULL || esp_netif_get_ip_info(ap, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "the setup network has no address yet");
        return ESP_ERR_INVALID_STATE;
    }
    (void)snprintf(s_address, sizeof(s_address), IPSTR, IP2STR(&ip.ip));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Ours plus protocomm's. The default of eight is enough for neither, and
     * running out shows up as one endpoint quietly not existing. */
    config.max_uri_handlers = 24;
    config.lru_purge_enable = true;
    /* Captive portal probes arrive from several apps at once on a phone. */
    config.max_open_sockets = 7;
    config.stack_size = 6144;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "the setup server did not start: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    const httpd_uri_t root = {
        .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL
    };
    const httpd_uri_t networks = {
        .uri = "/networks", .method = HTTP_GET, .handler = networks_handler, .user_ctx = NULL
    };
    const httpd_uri_t apply = {
        .uri = "/apply", .method = HTTP_POST, .handler = apply_handler, .user_ctx = NULL
    };
    const httpd_uri_t result = {
        .uri = "/result", .method = HTTP_GET, .handler = result_handler, .user_ctx = NULL
    };

    err = httpd_register_uri_handler(s_server, &root);
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_server, &networks);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_server, &apply);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_server, &result);
    }
    for (size_t i = 0; err == ESP_OK && i < sizeof(HK_PORTAL_PROBES) / sizeof(*HK_PORTAL_PROBES); i++) {
        const httpd_uri_t probe = {
            .uri = HK_PORTAL_PROBES[i], .method = HTTP_GET,
            .handler = redirect_handler, .user_ctx = NULL
        };
        err = httpd_register_uri_handler(s_server, &probe);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "a setup page handler did not register: %s", esp_err_to_name(err));
        hk_portal_stop();
        return err;
    }

    if (hk_portal_dns_start(ip.ip.addr) == ESP_OK) {
        s_dns_running = true;
    } else {
        /* The form still works for someone who types the address, so this is a
         * degraded portal rather than none -- but nothing will open by itself,
         * which is the whole point of the app-less path. */
        ESP_LOGE(TAG, "the captive responder did not start; the setup page will "
                      "not open on its own, only at http://%s/", s_address);
    }

    s_state = HK_PORTAL_IDLE;
    ESP_LOGI(TAG, "setup page open at http://%s/", s_address);
    return ESP_OK;
}

httpd_handle_t *hk_portal_server_slot(void)
{
    return &s_server;
}

void hk_portal_stop(void)
{
    if (s_dns_running) {
        hk_portal_dns_stop();
        s_dns_running = false;
    }
    if (s_server != NULL) {
        (void)httpd_stop(s_server);
        s_server = NULL;
    }
    s_state = HK_PORTAL_IDLE;
}
