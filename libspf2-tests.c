/*
 * Regression tests for the PLD patches to libspf2, run against the built
 * library with offline DNS (zone resolver plus a layer returning chosen
 * h_errno values).
 *
 * What holds:
 *  - a record keeps every mechanism up to 255 of them and 16384 compiled
 *    bytes; past that it is a permerror with SPF_E_BIG_MECH, through DNS,
 *    fallback and include;
 *  - modifiers past those limits are ignored, never an error (RFC 7208 6);
 *    RFC 7208 sets no limit on a single literal, modifier or explanation;
 *  - the checked domain's own record gives RFC 7208 results: permerror for
 *    one that does not compile, temperror for DNS errors other than
 *    NXDOMAIN, none for an answer without TXT records or for a malformed
 *    domain (RFC 7208 4.3), permerror when that domain is an include target;
 *  - at most 10 DNS terms per check, include targets counted in, so nested
 *    includes cannot multiply the lookups (RFC 7208 4.6.4);
 *  - redirect= applies only when no mechanism matched; redirect= and exp=
 *    appear at most once;
 *  - a ':' or '=' with an empty domain-spec is a permerror (RFC 7208 5);
 *  - %{xN} keeps the rightmost N parts (RFC 7208 7.3);
 *  - a domain name over 253 chars loses labels on the left (RFC 7208 7.3);
 *  - a record prints back as written;
 *  - the response does not keep a pointer to the freed record;
 *  - the DNS cache keeps a long TTL of SPF-type records.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>

#include "spf.h"
#include "spf_dns.h"
#include "spf_dns_rr.h"
#include "spf_dns_zone.h"
#include "spf_dns_cache.h"
#include "spf_record.h"
#include "spf_internal.h"

#define LIMIT	16384

static SPF_server_t		*srv;
static SPF_dns_server_t	*zone;
static int				 failures;

static void check(int ok, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", ok ? "ok    " : "FAILED", what,
			detail ? " | " : "", detail ? detail : "");
	failures += !ok;
}

static void txt(const char *domain, const char *record)
{
	SPF_dns_zone_add_str(zone, domain, ns_t_txt, NETDB_SUCCESS, record);
}

/* ---- queries ---- */

static void query(const char *what, const char *from, const char *ip,
		const char *fallback, SPF_result_t want, SPF_errcode_t want_err)
{
	SPF_request_t	*req = SPF_request_new(srv);
	SPF_response_t	*resp = NULL;
	SPF_errcode_t	 ret;
	const char		*h;
	char			 hdr[64], detail[256];
	int				 ok;

	SPF_request_set_ipv4_str(req, ip);
	SPF_request_set_env_from(req, from);
	ret = fallback ? SPF_request_query_fallback(req, &resp, fallback)
			: SPF_request_query_mailfrom(req, &resp);
	h = SPF_response_get_received_spf(resp);
	snprintf(hdr, sizeof(hdr), "Received-SPF: %s ", SPF_strresult(want));
	ok = SPF_response_result(resp) == want
			&& SPF_response_errcode(resp) == want_err && ret == want_err
			&& h != NULL && strncmp(h, hdr, strlen(hdr)) == 0
			&& resp->spf_record_exp == NULL;
	snprintf(detail, sizeof(detail), "result=%s errcode=%d ret=%d header=%.40s",
			SPF_strresult(SPF_response_result(resp)),
			SPF_response_errcode(resp), ret, h ? h : "(null)");
	check(ok, what, ok ? NULL : detail);
	SPF_response_free(resp);
	SPF_request_free(req);
}

/* ---- record builders ---- */

static char *rec_new(const char *start)
{
	char *b = malloc(1 << 17);
	sprintf(b, "v=spf1%s", start);
	return b;
}

static void add_ip4(char *b, int n)
{
	for (int i = 0; i < n; i++)
		sprintf(b + strlen(b), " ip4:10.%d.%d.1", i / 250, i % 250);
}

static void add_long(char *b, const char *prefix, int n, int len)
{
	for (int i = 0; i < n; i++) {
		char *p = b + strlen(b);
		p += sprintf(p, prefix, i);
		memset(p, 'a', len);
		p[len] = '\0';
	}
}

static size_t compiled_len(const char *rec, int *ok)
{
	SPF_response_t	*resp = SPF_response_new(NULL);
	SPF_record_t	*r = NULL;
	size_t			 len = 0;

	*ok = SPF_record_compile(srv, resp, &r, rec) == SPF_E_SUCCESS;
	if (r) {
		len = r->mech_len;
		SPF_record_free(r);
	}
	SPF_response_free(resp);
	return len;
}

/*
 * "at": the largest record whose mechanism section is exactly LIMIT bytes;
 * "over": the first one past it. String literals are at most 255 chars,
 * so the section is filled with long exists: values.
 */
static void boundary(char **at, char **over)
{
	char *prev = NULL;

	for (int last = 1; last < 256; last++) {
		char	*b = rec_new(" ip4:192.0.2.1");
		int		 ok;
		size_t	 len;

		add_long(b, " exists:%d", 62, 249);
		add_long(b, " exists:%d", 1, last);
		strcat(b, " -all");
		len = compiled_len(b, &ok);
		if (!ok) {
			*at = prev;
			*over = b;
			return;
		}
		free(prev);
		prev = len == LIMIT ? b : NULL;
		if (!prev)
			free(b);
	}
	free(prev);
	*at = *over = NULL;
}

static void test_limits(void)
{
	char *e2o = rec_new(""), *r255 = rec_new(""), *r256 = rec_new("");
	char *m256 = rec_new(" ip4:192.0.2.1 -all"), *modover = rec_new(" ip4:192.0.2.1 -all");
	char *modname = rec_new(" ip4:192.0.2.1 -all"), *modexp = rec_new(" ip4:192.0.2.1 -all");
	char *modlit = rec_new(" ip4:192.0.2.1 -all"), *modmac = rec_new(" ip4:192.0.2.1 -all");
	char *modbad = rec_new(" ip4:192.0.2.1 -all");
	char *mech_at = NULL, *mech_over = NULL;

	/* 83 ip4 + 8 ip6 + -all behind include:, as 28bgbqr47._spf._d.mim.ec */
	add_ip4(e2o, 83);
	for (int i = 1; i <= 8; i++)
		sprintf(e2o + strlen(e2o), " ip6:2001:db8:%x::/48", i);
	strcat(e2o, " -all");
	txt("e2o-child.test", e2o);
	txt("e2o.test", "v=spf1 include:e2o-child.test -all");
	query("mechanism 66 of 92 behind include: matches", "u@e2o.test", "10.0.65.1", NULL,
			SPF_RESULT_PASS, SPF_E_SUCCESS);
	query("trailing -all of a 92-mechanism record applies", "u@e2o-child.test", "192.0.2.9",
			NULL, SPF_RESULT_FAIL, SPF_E_SUCCESS);

	add_ip4(r255, 254); strcat(r255, " ip4:192.0.2.1");
	add_ip4(r256, 255); strcat(r256, " ip4:192.0.2.1");
	add_long(m256, " x%d=", 256, 1);
	add_long(modover, " x%d=", 70, 250);
	add_long(modname, " x%d", 1, 600);
	strcat(modname, "=a");
	add_long(modexp, " x%d=", 255, 1);
	strcat(modexp, " exp=e.target.test");
	add_long(modlit, " x%d=", 1, 300);
	strcat(modmac, " x=");
	for (int i = 0; i < 150; i++)
		strcat(modmac, "%{d}");
	add_long(modbad, " x%d=", 1, 300);
	strcat(modbad, "%{z}");
	boundary(&mech_at, &mech_over);
	check(mech_at != NULL, "record of exactly 16384 compiled bytes built", NULL);
	if (!mech_at)
		goto out;

	struct { const char *what, *dom, *rec; SPF_result_t want; SPF_errcode_t err; } c[] = {
		{ "255 mechanisms",            "mech255.test",  r255,      SPF_RESULT_PASS,      SPF_E_SUCCESS },
		{ "256 mechanisms",            "mech256.test",  r256,      SPF_RESULT_PERMERROR, SPF_E_BIG_MECH },
		{ "16384 bytes of mechanisms", "mechat.test",   mech_at,   SPF_RESULT_PASS,      SPF_E_SUCCESS },
		{ "past 16384 bytes of mechanisms", "mechover.test", mech_over, SPF_RESULT_PERMERROR, SPF_E_BIG_MECH },
		{ "256 unknown modifiers",     "mod256.test",   m256,      SPF_RESULT_PASS,      SPF_E_SUCCESS },
		{ "unknown modifiers past 16384 bytes", "modover.test", modover, SPF_RESULT_PASS, SPF_E_SUCCESS },
		{ "unknown modifier with a 600-char name", "modname.test", modname, SPF_RESULT_PASS, SPF_E_SUCCESS },
		{ "exp= after 255 unknown modifiers", "modexp.test", modexp, SPF_RESULT_PASS, SPF_E_SUCCESS },
		{ "unknown modifier with a 300-char literal", "modlit.test", modlit, SPF_RESULT_PASS, SPF_E_SUCCESS },
		{ "unknown modifier compiled past 511 bytes", "modmac.test", modmac, SPF_RESULT_PASS, SPF_E_SUCCESS },
		{ "syntax error after a 300-char literal", "modbad.test", modbad, SPF_RESULT_PERMERROR, SPF_E_INVALID_VAR },
	};
	char what[128], from[64];

	for (size_t i = 0; i < sizeof(c) / sizeof(*c); i++) {
		txt(c[i].dom, c[i].rec);
		snprintf(from, sizeof(from), "u@%s", c[i].dom);
		snprintf(what, sizeof(what), "%s, own record", c[i].what);
		query(what, from, "192.0.2.1", NULL, c[i].want, c[i].err);
		snprintf(what, sizeof(what), "%s, fallback", c[i].what);
		query(what, "u@nospf.test", "192.0.2.1", c[i].rec, c[i].want, c[i].err);
	}
	query("-all behind 256 unknown modifiers", "u@mod256.test", "192.0.2.9", NULL,
			SPF_RESULT_FAIL, SPF_E_SUCCESS);
	txt("inclbig.test", "v=spf1 include:mech256.test -all");
	query("256 mechanisms behind include:", "u@inclbig.test", "192.0.2.1", NULL,
			SPF_RESULT_PERMERROR, SPF_E_BIG_MECH);
out:
	free(e2o); free(r255); free(r256);
	free(m256); free(modover); free(modname); free(modexp);
	free(modlit); free(modmac); free(modbad);
	free(mech_at); free(mech_over);
}

/* ---- results for the domain's own record ---- */

static SPF_dns_rr_t *fake_lookup(SPF_dns_server_t *s, const char *domain,
		ns_type rr_type, int should_cache)
{
	if (rr_type == ns_t_txt && strcmp(domain, "norecovery.test") == 0)
		return SPF_dns_rr_new_init(s, domain, rr_type, 0, NO_RECOVERY);
	if (rr_type == ns_t_txt && strcmp(domain, "unknownerr.test") == 0)
		return SPF_dns_rr_new_init(s, domain, rr_type, 0, 77);
	if (rr_type == ns_t_txt && strcmp(domain, "notxt.test") == 0)
		return SPF_dns_rr_new_init(s, domain, rr_type, 0, NETDB_SUCCESS);
	/* glibc fails names it cannot encode with NO_RECOVERY */
	if (domain[0] == '.' || strstr(domain, "..") || strcspn(domain, ".") > 63)
		return SPF_dns_rr_new_init(s, domain, rr_type, 0, NO_RECOVERY);
	return SPF_dns_lookup(s->layer_below, domain, rr_type, should_cache);
}

static void test_results(void)
{
	txt("syntax.test", "v=spf1 ip4:300.1.1.1 -all");
	query("syntax error, own record", "u@syntax.test", "192.0.2.1", NULL,
			SPF_RESULT_PERMERROR, SPF_E_INVALID_IP4);
	query("syntax error, fallback", "u@nospf.test", "192.0.2.1",
			"v=spf1 ip4:300.1.1.1 -all", SPF_RESULT_PERMERROR, SPF_E_INVALID_IP4);
	query("DNS NO_RECOVERY", "u@norecovery.test", "192.0.2.1", NULL,
			SPF_RESULT_TEMPERROR, SPF_E_DNS_ERROR);
	query("DNS unknown h_errno", "u@unknownerr.test", "192.0.2.1", NULL,
			SPF_RESULT_TEMPERROR, SPF_E_DNS_ERROR);
	query("DNS success without TXT records", "u@notxt.test", "192.0.2.1", NULL,
			SPF_RESULT_NONE, SPF_E_NOT_SPF);

	char from[128], rec[128], label[65];
	memset(label, 'a', 64);
	label[64] = '\0';
	snprintf(from, sizeof(from), "u@%s.test", label);
	query("label of 64 chars", from, "192.0.2.1", NULL,
			SPF_RESULT_NONE, SPF_E_NOT_SPF);
	query("empty label", "u@a..test", "192.0.2.1", NULL,
			SPF_RESULT_NONE, SPF_E_NOT_SPF);
	query("empty first label", "u@.test", "192.0.2.1", NULL,
			SPF_RESULT_NONE, SPF_E_NOT_SPF);
	snprintf(rec, sizeof(rec), "v=spf1 include:%s.test -all", label);
	txt("badinclude.test", rec);
	query("include of a malformed domain", "u@badinclude.test", "192.0.2.1",
			NULL, SPF_RESULT_PERMERROR, SPF_E_NOT_SPF);
	label[63] = '\0';
	snprintf(from, sizeof(from), "u@%s.test", label);
	txt(from + 2, "v=spf1 -all");
	query("label of 63 chars", from, "192.0.2.1", NULL,
			SPF_RESULT_FAIL, SPF_E_SUCCESS);
}

/* ---- DNS lookup limit ---- */

static void test_dns_limit(void)
{
	/* include:t counts as one, the target's a terms as the rest */
	txt("dns10.test", "v=spf1 include:nine.test -all");
	txt("nine.test", "v=spf1 a a a a a a a a a -all");
	txt("dns11.test", "v=spf1 include:ten.test -all");
	txt("ten.test", "v=spf1 a a a a a a a a a a -all");
	query("10 DNS terms through include", "u@dns10.test", "192.0.2.1", NULL,
			SPF_RESULT_FAIL, SPF_E_SUCCESS);
	query("11 DNS terms through include", "u@dns11.test", "192.0.2.1", NULL,
			SPF_RESULT_PERMERROR, SPF_E_BIG_DNS);
}

/* ---- redirect= and exp= ---- */

static void explanation(const char *what, const char *from, size_t want_len)
{
	SPF_request_t	*req = SPF_request_new(srv);
	SPF_response_t	*resp = NULL;
	const char		*e;
	char			 detail[64];

	SPF_request_set_ipv4_str(req, "192.0.2.1");
	SPF_request_set_env_from(req, from);
	SPF_request_query_mailfrom(req, &resp);
	e = SPF_response_get_explanation(resp);
	snprintf(detail, sizeof(detail), "length %zu", e ? strlen(e) : 0);
	check(e && strlen(e) == want_len, what, detail);
	SPF_response_free(resp);
	SPF_request_free(req);
}

static void test_modifiers(void)
{
	txt("target.test", "v=spf1 -all");
	txt("open.test", "v=spf1 +all");
	txt("redirfirst.test", "v=spf1 redirect=target.test ip4:192.0.2.1");
	txt("redirtwice.test", "v=spf1 redirect=target.test redirect=open.test");
	txt("redirbad.test", "v=spf1 redirect=open.test redirect=%{z}");
	txt("exponce.test", "v=spf1 -all exp=e.target.test");
	txt("exptwice.test", "v=spf1 -all exp=e.target.test exp=f.target.test");

	query("redirect= before a matching mechanism", "u@redirfirst.test", "192.0.2.1",
			NULL, SPF_RESULT_PASS, SPF_E_SUCCESS);
	query("redirect= when nothing matched", "u@redirfirst.test", "192.0.2.9",
			NULL, SPF_RESULT_FAIL, SPF_E_SUCCESS);
	query("two redirect=", "u@redirtwice.test", "192.0.2.1", NULL,
			SPF_RESULT_PERMERROR, SPF_E_SYNTAX);
	query("valid redirect= then invalid redirect=", "u@redirbad.test", "192.0.2.1",
			NULL, SPF_RESULT_PERMERROR, SPF_E_SYNTAX);
	query("one exp=", "u@exponce.test", "192.0.2.1", NULL,
			SPF_RESULT_FAIL, SPF_E_SUCCESS);
	query("two exp=", "u@exptwice.test", "192.0.2.1", NULL,
			SPF_RESULT_PERMERROR, SPF_E_SYNTAX);

	char text[601];
	memset(text, 'a', 600);
	text[600] = '\0';
	txt("e.long.test", text);
	txt("explong.test", "v=spf1 -all exp=e.long.test");
	explanation("600-char explanation kept whole", "u@explong.test", 600);

	/* exp= target of 316 chars: 255 after one label, 194 after two */
	char rec[600], *p = rec + sprintf(rec, "v=spf1 -all exp=");
	for (int i = 0; i < 5; i++, p += 61)
		sprintf(p, "%c%059d.", 'f' + i, 0);
	strcpy(p, "e.long.test");
	txt(strchr(strchr(rec + 16, '.') + 1, '.') + 1, "explained");
	txt("exptrunc.test", rec);
	explanation("exp= with a 316-char name", "u@exptrunc.test", 9);
}

/* ---- empty domain-spec ---- */

static void test_empty_domain(void)
{
	static const char *bad[] = { "a:", "a:/24", "a://64", "mx:", "ptr:",
								 "include:", "exists:", "redirect=", "exp=" };
	char rec[64], what[64];

	for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
		snprintf(rec, sizeof(rec), "v=spf1 ip4:192.0.2.1 %s -all", bad[i]);
		snprintf(what, sizeof(what), "empty domain-spec: %s", bad[i]);
		query(what, "u@nospf.test", "192.0.2.1", rec,
				SPF_RESULT_PERMERROR, SPF_E_MISSING_OPT);
	}
	query("unknown modifier with an empty value", "u@nospf.test", "192.0.2.1",
			"v=spf1 ip4:192.0.2.1 x= -all", SPF_RESULT_PASS, SPF_E_SUCCESS);
	query("a with a CIDR and no domain", "u@nospf.test", "192.0.2.9",
			"v=spf1 a/24 -all", SPF_RESULT_FAIL, SPF_E_SUCCESS);
}

/* ---- macro truncation ---- */

static void expand(const char *from, const char *helo, const char *macro,
		const char *want)
{
	SPF_request_t	*req = SPF_request_new(srv);
	SPF_response_t	*resp = SPF_response_new(req);
	SPF_macro_t		*mac = NULL;
	char			*buf = NULL, what[128], detail[128];
	size_t			 buflen = 0;
	int				 ok;

	SPF_request_set_ipv4_str(req, "192.0.2.1");
	SPF_request_set_env_from(req, from);
	SPF_request_set_helo_dom(req, helo);
	ok = SPF_record_compile_macro(srv, resp, &mac, macro) == SPF_E_SUCCESS
		&& SPF_record_expand_data(srv, req, resp, SPF_macro_data(mac),
				mac->macro_len, &buf, &buflen) == SPF_E_SUCCESS
		&& strcmp(buf, want) == 0;
	snprintf(what, sizeof(what), "%s with %s / HELO \"%s\" -> \"%s\"",
			macro, from, helo, want);
	snprintf(detail, sizeof(detail), "got \"%s\"", buf ? buf : "(null)");
	check(ok, what, ok ? NULL : detail);
	free(buf);
	if (mac)
		SPF_macro_free(mac);
	SPF_response_free(resp);
	SPF_request_free(req);
}

static void test_macros(void)
{
	expand("user@a.b.c", "h.test", "%{l1}", "user");
	expand("user@a.b.c", "h.test", "%{o1}", "c");
	expand("user@a.b.c", "h.test", "%{o2}", "b.c");
	expand("user@a.b.c", "h.test", "%{o5}", "a.b.c");
	expand("user@a.b.c", "h.test", "%{o2r}", "b.a");
	expand("-foo@a.test", "h.test", "%{l1-}", "foo");
	expand("-foo@a.test", "h.test", "%{l2-}", ".foo");
	expand("-foo@a.test", "h.test", "%{l1r-}", "");
	expand("-foo@a.test", "h.test", "%{l2r-}", "foo.");
	expand("user@a.test", "", "%{h1}", "");
	expand("user@a.test", "", "%{h1r}", "");
}

/* ---- domain names over 253 chars ---- */

static void test_truncation(void)
{
	/* five 60-char labels + "long.test": 314 chars, exactly 253 after
	 * dropping the first label */
	char long_name[400] = "", short_name[400], rec[600];
	const char *tail;

	for (int i = 0; i < 5; i++) {
		char *p = long_name + strlen(long_name);
		memset(p, 'a' + i, 60);
		strcpy(p + 60, ".");
	}
	strcat(long_name, "long.test");
	tail = strchr(long_name, '.') + 1;
	strcpy(short_name, tail);

	SPF_dns_zone_add_str(zone, short_name, ns_t_a, NETDB_SUCCESS, "192.0.2.1");
	SPF_dns_zone_add_str(zone, short_name, ns_t_txt, NETDB_SUCCESS, "v=spf1 +all");

	snprintf(rec, sizeof(rec), "v=spf1 a:%s -all", long_name);
	txt("trunca.test", rec);
	query("a: with a 314-char name", "u@trunca.test", "192.0.2.1", NULL,
			SPF_RESULT_PASS, SPF_E_SUCCESS);
	/* 63.63.63.61: 253 chars, 254 with the root dot, both kept */
	char max_name[300] = "";
	for (int i = 0; i < 4; i++) {
		char *p = max_name + strlen(max_name);
		memset(p, 'm' + i, i < 3 ? 63 : 61);
		strcpy(p + (i < 3 ? 63 : 61), i < 3 ? "." : "");
	}
	SPF_dns_zone_add_str(zone, max_name, ns_t_a, NETDB_SUCCESS, "192.0.2.1");
	snprintf(rec, sizeof(rec), "v=spf1 a:%s -all", max_name);
	txt("truncmax.test", rec);
	query("a: with a 253-char name", "u@truncmax.test", "192.0.2.1", NULL,
			SPF_RESULT_PASS, SPF_E_SUCCESS);
	snprintf(rec, sizeof(rec), "v=spf1 a:%s. -all", max_name);
	txt("truncmaxdot.test", rec);
	query("a: with a 253-char name and the root dot", "u@truncmaxdot.test", "192.0.2.1",
			NULL, SPF_RESULT_PASS, SPF_E_SUCCESS);

	snprintf(rec, sizeof(rec), "v=spf1 exists:%s -all", long_name);
	txt("truncexists.test", rec);
	query("exists: with a 314-char name", "u@truncexists.test", "192.0.2.9", NULL,
			SPF_RESULT_PASS, SPF_E_SUCCESS);
	snprintf(rec, sizeof(rec), "v=spf1 include:%s -all", long_name);
	txt("truncinclude.test", rec);
	query("include: with a 314-char name", "u@truncinclude.test", "192.0.2.9", NULL,
			SPF_RESULT_PASS, SPF_E_SUCCESS);
}

/* ---- printing a compiled record ---- */

/* Fill the alignment padding after each modifier string block with
 * pad, repeated: the printer must not read past a literal. */
static void fill_padding(SPF_record_t *r, const char *pad)
{
	SPF_mod_t *mod = r->mod_first;

	for (int i = 0; i < r->num_mod; i++, mod = SPF_mod_next(mod))
		for (SPF_data_t *d = SPF_mod_data(mod); d < SPF_mod_end_data(mod);
				d = SPF_data_next(d)) {
			if (d->ds.parm_type != PARM_STRING)
				continue;
			char *p = SPF_data_str(d) + d->ds.len;
			for (int j = 0; p < (char *)SPF_data_next(d); j++)
				*p++ = pad[j % strlen(pad)];
		}
}

static char *stringify(const char *rec, const char *pad)
{
	SPF_response_t	*resp = SPF_response_new(NULL);
	SPF_record_t	*r = NULL;
	char			*buf = NULL;
	size_t			 len = 0;

	if (SPF_record_compile(srv, resp, &r, rec) == SPF_E_SUCCESS) {
		fill_padding(r, pad);
		SPF_record_stringify(r, &buf, &len);
	}
	if (r)
		SPF_record_free(r);
	SPF_response_free(resp);
	return buf;
}

/* rec is in the printer's own form, so it must come back unchanged */
static void prints_back(const char *what, const char *rec, const char *pad)
{
	char	*out = stringify(rec, pad);
	char	 detail[64];
	int		 ok = out && strcmp(out, rec) == 0;
	size_t	 n = out ? strlen(out) : 0;

	snprintf(detail, sizeof(detail), "ends in \"%s\"",
			out ? out + n - (n < 12 ? n : 12) : "(null)");
	check(ok, what, ok ? NULL : detail);
	free(out);
}

static void test_print(void)
{
	char rec[400];

	prints_back("literal ending in %, padding \"20\"", "v=spf1 -all x=%%", "20");
	prints_back("literal ending in %2, padding \"0\"", "v=spf1 -all x=%%2", "0");
	/* 254 + '%' fill one 255-char block, "2" starts the next */
	snprintf(rec, sizeof(rec), "v=spf1 -all x=%0254d%%%%2", 0);
	prints_back("% at the end of a full string block", rec, "0");
	snprintf(rec, sizeof(rec), "v=spf1 -all x=%0253d%%%%2", 0);
	prints_back("%2 at the end of a full string block", rec, "0");
}

/* ---- DNS cache TTL ---- */

static void test_cache_ttl(void)
{
	SPF_dns_server_t	*z = SPF_dns_zone_new(NULL, "ttl", 0);
	SPF_dns_server_t	*cache;
	SPF_dns_rr_t		*rr;
	char				 detail[64];

	SPF_dns_zone_add_str(z, "spf.test", ns_t_spf, NETDB_SUCCESS, "v=spf1 -all");
	cache = SPF_dns_cache_new(z, "cache", 0, 8);
	/* the first lookup fills the cache, the second is served from it */
	SPF_dns_rr_free(SPF_dns_lookup(cache, "spf.test", ns_t_spf, TRUE));
	rr = SPF_dns_lookup(cache, "spf.test", ns_t_spf, TRUE);
	snprintf(detail, sizeof(detail), "ttl=%ld", rr ? (long)rr->ttl : -1L);
	check(rr && rr->ttl == 24 * 60 * 60,
			"cache keeps the 86400 s TTL of an SPF-type record", detail);
	SPF_dns_rr_free(rr);
	SPF_dns_free(cache);	/* frees the zone below it too */
}

int main(void)
{
	SPF_dns_server_t *fake = calloc(1, sizeof(*fake));

	setvbuf(stdout, NULL, _IOLBF, 0);

	zone = SPF_dns_zone_new(NULL, "zone", 0);
	fake->lookup = fake_lookup;
	fake->layer_below = zone;
	fake->name = "fake";
	srv = SPF_server_new_dns(fake, 0);

	test_limits();
	test_results();
	test_dns_limit();
	test_modifiers();
	test_empty_domain();
	test_macros();
	test_truncation();
	test_print();
	test_cache_ttl();

	/* a resolver given to SPF_server_new_dns() and one without a
	 * destructor are the caller's to free */
	SPF_server_free(srv);
	SPF_dns_free(zone);
	free(fake);

	printf("%d failure(s)\n", failures);
	return failures != 0;
}
