/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat 0.13.3: a set's manifest, read from the card by the file browser (card_browse.c, "Install set").
 *
 * A set is a folder on the card with the asset pack, the engine module(s) and, for the record, the firmware package of one build, and a manifest named
 * *.set.json that says which build they are for and which files belong to it. docs/orangecat/SET_FORMAT.md has the format; tools/oc_manifest.py
 * writes it. The text is the JSON that script writes (json.dumps, indent 2): objects, one array of strings, decimal numbers, strings without escapes.
 * This reader is not a general JSON parser. It looks for each key as "key" followed by a colon, reads the value after it, and refuses anything it does not
 * expect: every read stays inside the n bytes it was given, a file name is 1 to 47 plain characters (no / \ : * ? < > | " and no control characters, not
 * starting with a dot), a number is decimal and fits 32 bits. Pure functions on a buffer, so the host tests (usb_host_test.c) and the fuzz loop in it
 * run the code the unit runs.
 *
 * Revisions:
 *   0.13.3: first version.
 */
#ifndef OC_SET_H
#define OC_SET_H
#define OC_SET_NAME 48u                                  /* a file name with its NUL (card_browse.c FB_NAME) */
#define OC_SET_OCM 2u                                    /* modules a set may name (SHAPES, the Prophet; the area holds one of them) */
typedef struct {
    uint32_t version;                                    /* 0x00MMmmpp from "x.y.z" */
    uint32_t pack_hash, pack_len, iface, api_addr;
    uint32_t nocm;
    char fwsc[OC_SET_NAME];                              /* "" when the manifest names no firmware package */
    char pak[OC_SET_NAME];
    char ocm[OC_SET_OCM][OC_SET_NAME];
} oc_set_t;
enum { OC_SET_OK = 0, OC_SET_BAD = -1, OC_SET_NAME_BAD = -2, OC_SET_MANY = -3 };
enum { OC_SET_THIS = 0, OC_SET_NEWER = 1, OC_SET_OLDER = 2, OC_SET_OTHER = 3 };   /* oc_set_vs: the set is for this build / a newer / an older version / another build of the same version */

static int os_ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }
/* the value after "key": in s[0..n): a pointer to its first byte, or 0 */
static const char *os_find(const char *s, uint32_t n, const char *key)
{
    uint32_t i, k, kl = 0;
    while (key[kl]) kl++;
    for (i = 0; i + kl + 2u < n; i++) {
        if (s[i] != '"') continue;
        for (k = 0; k < kl && s[i + 1u + k] == key[k]; k++)
            ;
        if (k != kl || s[i + 1u + kl] != '"') continue;
        for (k = i + kl + 2u; k < n && os_ws(s[k]); k++)
            ;
        if (k >= n || s[k] != ':') continue;
        for (k++; k < n && os_ws(s[k]); k++)
            ;
        return k < n ? s + k : 0;
    }
    return 0;
}
/* a decimal number at p (end e) into *v: 0 or -1 */
static int os_num(const char *p, const char *e, uint32_t *v)
{
    uint32_t r = 0, d = 0;
    for (; p < e && *p >= '0' && *p <= '9'; p++, d++) {
        uint32_t c = (uint32_t)(*p - '0');
        if (r > 429496729u || (r == 429496729u && c > 5u)) return -1;
        r = r * 10u + c;
    }
    if (!d) return -1;
    *v = r;
    return 0;
}
/* a quoted string at p (end e) into d (cap bytes with the NUL); *end after the closing quote. 0, or -1 (not a string, too long, empty) */
static int os_str(const char *p, const char *e, char *d, uint32_t cap, const char **end)
{
    uint32_t n = 0;
    if (p >= e || *p != '"') return -1;
    for (p++; p < e && *p != '"'; p++, n++) {
        unsigned char c = (unsigned char)*p;
        if (n + 1u >= cap || c < 32u || c > 126u || c == '\\') return -1;
        d[n] = (char)c;
    }
    if (p >= e || !n) return -1;
    d[n] = 0;
    if (end) *end = p + 1;
    return 0;
}
/* a file name: plain, in the manifest's own folder */
static int os_name_ok(const char *n)
{
    uint32_t i;
    if (n[0] == '.') return 0;
    for (i = 0; n[i]; i++) {
        char c = n[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '"') return 0;
    }
    return 1;
}
/* "x.y.z" (each part 0..255) at p into 0x00MMmmpp */
static int os_version(const char *p, const char *e, uint32_t *v)
{
    char t[16];
    uint32_t part[3], k, i = 0;
    if (os_str(p, e, t, sizeof t, 0)) return -1;
    for (k = 0; k < 3u; k++) {
        uint32_t r = 0, d = 0;
        for (; t[i] >= '0' && t[i] <= '9'; i++, d++) {
            r = r * 10u + (uint32_t)(t[i] - '0');
            if (r > 255u) return -1;
        }
        if (!d) return -1;
        part[k] = r;
        if (k < 2u) { if (t[i] != '.') return -1; i++; }
    }
    if (t[i]) return -1;
    *v = part[0] << 16 | part[1] << 8 | part[2];
    return 0;
}
/* the manifest text s[0..n) into *o: OC_SET_OK, or OC_SET_BAD (a key missing or a value that is not what it should be), OC_SET_NAME_BAD (a file name
 * that is not plain), OC_SET_MANY (more modules than OC_SET_OCM). The pack is required (a module reads its tables); the firmware package and the modules are optional. */
static int oc_set_parse(const char *s, uint32_t n, oc_set_t *o)
{
    const char *p, *e = s + n, *files;
    uint32_t i;
    for (i = 0; i < sizeof *o; i++) ((char *)o)[i] = 0;
    if (!n || !(p = os_find(s, n, "version")) || os_version(p, e, &o->version)) return OC_SET_BAD;
    if (!(p = os_find(s, n, "pack_hash")) || os_num(p, e, &o->pack_hash)) return OC_SET_BAD;
    if (!(p = os_find(s, n, "pack_len")) || os_num(p, e, &o->pack_len)) return OC_SET_BAD;
    if (!(p = os_find(s, n, "iface")) || os_num(p, e, &o->iface)) return OC_SET_BAD;
    if (!(p = os_find(s, n, "api_addr")) || os_num(p, e, &o->api_addr)) return OC_SET_BAD;
    if (!(files = os_find(s, n, "files")) || *files != '{') return OC_SET_BAD;
    if (!(p = os_find(files, (uint32_t)(e - files), "pak")) || os_str(p, e, o->pak, sizeof o->pak, 0)) return OC_SET_BAD;
    if ((p = os_find(files, (uint32_t)(e - files), "fwsc")) && os_str(p, e, o->fwsc, sizeof o->fwsc, 0)) return OC_SET_BAD;
    if ((p = os_find(files, (uint32_t)(e - files), "ocm"))) {
        if (*p != '[') return OC_SET_BAD;
        for (p++; p < e && os_ws(*p); p++)
            ;
        while (p < e && *p != ']') {
            if (o->nocm >= OC_SET_OCM) return OC_SET_MANY;
            if (os_str(p, e, o->ocm[o->nocm], OC_SET_NAME, &p)) return OC_SET_BAD;
            o->nocm++;
            for (; p < e && os_ws(*p); p++)
                ;
            if (p < e && *p == ',') {
                for (p++; p < e && os_ws(*p); p++)
                    ;
                if (p < e && *p == ']') return OC_SET_BAD;      /* (a comma before the end) */
            } else if (p < e && *p != ']') {
                return OC_SET_BAD;
            }
        }
        if (p >= e) return OC_SET_BAD;
    }
    if (!os_name_ok(o->pak) || (o->fwsc[0] && !os_name_ok(o->fwsc))) return OC_SET_NAME_BAD;
    for (i = 0; i < o->nocm; i++)
        if (!os_name_ok(o->ocm[i])) return OC_SET_NAME_BAD;
    return OC_SET_OK;
}
/* the set against the firmware it is read by: the version first (a set for a newer one wants the firmware updated first), then the identifiers the build was
 * compiled with (interface identifier, table address, pack hash and length: the same build) */
static int oc_set_vs(const oc_set_t *o, uint32_t version, uint32_t iface, uint32_t api_addr, uint32_t pack_hash, uint32_t pack_len)
{
    if (o->version > version) return OC_SET_NEWER;
    if (o->version < version) return OC_SET_OLDER;
    return o->iface == iface && o->api_addr == api_addr && o->pack_hash == pack_hash && o->pack_len == pack_len ? OC_SET_THIS : OC_SET_OTHER;
}
#endif
