#include "hk_test.h"
#include "hk_portal_form.h"

#include <string.h>

/** Parse a NUL-terminated literal, the way every call site in a test wants it. */
static bool parse(const char *body, hk_portal_credentials_t *out)
{
    return hk_portal_parse_form(body, strlen(body), out);
}

void test_portal(void)
{
    hk_portal_credentials_t c;

    /* ===== the ordinary case ===== */
    HK_CHECK(parse("ssid=Home&password=secret", &c));
    HK_CHECK(strcmp(c.ssid, "Home") == 0);
    HK_CHECK(strcmp(c.password, "secret") == 0);

    /* Field order is the browser's business, not ours. */
    HK_CHECK(parse("password=secret&ssid=Home", &c));
    HK_CHECK(strcmp(c.ssid, "Home") == 0);

    /* ===== an open network has no password, and that is not an error ===== */
    HK_CHECK(parse("ssid=Cafe", &c));
    HK_CHECK(strcmp(c.ssid, "Cafe") == 0);
    HK_CHECK(c.password[0] == '\0');
    HK_CHECK(parse("ssid=Cafe&password=", &c));
    HK_CHECK(c.password[0] == '\0');

    /* ===== escaping ===== */
    HK_CHECK(parse("ssid=My+Net&password=a%20b", &c));
    HK_CHECK(strcmp(c.ssid, "My Net") == 0);
    HK_CHECK(strcmp(c.password, "a b") == 0);

    /* An SSID is bytes, not text. Non-ASCII must survive byte for byte. */
    HK_CHECK(parse("ssid=Ka%C4%9Fan&password=x", &c));
    HK_CHECK(strcmp(c.ssid, "Ka\xC4\x9F" "an") == 0);

    /* Characters that would otherwise end a field must survive escaping. */
    HK_CHECK(parse("ssid=a%26b&password=p%3Dq", &c));
    HK_CHECK(strcmp(c.ssid, "a&b") == 0);
    HK_CHECK(strcmp(c.password, "p=q") == 0);

    /* Lower and upper case hex are the same escape. */
    HK_CHECK(parse("ssid=%2f&password=%2F", &c));
    HK_CHECK(strcmp(c.ssid, "/") == 0);
    HK_CHECK(strcmp(c.password, "/") == 0);

    /* ===== malformed escaping is refused, not repaired ===== */
    HK_CHECK(!parse("ssid=%zz", &c));
    HK_CHECK(!parse("ssid=%4", &c));
    HK_CHECK(!parse("ssid=abc%", &c));
    /* A NUL would end the string early for the next reader, so the two halves
     * would disagree about what was sent. */
    HK_CHECK(!parse("ssid=a%00b", &c));

    /* ===== a rejection must not leave a usable fragment ===== */
    HK_CHECK(!parse("ssid=Home&password=%zz", &c));
    HK_CHECK(c.ssid[0] == '\0');
    HK_CHECK(c.password[0] == '\0');

    /* ===== bounds ===== */
    {
        char body[256];
        char ssid[HK_PORTAL_SSID_MAX + 1];
        memset(ssid, 'a', sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';

        /* Exactly at the limit is a legal SSID. */
        snprintf(body, sizeof(body), "ssid=%s", ssid);
        HK_CHECK(parse(body, &c));
        HK_CHECK(strlen(c.ssid) == HK_PORTAL_SSID_MAX);

        /* One over is refused. Truncating would join a different network than
         * the one the user picked, and say nothing about it. */
        snprintf(body, sizeof(body), "ssid=%sa", ssid);
        HK_CHECK(!parse(body, &c));
    }
    {
        char body[256];
        char password[HK_PORTAL_PASSWORD_MAX + 1];
        memset(password, 'p', sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';

        snprintf(body, sizeof(body), "ssid=Home&password=%s", password);
        HK_CHECK(parse(body, &c));
        HK_CHECK(strlen(c.password) == HK_PORTAL_PASSWORD_MAX);

        snprintf(body, sizeof(body), "ssid=Home&password=%sp", password);
        HK_CHECK(!parse(body, &c));
    }

    /* ===== a missing SSID is never a credential ===== */
    HK_CHECK(!parse("", &c));
    HK_CHECK(!parse("password=secret", &c));
    HK_CHECK(!parse("ssid=", &c));
    HK_CHECK(!parse("ssid=&password=x", &c));
    HK_CHECK(!hk_portal_parse_form(NULL, 0, &c));
    HK_CHECK(!hk_portal_parse_form("ssid=Home", 9, NULL));

    /* ===== a repeated key is refused rather than resolved ===== */
    /* Two answers to one question; whichever we picked would be a rule the
     * sender does not necessarily share. */
    HK_CHECK(!parse("ssid=Home&ssid=Other", &c));
    HK_CHECK(!parse("ssid=Home&password=a&password=b", &c));

    /* ===== fields we did not ask for are ignored ===== */
    HK_CHECK(parse("submit=Join&ssid=Home&extra=1", &c));
    HK_CHECK(strcmp(c.ssid, "Home") == 0);
    /* A key that merely starts the same way is not the key. */
    HK_CHECK(!parse("ssid_confirm=Home", &c));
    HK_CHECK(!parse("myssid=Home", &c));

    /* ===== the body is bounded by length, not by a NUL ===== */
    {
        const char raw[] = "ssid=Home&password=secretGARBAGE";
        HK_CHECK(hk_portal_parse_form(raw, 25, &c));
        HK_CHECK(strcmp(c.ssid, "Home") == 0);
        HK_CHECK(strcmp(c.password, "secret") == 0);
    }
}
