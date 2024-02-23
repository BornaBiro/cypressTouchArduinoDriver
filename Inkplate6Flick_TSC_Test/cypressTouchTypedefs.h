#ifndef __CYPRESSTOUCHTYPEDEFS_H__
#define __CYPRESSTOUCHTYPEDEFS_H__

struct cyttsp_bootloader_data {
	uint8_t bl_file;
	uint8_t bl_status;
	uint8_t bl_error;
	uint8_t blver_hi;
	uint8_t blver_lo;
	uint8_t bld_blver_hi;
	uint8_t bld_blver_lo;
	uint8_t ttspver_hi;
	uint8_t ttspver_lo;
	uint8_t appid_hi;
	uint8_t appid_lo;
	uint8_t appver_hi;
	uint8_t appver_lo;
	uint8_t cid_0;
	uint8_t cid_1;
	uint8_t cid_2;
};

struct cyttsp_sysinfo_data {
	uint8_t hst_mode;
	uint8_t mfg_stat;
	uint8_t mfg_cmd;
	uint8_t cid[3];
	uint8_t tt_undef1;
	uint8_t uid[8];
	uint8_t bl_verh;
	uint8_t bl_verl;
	uint8_t tts_verh;
	uint8_t tts_verl;
	uint8_t app_idh;
	uint8_t app_idl;
	uint8_t app_verh;
	uint8_t app_verl;
	uint8_t tt_undef[5];
	uint8_t scn_typ;
	uint8_t act_intrvl;
	uint8_t tch_tmout;
	uint8_t lp_intrvl;
};

#endif