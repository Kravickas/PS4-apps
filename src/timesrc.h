/* Time sources for the clock: the scene's own time of day (in-game: sun_angle 0 = 06:00 sunrise,
   pi/2 = 12:00, pi = 18:00), the console clock (sceRtcGetCurrentTick, UTC) and internet time
   (SNTP, RFC 4330, on its own thread; until an answer arrives, or when every server fails, the
   console clock stands in). Local time: the console's time zone setting plus its daylight saving
   switch (ts_tz_refresh); 12 / 24 h follows the console's setting.
   Network (libSceNet, the OpenOrbis sample's sequence): the NET sysmodule, sceNetInit, a memory
   pool for the resolver; constants of the PS4 socket layer as shadPS4 implements them (AF_INET 2,
   SOCK_DGRAM 2, SOL_SOCKET 0xFFFF, SO_RCVTIMEO 0x1106 in microseconds, sockaddr_in with sin_len
   and sin_vport). The thread starts the first time internet time is selected. */
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t tick; /* microseconds since 0001-01-01 00:00 */
} TsRtcTick;
extern int sceRtcGetCurrentTick(TsRtcTick* tick);
extern int sceRtcConvertUtcToLocalTime(TsRtcTick* utc, TsRtcTick* local);
typedef struct {
    uint16_t year, month, day, hour, minute, second;
    uint32_t microsecond;
} TsRtcDateTime;
typedef struct {
    int64_t t;
    uint32_t west_sec, dst_sec;
} TsTimesec; /* OrbisTimesec */
extern int sceRtcGetCurrentClockLocalTime(TsRtcDateTime* t);
extern int sceRtcGetTick(const TsRtcDateTime* t, TsRtcTick* tick);
extern int sceKernelConvertUtcToLocaltime(int64_t utc, int64_t* local, TsTimesec* st,
                                          uint64_t* dst_sec);
extern int sceSystemServiceParamGetInt(int32_t id, int32_t* value);
extern int sceSysmoduleLoadModuleInternal(uint32_t id);
extern int sceNetInit(void);
extern int sceNetPoolCreate(const char* name, int size, int flags);
extern int sceNetResolverCreate(const char* name, int memid, int flags);
extern int sceNetResolverStartNtoa(int rid, const char* host, uint32_t* addr, int timeout,
                                   int retry, int flags);
extern int sceNetResolverDestroy(int rid);
extern int sceNetSocket(const char* name, int family, int type, int protocol);
extern int sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen);
extern int sceNetSendto(int s, const void* buf, unsigned long len, int flags, const void* addr,
                        uint32_t addrlen);
extern int sceNetRecvfrom(int s, void* buf, unsigned long len, int flags, void* addr,
                          uint32_t* addrlen);
extern int sceNetSocketClose(int s);

#define TS_SYSMODULE_INTERNAL_NET 0x8000001Cu
#define TS_PARAM_TIME_FORMAT 3 /* 0: 12 h, 1: 24 h */
#define TS_PARAM_TIME_ZONE 4
#define TS_PARAM_SUMMERTIME 5    /* the daylight saving switch: 0 / 1 */
#define TS_TZ_EVERY_US 60000000u /* re-read the time zone every minute */
#define TS_TZ_BAD (-99999)
#define TS_UNIX_EPOCH_TICK 62135596800000000ULL
#define TS_NTP_EPOCH_TICK (TS_UNIX_EPOCH_TICK - 2208988800ULL * 1000000ULL) /* 1900-01-01 */
#define TS_DAY_US 86400000000LL
#define TS_NTP_EVERY_US 1800000000u /* re-sync every 30 min */
#define TS_NTP_RETRY_US 30000000u   /* after a failed round of every server: 30 s */
#define TS_NTP_TIMEOUT_US 2000000   /* one server's answer */
#define TS_SLEW_US_PER_S 50000.0    /* corrections under TS_STEP_US: 50 ms per second */
#define TS_STEP_US 2000000LL        /* larger ones (or the first answer) step at once */

enum { TS_INGAME = 0, TS_CONSOLE = 1, TS_NET = 2 };

typedef struct {
    int source;          /* TS_INGAME / TS_CONSOLE / TS_NET */
    int h12;             /* the console shows 12-hour time */
    int net_started;     /* the thread was started */
    volatile int net_ok; /* an answer arrived: net_off is valid (release / acquire) */
    int64_t net_off;     /* UTC tick - process time (us), written by the thread */
    int shown_ok;        /* shown_off is initialised */
    int64_t shown_off;   /* what the clock uses: slews toward net_off */
    uint64_t last_us;    /* process time of the previous ts_utc call */
    int tz_min;          /* local time - UTC (minutes) */
    int tz_src;          /* where it came from: 4 settings, 3 kernel, 2 RTC local clock, 1 RTC */
    uint64_t tz_next_us; /* process time of the next re-read */
} TimeSrc;
static TimeSrc g_ts;

/* The console's time zone setting in minutes from its raw value, whatever its unit: every real zone
   is 0 or a multiple of 15 min within +-14 h, so 1..14 can only be hours, a multiple of 15 up to
   840 minutes, a multiple of 900 up to 50400 seconds. TS_TZ_BAD: none of these. */
static int ts_tz_decode(int v) {
    int a = v < 0 ? -v : v;
    if (a == 0)
        return 0;
    if (a <= 14)
        return v * 60;
    if (a <= 840 && a % 15 == 0)
        return v;
    if (a <= 50400 && a % 900 == 0)
        return v / 60;
    return TS_TZ_BAD;
}
static long long ts_round_min(long long us) {
    return us >= 0 ? (us + 30000000LL) / 60000000LL : -((-us + 30000000LL) / 60000000LL);
}
/* Local time - UTC, from the console's own settings: its time zone plus one hour when its daylight
   saving switch is on (the PS4's Date and Time settings; the previous build's libSceRtc conversion
   gave +0 on hardware). The kernel's and libSceRtc's conversions are read too: fallbacks, and a
   "tz" trace line (return codes, offsets, the raw settings) whenever the result changes. */
static void ts_tz_refresh(void) {
    TsRtcTick u, l, lb = {0}, u2;
    sceRtcGetCurrentTick(&u);
    l.tick = u.tick;
    int ra = sceRtcConvertUtcToLocalTime(&u, &l);
    long long oa = ts_round_min((long long)l.tick - (long long)u.tick);
    TsRtcDateTime dt;
    int rb = sceRtcGetCurrentClockLocalTime(&dt);
    if (rb == 0)
        rb = sceRtcGetTick(&dt, &lb);
    sceRtcGetCurrentTick(&u2);
    long long ob = rb == 0 ? ts_round_min((long long)lb.tick - (long long)u2.tick) : 0;
    int64_t us = (int64_t)((u.tick - TS_UNIX_EPOCH_TICK) / 1000000ULL), loc = 0;
    TsTimesec st = {0, 0, 0};
    uint64_t dsts = 0;
    int rc = sceKernelConvertUtcToLocaltime(us, &loc, &st, &dsts);
    long long oc = (loc - us) / 60;
    int32_t tz = 0, sm = 0;
    int rd = sceSystemServiceParamGetInt(TS_PARAM_TIME_ZONE, &tz);
    int rs = sceSystemServiceParamGetInt(TS_PARAM_SUMMERTIME, &sm);
    int od = ts_tz_decode(tz), min = 0, src = 0;
    if (rd == 0 && rs == 0 && od != TS_TZ_BAD) {
        min = od + (sm ? 60 : 0);
        src = 4;
    } else if (rc == 0) {
        min = (int)oc;
        src = 3;
    } else if (rb == 0) {
        min = (int)ob;
        src = 2;
    } else if (ra == 0) {
        min = (int)oa;
        src = 1;
    }
    if (min != g_ts.tz_min || src != g_ts.tz_src || g_ts.tz_next_us == 0) {
        static const char* const k[13] = {
            "tz used=",   " src=",         " settings_ret=", " zone_raw=",       " dst_ret=",
            " dst=",      " kernel_ret=",  " kernel=",       " kernel_dst_sec=", " rtclocal_ret=",
            " rtclocal=", " rtcconv_ret=", " rtcconv="};
        long long v[13] = {min, src, rd, tz, rs, sm, rc, oc, (long long)st.dst_sec, rb, ob, ra, oa};
        char L[320];
        int p = 0;
        for (int i = 0; i < 13; i++) {
            for (const char* q = k[i]; *q; q++)
                L[p++] = *q;
            p += lg_i64(L + p, v[i]);
        }
        L[p++] = '\n';
        trace_line(L, (unsigned long)p);
    }
    g_ts.tz_min = min;
    g_ts.tz_src = src;
    g_ts.tz_next_us = sceKernelGetProcessTime() + TS_TZ_EVERY_US;
}

static void ts_init(void) {
    int32_t v = 1;
    g_ts.source = TS_INGAME;
    g_ts.h12 = (sceSystemServiceParamGetInt(TS_PARAM_TIME_FORMAT, &v) == 0 && v == 0);
    ts_tz_refresh();
}
/* local tick = UTC tick + the time zone (re-read every minute) */
static uint64_t ts_local_tick(uint64_t utc) {
    if (sceKernelGetProcessTime() >= g_ts.tz_next_us)
        ts_tz_refresh();
    return (uint64_t)((int64_t)utc + (int64_t)g_ts.tz_min * 60000000LL);
}

/* NTP 32.32 fixed point (big-endian) <-> microseconds since 1900 */
static uint64_t ts_ntp_us(const unsigned char* p) {
    uint64_t s = ((uint64_t)p[0] << 24) | ((uint64_t)p[1] << 16) | ((uint64_t)p[2] << 8) | p[3];
    uint64_t f = ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
    return s * 1000000ULL + ((f * 1000000ULL) >> 32);
}
static void ts_put_ntp(unsigned char* p, uint64_t us) {
    uint64_t s = us / 1000000ULL, f = ((us % 1000000ULL) << 32) / 1000000ULL;
    for (int i = 0; i < 4; i++) {
        p[i] = (unsigned char)(s >> (24 - 8 * i));
        p[4 + i] = (unsigned char)(f >> (24 - 8 * i));
    }
}

/* One SNTP exchange with host: 0 and *off (UTC tick - process time) on success. */
static int ts_ntp_query(int rid, const char* host, int64_t* off) {
    uint32_t addr = 0;
    if (sceNetResolverStartNtoa(rid, host, &addr, 0, 0, 0) < 0 || addr == 0)
        return -1;
    int s = sceNetSocket("ntp", 2, 2, 0);
    if (s < 0)
        return -2;
    int tmo = TS_NTP_TIMEOUT_US;
    sceNetSetsockopt(s, 0xFFFF, 0x1106, &tmo, sizeof(tmo));
    unsigned char sa[16] = {16, 2, 0, 123}; /* sin_len, AF_INET, port 123 (big-endian) */
    for (int i = 0; i < 4; i++)
        sa[4 + i] = ((const unsigned char*)&addr)[i]; /* network order from the resolver */
    unsigned char q[48] = {0x23};                     /* LI 0, version 4, mode 3 (client) */
    TsRtcTick now;
    sceRtcGetCurrentTick(&now);
    uint64_t cookie = now.tick - TS_NTP_EPOCH_TICK; /* our transmit time: echoed as originate */
    ts_put_ntp(q + 40, cookie);
    unsigned char r[48] = {0};
    int ret = -3;
    uint64_t t1 = sceKernelGetProcessTime();
    if (sceNetSendto(s, q, sizeof(q), 0, sa, sizeof(sa)) == (int)sizeof(q)) {
        uint32_t al = sizeof(sa);
        int n = sceNetRecvfrom(s, r, sizeof(r), 0, sa, &al);
        uint64_t t4 = sceKernelGetProcessTime();
        int cookie_ok = 1;
        for (int i = 0; i < 8; i++)
            cookie_ok &= r[24 + i] == q[40 + i];
        if (n >= 48 && (r[0] & 7) == 4 && (r[0] >> 6) != 3 && r[1] >= 1 && r[1] <= 15 &&
            cookie_ok) {
            uint64_t t2 = ts_ntp_us(r + 32), t3 = ts_ntp_us(r + 40);
            /* the server's time at t4: its transmit time + half the path (round trip minus its
               own hold time) */
            int64_t rtt = (int64_t)(t4 - t1) - (int64_t)(t3 - t2);
            if (t3 != 0 && rtt >= 0) {
                *off = (int64_t)(TS_NTP_EPOCH_TICK + t3) + rtt / 2 - (int64_t)t4;
                ret = 0;
            } else
                ret = -5;
        } else
            ret = -4;
    }
    sceNetSocketClose(s);
    return ret;
}

static void* ts_net_thread(void* arg) {
    (void)arg;
    static const char* const hosts[3] = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    sceSysmoduleLoadModuleInternal(TS_SYSMODULE_INTERNAL_NET);
    sceNetInit(); /* already initialised: an error we can ignore */
    int pool = sceNetPoolCreate("ts_net", 16 * 1024, 0);
    int rid = pool >= 0 ? sceNetResolverCreate("ts_res", pool, 0) : -1;
    for (int h = 0;;) {
        int ok = 0;
        for (int i = 0; i < 3 && !ok && rid >= 0; i++, h = (h + 1) % 3) {
            int64_t off;
            if (ts_ntp_query(rid, hosts[h], &off) == 0) {
                __atomic_store_n(&g_ts.net_off, off, __ATOMIC_RELAXED);
                __atomic_store_n(&g_ts.net_ok, 1, __ATOMIC_RELEASE);
                ok = 1;
            }
        }
        sceKernelUsleep(ok ? TS_NTP_EVERY_US : TS_NTP_RETRY_US);
    }
    return 0;
}

static void ts_set_source(int src) {
    g_ts.source = src;
    if (src == TS_NET && !g_ts.net_started) {
        void* thr = 0;
        g_ts.net_started = scePthreadCreate(&thr, 0, ts_net_thread, 0, "ts_net") == 0;
    }
}

/* Internet time is in use (an answer arrived): the dot is green; otherwise the console stands in.
 */
static int ts_net_valid(void) {
    return __atomic_load_n(&g_ts.net_ok, __ATOMIC_ACQUIRE);
}

/* The current UTC tick of the selected clock (console or internet), smooth frame to frame. */
static uint64_t ts_utc(void) {
    uint64_t us = sceKernelGetProcessTime();
    double dt = g_ts.last_us ? (double)(us - g_ts.last_us) * 1e-6 : 0.0;
    g_ts.last_us = us;
    if (g_ts.source == TS_NET && ts_net_valid()) {
        int64_t target = __atomic_load_n(&g_ts.net_off, __ATOMIC_RELAXED),
                d = target - g_ts.shown_off;
        if (!g_ts.shown_ok || d > TS_STEP_US || d < -TS_STEP_US) {
            g_ts.shown_off = target;
            g_ts.shown_ok = 1;
        } else { /* slew: no visible jump in the clock or the sun */
            int64_t m = (int64_t)(TS_SLEW_US_PER_S * dt);
            g_ts.shown_off += d > m ? m : d < -m ? -m : d;
        }
        return (uint64_t)((int64_t)us + g_ts.shown_off);
    }
    TsRtcTick t;
    sceRtcGetCurrentTick(&t);
    return t.tick;
}

/* Seconds since local midnight (0 .. 86400) of the selected clock; in-game: from sun_angle. */
static double ts_local_seconds(float sun_angle) {
    if (g_ts.source == TS_INGAME) {
        double s = 21600.0 + (double)sun_angle * (86400.0 / 6.283185307179586);
        return s >= 86400.0 ? s - 86400.0 : s;
    }
    uint64_t l = ts_local_tick(ts_utc());
    return (double)(int64_t)(l % (uint64_t)TS_DAY_US) * 1e-6;
}

/* Days since 0001-01-01 of the local date (the selected clock; in-game: the console's). */
static long ts_local_days(void) {
    TsRtcTick u;
    if (g_ts.source == TS_INGAME)
        sceRtcGetCurrentTick(&u);
    else
        u.tick = ts_utc();
    return (long)(ts_local_tick(u.tick) / (uint64_t)TS_DAY_US);
}

/* "hh:mm:ss" (24 h) or "h:mm:ss AM" (12 h) into p (>= 12 bytes). */
static void ts_format(char* p, double sec) {
    int s = (int)sec, h = s / 3600, m = s / 60 % 60, ss = s % 60, k = 0;
    const char* ap = 0;
    if (g_ts.h12) {
        ap = h < 12 ? "AM" : "PM";
        h = h % 12 == 0 ? 12 : h % 12;
        if (h >= 10)
            p[k++] = (char)('0' + h / 10);
    } else
        p[k++] = (char)('0' + h / 10);
    p[k++] = (char)('0' + h % 10);
    p[k++] = ':';
    p[k++] = (char)('0' + m / 10);
    p[k++] = (char)('0' + m % 10);
    p[k++] = ':';
    p[k++] = (char)('0' + ss / 10);
    p[k++] = (char)('0' + ss % 10);
    if (ap) {
        p[k++] = ' ';
        p[k++] = ap[0];
        p[k++] = ap[1];
    }
    p[k] = 0;
}
