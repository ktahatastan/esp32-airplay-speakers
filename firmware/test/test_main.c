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
    printf("%d checks, %d failures\n", hk_test_checks, hk_test_failures);
    return hk_test_failures == 0 ? 0 : 1;
}
