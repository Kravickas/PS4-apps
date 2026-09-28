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
extern int sceNetConnect(int s, const void* addr, uint32_t addrlen);
extern int sceNetSend(int s, const void* buf, unsigned long len, int flags);
extern int sceNetRecv(int s, void* buf, unsigned long len, int flags);

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
    int source;             /* TS_INGAME / TS_CONSOLE / TS_NET */
    int h12;                /* the console shows 12-hour time */
    int net_started;        /* the thread was started */
    volatile int net_ok;    /* an answer arrived: net_off is valid (release / acquire) */
    int64_t net_off;        /* UTC tick - process time (us), written by the thread */
    int shown_ok;           /* shown_off is initialised */
    int64_t shown_off;      /* what the clock uses: slews toward net_off */
    uint64_t last_us;       /* process time of the previous ts_utc call */
    int tz_min;             /* local time - UTC (minutes): the console's */
    volatile int net_tz_ok; /* internet time's own zone: from ip-api.com (the thread) */
    int net_tz_min;         /* its offset (minutes) */
    char net_tz_name[48];   /* its IANA name, for the trace */
    volatile int tz_http_ret[2], tz_http_tries; /* the lookups' last results (ts_http_tz) */
    int utc_is_net;      /* the last ts_utc() was internet time (ts_local_tick uses the same) */
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
/* Internet time's zone when ip-api.com has not answered: the offset of the console's displayed
   clock (its UTC + its zone) from internet UTC, to the nearest 15 min - the zone its owner reads.
   TS_TZ_BAD when not a real zone. */
static int ts_display_tz(uint64_t net_utc) {
    TsRtcTick c;
    sceRtcGetCurrentTick(&c);
    long long d = (long long)c.tick + (long long)g_ts.tz_min * 60000000LL - (long long)net_utc;
    long long q = d >= 0 ? (d + 450000000LL) / 900000000LL : -((-d + 450000000LL) / 900000000LL);
    return (q >= -56 && q <= 56) ? (int)(q * 15) : TS_TZ_BAD;
}
/* local tick = UTC tick + the zone: internet time its own (ip-api.com, else the console's displayed
   clock), the console its setting (re-read every minute) */
static int ts_net_valid(void);
static int g_ts_net_tz_used = TS_TZ_BAD, g_ts_net_tz_src = 0;
static uint64_t ts_local_tick(uint64_t utc) {
    if (sceKernelGetProcessTime() >= g_ts.tz_next_us)
        ts_tz_refresh();
    int m = g_ts.tz_min;
    static int tr[3] = {1, 1, -1}; /* trace the lookups' results when they change */
    int r0 = __atomic_load_n(&g_ts.tz_http_ret[0], __ATOMIC_RELAXED),
        r1 = __atomic_load_n(&g_ts.tz_http_ret[1], __ATOMIC_RELAXED),
        nt = __atomic_load_n(&g_ts.tz_http_tries, __ATOMIC_ACQUIRE);
    if (nt != tr[2] && nt > 0) {
        static const char* const k[3] = {"tz_http ipapi=", " worldtimeapi=", " tries="};
        long long v[3] = {r0, r1, nt};
        char L[96];
        int p = 0;
        for (int i = 0; i < 3; i++) {
            for (const char* q = k[i]; *q; q++)
                L[p++] = *q;
            p += lg_i64(L + p, v[i]);
        }
        L[p++] = '\n';
        if (r0 != tr[0] || r1 != tr[1] || nt <= 3) /* not every retry: when the results change */
            trace_line(L, (unsigned long)p);
        tr[0] = r0;
        tr[1] = r1;
        tr[2] = nt;
    }
    /* the clock ts_utc just returned (internet only once it has an answer) */
    if (g_ts.source == TS_NET && g_ts.utc_is_net) {
        int src = 3, dm = TS_TZ_BAD;
        if (__atomic_load_n(&g_ts.net_tz_ok, __ATOMIC_ACQUIRE)) {
            m = __atomic_load_n(&g_ts.net_tz_min, __ATOMIC_RELAXED);
            src = 1;
        } else if ((dm = ts_display_tz(utc)) != TS_TZ_BAD) {
            m = dm;
            src = 2;
        }
        if (m != g_ts_net_tz_used || src != g_ts_net_tz_src) { /* trace: "tz_net" */
            static const char* const k[3] = {"tz_net used=", " src=", " console_tz="};
            long long v[3] = {m, src, g_ts.tz_min};
            char L[160];
            int p = 0;
            for (int i = 0; i < 3; i++) {
                for (const char* q = k[i]; *q; q++)
                    L[p++] = *q;
                p += lg_i64(L + p, v[i]);
            }
            const char* z = src == 1 ? " zone=" : "";
            for (; *z; z++)
                L[p++] = *z;
            for (int j = 0; src == 1 && g_ts.net_tz_name[j] && j < 47; j++)
                L[p++] = g_ts.net_tz_name[j];
            L[p++] = '\n';
            trace_line(L, (unsigned long)p);
            g_ts_net_tz_used = m;
            g_ts_net_tz_src = src;
        }
    }
    return (uint64_t)((int64_t)utc + (int64_t)m * 60000000LL);
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

/* Internet time's own time zone (the console's own may be set wrong - this one's is UTC+0 with
   summer time while its owner lives at UTC+2): asked by the caller's IP over plain HTTP/1.0,
   port 80. Two independent services, in order (DNS blocklists often carry one of them):
   1. ip-api.com: GET /json/?fields=status,timezone,offset - "offset" is "Timezone UTC DST offset in
      seconds" (ip-api.com/docs/api:json), with "status":"success";
   2. worldtimeapi.org: GET /api/ip - "utc_offset" "+HH:MM", DST included ("raw_offset" is not).
   Accepted: HTTP 200 and an offset that is a real zone (a multiple of 15 min within +-14 h).
   Returns 0 and *min / name, or the failing step: -1 DNS, -2 socket, -3 connect / send, -4 not
   HTTP 200 or no field, -5 not a real zone. */
static int ts_http_get(int rid, const char* host, const char* req, int req_len, char* r, int cap) {
    uint32_t addr = 0;
    if (sceNetResolverStartNtoa(rid, host, &addr, 0, 0, 0) < 0 || addr == 0)
        return -1;
    int s = sceNetSocket("tz", 2, 1, 0); /* AF_INET, SOCK_STREAM */
    if (s < 0)
        return -2;
    int tmo = 5000000;
    sceNetSetsockopt(s, 0xFFFF, 0x1106, &tmo, sizeof(tmo));
    sceNetSetsockopt(s, 0xFFFF, 0x1105, &tmo, sizeof(tmo));
    unsigned char sa[16] = {16, 2, 0, 80}; /* sin_len, AF_INET, port 80 (big-endian) */
    for (int i = 0; i < 4; i++)
        sa[4 + i] = ((const unsigned char*)&addr)[i];
    int n = -3;
    if (sceNetConnect(s, sa, sizeof(sa)) == 0 &&
        sceNetSend(s, req, (unsigned long)req_len, 0) == req_len) {
        n = 0;
        for (;;) {
            int k = sceNetRecv(s, r + n, (unsigned long)(cap - 1 - n), 0);
            if (k <= 0)
                break;
            n += k;
            if (n >= cap - 1)
                break;
        }
        r[n] = 0;
    }
    sceNetSocketClose(s);
    if (n < 0)
        return n;
    /* "HTTP/1.x 200" */
    return (n > 12 && r[0] == 'H' && r[1] == 'T' && r[2] == 'T' && r[3] == 'P' && r[9] == '2' &&
            r[10] == '0' && r[11] == '0')
               ? n
               : -4;
}
static const char* ts_find(const char* r, const char* key) {
    for (; *r; r++) {
        int j = 0;
        while (key[j] && r[j] == key[j])
            j++;
        if (!key[j])
            return r + j;
    }
    return 0;
}
static void ts_copy_name(const char* q, char* name, int name_len) {
    int j = 0;
    for (; q && q[j] && q[j] != '"' && j < name_len - 1; j++)
        name[j] = q[j];
    name[j] = 0;
}
static int ts_tz_ok_sec(int v) {
    int a = v < 0 ? -v : v;
    return a <= 14 * 3600 && a % 900 == 0;
}
static int ts_http_tz(int rid, int service, int* min, char* name, int name_len) {
    static const char q1[] =
        "GET /json/?fields=status,timezone,offset HTTP/1.0\r\n"
        "Host: ip-api.com\r\nUser-Agent: ShadCube4\r\nConnection: close\r\n\r\n";
    static const char q2[] =
        "GET /api/ip HTTP/1.0\r\n"
        "Host: worldtimeapi.org\r\nUser-Agent: ShadCube4\r\nConnection: close\r\n\r\n";
    char r[2048];
    int n = service == 0 ? ts_http_get(rid, "ip-api.com", q1, sizeof(q1) - 1, r, sizeof(r))
                         : ts_http_get(rid, "worldtimeapi.org", q2, sizeof(q2) - 1, r, sizeof(r));
    if (n < 0)
        return n;
    int v = 0, sign = 1, digits = 0;
    if (service == 0) { /* "status":"success", "offset":<seconds> */
        const char* of = ts_find(r, "\"offset\":");
        if (!ts_find(r, "\"status\":\"success\"") || !of)
            return -4;
        if (*of == '-') {
            sign = -1;
            of++;
        }
        for (; *of >= '0' && *of <= '9' && digits < 7; of++, digits++)
            v = v * 10 + (*of - '0');
        v *= sign;
    } else { /* "utc_offset":"+HH:MM" */
        const char* of = ts_find(r, "\"utc_offset\":\"");
        if (!of || (of[0] != '+' && of[0] != '-') || of[3] != ':')
            return -4;
        for (int i = 1; i <= 5; i++)
            if (i != 3 && (of[i] < '0' || of[i] > '9'))
                return -4;
        v = (of[0] == '-' ? -1 : 1) * (((of[1] - '0') * 10 + (of[2] - '0')) * 3600 +
                                       ((of[4] - '0') * 10 + (of[5] - '0')) * 60);
        digits = 1;
    }
    if (!digits || !ts_tz_ok_sec(v))
        return -5;
    *min = v / 60;
    ts_copy_name(ts_find(r, "\"timezone\":\""), name, name_len);
    return 0;
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
        int tz = 0;
        if (ok) { /* internet time's own zone, with every sync (follows DST changes) */
            for (int sv = 0; sv < 2 && !tz; sv++) {
                int m;
                char nm[48];
                int r = ts_http_tz(rid, sv, &m, nm, sizeof(nm));
                __atomic_store_n(&g_ts.tz_http_ret[sv], r, __ATOMIC_RELAXED);
                if (r == 0) {
                    for (int j = 0; j < (int)sizeof(nm); j++)
                        g_ts.net_tz_name[j] = nm[j];
                    __atomic_store_n(&g_ts.net_tz_min, m, __ATOMIC_RELAXED);
                    __atomic_store_n(&g_ts.net_tz_ok, 1, __ATOMIC_RELEASE);
                    tz = 1;
                }
            }
            __atomic_add_fetch(&g_ts.tz_http_tries, 1, __ATOMIC_RELEASE);
        }
        /* synced with a zone: 30 min; synced but no zone yet: a minute; no sync: 30 s */
        sceKernelUsleep(!ok                      ? TS_NTP_RETRY_US
                        : (tz || g_ts.net_tz_ok) ? TS_NTP_EVERY_US
                                                 : 60000000u);
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
        g_ts.utc_is_net = 1;
        return (uint64_t)((int64_t)us + g_ts.shown_off);
    }
    g_ts.utc_is_net = 0;
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
