#include "hk_test.h"

int hk_test_failures = 0;
int hk_test_checks = 0;

int main(void)
{
    printf("Harman Kardom host tests\n");
    HK_RUN(test_pins);
    HK_RUN(test_identity);
    HK_RUN(test_version);
    HK_RUN(test_button);
    HK_RUN(test_led);
    HK_RUN(test_provision);
    HK_RUN(test_schema);
    HK_RUN(test_manifest);
    HK_RUN(test_gate);
    HK_RUN(test_ota);
    HK_RUN(test_health);
    HK_RUN(test_power);
    HK_RUN(test_audio);
    HK_RUN(test_limiter);
    HK_RUN(test_biquad);
    printf("%d checks, %d failures\n", hk_test_checks, hk_test_failures);
    return hk_test_failures == 0 ? 0 : 1;
}
