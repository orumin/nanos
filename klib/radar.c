#include <kernel.h>
#include <http.h>
#include <log.h>
#include <lwip.h>

#define RADAR_HOSTNAME  "radar.relayered.net"
#define RADAR_PORT      443

#define RADAR_CA_CERT   "-----BEGIN CERTIFICATE-----\n\
MIIE0DCCA7igAwIBAgIBBzANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\
EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\
EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\
ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTExMDUwMzA3MDAwMFoXDTMxMDUwMzA3\
MDAwMFowgbQxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\
EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjEtMCsGA1UE\
CxMkaHR0cDovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkvMTMwMQYDVQQD\
EypHbyBEYWRkeSBTZWN1cmUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\
MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC54MsQ1K92vdSTYuswZLiBCGzD\
BNliF44v/z5lz4/OYuY8UhzaFkVLVat4a2ODYpDOD2lsmcgaFItMzEUz6ojcnqOv\
K/6AYZ15V8TPLvQ/MDxdR/yaFrzDN5ZBUY4RS1T4KL7QjL7wMDge87Am+GZHY23e\
cSZHjzhHU9FGHbTj3ADqRay9vHHZqm8A29vNMDp5T19MR/gd71vCxJ1gO7GyQ5HY\
pDNO6rPWJ0+tJYqlxvTV0KaudAVkV4i1RFXULSo6Pvi4vekyCgKUZMQWOlDxSq7n\
eTOvDCAHf+jfBDnCaQJsY1L6d8EbyHSHyLmTGFBUNUtpTrw700kuH9zB0lL7AgMB\
AAGjggEaMIIBFjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNV\
HQ4EFgQUQMK9J47MNIMwojPX+2yz8LQsgM4wHwYDVR0jBBgwFoAUOpqFBxBnKLbv\
9r0FQW4gwZTaD94wNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8v\
b2NzcC5nb2RhZGR5LmNvbS8wNQYDVR0fBC4wLDAqoCigJoYkaHR0cDovL2NybC5n\
b2RhZGR5LmNvbS9nZHJvb3QtZzIuY3JsMEYGA1UdIAQ/MD0wOwYEVR0gADAzMDEG\
CCsGAQUFBwIBFiVodHRwczovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkv\
MA0GCSqGSIb3DQEBCwUAA4IBAQAIfmyTEMg4uJapkEv/oV9PBO9sPpyIBslQj6Zz\
91cxG7685C/b+LrTW+C05+Z5Yg4MotdqY3MxtfWoSKQ7CC2iXZDXtHwlTxFWMMS2\
RJ17LJ3lXubvDGGqv+QqG+6EnriDfcFDzkSnE3ANkR/0yBOtg2DZ2HKocyQetawi\
DsoXiWJYRBuriSUBAA/NxBti21G00w9RKpv0vHP8ds42pM3Z2Czqrpv1KrKQ0U11\
GIo/ikGQI31bS/6kA1ibRrLDYGCD+H1QQc7CoZDDu+8CL9IVVO5EFdkKrqeKM+2x\
LXY2JtwE65/3YR8V3Idv7kaWKK2hJn0KCacuBKONvPi8BDAB\
-----END CERTIFICATE-----"

#define RADAR_STATS_INTERVAL    seconds(60)
#define RADAR_STATS_BATCH_SIZE  5

declare_closure_struct(0, 1, void, retry_timer_func,
    u64, overruns);
declare_closure_struct(0, 1, void, telemetry_stats,
    u64, overruns);

static struct telemetry {
    heap h;
    heap phys;
    tuple env;
    buffer auth_header;
    klog_dump dump;
    u64 boot_id;
    boolean running;
    timestamp retry_backoff;
    closure_struct(retry_timer_func, retry_func);
    closure_struct(telemetry_stats, stats_func);
    u64 stats_mem_used[RADAR_STATS_BATCH_SIZE];
    int stats_count;
    void (*rprintf)(const char *format, ...);
    tuple (*allocate_tuple)(void);
    void (*table_set)(table z, void *c, void *v);
    void *(*table_find)(table z, void *c);
    void (*deallocate_table)(table t);
    void (*destruct_tuple)(tuple t, boolean recursive);
    void (*timm_dealloc)(tuple t);
    symbol (*intern)(string name);
    void *(*klib_sym)(klib kl, symbol s);
    void (*klog_load)(klog_dump dest, status_handler sh);
    void (*klog_dump_clear)(void);
    void (*klog_set_boot_id)(u64 id);
    buffer (*allocate_buffer)(heap h, bytes s);
    void (*buffer_write)(buffer b, const void *source, bytes length);
    int (*buffer_strstr)(buffer b, const char *str);
    void (*bprintf)(buffer b, const char *fmt, ...);
    timer (*register_timer)(clock_id id, timestamp val, boolean absolute,
            timestamp interval, timer_handler n);
    err_t (*dns_gethostbyname)(const char *hostname, ip_addr_t *addr,
            dns_found_callback found, void *callback_arg);
    buffer_handler (*allocate_http_parser)(heap h, value_handler each);
    status (*http_request)(heap h, buffer_handler bh, http_method method,
            tuple headers, buffer body);
    int (*tls_connect)(ip_addr_t *addr, u16 port, connection_handler ch);
} telemetry;

#undef sym
#define sym(name)   sym_intern(name, telemetry.intern)

#define kfunc(name) telemetry.name

/* To be used with literal strings only */
#define buffer_write_cstring(b, s)  kfunc(buffer_write)(b, s, sizeof(s) - 1)

static void telemetry_crash_report(void);
static void telemetry_boot(void);

static void telemetry_dns_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    connection_handler ch = (connection_handler)callback_arg;
    if (ipaddr) {
        if (telemetry.tls_connect((ip_addr_t *)ipaddr, RADAR_PORT, ch) == 0)
            return;
        else
            kfunc(rprintf)("Radar: failed to connect to server\n");
    } else {
        kfunc(rprintf)("Radar: failed to look up server hostname\n");
    }
    deallocate_closure(ch);
}

static boolean telemetry_req(const char *url, buffer data, buffer_handler bh)
{
    tuple req = kfunc(allocate_tuple)();
    if (req == INVALID_ADDRESS)
        return false;
    kfunc(table_set)(req, sym(url), alloca_wrap_cstring(url));
    kfunc(table_set)(req, sym(Host), alloca_wrap_cstring(RADAR_HOSTNAME));
    kfunc(table_set)(req, sym(RADAR-KEY), telemetry.auth_header);
    kfunc(table_set)(req, sym(Content-Type), alloca_wrap_cstring("application/json"));
    status s = kfunc(http_request)(telemetry.h, bh, HTTP_REQUEST_METHOD_POST, req, data);
    kfunc(deallocate_table)(req);
    if (is_ok(s)) {
        return true;
    } else {
        kfunc(timm_dealloc)(s);
        return false;
    }
}

closure_function(2, 1, status, telemetry_recv,
                 value_handler, vh, buffer_handler, out,
                 buffer, data)
{
    if (data) {
        value_handler vh = bound(vh);
        if (vh) {
            buffer_handler parser = kfunc(allocate_http_parser)(telemetry.h, vh);
            if (parser != INVALID_ADDRESS) {
                apply(parser, data);
            } else {
                kfunc(rprintf)("Radar: failed to allocate HTTP parser\n");
                apply(vh, 0);
            }
        }
        apply(bound(out), 0);   /* close connection */
    } else {  /* connection closed */
        closure_finish();
        if (telemetry.dump) {
            /* We just sent a crash report: clear the log dump (so that it's not
             * sent again at the next boot), then send a boot event. */
            kfunc(klog_dump_clear)();
            deallocate(telemetry.h, telemetry.dump, sizeof(*telemetry.dump));
            telemetry.dump = 0;
            telemetry_boot();
        } else if (!telemetry.running) {
            /* The boot event has been sent: start collecting usage metrics. */
            telemetry.stats_count = 0;
            kfunc(register_timer)(CLOCK_ID_MONOTONIC, RADAR_STATS_INTERVAL, false,
                    RADAR_STATS_INTERVAL, (timer_handler)&telemetry.stats_func);
            telemetry.running = true;
        }
    }
    return STATUS_OK;
}

closure_function(3, 1, buffer_handler, telemetry_ch,
                 const char *, url, buffer, data, value_handler, vh,
                 buffer_handler, out)
{
    buffer data = bound(data);
    buffer_handler in = 0;
    if (out) {
        boolean success = telemetry_req(bound(url), data, out);
        if (success)
            in = closure(telemetry.h, telemetry_recv, bound(vh), out);
        else
            deallocate_buffer(data);
    } else {    /* connection failed */
        deallocate_buffer(data);
    }
    closure_finish();
    return in;
}

boolean telemetry_send(const char *url, buffer data, value_handler vh)
{
    connection_handler ch = closure(telemetry.h, telemetry_ch, url, data, vh);
    if (ch == INVALID_ADDRESS)
        return false;
    ip_addr_t radar_addr;
    err_t err = kfunc(dns_gethostbyname)(RADAR_HOSTNAME, &radar_addr, telemetry_dns_cb, ch);
    switch (err) {
    case ERR_OK:
        if (telemetry.tls_connect(&radar_addr, RADAR_PORT, ch) == 0)
            return true;
        break;
    case ERR_INPROGRESS:
        return true;
    }
    deallocate_closure(ch);
    return false;
}

define_closure_function(0, 1, void, retry_timer_func,
                        u64, overruns)
{
    if (telemetry.dump)
        telemetry_crash_report();
    else
        telemetry_boot();
}

static void telemetry_retry(void)
{
    kfunc(register_timer)(CLOCK_ID_MONOTONIC, telemetry.retry_backoff, false, 0,
            init_closure(&telemetry.retry_func, retry_timer_func));
    if (telemetry.retry_backoff < seconds(600))
        telemetry.retry_backoff <<= 1;
}

static void telemetry_crash_report(void)
{
    buffer b = kfunc(allocate_buffer)(telemetry.h, PAGESIZE);
    if (b == INVALID_ADDRESS)
        goto error;
    kfunc(bprintf)(b, "{\"bootID\":%ld", telemetry.dump->boot_id);
    buffer nanos_ver = kfunc(table_find)(telemetry.env, sym(NANOS_VERSION));
    if (nanos_ver)
        kfunc(bprintf)(b, ",\"nanosVersion\":\"%b\"", nanos_ver);
    buffer ops_ver = kfunc(table_find)(telemetry.env, sym(OPS_VERSION));
    if (ops_ver)
        kfunc(bprintf)(b, ",\"opsVersion\":\"%b\"", ops_ver);
    buffer_write_cstring(b, ",\"dump\":\"");
    for (int i = 0; (i < sizeof(telemetry.dump->msgs)) && telemetry.dump->msgs[i]; i++) {
        /* Escape JSON special characters. */
        char c = telemetry.dump->msgs[i];
        switch (c) {
        case '\n':
            buffer_write_cstring(b, "\\n");
            break;
        case '"':
            buffer_write_cstring(b, "\\\"");
            break;
        case '/':
            buffer_write_cstring(b, "\\/");
            break;
        case '\\':
            buffer_write_cstring(b, "\\\\");
            break;
        case '\t':
            buffer_write_cstring(b, "\\t");
            break;
        case '\r':
            buffer_write_cstring(b, "\\r");
            break;
        case '\b':
            buffer_write_cstring(b, "\\b");
            break;
        case '\f':
            buffer_write_cstring(b, "\\f");
            break;
        default:
            kfunc(buffer_write)(b, &c, 1);
        }
    }
    buffer_write_cstring(b, "\"}\r\n");
    if (!telemetry_send("/crashes", b, 0)) {
        deallocate_buffer(b);
        goto error;
    }
    return;
  error:
    telemetry_retry();
}

closure_function(0, 1, void, telemetry_boot_recv,
                 value, v)
{
    telemetry.boot_id = 0;
    if (!v) /* couldn't allocate HTTP parser */
        return;
    buffer content = kfunc(table_find)(v, sym(content));
    if (content) {
        int index = kfunc(buffer_strstr)(content, "\"id\"");
        if (index < 0)
            goto exit;
        buffer_consume(content, index);
        buffer_consume(content, buffer_strchr(content, ':') + 1);
        index = buffer_strchr(content, ',');
        if (index < 0) {
            index = buffer_strchr(content, '}');
            if (index < 0)
                goto exit;
        }
        parse_int(alloca_wrap_buffer(buffer_ref(content, 0), index), 10, &telemetry.boot_id);
        kfunc(klog_set_boot_id)(telemetry.boot_id);
    }
  exit:
    kfunc(destruct_tuple)(v, true);
    closure_finish();
}

static void telemetry_boot(void)
{
    buffer b = kfunc(allocate_buffer)(telemetry.h, 64);
    if (b == INVALID_ADDRESS)
        goto error;
    value_handler vh = closure(telemetry.h, telemetry_boot_recv);
    if (vh == INVALID_ADDRESS) {
        goto err_free_buf;
    }
    buffer_write_cstring(b, "{");
    buffer nanos_ver = kfunc(table_find)(telemetry.env, sym(NANOS_VERSION));
    if (nanos_ver)
        kfunc(bprintf)(b, "\"nanosVersion\":\"%b\"", nanos_ver);
    buffer ops_ver = kfunc(table_find)(telemetry.env, sym(OPS_VERSION));
    if (ops_ver)
        kfunc(bprintf)(b, ",\"opsVersion\":\"%b\"", ops_ver);
    buffer_write_cstring(b, "}\r\n");
    if (!telemetry_send("/boots", b, vh)) {
        deallocate_closure(vh);
        goto err_free_buf;
    }
    return;
  err_free_buf:
    deallocate_buffer(b);
  error:
    telemetry_retry();
}

define_closure_function(0, 1, void, telemetry_stats,
                        u64, overruns)
{
    telemetry.stats_mem_used[telemetry.stats_count++] = heap_allocated(telemetry.phys);
    if (telemetry.stats_count == RADAR_STATS_BATCH_SIZE) {
        telemetry.stats_count = 0;
        buffer b = kfunc(allocate_buffer)(telemetry.h, 128);
        if (b == INVALID_ADDRESS) {
            kfunc(rprintf)("%s: failed to allocate buffer\n", __func__);
            return;
        }
        kfunc(bprintf)(b, "{\"bootID\":%ld,\"memUsed\":[", telemetry.boot_id);
        for (int i = 0; i < RADAR_STATS_BATCH_SIZE; i++)
            kfunc(bprintf)(b, "%ld%s", telemetry.stats_mem_used[i],
                    (i < RADAR_STATS_BATCH_SIZE - 1) ? "," : "");
        buffer_write_cstring(b, "]}\r\n");
        if (!telemetry_send("/machine-stats", b, 0)) {
            kfunc(rprintf)("%s: failed to send stats\n", __func__);
            deallocate_buffer(b);
        }
    }
}

closure_function(0, 1, void, klog_dump_loaded,
                 status, s)
{
    if (is_ok(s)) {
        if (telemetry.dump->exit_code != 0) {
            telemetry_crash_report();
        } else {
            deallocate(telemetry.h, telemetry.dump, sizeof(*telemetry.dump));
            telemetry.dump = 0;
            telemetry_boot();
        }
    } else
        kfunc(timm_dealloc)(s);
    closure_finish();
}

closure_function(0, 2, void, tls_loaded,
                 klib, kl, status, s)
{
    closure_finish();
    if (is_ok(s)) {
        int (*tls_set_cacert)(void *, u64) = kfunc(klib_sym)(kl, sym(tls_set_cacert));
        if (tls_set_cacert(RADAR_CA_CERT, sizeof(RADAR_CA_CERT)) == 0) {
            telemetry.tls_connect = kfunc(klib_sym)(kl, sym(tls_connect));
            telemetry.dump = allocate(telemetry.h, sizeof(*telemetry.dump));
            if (telemetry.dump != INVALID_ADDRESS) {
                status_handler sh = closure(telemetry.h, klog_dump_loaded);
                if (sh != INVALID_ADDRESS)
                    kfunc(klog_load)(telemetry.dump, sh);
                else
                    deallocate(telemetry.h, telemetry.dump, sizeof(*telemetry.dump));
            }
        }
    } else {
        kfunc(timm_dealloc)(s);
    }
}

int init(void *md, klib_get_sym get_sym, klib_add_sym add_sym)
{
    telemetry.rprintf = get_sym("rprintf");
    if (!telemetry.rprintf)
        return KLIB_INIT_FAILED;
    void *(*get_kernel_heaps)(void) = get_sym("get_kernel_heaps");
    void *(*get_environment)(void) = get_sym("get_environment");
    u64 (*random_u64)(void) = get_sym("random_u64");
    void (*load_klib)(const char *, klib_handler) = get_sym("load_klib");
    if (!get_kernel_heaps || !get_environment || !random_u64 || !load_klib ||
            !(telemetry.allocate_tuple = get_sym("allocate_tuple")) ||
            !(telemetry.table_set = get_sym("table_set")) ||
            !(telemetry.table_find = get_sym("table_find")) ||
            !(telemetry.deallocate_table = get_sym("deallocate_table")) ||
            !(telemetry.destruct_tuple = get_sym("destruct_tuple")) ||
            !(telemetry.timm_dealloc = get_sym("timm_dealloc")) ||
            !(telemetry.intern = get_sym("intern")) ||
            !(telemetry.klib_sym = get_sym("klib_sym")) ||
            !(telemetry.klog_load = get_sym("klog_load")) ||
            !(telemetry.klog_dump_clear = get_sym("klog_dump_clear")) ||
            !(telemetry.klog_set_boot_id = get_sym("klog_set_boot_id")) ||
            !(telemetry.allocate_buffer = get_sym("allocate_buffer")) ||
            !(telemetry.buffer_write = get_sym("buffer_write")) ||
            !(telemetry.buffer_strstr = get_sym("buffer_strstr")) ||
            !(telemetry.bprintf = get_sym("bprintf")) ||
            !(telemetry.register_timer = get_sym("kern_register_timer")) ||
            !(telemetry.dns_gethostbyname = get_sym("dns_gethostbyname")) ||
            !(telemetry.allocate_http_parser = get_sym("allocate_http_parser")) ||
            !(telemetry.http_request = get_sym("http_request"))) {
        kfunc(rprintf)("Radar: kernel symbols not found\n");
        return KLIB_INIT_FAILED;
    }
    kernel_heaps kh = get_kernel_heaps();
    telemetry.h = heap_general(kh);
    telemetry.phys = (heap)heap_physical(kh);
    klib_handler tls_handler = closure(telemetry.h, tls_loaded);
    if (tls_handler == INVALID_ADDRESS) {
        return KLIB_INIT_FAILED;
    }
    telemetry.env = get_environment();
    telemetry.auth_header = kfunc(table_find)(telemetry.env, sym(RADAR_KEY));
    telemetry.retry_backoff = seconds(1);
    telemetry.running = false;
    init_closure(&telemetry.stats_func, telemetry_stats);
    load_klib("/klib/tls", tls_handler);
    return KLIB_INIT_OK;
}
