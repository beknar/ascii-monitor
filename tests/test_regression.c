/* Regression tests: guarantee the behavior that existed before the color
 * feature is preserved.
 *
 *  1. calc_cpu_usage must match, bit-for-bit, the original inline formula
 *     that lived in src/main.cpp before it was extracted into monitor.c.
 *  2. The /proc parsing API (get_meminfo, get_num_cpus, get_cpu_times)
 *     must keep working as before.
 */
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
    return d < 1e-9;
}

/* Verbatim copy of the original calc_cpu_usage from main.cpp (pre-color),
 * used as the golden reference. */
static double ref_calc(cpu_times_t a, cpu_times_t b) {
    unsigned long long idle_a = a.idle + a.iowait;
    unsigned long long idle_b = b.idle + b.iowait;
    unsigned long long nonidle_a = a.user + a.nice + a.system + a.irq + a.softirq + a.steal;
    unsigned long long nonidle_b = b.user + b.nice + b.system + b.irq + b.softirq + b.steal;
    unsigned long long total_a = idle_a + nonidle_a;
    unsigned long long total_b = idle_b + nonidle_b;
    unsigned long long totald = total_b - total_a;
    unsigned long long idled = idle_b - idle_a;
    if (totald == 0) return 0.0;
    return (double)(totald - idled) * 100.0 / (double)totald;
}

int main(void) {
    printf("[regression] calc_cpu_usage matches original formula\n");
    /* Monotonically increasing (valid) sample pairs. */
    cpu_times_t samples[][2] = {
        {{100,10,20,300,5,1,2,0}, {150,15,30,380,10,2,4,0}},
        {{0,0,0,0,0,0,0,0},       {73,4,11,250,9,0,3,1}},
        {{1000,50,200,5000,40,5,8,2}, {1200,80,260,5400,55,7,12,3}},
        {{42,0,0,42,0,0,0,0},     {84,0,0,42,0,0,0,0}},
    };
    int n = (int)(sizeof(samples) / sizeof(samples[0]));
    for (int i = 0; i < n; i++) {
        double got = calc_cpu_usage(&samples[i][0], &samples[i][1]);
        double ref = ref_calc(samples[i][0], samples[i][1]);
        char msg[96];
        snprintf(msg, sizeof(msg), "sample %d matches reference (%.6f)", i, ref);
        CHECK(approx(got, ref), msg);
    }

    printf("[regression] /proc sampling API still works\n");
    int ncpus = get_num_cpus();
    CHECK(ncpus >= 1, "get_num_cpus >= 1");

    meminfo_t m = {0,0};
    int rc = get_meminfo(&m);
    CHECK(rc == 0, "get_meminfo returns 0");
    CHECK(m.mem_total_kb > 0, "MemTotal > 0");
    CHECK(m.mem_available_kb <= m.mem_total_kb, "MemAvailable <= MemTotal");

    cpu_times_t agg = {0,0,0,0,0,0,0,0};
    CHECK(get_cpu_times(-1, &agg) == 0, "aggregate cpu (-1) parses");
    cpu_times_t c0 = {0,0,0,0,0,0,0,0};
    CHECK(get_cpu_times(0, &c0) == 0, "cpu0 parses");

    printf("[regression] get_disks discovers filesystems\n");
    diskinfo_t disks[32];
    int nd = get_disks(disks, 32);
    /* >= 0 (no error); most hosts have >=1, but a thin jail may legitimately
       enumerate none, so don't require >=1. */
    CHECK(nd >= 0, "get_disks does not error");
    int disks_valid = 1;
    for (int i = 0; i < nd; i++) {
        if (disks[i].mount[0] == '\0') disks_valid = 0;
        if (disks[i].used_pct < 0.0 || disks[i].used_pct > 100.0) disks_valid = 0;
    }
    CHECK(disks_valid, "every discovered disk has a mount and used_pct in [0,100]");
    { char msg[64]; snprintf(msg, sizeof(msg), "(discovered %d filesystem(s))", nd);
      printf("  note: %s\n", msg); }

    if (failures) { printf("[regression] FAILED (%d)\n", failures); return 1; }
    printf("[regression] PASSED\n");
    return 0;
}
