#ifndef MDSS_AMS520_H
#define MDSS_AMS520_H
#include "lcd_effect.h"
#include "mdss_panel.h"

/*********************************** head cmd *************************************/
//static char ams520_addr_mode[] = {0x36,0x00};

static struct dsi_cmd_desc ams520_packet_head_cmds[] = {
//	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ams520_addr_mode)}, ams520_addr_mode},
};
/*********************************** ce head cmd *************************************/
//struct dsi_ctrl_hdr cmd_head={0x29,1,0,0,0,2};
static char ams520_normal_mode_cmd[] = {0x00,0x00};

static struct dsi_cmd_desc ams520_ct_cold_cmd[] = {
  {{0x29,1,0,0,0,2},ams520_normal_mode_cmd},
};

static struct dsi_cmd_desc ams520_ct_default_cmd[] = {
  {{0x29,1,0,0,0,2},ams520_normal_mode_cmd},
};

static struct dsi_cmd_desc ams520_ct_warm_cmd[] = {
  {{0x29,1,0,0,0,2},ams520_normal_mode_cmd},
};

static char ams520_ce_soft_cmd1[] = {0x00,0x00};
static char ams520_ce_default_cmd1[] = {0x00,0x00};
static char ams520_ce_bright_cmd1[] = {0x00,0x00};

static struct dsi_cmd_desc ams520_ce_soft_cmd[] = {
	{{0x29,0,0,0,0,2},ams520_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ams520_ce_soft_cmd1},
};
static struct dsi_cmd_desc ams520_ce_default_cmd[] = {
	{{0x29,0,0,0,0,2},ams520_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ams520_ce_default_cmd1},
};
static struct dsi_cmd_desc ams520_ce_bright_cmd[] = {
	{{0x29,0,0,0,0,2},ams520_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ams520_ce_bright_cmd1},
};

/*********************************** ce *************************************/
static char ams520_ce_enable0[] = {0xF0,0x5A,0x5A};
static char ams520_seed_edbe[] = {0xED,0xBE};
static char ams520_seed_b008[] = {0xB0,0x08};
static char ams520_ce_seed_setv1[] = {0xED,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,
			0x00,0xFF,0x00,0xFF,0xFF,0xFF,0x00,
			0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0xFF};//original 7300K
static char ams520_seed_b01d[] = {0xB0,0x1D};
static char ams520_ce_seed_setv2[] = {0xED,0xA3,0x01,0x04,0x44,0xE0,0x12,0x03,
			0x08,0xB7,0x4B,0xF4,0xD9,0xAB,0x09,
			0xBB,0xEB,0xE7,0x17,0xFF,0xFF,0xFF};//sRGB 7300K
static char ams520_seed_b032[] = {0xB0,0x32};
static char ams520_ce_seed_setv3[] = {0xED,0xEA,0x01,0x06,0x00,0xD2,0x06,0x04,
			0x09,0xCC,0x00,0xEB,0xD0,0xFC,0x0B,
			0xD8,0xE9,0xE6,0x0D,0xFF,0xFF,0xE7};//adobe 7300K

static char ams520_ce0[] = {0x57,0x4C};//Set1
static char ams520_ce1[] = {0x57,0x44};//Set2
static char ams520_ce2[] = {0x57,0xA4};//Set3
//static char ams520_ce3[] = {0x57,0xA4};//CRC off
static char ams520_ce_lock0[] = {0xF0,0xA5,0xA5};

char *ams520_ce[] = {
	ams520_ce0,
	ams520_ce1,
	ams520_ce2,
	ams520_ce0,
};
static char *ams520_ce_enable[] = {
	ams520_ce_enable0,
};
static char *ams520_seed_ed[] = {
	ams520_seed_edbe,
};
static char *ams520_seed_b01[] = {
	ams520_seed_b008,
};
static char *ams520_ce_seed_set1[] = {
	ams520_ce_seed_setv1,
};
static char *ams520_seed_b02[] = {
	ams520_seed_b01d,
};
static char *ams520_ce_seed_set2[] = {
	ams520_ce_seed_setv2,
};
static char *ams520_seed_b03[] = {
	ams520_seed_b032,
};
static char *ams520_ce_seed_set3[] = {
	ams520_ce_seed_setv3,
};

static char *ams520_ce_lock[] = {
	ams520_ce_lock0,
};

static struct dsi_cmd_desc ams520_effect_ce[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ce0)}, ams520_ce0},
};
static struct dsi_cmd_desc ams520_effect_ce_enable[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ce_enable0)}, ams520_ce_enable0},
};
static struct dsi_cmd_desc ams520_effect_ed[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_seed_edbe)}, ams520_seed_edbe},
};
static struct dsi_cmd_desc ams520_effect_b01[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_seed_b008)}, ams520_seed_b008},
};
static struct dsi_cmd_desc ams520_effect_ce_seed_set1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ce_seed_setv1)}, ams520_ce_seed_setv1},
};
static struct dsi_cmd_desc ams520_effect_b02[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_seed_b01d)}, ams520_seed_b01d},
};
static struct dsi_cmd_desc ams520_effect_ce_seed_set2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ce_seed_setv2)}, ams520_ce_seed_setv2},
};
static struct dsi_cmd_desc ams520_effect_b03[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_seed_b032)}, ams520_seed_b032},
};
static struct dsi_cmd_desc ams520_effect_ce_seed_set3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ce_seed_setv3)}, ams520_ce_seed_setv3},
};
static struct dsi_cmd_desc ams520_effect_ce_lock[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ams520_ce_lock0)}, ams520_ce_lock0},
};

struct lcd_effect_cmds ams520_effect_ce_cmd[] = {
	{{ams520_ce_enable, ARRAY_SIZE(ams520_ce_enable)}, {ams520_effect_ce_enable, ARRAY_SIZE(ams520_effect_ce_enable)}},
	{{ams520_seed_ed, ARRAY_SIZE(ams520_seed_ed)}, {ams520_effect_ed, ARRAY_SIZE(ams520_effect_ed)}},
	{{ams520_seed_b01, ARRAY_SIZE(ams520_seed_b01)}, {ams520_effect_b01, ARRAY_SIZE(ams520_effect_b01)}},
	{{ams520_ce_seed_set1, ARRAY_SIZE(ams520_ce_seed_set1)}, {ams520_effect_ce_seed_set1, ARRAY_SIZE(ams520_effect_ce_seed_set1)}},
	{{ams520_seed_b02, ARRAY_SIZE(ams520_seed_b02)}, {ams520_effect_b02, ARRAY_SIZE(ams520_effect_b02)}},
	{{ams520_ce_seed_set2, ARRAY_SIZE(ams520_ce_seed_set2)}, {ams520_effect_ce_seed_set2, ARRAY_SIZE(ams520_effect_ce_seed_set2)}},
	{{ams520_seed_b03, ARRAY_SIZE(ams520_seed_b03)}, {ams520_effect_b03, ARRAY_SIZE(ams520_effect_b03)}},
	{{ams520_ce_seed_set3, ARRAY_SIZE(ams520_ce_seed_set3)}, {ams520_effect_ce_seed_set3, ARRAY_SIZE(ams520_effect_ce_seed_set3)}},
	{{ams520_ce_lock, ARRAY_SIZE(ams520_ce_lock)}, {ams520_effect_ce_lock, ARRAY_SIZE(ams520_effect_ce_lock)}},
	{{ams520_ce_enable, ARRAY_SIZE(ams520_ce_enable)}, {ams520_effect_ce_enable, ARRAY_SIZE(ams520_effect_ce_enable)}},
	{{ams520_seed_ed, ARRAY_SIZE(ams520_seed_ed)}, {ams520_effect_ed, ARRAY_SIZE(ams520_effect_ed)}},
	{{ams520_ce, ARRAY_SIZE(ams520_ce)}, {ams520_effect_ce, ARRAY_SIZE(ams520_effect_ce)}},
	{{ams520_ce_lock, ARRAY_SIZE(ams520_ce_lock)}, {ams520_effect_ce_lock, ARRAY_SIZE(ams520_effect_ce_lock)}},
};

/*********************************** ct *************************************/
static char ams520_ct_seed_setv1[] = {0xED,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,
			0x00,0xFF,0x00,0xFF,0xFF,0xFF,0x00,
			0xFF,0xFF,0xFF,0x00,0xFF,0xF6,0xD1,};//original 6000
static char ams520_ct_seed_setv2[] = {0xED,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,
			0x00,0xFF,0x00,0xFF,0xFF,0xFF,0x00,
			0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0xFF};//original 7300K
static char ams520_ct_seed_setv3[] = {0xED,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,
			0x00,0xFF,0x00,0xFF,0xFF,0xFF,0x00,
			0xFF,0xFF,0xFF,0x00,0xD0,0xE8,0xFF,
};
static char ams520_ct_seed_setv4[] = {0xED,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,
			0x00,0xFF,0x00,0xFF,0xFF,0xFF,0x00,
			0xFF,0xFF,0xFF,0x00,0xD0,0xc6,0x73,
};

static char *ams520_ct_seed_set1[] = {
	ams520_ct_seed_setv1,
	ams520_ct_seed_setv1,
	ams520_ct_seed_setv1,
	ams520_ct_seed_setv4,
};
static char *ams520_ct_seed_set2[] = {
	ams520_ct_seed_setv2,
};
static char *ams520_ct_seed_set3[] = {
	ams520_ct_seed_setv3,
};

static struct dsi_cmd_desc ams520_effect_ct_seed_set1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ct_seed_setv1)}, ams520_ct_seed_setv1},
};
static struct dsi_cmd_desc ams520_effect_ct_seed_set2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ct_seed_setv2)}, ams520_ct_seed_setv2},
};
static struct dsi_cmd_desc ams520_effect_ct_seed_set3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ams520_ct_seed_setv3)}, ams520_ct_seed_setv3},
};

struct lcd_effect_cmds ams520_effect_ct_cmd[] = {
	{{ams520_ce_enable, ARRAY_SIZE(ams520_ce_enable)}, {ams520_effect_ce_enable, ARRAY_SIZE(ams520_effect_ce_enable)}},
	{{ams520_seed_ed, ARRAY_SIZE(ams520_seed_ed)}, {ams520_effect_ed, ARRAY_SIZE(ams520_effect_ed)}},
	{{ams520_seed_b01, ARRAY_SIZE(ams520_seed_b01)}, {ams520_effect_b01, ARRAY_SIZE(ams520_effect_b01)}},
	{{ams520_ct_seed_set1, ARRAY_SIZE(ams520_ct_seed_set1)}, {ams520_effect_ct_seed_set1, ARRAY_SIZE(ams520_effect_ct_seed_set1)}},
	{{ams520_seed_b02, ARRAY_SIZE(ams520_seed_b02)}, {ams520_effect_b02, ARRAY_SIZE(ams520_effect_b02)}},
	{{ams520_ct_seed_set2, ARRAY_SIZE(ams520_ct_seed_set2)}, {ams520_effect_ct_seed_set2, ARRAY_SIZE(ams520_effect_ct_seed_set2)}},
	{{ams520_seed_b03, ARRAY_SIZE(ams520_seed_b03)}, {ams520_effect_b03, ARRAY_SIZE(ams520_effect_b03)}},
	{{ams520_ct_seed_set3, ARRAY_SIZE(ams520_ct_seed_set3)}, {ams520_effect_ct_seed_set3, ARRAY_SIZE(ams520_effect_ct_seed_set3)}},
	{{ams520_ce_lock, ARRAY_SIZE(ams520_ce_lock)}, {ams520_effect_ce_lock, ARRAY_SIZE(ams520_effect_ce_lock)}},
	{{ams520_ce_enable, ARRAY_SIZE(ams520_ce_enable)}, {ams520_effect_ce_enable, ARRAY_SIZE(ams520_effect_ce_enable)}},
	{{ams520_seed_ed, ARRAY_SIZE(ams520_seed_ed)}, {ams520_effect_ed, ARRAY_SIZE(ams520_effect_ed)}},
	{{ams520_ce, ARRAY_SIZE(ams520_ce)}, {ams520_effect_ce, ARRAY_SIZE(ams520_effect_ce)}},
	{{ams520_ce_lock, ARRAY_SIZE(ams520_ce_lock)}, {ams520_effect_ce_lock, ARRAY_SIZE(ams520_effect_ce_lock)}},
};

/************************************ cabc ***********************************/
static char ams520_cabc0[] = {0x55, 0x00};
static char ams520_cabc1[] = {0x55, 0x01};
static char ams520_cabc2[] = {0x55, 0x02};
static char ams520_cabc3[] = {0x55, 0x03};

char *ams520_cabc[] = {
	ams520_cabc0,
	ams520_cabc1,
	ams520_cabc2,
	ams520_cabc3,
};

static struct dsi_cmd_desc ams520_effect_cabc[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ams520_cabc0)}, ams520_cabc0},
};
struct lcd_effect_cmds ams520_effect_cabc_cmd[] = {
	{{ams520_cabc, ARRAY_SIZE(ams520_cabc)}, {ams520_effect_cabc, ARRAY_SIZE(ams520_effect_cabc)}},
};
/*************************************  hbm ***********************************/
static char ams520_hbm0[] = {0x53,0x20};//dimming speed 1frames
static char ams520_hbm1[] = {0x53,0xe0};
static char ams520_hbm2[] = {0x53,0x28};//dimming speed 32frames
static char ams520_hbm3[] = {0x53,0xe8};


static char *ams520_hbm[] = {
	ams520_hbm0,//32frames
	ams520_hbm1,
	ams520_hbm2,
	ams520_hbm3,
};

static struct dsi_cmd_desc ams520_effect_hbm[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ams520_hbm0)}, ams520_hbm0},
};

struct lcd_effect_cmds ams520_effect_hbm_cmd[] = {
	{{ams520_hbm, ARRAY_SIZE(ams520_hbm)}, {ams520_effect_hbm, ARRAY_SIZE(ams520_effect_hbm)}},
};


/************************************** normal mode **************************************/
static struct dsi_cmd_desc ams520_normal_mode[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ams520_normal_mode_cmd)}, ams520_normal_mode_cmd},
};

/*********************************** all effect ************************************/
struct lcd_effect ams520_effect[] = {
	{"ce", ARRAY_SIZE(ams520_ce), 0, {ams520_effect_ce_cmd, ARRAY_SIZE(ams520_effect_ce_cmd)}},
	{"ct", ARRAY_SIZE(ams520_ce), 0, {ams520_effect_ct_cmd, ARRAY_SIZE(ams520_effect_ct_cmd)}},
	{"cabc", ARRAY_SIZE(ams520_cabc), 0, {ams520_effect_cabc_cmd, ARRAY_SIZE(ams520_effect_cabc_cmd)}},
	{"hbm", ARRAY_SIZE(ams520_hbm), 0, {ams520_effect_hbm_cmd, ARRAY_SIZE(ams520_effect_hbm_cmd)}},
};
/**************************************************************************************/

/************************************** all mode **************************************/
struct lcd_mode ams520_mode[] = {
	{"custom_mode", 0, {ams520_normal_mode, ARRAY_SIZE(ams520_normal_mode)}},
	{"ct_warm_mode", 0, {ams520_ct_warm_cmd, ARRAY_SIZE(ams520_ct_warm_cmd)}},
	{"ct_default_mode", 0, {ams520_ct_default_cmd, ARRAY_SIZE(ams520_ct_default_cmd)}},
	{"ct_cold_mode", 0, {ams520_ct_cold_cmd, ARRAY_SIZE(ams520_ct_cold_cmd)}},
	{"ce_soft_mode", 0, {ams520_ce_soft_cmd, ARRAY_SIZE(ams520_ce_soft_cmd)}},
	{"ce_default_mode", 0, {ams520_ce_default_cmd, ARRAY_SIZE(ams520_ce_default_cmd)}},
	{"ce_bright_mode", 0, {ams520_ce_bright_cmd, ARRAY_SIZE(ams520_ce_bright_cmd)}},
};
/**************************************************************************************/
static struct lcd_cmds ams520_head_cmds =
	{ams520_packet_head_cmds, ARRAY_SIZE(ams520_packet_head_cmds)};

static struct lcd_effect_data ams520_effect_data =
	{ams520_effect, &ams520_head_cmds, ARRAY_SIZE(ams520_effect)};

static struct lcd_mode_data ams520_mode_data =
	{ams520_mode, &ams520_head_cmds, ARRAY_SIZE(ams520_mode), 0};

/**************************************************************************************/
struct panel_effect_data lcd_ams520_data = {
	&ams520_effect_data,
	&ams520_mode_data,
};

#endif
