// System sampling for ascii-monitor.
//
// The monitor.h interface is platform-neutral; the implementations of
// get_meminfo() and get_cpu_times() are necessarily OS-specific because each
// kernel exposes these counters differently:
//
//   Linux   : parse /proc/meminfo and /proc/stat
//   FreeBSD : sysctl (hw.physmem, vm.stats.vm.*, kern.cp_time[s]) — no /proc
//   OpenBSD : sysctl (VM_UVMEXP for memory, KERN_CPTIME/KERN_CPTIME2 for CPU)
//   Solaris : sysconf (_SC_*PHYS_PAGES) for memory, kstat (cpu_stat) for CPU
//
// get_num_cpus(), calc_cpu_usage() and usage_level() are platform-neutral
// (POSIX / pure math), so they're shared above the per-OS sampling code.
//
// cpu_times_t is modeled on Linux's eight /proc/stat fields. Platforms that
// don't split the time that finely fill only the fields they have and leave the
// rest 0 — the UI's usage math (idle+iowait vs. everything else) still works.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/monitor.h"

// Logical CPU count — POSIX, identical on Linux/FreeBSD/Solaris.
int get_num_cpus() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}

// CPU utilization (0..100) between two cumulative samples. Pure math, shared
// across platforms; relies only on the cpu_times_t fields each OS fills in.
double calc_cpu_usage(const cpu_times_t *a, const cpu_times_t *b) {
    if (!a || !b) return 0.0;
    unsigned long long idle_a = a->idle + a->iowait;
    unsigned long long idle_b = b->idle + b->iowait;
    unsigned long long nonidle_a = a->user + a->nice + a->system + a->irq + a->softirq + a->steal;
    unsigned long long nonidle_b = b->user + b->nice + b->system + b->irq + b->softirq + b->steal;
    unsigned long long total_a = idle_a + nonidle_a;
    unsigned long long total_b = idle_b + nonidle_b;
    if (total_b <= total_a) return 0.0;
    unsigned long long totald = total_b - total_a;
    unsigned long long idled = (idle_b > idle_a) ? (idle_b - idle_a) : 0;
    if (totald == 0) return 0.0;
    return (double)(totald - idled) * 100.0 / (double)totald;
}

// Map a utilization percentage to a severity level for color selection.
usage_level_t usage_level(double pct) {
    if (pct >= 85.0) return USAGE_HIGH;
    if (pct >= 60.0) return USAGE_MED;
    return USAGE_LOW;
}

// Filled-cell count for a bar `cells` wide at `pct` percent, rounded to the
// nearest cell and clamped to [0, cells]. Kept here (not in the renderer) so the
// bar math is testable without a terminal.
int bar_fill_cells(double pct, int cells) {
    if (cells <= 0) return 0;
    if (pct <= 0.0) return 0;
    if (pct >= 100.0) return cells;
    int f = (int)((pct / 100.0) * cells + 0.5);  /* pct>0 here, so +0.5 rounds */
    if (f < 0) f = 0;
    if (f > cells) f = cells;
    return f;
}

// ===========================================================================
#if defined(__linux__)
// ---------------------------------------------------------------------------
// Linux: /proc/meminfo + /proc/stat
// ---------------------------------------------------------------------------

int get_meminfo(meminfo_t *m) {
    if (!m) return -1;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if (sscanf(line, "MemTotal: %lu kB", &m->mem_total_kb) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", &m->mem_available_kb) == 1) continue;
    }
    free(line);
    fclose(f);
    if (m->mem_total_kb == 0) return -1;
    return 0;
}

int get_cpu_times(int cpu, cpu_times_t *t) {
    if (!t) return -1;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char buf[512];
    int found = 0;
    char target[32];
    if (cpu == -1) {
        strcpy(target, "cpu");
    } else {
        snprintf(target, sizeof(target), "cpu%d", cpu);
    }
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, target, strlen(target)) == 0 && (buf[strlen(target)] == ' ' || buf[strlen(target)] == '\t')) {
            // parse fields
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            int rc = sscanf(buf + strlen(target), "%llu %llu %llu %llu %llu %llu %llu %llu",
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            if (rc >= 4) {
                t->user = user; t->nice = nice; t->system = system; t->idle = idle;
                t->iowait = (rc >=5) ? iowait : 0;
                t->irq = (rc >=6) ? irq : 0;
                t->softirq = (rc >=7) ? softirq : 0;
                t->steal = (rc >=8) ? steal : 0;
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found ? 0 : -1;
}

// ===========================================================================
#elif defined(__FreeBSD__)
// ---------------------------------------------------------------------------
// FreeBSD: sysctl. There is no /proc/meminfo or /proc/stat.
// ---------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>   // CPUSTATES, CP_USER..CP_IDLE

static int sysctl_uint(const char *name, unsigned int *out) {
    size_t len = sizeof(*out);
    return sysctlbyname(name, out, &len, NULL, 0);
}

int get_meminfo(meminfo_t *m) {
    if (!m) return -1;

    unsigned long physmem = 0;
    size_t len = sizeof(physmem);
    if (sysctlbyname("hw.physmem", &physmem, &len, NULL, 0) != 0) return -1;

    int pagesize = 0;
    len = sizeof(pagesize);
    if (sysctlbyname("hw.pagesize", &pagesize, &len, NULL, 0) != 0 || pagesize <= 0)
        return -1;

    // "Available" ~= free + inactive + cache pages (cache may be absent on
    // newer FreeBSD, in which case the sysctl just stays 0 — fine).
    unsigned int free_c = 0, inact_c = 0, cache_c = 0;
    sysctl_uint("vm.stats.vm.v_free_count", &free_c);
    sysctl_uint("vm.stats.vm.v_inactive_count", &inact_c);
    sysctl_uint("vm.stats.vm.v_cache_count", &cache_c);

    m->mem_total_kb = physmem / 1024UL;
    unsigned long avail_bytes =
        ((unsigned long)free_c + inact_c + cache_c) * (unsigned long)pagesize;
    m->mem_available_kb = avail_bytes / 1024UL;
    if (m->mem_total_kb == 0) return -1;
    return 0;
}

int get_cpu_times(int cpu, cpu_times_t *t) {
    if (!t) return -1;

    long c[CPUSTATES];
    if (cpu == -1) {
        // Aggregate across all CPUs: kern.cp_time is one CPUSTATES array.
        size_t len = sizeof(c);
        if (sysctlbyname("kern.cp_time", c, &len, NULL, 0) != 0) return -1;
    } else {
        // Per-CPU: kern.cp_times is ncpu * CPUSTATES longs.
        int ncpu = get_num_cpus();
        if (cpu < 0 || cpu >= ncpu) return -1;
        size_t len = 0;
        if (sysctlbyname("kern.cp_times", NULL, &len, NULL, 0) != 0) return -1;
        long *all = (long *)malloc(len);
        if (!all) return -1;
        if (sysctlbyname("kern.cp_times", all, &len, NULL, 0) != 0 ||
            (size_t)(cpu + 1) * CPUSTATES * sizeof(long) > len) {
            free(all);
            return -1;
        }
        for (int i = 0; i < CPUSTATES; i++) c[i] = all[cpu * CPUSTATES + i];
        free(all);
    }

    // FreeBSD states: user, nice, sys, intr, idle (no iowait/softirq/steal).
    t->user = c[CP_USER];
    t->nice = c[CP_NICE];
    t->system = c[CP_SYS];
    t->idle = c[CP_IDLE];
    t->iowait = 0;
    t->irq = c[CP_INTR];
    t->softirq = 0;
    t->steal = 0;
    return 0;
}

// ===========================================================================
#elif defined(__sun)
// ---------------------------------------------------------------------------
// Solaris (illumos): sysconf for memory, kstat (cpu_stat) for CPU.
// Solaris /proc is the *process* filesystem — no /proc/meminfo or /proc/stat.
// Build needs -lkstat.
// ---------------------------------------------------------------------------
#include <kstat.h>
#include <sys/sysinfo.h>    // cpu_stat_t, CPU_IDLE/USER/KERNEL/WAIT, CPU_STATES

int get_meminfo(meminfo_t *m) {
    if (!m) return -1;
    long pages = sysconf(_SC_PHYS_PAGES);
    long avail = sysconf(_SC_AVPHYS_PAGES);
    long pagesz = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || pagesz <= 0) return -1;
    m->mem_total_kb = ((unsigned long long)pages * (unsigned long long)pagesz) / 1024ULL;
    m->mem_available_kb = (avail > 0)
        ? ((unsigned long long)avail * (unsigned long long)pagesz) / 1024ULL
        : 0;
    return 0;
}

int get_cpu_times(int cpu, cpu_times_t *t) {
    if (!t) return -1;
    kstat_ctl_t *kc = kstat_open();
    if (!kc) return -1;

    unsigned long long user = 0, kern = 0, wait = 0, idle = 0;
    int found = 0;
    for (kstat_t *ks = kc->kc_chain; ks != NULL; ks = ks->ks_next) {
        if (strcmp(ks->ks_module, "cpu_stat") != 0) continue;
        if (cpu != -1 && ks->ks_instance != cpu) continue;
        if (kstat_read(kc, ks, NULL) == -1) continue;
        cpu_stat_t *cs = (cpu_stat_t *)ks->ks_data;
        user += cs->cpu_sysinfo.cpu[CPU_USER];
        kern += cs->cpu_sysinfo.cpu[CPU_KERNEL];
        wait += cs->cpu_sysinfo.cpu[CPU_WAIT];
        idle += cs->cpu_sysinfo.cpu[CPU_IDLE];
        found = 1;
        if (cpu != -1) break;          // a specific CPU: done
    }
    kstat_close(kc);
    if (!found) return -1;

    // Solaris folds nice into user and has no irq/softirq/steal accounting.
    t->user = user;
    t->nice = 0;
    t->system = kern;
    t->idle = idle;
    t->iowait = wait;
    t->irq = 0;
    t->softirq = 0;
    t->steal = 0;
    return 0;
}

// ===========================================================================
#elif defined(__OpenBSD__)
// ---------------------------------------------------------------------------
// OpenBSD: sysctl. Memory from the uvm page stats (VM_UVMEXP); CPU times from
// KERN_CPTIME (aggregate) / KERN_CPTIME2 (per-CPU). No /proc, no kstat.
// ---------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/sched.h>     // CPUSTATES, CP_USER..CP_IDLE
#include <uvm/uvmexp.h>    // struct uvmexp
#include <stdint.h>

int get_meminfo(meminfo_t *m) {
    if (!m) return -1;
    struct uvmexp uvm;
    int mib[2] = { CTL_VM, VM_UVMEXP };
    size_t len = sizeof(uvm);
    if (sysctl(mib, 2, &uvm, &len, NULL, 0) == -1) return -1;
    unsigned long long pg = (unsigned long long)uvm.pagesize;
    // Total = all managed physical pages; "available" ~= free + inactive
    // (reclaimable), mirroring the FreeBSD estimate (OpenBSD has no separate
    // cache queue), so the memory bar is an estimate, not an exact figure.
    m->mem_total_kb = ((unsigned long long)uvm.npages * pg) / 1024ULL;
    unsigned long long avail =
        ((unsigned long long)uvm.free + (unsigned long long)uvm.inactive) * pg;
    m->mem_available_kb = avail / 1024ULL;
    if (m->mem_total_kb == 0) return -1;
    return 0;
}

int get_cpu_times(int cpu, cpu_times_t *t) {
    if (!t) return -1;
    uint64_t c[CPUSTATES];
    if (cpu == -1) {
        // Aggregate across all CPUs (long[CPUSTATES]).
        long cp[CPUSTATES];
        int mib[2] = { CTL_KERN, KERN_CPTIME };
        size_t len = sizeof(cp);
        if (sysctl(mib, 2, cp, &len, NULL, 0) == -1) return -1;
        for (int i = 0; i < CPUSTATES; i++) c[i] = (uint64_t)cp[i];
    } else {
        // Per-CPU (uint64_t[CPUSTATES]); the CPU index is the 3rd mib element.
        int mib[3] = { CTL_KERN, KERN_CPTIME2, cpu };
        size_t len = sizeof(c);
        if (sysctl(mib, 3, c, &len, NULL, 0) == -1) return -1;
    }
    // OpenBSD states: user, nice, sys, intr, idle (no iowait/softirq/steal).
    t->user = c[CP_USER];
    t->nice = c[CP_NICE];
    t->system = c[CP_SYS];
    t->idle = c[CP_IDLE];
    t->iowait = 0;
    t->irq = c[CP_INTR];
    t->softirq = 0;
    t->steal = 0;
    return 0;
}

// ===========================================================================
#else
#error "ascii-monitor: unsupported platform (need Linux, FreeBSD, OpenBSD, or Solaris)"
#endif
