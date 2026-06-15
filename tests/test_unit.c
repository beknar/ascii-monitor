/* Unit tests: pure-function correctness of the color/threshold and CPU math
 * logic. No ncurses or /proc access required, so these run anywhere. */
#include <stdio.h>
#include "../include/monitor.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok  : %s\n", msg); } \
    else      { printf("  FAIL: %s\n", msg); failures++; } \
} while (0)

static int approx(double a, double b) {
    double d = a - b;
    if (d < 0) d = -d;
    return d < 1e-6;
}

int main(void) {
    printf("[unit] usage_level thresholds\n");
    CHECK(usage_level(0.0)   == USAGE_LOW,  "0% is LOW");
    CHECK(usage_level(59.9)  == USAGE_LOW,  "59.9% is LOW");
    CHECK(usage_level(60.0)  == USAGE_MED,  "60% is MED (lower boundary)");
    CHECK(usage_level(84.9)  == USAGE_MED,  "84.9% is MED");
    CHECK(usage_level(85.0)  == USAGE_HIGH, "85% is HIGH (lower boundary)");
    CHECK(usage_level(100.0) == USAGE_HIGH, "100% is HIGH");
    CHECK(USAGE_LOW < USAGE_MED && USAGE_MED < USAGE_HIGH, "levels are ordered");

    printf("[unit] calc_cpu_usage\n");
    cpu_times_t zero = {0,0,0,0,0,0,0,0};
    CHECK(approx(calc_cpu_usage(&zero, &zero), 0.0), "no delta -> 0%");

    cpu_times_t busy = {100,0,0,0,0,0,0,0};       /* +100 user, +0 idle */
    CHECK(approx(calc_cpu_usage(&zero, &busy), 100.0), "all-busy delta -> 100%");

    cpu_times_t half = {50,0,0,50,0,0,0,0};       /* user=50 idle=50 */
    CHECK(approx(calc_cpu_usage(&zero, &half), 50.0), "half-busy delta -> 50%");

    cpu_times_t io = {0,0,0,0,100,0,0,0};          /* iowait counts as idle */
    CHECK(approx(calc_cpu_usage(&zero, &io), 0.0), "iowait treated as idle -> 0%");

    cpu_times_t decreasing = {10,0,0,10,0,0,0,0};  /* b < a should not crash */
    CHECK(approx(calc_cpu_usage(&busy, &decreasing), 0.0), "decreasing sample -> 0%");

    CHECK(approx(calc_cpu_usage(NULL, &busy), 0.0), "NULL input -> 0% (no crash)");

    if (failures) { printf("[unit] FAILED (%d)\n", failures); return 1; }
    printf("[unit] PASSED\n");
    return 0;
}
