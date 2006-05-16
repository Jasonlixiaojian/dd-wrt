/*
 * Wireless Network Adapter Configuration Utility
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmparams.h>
#include <shutils.h>
#include <wlutils.h>

/* phy types */
#define	PHY_TYPE_A		0
#define	PHY_TYPE_B		1
#define	PHY_TYPE_G		2
#define	PHY_TYPE_NULL		0xf

/* how many times to attempt to bring up a virtual i/f when
	 we are in APSTA mode and IOVAR set of "bss" "up" returns busy */
#define MAX_BSS_UP_RETRIES 5

/* parts of an idcode: */
#define	IDCODE_MFG_MASK		0x00000fff
#define	IDCODE_MFG_SHIFT	0
#define	IDCODE_ID_MASK		0x0ffff000
#define	IDCODE_ID_SHIFT		12
#define	IDCODE_REV_MASK		0xf0000000
#define	IDCODE_REV_SHIFT	28

/*
 * Debugging Macros
 */
#define WLCONF_DBG(fmt, arg...) cprintf(fmt,## arg)
#define WL_IOCTL(name, cmd, buf, len)			(ret = wl_ioctl(name, cmd, buf, len))
#define WL_SETINT(name, cmd, val)			(ret = wlconf_setint(name, cmd, val))
#define WL_GETINT(name, cmd, pval)			(ret = wlconf_getint(name, cmd, pval))
#define WL_IOVAR_SET(ifname, iovar, param, paramlen)	(ret = wl_iovar_set(ifname, iovar, param, paramlen))
#define WL_IOVAR_SETINT(ifname, iovar, val)		(ret = wl_iovar_setint(ifname, iovar, val))
#define WL_BSSIOVAR_SETBUF(ifname, iovar, bssidx, param, paramlen, buf, buflen) \
		(ret = wl_bssiovar_setbuf(ifname, iovar, bssidx, param, paramlen, buf, buflen))
#define WL_BSSIOVAR_SET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_set(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_GET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_get(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)	(ret = wl_bssiovar_setint(ifname, iovar, bssidx, val))

static int
wlconf_getint(char* ifname, int cmd, int *pval)
{
	return wl_ioctl(ifname, cmd, pval, sizeof(int));
}

static int
wlconf_setint(char* ifname, int cmd, int val)
{
	return wl_ioctl(ifname, cmd, &val, sizeof(int));
}

/* set WEP key */
static int
wlconf_set_wep_key(char *name, char *prefix, int bsscfg_idx, int i)
{
	wl_wsec_key_t key;
	char wl_key[] = "wlXXXXXXXXXX_keyXXXXXXXXXX";
	char *keystr, hex[] = "XX";
	unsigned char *data = key.data;
	int ret = 0;

	memset(&key, 0, sizeof(key));
	key.index = i - 1;
	sprintf(wl_key, "%skey%d", prefix, i);
	keystr = nvram_safe_get(wl_key);

	switch (strlen(keystr)) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		key.len = strlen(keystr);
		strcpy(key.data, keystr);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		key.len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (unsigned char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		key.len = 0;
		break;
	}

	/* Set current WEP key */
	if (key.len && i == atoi(nvram_safe_get(strcat_r(prefix, "key", wl_key))))
		key.flags = WL_PRIMARY_KEY;

	WL_BSSIOVAR_SET(name, "wsec_key", bsscfg_idx, &key, sizeof(key));

	return ret;
}

extern struct nvram_tuple router_defaults[];

/* Keep this table in order */
static struct {
	int locale;
	char **names;
	char *abbr;
} countries[] = {
	{ WLC_WW,  ((char *[]) { "Worldwide", "WW", NULL }), "AU" },
	{ WLC_THA, ((char *[]) { "Thailand", "THA", NULL }), "TH" },
	{ WLC_ISR, ((char *[]) { "Israel", "ISR", NULL }), "IL" },
	{ WLC_JDN, ((char *[]) { "Jordan", "JDN", NULL }), "JO" },
	{ WLC_PRC, ((char *[]) { "China", "P.R. China", "PRC", NULL }), "CN" },
	{ WLC_JPN, ((char *[]) { "Japan", "JPN", NULL }), "JP" },
	{ WLC_FCC, ((char *[]) { "USA", "Canada", "ANZ", "New Zealand", "FCC", NULL }), "US" },
	{ WLC_EUR, ((char *[]) { "Europe", "EUR", NULL }), "DE" },
	{ WLC_USL, ((char *[]) { "USA Low", "USALow", "USL", NULL }), "US" },
	{ WLC_JPH, ((char *[]) { "Japan High", "JapanHigh", "JPH", NULL }), "JP" },
	{ WLC_ALL, ((char *[]) { "All", "AllTheChannels", NULL }), "All" },
};

/* validate/restore all per-interface related variables */
static void
wlconf_validate_all(char *prefix, bool restore)
{
	struct nvram_tuple *t;
	char tmp[100];
	char *v;
	for (t = router_defaults; t->name; t++) {
		if (!strncmp(t->name, "wl_", 3)) {
			strcat_r(prefix, &t->name[3], tmp);
			if (!restore && nvram_get(tmp))
				continue;
			v = nvram_get(t->name);
			nvram_set(tmp, v ? v : t->value);
		}
	}
}

/* restore specific per-interface variable */
static void
wlconf_restore_var(char *prefix, char *name)
{
	struct nvram_tuple *t;
	char tmp[100];
	for (t = router_defaults; t->name; t++) {
		if (!strncmp(t->name, "wl_", 3) && !strcmp(&t->name[3], name)) {
			nvram_set(strcat_r(prefix, name, tmp), t->value);
			break;
		}
	}
}
static int 
wlconf_akm_options(char *prefix)
{
        char comb[32];
        char *wl_akm;
        int akm_ret_val = 0;
        char akm[32];
        char *next;
	cprintf("get akm options for prefix %s\n",prefix);
        wl_akm = nvram_safe_get(strcat_r(prefix, "akm", comb));
	foreach(akm, wl_akm, next) {
		if (!strcmp(akm, "wpa"))
			akm_ret_val |= WPA_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk"))
			akm_ret_val |= WPA_AUTH_PSK;
#ifdef BCMWPA2
		if (!strcmp(akm, "wpa2"))
			akm_ret_val |= WPA2_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk2"))
			akm_ret_val |= WPA2_AUTH_PSK;
#endif
	}
	return akm_ret_val;
}


/* Set up wsec */
static int
wlconf_set_wsec(char *ifname, char *prefix, int bsscfg_idx)
{
	char tmp[100];
	int val = 0;
	int akm_val;
	int ret;
	
	/* Set wsec bitvec */
	cprintf("set security for prefix %s\n",prefix);
	akm_val = wlconf_akm_options(prefix); 
	if (akm_val != 0) {
		if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
			val = TKIP_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
			val = AES_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip+aes"))
			val = TKIP_ENABLED | AES_ENABLED;
	}
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled"))
		val |= WEP_ENABLED;
	WL_BSSIOVAR_SETINT(ifname, "wsec", bsscfg_idx, val);
	/* Set wsec restrict if WSEC_ENABLED */
	WL_BSSIOVAR_SETINT(ifname, "wsec_restrict", bsscfg_idx, val ? 1 : 0);


	return 0;
}

#ifdef BCMWPA2
static int 
wlconf_set_preauth(char *name, int bsscfg_idx, int preauth)
{
	int8 cap[WPA_CAP_LEN];
	int ret;

	WL_BSSIOVAR_GET(name, "wpa_cap", bsscfg_idx, cap, WPA_CAP_LEN);
	if (ret != 0) return -1;

	if (preauth)
		cap[0] |= WPA_CAP_WPA2_PREAUTH;
	else
		cap[0] &= ~WPA_CAP_WPA2_PREAUTH; 

	WL_BSSIOVAR_SET(name, "wpa_cap", bsscfg_idx, cap, WPA_CAP_LEN);

	return ret;
}
#endif

/* Set up WME */
static void
wlconf_set_wme(char *name, char *prefix)
{
	int i, j, k;
	int val, ret;
	int phytype, gmode, no_ack;
	wme_acparam_t *acparams;
	char buf[WLC_IOCTL_MAXLEN];
	char *v, *nv_value, nv[100];
	char nv_name[] = "%swme_%s_%s";
	char *ac[] = {"be", "bk", "vi", "vo"};
	char *cwmin, *cwmax, *aifsn, *txop_b, *txop_ag, *admin_forced;
	char **locals[] = {&cwmin, &cwmax, &aifsn, &txop_b, &txop_ag, &admin_forced};
	struct {char *req; char *str;} mode[] = {{"wme_ac_sta", "sta"}, {"wme_ac_ap", "ap"}};

	/* query the phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	/* get gmode */
	gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", nv)));

	/* WME sta setting first */
	for (i = 0; i < 2; i++) {
		/* build request block */
		memset(buf, 0, sizeof(buf));
		strcpy(buf, mode[i].req);
		/* put push wmeac params after "wme-ac" in buf */
		acparams = (wme_acparam_t *)(buf + strlen(buf) + 1);
		for (j = 0; j < AC_MAX; j++) {
			/* get packed nvram parameter */
			snprintf(nv, sizeof(nv), nv_name, prefix, mode[i].str, ac[j]);
			nv_value = nvram_safe_get(nv);
			strcpy(nv, nv_value);
			/* unpack it */
			for (v = nv, k = 0; (int)(v - nv) < strlen(nv_value) && 
				k < (sizeof(locals)/sizeof(locals[0])); k++) {
				*locals[k] = v;
				while((int)(v - nv) < strlen(nv_value) && *v != ' ')
					v++;
				*v = 0;
				v++;
			}
			if (k != (sizeof(locals)/sizeof(locals[0])))
				return;

			/* update CWmin */
			acparams->ECW &= ~WME_CWMIN_MASK;
			val = atoi(cwmin);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= (k ? k - 1 : 0) & WME_CWMIN_MASK;
			/* update CWmax */
			acparams->ECW &= ~WME_CWMAX_MASK;
			val = atoi(cwmax);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= ((k ? k - 1 : 0) << WME_CWMAX_SHIFT) & WME_CWMAX_MASK;
			/* update AIFSN */
			acparams->ACI &= ~WME_AIFS_MASK;
			acparams->ACI |= atoi(aifsn) & WME_AIFS_MASK;
			/* update ac */
			acparams->ACI &= ~WME_ACI_MASK;
			acparams->ACI |= j << WME_ACI_SHIFT;
			/* update TXOP */
			if (phytype == PHY_TYPE_B || gmode == 0)
				val = atoi(txop_b);
			else
				val = atoi(txop_ag);
			acparams->TXOP = val / 32;
			/* update acm */
			acparams->ACI &= ~WME_ACM_MASK;
			val = strcmp(admin_forced, "on") ? 0 : 1;
			acparams->ACI |= val << 4;

			/* configure driver */
			WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));
		}
	}

	/* set no-ack */
	v = nvram_safe_get(strcat_r(prefix, "wme_no_ack", nv));
	no_ack = strcmp(v, "on") ? 0 : 1;
	
	WL_IOVAR_SETINT(name, "wme_noack", no_ack);
}

#if defined(linux)
#include <unistd.h>
static void
sleep_ms(const unsigned int ms)
{
	usleep(1000*ms);
}
#endif

/*
* The following condition(s) must be met when Auto Channel Selection 
* is enabled.
*  - the I/F is up (change radio channel requires it is up?)
*  - the AP must not be associated (setting SSID to empty should 
*    make sure it for us)
*/
static uint8
wlconf_auto_channel(char *name)
{
	int chosen = 0;
	wl_uint32_list_t request;
	int phytype;
	int ret;
	int i;
	
	/* query the phy type */
	WL_GETINT(name, WLC_GET_PHYTYPE, &phytype);

	request.count = 0;	/* let the ioctl decide */
	WL_IOCTL(name, WLC_START_CHANNEL_SEL, &request, sizeof(request));
	if (!ret) {
		sleep_ms(phytype == PHY_TYPE_A ? 1000 : 750);
		for (i = 0; i < 100; i++) {
			WL_GETINT(name, WLC_GET_CHANNEL_SEL, &chosen);
			if (!ret)
				break;
			sleep_ms(100);
		}
	}
	WLCONF_DBG("interface %s: channel selected %d\n", name, chosen);
	return chosen;
}

/* PHY type/BAND conversion */
#define WLCONF_PHYTYPE2BAND(phy)	((phy) == PHY_TYPE_A ? WLC_BAND_A : WLC_BAND_B)
/* PHY type conversion */
#define WLCONF_PHYTYPE2STR(phy)	((phy) == PHY_TYPE_A ? "a" : \
				 (phy) == PHY_TYPE_B ? "b" : "g")
#define WLCONF_STR2PHYTYPE(phy)	((phy) && (phy)[0] == 'a' ? PHY_TYPE_A : \
				 (phy) && (phy)[0] == 'b' ? PHY_TYPE_B : PHY_TYPE_G)
				 
#define PREFIX_LEN 32			/* buffer size for wlXXX_ prefix */

struct bsscfg_info {
	int idx;			/* bsscfg index */
	char ifname[PREFIX_LEN];	/* OS name of interface (debug only) */
	char prefix[PREFIX_LEN];	/* prefix for nvram params (eg. "wl0.1_") */
};

struct bsscfg_list {
	int count;
	struct bsscfg_info bsscfgs[WL_MAXBSSCFG];
};

struct bsscfg_list *
wlconf_get_bsscfgs(char* ifname, char* prefix)
{
	char var[80];
	char tmp[100];
	char *next;
	
	struct bsscfg_list *bclist;
	struct bsscfg_info *bsscfg;
	
	bclist = (struct bsscfg_list*)malloc(sizeof(struct bsscfg_list));
	if (bclist == NULL)
		return NULL;
	memset(bclist, 0, sizeof(struct bsscfg_list));

	/* Set up Primary BSS Config information */
	bsscfg = &bclist->bsscfgs[0];
	bsscfg->idx = 0;
	strncpy(bsscfg->ifname, ifname, PREFIX_LEN-1);
	strcpy(bsscfg->prefix, prefix);
	bclist->count = 1;

	/* additional virtual BSS Configs from wlX_vifs */
	foreach(var, nvram_safe_get(strcat_r(prefix, "vifs", tmp)), next) {
		if (bclist->count == WL_MAXBSSCFG) {
			WLCONF_DBG("wlconf(%s): exceeded max number of BSS Configs (%d) in nvram %s\n"
			           "while configuring interface \"%s\"\n",
			           ifname, WL_MAXBSSCFG, strcat_r(prefix, "vifs", tmp), var);
			continue;
		}
		bsscfg = &bclist->bsscfgs[bclist->count];
		if (get_ifname_unit(var, NULL, &bsscfg->idx) != 0) {
			WLCONF_DBG("wlconfg(%s): unable to parse unit.subunit in interface name \"%s\"\n",
			           ifname, var);
			continue;
		}
		strncpy(bsscfg->ifname, var, PREFIX_LEN-1);
		snprintf(bsscfg->prefix, PREFIX_LEN, "%s_", bsscfg->ifname);
		bclist->count++;
	}

	return bclist;
}

static void
wlconf_security_options(char *name, char *prefix, int bsscfg_idx, bool wet)
{
	int i;
	int val;
	int ret;
	char tmp[100];

	/* Set WSEC */
	/*
	* Need to check errors (card may have changed) and change to
	* defaults since the new chip may not support the requested 
	* encryptions after the card has been changed.
	*/
	if (wlconf_set_wsec(name, prefix, bsscfg_idx)) {
		/* change nvram only, code below will pass them on */
		wlconf_restore_var(prefix, "auth_mode");
		wlconf_restore_var(prefix, "auth");
		/* reset wep to default */
		wlconf_restore_var(prefix, "crypto");
		wlconf_restore_var(prefix, "wep");
		wlconf_set_wsec(name, prefix, bsscfg_idx);
	}

 	val = wlconf_akm_options(prefix);
	/* In wet mode enable in driver wpa supplicant */
	if (wet && ((val & WPA_AUTH_PSK)
#ifdef BCMWPA2
		    || (val & WPA2_AUTH_PSK)
#endif
	    )) {
		wsec_pmk_t psk;
		char *key;

		if (((key = nvram_get(strcat_r(prefix, "wpa_psk", tmp))) != NULL)
				&& (strlen(key) < WSEC_MAX_PSK_LEN)) {
			psk.key_len = (ushort) strlen(key);
			psk.flags = WSEC_PASSPHRASE;
			strcpy(psk.key, key);
			WL_IOCTL(name, WLC_SET_WSEC_PMK, &psk, sizeof(psk));
		}
		wl_iovar_setint(name, "sup_wpa", 1);
	}
	WL_BSSIOVAR_SETINT(name, "wpa_auth", bsscfg_idx, val);

	/* EAP Restrict if we have an AKM or radius authentication */
	val = ((val != 0) || (nvram_match(strcat_r(prefix, "auth_mode", tmp), "radius")));
	WL_BSSIOVAR_SETINT(name, "eap_restrict", bsscfg_idx, val);
	
	/* Set WEP keys */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled")) {
		for (i = 1; i <= DOT11_MAX_DEFAULT_KEYS; i++)
			wlconf_set_wep_key(name, prefix, bsscfg_idx, i);
	}

	/* Set 802.11 authentication mode - open/shared */
	val = atoi(nvram_safe_get(strcat_r(prefix, "auth", tmp)));
	WL_BSSIOVAR_SETINT(name, "auth", bsscfg_idx, val);
}

/* configure the specified wireless interface */
int
wlconf(char *name)
{
	int restore_defaults, val, unit, phytype, gmode = 0, ret = 0;
	int bcmerr;
	int error_bg, error_a;
	struct bsscfg_list *bclist = NULL;
	struct bsscfg_info *bsscfg;
	char tmp[100], prefix[PREFIX_LEN];
	char var[80], *next, phy[] = "a", *str;
	unsigned char buf[WLC_IOCTL_MAXLEN];
	char *country;
	wlc_rev_info_t rev;
	channel_info_t ci;
	struct maclist *maclist;
	struct ether_addr *ea;
	wlc_ssid_t ssid;
	wl_rateset_t rs;
	unsigned int i;
	char eaddr[32];
	int ap, apsta, wds, sta = 0, wet = 0;
	char country_code[4];
	int nas_will_run = 0;
	char *afterburner;
	char *wme;
#ifdef BCMWPA2
	char *preauth;
	int set_preauth;
#endif
	int ii;
	int wlunit = -1;
	int wlsubunit = -1;
	int wl_ap_build = 0; /* wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_SMLEN];

	/* wlconf doesn't work for virtual i/f, so if we are given a 
	   virtual i/f return 0 if that interface is in it's parent's "vifs"
	   list otherwise return -1 */
	cprintf("get_ifname_unit\n");
	if( get_ifname_unit( name, &wlunit, &wlsubunit ) == 0 )
	{
		if( wlsubunit >= 0 )
		{
			/* we have been given a virtual i/f, 
				 is it in it's parent i/f's virtual i/f list? */
			sprintf( tmp, "wl%d_vifs", wlunit );
			
			if( strstr( nvram_safe_get( tmp ), name ) == NULL )
				return -1; /* config error */
			else
 				return 0; /* okay */
		}
	}
	else
	{
		return -1;
	}

	/* clean up tmp */
	memset(tmp, 0, sizeof(tmp) );

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we 
		 can't use the same iovars to configure the wl.
		 so we use "wl_ap_build" to help us know how to configure the driver */
	if (wl_iovar_get(name, "cap", (void *)caps, WLC_IOCTL_SMLEN))
		return -1;
	
	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		}
	}

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* Get MAC address */
	(void) wl_hwaddr(name, buf);

	/* Get instance */
	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	/* Restore defaults if per-interface parameters do not exist */
	restore_defaults = !nvram_get(strcat_r(prefix, "ifname", tmp));
	wlconf_validate_all(prefix, restore_defaults);
	nvram_set(strcat_r(prefix, "ifname", tmp), name);
	nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa(buf, eaddr));
	snprintf(buf, sizeof(buf), "%d", unit);
	nvram_set(strcat_r(prefix, "unit", tmp), buf);


	/* Bring the interface down */
	WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));

	/* Disable all BSS Configs */
	cprintf("disable all virtual configs\n");
	for (i = 0; i < WL_MAXBSSCFG; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;
		setbuf.bsscfg_idx = i;
		setbuf.enable = 0;

		ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
		if (ret) {
			wl_iovar_getint(name, "bcmerror", &bcmerr);
			/* fail quietly on a range error since the driver may
			 * support fewer bsscfgs than we are prepared to configure
			 */
			if (bcmerr == BCME_RANGE)
				break;
		}
		if (ret)
			WLCONF_DBG("%d:(%s): setting bsscfg #%d iovar \"bss\" to 0 (down) failed, ret = %d, bcmerr = %d\n",
				__LINE__, name, i, ret, bcmerr);
	}


	/* Get the list of BSS Configs */
	cprintf("get new virtual config\n");
	bclist = wlconf_get_bsscfgs(name, prefix);
	if (bclist == NULL) {
		ret = -1;
		goto exit;
	}


	/* create a wlX.Y_ifname nvram setting */
	for (i = 1; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
#if defined(linux)
		strcpy(var, bsscfg->ifname);
#endif
		nvram_set(strcat_r(bsscfg->prefix, "ifname", tmp), var);
	}

	if( wl_ap_build ) {	
		/* Enable MSSID mode if appropriate */
		WL_IOVAR_SETINT(name, "mssid", (bclist->count > 1));
		WL_IOVAR_SETINT(name, "mssid_extended", (bclist->count > 1));

		/*
		 * Set SSID for each BSS Config
		 */
		for (i = 0; i < bclist->count; i++) {
			cprintf("set ssid for each virtual config %d\n",i);
			bsscfg = &bclist->bsscfgs[i];
			strcat_r(bsscfg->prefix, "ssid", tmp);
			ssid.SSID_len = strlen(nvram_safe_get(tmp));
			if (ssid.SSID_len > sizeof(ssid.SSID))
				ssid.SSID_len = sizeof(ssid.SSID);
			strncpy(ssid.SSID, nvram_safe_get(tmp), ssid.SSID_len);
			WLCONF_DBG("wlconfig(%s): configuring bsscfg #%d (%s) with SSID \"%.*s\"\n",
								 name, bsscfg->idx, bsscfg->ifname, ssid.SSID_len, ssid.SSID);
			WL_BSSIOVAR_SET(name, "ssid", bsscfg->idx, &ssid, sizeof(ssid));
			sleep(1);
		}
	}

	/* wlX_mode settings: AP, STA, WET, BSS/IBSS, APSTA */
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	ap = (!strcmp(str, "") || !strcmp(str, "ap") || !strcmp(str, "mssid"));
	apsta = (!strcmp(str, "apsta") || (!strcmp(str, "sta") && bclist->count > 1));
	sta = (!strcmp(str, "sta") && bclist->count == 1);
	wds = !strcmp(str, "wds");
	wet = !strcmp(str, "wet");

	/* Set AP mode */
	val = (ap || apsta || wds) ? 1 : 0;
	WL_IOCTL(name, WLC_SET_AP, &val, sizeof(val));

	WL_IOVAR_SETINT(name, "apsta", apsta);

	/* Set mode: WET */
	if (wet)
		WL_IOCTL(name, WLC_SET_WET, &wet, sizeof(wet));

	/* Set infra: BSS/IBSS (IBSS only for WET or STA modes) */
	val = 1;
	if (wet || sta)
		val = atoi(nvram_safe_get(strcat_r(prefix, "infra", tmp)));
	WL_IOCTL(name, WLC_SET_INFRA, &val, sizeof(val));

	/* Set The AP MAX Associations Limit */
	if (ap | apsta) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "maxassoc", tmp)));
		if (val > 0)
			WL_IOVAR_SETINT(name, "maxassoc", val);
	}
	cprintf("set virtual security\n");
	for (i = 0; i < bclist->count; i++) {
		char *subprefix;
		bsscfg = &bclist->bsscfgs[i];

#ifdef BCMWPA2
		/* XXXMSSID: The note about setting preauth now does not seem right.
		 * NAS brings the BSS up if it runs, so setting the preauth value 
		 * will make it in the bcn/prb. If that is right, we can move this 
		 * chunk out of wlconf.
		 */
		/* 
		 * Set The WPA2 Pre auth cap. only reason we are doing it here is the driver is down
		 * if we do it in the NAS we need to bring down the interface and up to make 
		 * it affect in the  beacons 
		 */
		if (ap || (apsta && bsscfg->idx != 0)) {
			set_preauth = 1;
			preauth = nvram_safe_get(strcat_r(bsscfg->prefix, "preauth", tmp));
			if (strlen (preauth) != 0) {
				set_preauth = atoi(preauth);
			}
			wlconf_set_preauth(name, bsscfg->idx, set_preauth);
		}
#endif

		subprefix = apsta ? prefix : bsscfg->prefix;
		cprintf("closednet %d\n",bsscfg->idx);
		/* Set network type */
		val = atoi(nvram_safe_get(strcat_r(subprefix, "closed", tmp)));
		WL_BSSIOVAR_SETINT(name, "closednet", bsscfg->idx, val);
		cprintf("ap isolate %d\n",bsscfg->idx);
		/* Set the ap isolate mode */
		val = atoi(nvram_safe_get(strcat_r(subprefix, "ap_isolate", tmp)));
		WL_BSSIOVAR_SETINT(name, "ap_isolate", bsscfg->idx, val);
	}

	/* Set up the country code */
	(void) strcat_r(prefix, "country_code", tmp);
	country = nvram_get(tmp);
	if (country) {
		strncpy(country_code, country, sizeof(country_code));
		WL_IOCTL(name, WLC_SET_COUNTRY, country_code, strlen(country_code)+1);
	}
	else {
		/* If country_code doesn't exist, check for country to be backward compatible */
		(void) strcat_r(prefix, "country", tmp);
		country = nvram_safe_get(tmp);
		for (val = 0; val < ARRAYSIZE(countries); val++) {
			char **synonym;
			for (synonym = countries[val].names; *synonym; synonym++)
				if (!strcmp(country, *synonym))
					break;
			if (*synonym)
				break;
		}

		/* Get the default country code if undefined or invalid and set the NVRAM */
		if (val >= ARRAYSIZE(countries)) {
			WL_IOCTL(name, WLC_GET_COUNTRY, country_code, sizeof(country_code));
		}
		else {
			strncpy(country_code, countries[val].abbr, sizeof(country_code));
			WL_IOCTL(name, WLC_SET_COUNTRY, country_code, strlen(country_code)+1);
		}

		/* Add the new NVRAM variable */
		nvram_set("wl_country_code", country_code);
		(void) strcat_r(prefix, "country_code", tmp);
		nvram_set(tmp, country_code);
	}

	/* Setup regulatory mode */
	strcat_r(prefix, "reg_mode", tmp);
	if (nvram_match(tmp, "off"))  {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
	} else if (nvram_match(tmp, "h")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));

		/* Set the CAC parameters */
		val = atoi(nvram_safe_get(strcat_r(prefix, "dfs_preism", tmp)));
		wl_iovar_setint(name, "dfs_preism", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "dfs_postism", tmp)));
		wl_iovar_setint(name, "dfs_postism", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "tpc_db", tmp)));
		WL_IOCTL(name, WLC_SEND_PWR_CONSTRAINT, &val, sizeof(val));

	} else if (nvram_match(tmp, "d")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
	}

	/* Set the MAC list */
	maclist = (struct maclist *) buf;
	maclist->count = 0;
	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {
		ea = maclist->ea;
		foreach(var, nvram_safe_get(strcat_r(prefix, "maclist", tmp)), next) {
			if ((&ea[1])->octet > &buf[sizeof(buf)])
				break;
			if (ether_atoe(var, ea->octet)) {
				maclist->count++;
				ea++;
			}
		}
	}
	WL_IOCTL(name, WLC_SET_MACLIST, buf, sizeof(buf));

	/* Set the MAC list mode */
	(void) strcat_r(prefix, "macmode", tmp);
	if (nvram_match(tmp, "deny"))
		val = WLC_MACMODE_DENY;
	else if (nvram_match(tmp, "allow"))
		val = WLC_MACMODE_ALLOW;
	else
		val = WLC_MACMODE_DISABLED;
	WL_IOCTL(name, WLC_SET_MACMODE, &val, sizeof(val));

	/* Enable or disable the radio */
	val = nvram_match(strcat_r(prefix, "radio", tmp), "0");
	val += WL_RADIO_SW_DISABLE << 16;
	WL_IOCTL(name, WLC_SET_RADIO, &val, sizeof(val));

	/* Get supported phy types */
	WL_IOCTL(name, WLC_GET_PHYLIST, var, sizeof(var));
	nvram_set(strcat_r(prefix, "phytypes", tmp), var);

	/* Get radio IDs */
	*(next = buf) = '\0';
	for (i = 0; i < strlen(var); i++) {
		/* Switch to band */
		phy[0] = var[i];
		val = WLCONF_STR2PHYTYPE(phy);
		val = WLCONF_PHYTYPE2BAND(val);
		WL_IOCTL(name, WLC_SET_BAND, &val, sizeof(val));
		/* Get radio ID on this band */
		WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
		next += sprintf(next, "%sBCM%X", i ? " " : "",
				(rev.radiorev & IDCODE_ID_MASK) >> IDCODE_ID_SHIFT);
	}
	nvram_set(strcat_r(prefix, "radioids", tmp), buf);

	/* Set band */
	str = nvram_get(strcat_r(prefix, "phytype", tmp));
	val = WLCONF_STR2PHYTYPE(str);
	val = WLCONF_PHYTYPE2BAND(val);
	WL_SETINT(name, WLC_SET_BAND, val);

	/* Check errors (card may have changed) */
	if (ret) {
		/* default band to the first band in band list */
		phy[0] = var[0];
		val = WLCONF_STR2PHYTYPE(phy);
		val = WLCONF_PHYTYPE2BAND(val);
		WL_SETINT(name, WLC_SET_BAND, val);
	}

	/* Get current core revision */
	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
	snprintf(buf, sizeof(buf), "%d", rev.corerev);
	nvram_set(strcat_r(prefix, "corerev", tmp), buf);

	/* Get current phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	snprintf(buf, sizeof(buf), "%s", WLCONF_PHYTYPE2STR(phytype));
	nvram_set(strcat_r(prefix, "phytype", tmp), buf);

	/* Set channel before setting gmode or rateset */
	/* Manual Channel Selection - when channel # is not 0 */
	val = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	if (val) {
		WL_SETINT(name, WLC_SET_CHANNEL, val);
		
		if (ret) {
			/* Use current channel (card may have changed) */
			WL_IOCTL(name, WLC_GET_CHANNEL, &ci, sizeof(ci));
			snprintf(buf, sizeof(buf), "%d", ci.target_channel);
			nvram_set(strcat_r(prefix, "channel", tmp), buf);
		}
	}
	
	/* Reset to hardware rateset (band may have changed) */
	WL_IOCTL(name, WLC_GET_RATESET, &rs, sizeof (wl_rateset_t));
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof (wl_rateset_t));

	/* Set gmode */
	if (phytype == PHY_TYPE_G) {

		int override = WLC_G_PROTECTION_OFF;
		int control = WLC_G_PROTECTION_CTL_OFF;

		/* Set gmode */
		gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", tmp)));
		WL_IOCTL(name, WLC_SET_GMODE, &gmode, sizeof(gmode));

		/* Set gmode protection override and control algorithm */
		strcat_r(prefix, "gmode_protection", tmp);
		if (nvram_match(tmp, "auto")) {
			override = WLC_G_PROTECTION_AUTO;
			control = WLC_G_PROTECTION_CTL_OVERLAP;
		}
		WL_IOCTL(name, WLC_SET_GMODE_PROTECTION_OVERRIDE, &override, sizeof(override));
		WL_IOCTL(name, WLC_SET_GMODE_PROTECTION_CONTROL, &control, sizeof(control));
	}

	/* Set AfterBurner option */
	wl_iovar_get(name, "cap", (void *)tmp, 100);
	/* search for "afterburner" string */
	foreach(var, tmp, next) {
		if (strncmp(var, "afterburner", sizeof(var)) == 0) {
			bool valid_option = TRUE;
			/* cap reported afterburner is supported.  get nvram afterburner option */
			afterburner = nvram_safe_get(strcat_r(prefix, "afterburner", tmp));
			if (!strcmp(afterburner, "on"))
				/* this is not an option from user */
				val = ON;
			else if (!strcmp(afterburner, "off"))
				val = OFF;
			else if (!strcmp(afterburner, "auto"))
				val = AUTO;
			else
				valid_option = FALSE;
			if (valid_option)
				wl_iovar_setint(name, "afterburner_override", val);
			break;
		}
	}

	/* Get current rateset (gmode may have changed) */
	WL_IOCTL(name, WLC_GET_CURR_RATESET, &rs, sizeof (wl_rateset_t));

	strcat_r(prefix, "rateset", tmp);
	if (nvram_match(tmp, "all"))  {
		/* Make all rates basic */
		for (i = 0; i < rs.count; i++)
			rs.rates[i] |= 0x80;
	} else if (nvram_match(tmp, "12")) {
		/* Make 1 and 2 basic */
		for (i = 0; i < rs.count; i++) {
			if ((rs.rates[i] & 0x7f) == 2 || (rs.rates[i] & 0x7f) == 4)
				rs.rates[i] |= 0x80;
			else
				rs.rates[i] &= ~0x80;
		}
	}

	/* Set rateset */
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof (wl_rateset_t));

	/* Allow short preamble override for b cards */
	if (phytype == PHY_TYPE_B || 
	    (phytype == PHY_TYPE_G && (gmode == GMODE_LEGACY_B || gmode == GMODE_AUTO))) {
		strcat_r(prefix, "plcphdr", tmp);		
		if (nvram_match(tmp, "long"))
			val = WLC_PLCP_AUTO;
		else
			val = WLC_PLCP_SHORT;
		WL_IOCTL(name, WLC_SET_PLCPHDR, &val, sizeof(val));
	}

	/* Set rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rate", tmp))) / 500000;

	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "bg_rate", val);
	error_a = wl_iovar_setint(name, "a_rate", val);

	if (error_bg && error_a) {
			/* both failed. Try default rate (card may have changed) */
		val = 0;
			
		error_bg = wl_iovar_setint(name, "bg_rate", val);
		error_a = wl_iovar_setint(name, "a_rate", val);
			
		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "rate", tmp), buf);
	}

	/* Set multicast rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "mrate", tmp))) / 500000;
	
	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "bg_mrate", val);
	error_a = wl_iovar_setint(name, "a_mrate", val);

	if (error_bg && error_a) {
		/* Try default rate (card may have changed) */
		val = 0;

		wl_iovar_setint(name, "bg_mrate", val);
		wl_iovar_setint(name, "a_mrate", val);
		
		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "mrate", tmp), buf);
	}

	/* Set fragmentation threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "frag", tmp)));
	wl_iovar_setint(name, "fragthresh", val);
	
	/* Set RTS threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rts", tmp)));
	wl_iovar_setint(name, "rtsthresh", val);

	/* Set DTIM period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "dtim", tmp)));
	WL_IOCTL(name, WLC_SET_DTIMPRD, &val, sizeof(val));

	/* Set beacon period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "bcn", tmp)));
	WL_IOCTL(name, WLC_SET_BCNPRD, &val, sizeof(val));

	/* AP only config */
	if (ap || apsta || wds) {
		/* Set lazy WDS mode */
		val = atoi(nvram_safe_get(strcat_r(prefix, "lazywds", tmp)));
		WL_IOCTL(name, WLC_SET_LAZYWDS, &val, sizeof(val));

		/* Set the WDS list */
		maclist = (struct maclist *) buf;
		maclist->count = 0;
		ea = maclist->ea;
		foreach(var, nvram_safe_get(strcat_r(prefix, "wds", tmp)), next) {
			if (ea->octet > &buf[sizeof(buf)])
				break;
			ether_atoe(var, ea->octet);
			maclist->count++;
			ea++;
		}
		WL_IOCTL(name, WLC_SET_WDSLIST, buf, sizeof(buf));

		/* Set WDS link detection timeout */ 
		val = atoi(nvram_safe_get(strcat_r(prefix, "wds_timeout", tmp)));
		wl_iovar_setint(name, "wdstimeout", val);
	}

	/* Set framebursting mode */
	val = nvram_match(strcat_r(prefix, "frameburst", tmp), "on");
	WL_IOCTL(name, WLC_SET_FAKEFRAG, &val, sizeof(val));

	/* Set WME mode */
	wme = nvram_safe_get(strcat_r(prefix, "wme", tmp));
	val = strcmp(wme, "on") ? 0 : 1;
	wl_iovar_set(name, "wme", &val, sizeof(val));
	if (val)
		wlconf_set_wme(name, prefix);
	
	/* Bring the interface back up */
	WL_IOCTL(name, WLC_UP, NULL, 0);

	/* Set antenna */
	val = atoi(nvram_safe_get(strcat_r(prefix, "antdiv", tmp)));
	WL_IOCTL(name, WLC_SET_ANTDIV, &val, sizeof(val));

	/* Auto Channel Selection - when channel # is 0 in AP mode
	 *
	 * The following condition(s) must be met in order for 
	 * Auto Channel Selection to work.
	 *  - the I/F must be up for the channel scan
	 *  - the AP must not be supporting a BSS (all BSS Configs must be disabled)
	 */
	if (ap || apsta) {
		if (!(val = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp))))) {
			/* select a channel */
			val = wlconf_auto_channel(name);
			/* switch to the selected channel */
			if (val != 0)
				WL_IOCTL(name, WLC_SET_CHANNEL, &val, sizeof(val));
			/* set the auto channel scan timer in the driver when in auto mode */
			val = 15;	/* 15 minutes for now */
			WL_IOCTL(name, WLC_SET_CS_SCAN_TIMER, &val, sizeof(val));
		}
		else {
			/* reset the channel scan timer in the driver when not in auto mode */
			val = 0;
			WL_IOCTL(name, WLC_SET_CS_SCAN_TIMER, &val, sizeof(val));
		}
	}

	/* Security settings for each BSS Configuration */
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		wlconf_security_options(name, bsscfg->prefix, bsscfg->idx, wet);
	}

	/*
	 * Finally enable BSS Configs or Join BSS
	 *
	 * AP: Enable BSS Config to bring AP up only when nas will not run
	 * STA: Join the BSS regardless.
	 */
	for (i = 0; i < bclist->count; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;

		setbuf.bsscfg_idx = bclist->bsscfgs[i].idx;
		setbuf.enable = 1;
		
		/* NAS runs if we have an AKM or radius authentication */
		nas_will_run = wlconf_akm_options(bclist->bsscfgs[i].prefix) ||
			nvram_match(strcat_r(bclist->bsscfgs[i].prefix, "auth_mode", tmp), "radius");

		if (((ap || apsta) && !nas_will_run) || sta || wet) {
			for( ii = 0; ii < MAX_BSS_UP_RETRIES; ii++ ) {
				if (wl_ap_build) {
					WL_IOVAR_SET(name, "bss", &setbuf, sizeof(setbuf));
				}
				else {
					strcat_r(prefix, "ssid", tmp);
					ssid.SSID_len = strlen(nvram_safe_get(tmp));
					if (ssid.SSID_len > sizeof(ssid.SSID))
						ssid.SSID_len = sizeof(ssid.SSID);
					strncpy(ssid.SSID, nvram_safe_get(tmp), ssid.SSID_len);
					cprintf("set SSID %s from %s\n",ssid.SSID,tmp);
					WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));
				}
				if (apsta && (ret != 0))
					sleep_ms(1000);
				else
					break;
			}
		}
	}
	
	ret = 0;
exit:
	if (bclist != NULL)
		free(bclist);
	
	return ret;
}

int
wlconf_down(char *name)
{
	int val, ret = 0;
	int i;
	int wlsubunit;
	int bcmerr;
	unsigned char buf[WLC_IOCTL_MAXLEN];
	struct maclist *maclist;
	struct {int bsscfg_idx; int enable;} setbuf;
	int wl_ap_build = 0; /* 1 = wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_SMLEN];
	char *next;
	wlc_ssid_t ssid;

	/* wlconf doesn't work for virtual i/f */
	if (get_ifname_unit(name, NULL, &wlsubunit) == 0 && wlsubunit >= 0) {
		WLCONF_DBG("wlconf: skipping virtual interface \"%s\"\n", name);
		return 0;
	}
	
	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we 
		 can't use the same iovars to configure the wl.
		 so we use "wl_ap_build" to help us know how to configure the driver */
	if (wl_iovar_get(name, "cap", (void *)caps, WLC_IOCTL_SMLEN))
		return -1;
	
	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		}
	}

	if( wl_ap_build ) {
		/* Bring down the interface */
		WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));
		
		/* Disable all BSS Configs */
		for (i = 0; i < WL_MAXBSSCFG; i++) {
			setbuf.bsscfg_idx = i;
			setbuf.enable = 0;
			
			ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
			if (ret) {
				wl_iovar_getint(name, "bcmerror", &bcmerr);
				/* fail quietly on a range error since the driver may
				 * support fewer bsscfgs than we are prepared to configure
				 */
				if (bcmerr == BCME_RANGE)
					break;
			}
		}
	}
	else {
		WL_IOCTL(name, WLC_GET_UP, &val, sizeof(val)); 	 
		if (val) { 	 
			/* Nuke SSID  */ 	 
			ssid.SSID_len = 0; 	 
			ssid.SSID[0] = '\0'; 	 
			WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid)); 	 
			
			/* Bring down the interface */
			WL_IOCTL(name, WLC_DOWN, NULL, sizeof(val));
		}
	}

	/* Nuke the WDS list */
	maclist = (struct maclist *) buf;
	maclist->count = 0;
	WL_IOCTL(name, WLC_SET_WDSLIST, buf, sizeof(buf));

	return 0;
}

#if defined(linux)
int
main(int argc, char *argv[])
{
	/* Check parameters and branch based on action */
	if (argc == 3 && !strcmp(argv[2], "up"))
		return wlconf(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "down"))
		return wlconf_down(argv[1]);
	else {
		fprintf(stderr, "Usage: wlconf <ifname> up|down\n");
		return -1;
	}
}
#endif
