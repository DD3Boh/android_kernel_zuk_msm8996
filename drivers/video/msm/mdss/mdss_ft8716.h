#ifndef MDSS_FT8716_H
#define MDSS_FT8716_H
#include "lcd_effect.h"
#include "mdss_panel.h"

/*********************************** head cmd *************************************/
//static char ft8716_addr_mode[] = {0x36,0x00};

static struct dsi_cmd_desc ft8716_packet_head_cmds[] = {
//	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ft8716_addr_mode)}, ft8716_addr_mode},
};
/*********************************** ce head cmd *************************************/
//struct dsi_ctrl_hdr cmd_head={0x29,1,0,0,0,2};
static char ft8716_normal_mode_cmd[] = {0x00,0x00};

static struct dsi_cmd_desc ft8716_ct_cold_cmd[] = {
  {{0x29,1,0,0,0,2},ft8716_normal_mode_cmd},
};

static struct dsi_cmd_desc ft8716_ct_default_cmd[] = {
  {{0x29,1,0,0,0,2},ft8716_normal_mode_cmd},
};

static struct dsi_cmd_desc ft8716_ct_warm_cmd[] = {
  {{0x29,1,0,0,0,2},ft8716_normal_mode_cmd},
};

static char ft8716_ce_soft_cmd1[] = {0x00,0x00};
static char ft8716_ce_default_cmd1[] = {0x00,0x00};
static char ft8716_ce_bright_cmd1[] = {0x00,0x00};

static struct dsi_cmd_desc ft8716_ce_soft_cmd[] = {
	{{0x29,0,0,0,0,2},ft8716_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ft8716_ce_soft_cmd1},
};
static struct dsi_cmd_desc ft8716_ce_default_cmd[] = {
	{{0x29,0,0,0,0,2},ft8716_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ft8716_ce_default_cmd1},
};
static struct dsi_cmd_desc ft8716_ce_bright_cmd[] = {
	{{0x29,0,0,0,0,2},ft8716_normal_mode_cmd},
	{{0x29,1,0,0,0,2},ft8716_ce_bright_cmd1},
};

/*********************************** ce *************************************/
static char ft8716_ce0[] = {0x00,0x00};

char *ft8716_ce[] = {
	ft8716_ce0,
};

static struct dsi_cmd_desc ft8716_effect_ce[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ce0)}, ft8716_ce0},
};

struct lcd_effect_cmds ft8716_effect_ce_cmd[] = {
	{{ft8716_ce, ARRAY_SIZE(ft8716_ce)}, {ft8716_effect_ce, ARRAY_SIZE(ft8716_effect_ce)}},
};
/*********************************** ct *************************************/
#if 1
static char ft8716_ct_page[] = {0x00,0x00};
static char ft8716_ct_head00[] = {0x00,0x80};
static char ft8716_ct_head10[] = {0x00,0x00};
static char ft8716_ct_head20[] = {0x00,0x00};
static char ft8716_ct_tail00[] = {0x00,0x00};
static char ft8716_ct_tail10[] = {0x00,0x00};

/*
static char ft8716_ct5k5_0[] = {0xE1,0x00,0x05,0x0C,0x17,0x20,0x26,0x33,0x47,0x51,0x66,0x74,0x80,0x78,0x6F,0x69,0x59,0x45,0x34,0x29,0x22,0x19,0x0B,0x04,0x03};
static char ft8716_ct5k5_1[] = {0xE2,0x00,0x05,0x0C,0x17,0x20,0x26,0x33,0x47,0x51,0x66,0x74,0x80,0x78,0x6F,0x69,0x59,0x45,0x34,0x29,0x22,0x19,0x0B,0x04,0x03};
static char ft8716_ct5k5_2[] = {0xE3,0x65,0x66,0x67,0x69,0x6C,0x6E,0x72,0x78,0x7A,0x83,0x88,0x8D,0x6C,0x67,0x60,0x52,0x40,0x2F,0x25,0x1E,0x15,0x09,0x04,0x03};
static char ft8716_ct5k5_3[] = {0xE4,0x65,0x66,0x67,0x69,0x6C,0x6E,0x72,0x78,0x7A,0x83,0x88,0x8D,0x6C,0x67,0x60,0x52,0x40,0x2F,0x25,0x1E,0x15,0x09,0x04,0x03};
static char ft8716_ct5k5_4[] = {0xE5,0x71,0x73,0x73,0x74,0x73,0x74,0x79,0x7F,0x82,0x89,0x8B,0x8E,0x6C,0x66,0x5F,0x4F,0x3F,0x2E,0x24,0x1D,0x14,0x08,0x04,0x03};
static char ft8716_ct5k5_5[] = {0xE6,0x71,0x73,0x73,0x74,0x73,0x74,0x79,0x7F,0x82,0x89,0x8B,0x8E,0x6C,0x66,0x5F,0x4F,0x3F,0x2E,0x24,0x1D,0x14,0x08,0x04,0x03};
*/
//4000k,use for night mode

static char ft8716_ct4k_0[] = {0x00,0x00};
static char ft8716_ct4k_1[] = {0x00,0x00};
static char ft8716_ct4k_2[] = {0x00,0x00};
static char ft8716_ct4k_3[] = {0x00,0x00};
static char ft8716_ct4k_4[] = {0x00,0x00};
static char ft8716_ct4k_5[] = {0x00,0x00};

static char ft8716_ct6k_0[] = {0x00,0x00};
static char ft8716_ct6k_1[] = {0x00,0x00};
static char ft8716_ct6k_2[] = {0x00,0x00};
static char ft8716_ct6k_3[] = {0x00,0x00};
static char ft8716_ct6k_4[] = {0x00,0x00};
static char ft8716_ct6k_5[] = {0x00,0x00};

static char ft8716_ct6k5_0[] = {0x00,0x00};
static char ft8716_ct6k5_1[] = {0x00,0x00};
static char ft8716_ct6k5_2[] = {0x00,0x00};
static char ft8716_ct6k5_3[] = {0x00,0x00};
static char ft8716_ct6k5_4[] = {0x00,0x00};
static char ft8716_ct6k5_5[] = {0x00,0x00};

static char ft8716_ct7k3_0[] = {0x00,0x00};
static char ft8716_ct7k3_1[] = {0x00,0x00};
static char ft8716_ct7k3_2[] = {0x00,0x00};
static char ft8716_ct7k3_3[] = {0x00,0x00};
static char ft8716_ct7k3_4[] = {0x00,0x00};
static char ft8716_ct7k3_5[] = {0x00,0x00};

static char ft8716_ct8k1_0[] = {0x00,0x00};
static char ft8716_ct8k1_1[] = {0x00,0x00};
static char ft8716_ct8k1_2[] = {0x00,0x00};
static char ft8716_ct8k1_3[] = {0x00,0x00};
static char ft8716_ct8k1_4[] = {0x00,0x00};
static char ft8716_ct8k1_5[] = {0x00,0x00};

static char ft8716_ct9k_0[] = {0x00,0x00};
static char ft8716_ct9k_1[] = {0x00,0x00};
static char ft8716_ct9k_2[] = {0x00,0x00};
static char ft8716_ct9k_3[] = {0x00,0x00};
static char ft8716_ct9k_4[] = {0x00,0x00};
static char ft8716_ct9k_5[] = {0x00,0x00};

static char ft8716_ct9k6_0[] = {0x00,0x00};
static char ft8716_ct9k6_1[] = {0x00,0x00};
static char ft8716_ct9k6_2[] = {0x00,0x00};
static char ft8716_ct9k6_3[] = {0x00,0x00};
static char ft8716_ct9k6_4[] = {0x00,0x00};
static char ft8716_ct9k6_5[] = {0x00,0x00};

char *ft8716_ct[] = {
	ft8716_ct_page,
};
char *ft8716_ct_head0[] = {
	ft8716_ct_head00,
};
char *ft8716_ct_head1[] = {
	ft8716_ct_head10,
};
char *ft8716_ct_head2[] = {
	ft8716_ct_head20,
};
char *ft8716_ct_tail0[] = {
	ft8716_ct_tail00,
};
char *ft8716_ct_tail1[] = {
	ft8716_ct_tail10,
};

char *ft8716_ct0[] = {
	ft8716_ct6k_0,
	ft8716_ct7k3_0,
	ft8716_ct9k_0,
	ft8716_ct4k_0,
	ft8716_ct6k5_0,
	ft8716_ct8k1_0,
	ft8716_ct9k6_0,
};
char *ft8716_ct1[] = {
	ft8716_ct6k_1,
	ft8716_ct7k3_1,
	ft8716_ct9k_1,
	ft8716_ct4k_1,
	ft8716_ct6k5_1,
	ft8716_ct8k1_1,
	ft8716_ct9k6_1,
};
char *ft8716_ct2[] = {
	ft8716_ct6k_2,
	ft8716_ct7k3_2,
	ft8716_ct9k_2,
	ft8716_ct4k_2,
	ft8716_ct6k5_2,
	ft8716_ct8k1_2,
	ft8716_ct9k_2,
	ft8716_ct9k6_2,
};
char *ft8716_ct3[] = {
	ft8716_ct6k_3,
	ft8716_ct7k3_3,
	ft8716_ct9k_3,
	ft8716_ct4k_3,
	ft8716_ct6k5_3,
	ft8716_ct8k1_3,
	ft8716_ct9k6_3,
};
char *ft8716_ct4[] = {
	ft8716_ct6k_4,
	ft8716_ct7k3_4,
	ft8716_ct9k_4,
	ft8716_ct4k_4,
	ft8716_ct6k5_4,
	ft8716_ct8k1_4,
	ft8716_ct9k6_4,
};
char *ft8716_ct5[] = {
	ft8716_ct6k_5,
	ft8716_ct7k3_5,
	ft8716_ct9k_5,
	ft8716_ct4k_5,
	ft8716_ct6k5_5,
	ft8716_ct8k1_5,
	ft8716_ct9k6_5,
};

static struct dsi_cmd_desc ft8716_effect_ct[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_page)}, ft8716_ct_page},
};
static struct dsi_cmd_desc ft8716_effect_ct_head0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_head00)}, ft8716_ct_head00},
};
static struct dsi_cmd_desc ft8716_effect_ct_head1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_head10)}, ft8716_ct_head10},
};
static struct dsi_cmd_desc ft8716_effect_ct_head2[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 5, sizeof(ft8716_ct_head20)}, ft8716_ct_head20},
};
static struct dsi_cmd_desc ft8716_effect_ct_tail0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_tail00)}, ft8716_ct_tail00},
};
static struct dsi_cmd_desc ft8716_effect_ct_tail1[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ct_tail10)}, ft8716_ct_tail10},
};

static struct dsi_cmd_desc ft8716_effect_ct0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct6k_0)}, ft8716_ct6k_0},
};
static struct dsi_cmd_desc ft8716_effect_ct1[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ct6k_1)}, ft8716_ct6k_1},
};
static struct dsi_cmd_desc ft8716_effect_ct2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct6k_2)}, ft8716_ct6k_2},
};
static struct dsi_cmd_desc ft8716_effect_ct3[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ct6k_3)}, ft8716_ct6k_3},
};
static struct dsi_cmd_desc ft8716_effect_ct4[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct6k_4)}, ft8716_ct6k_4},
};
static struct dsi_cmd_desc ft8716_effect_ct5[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ct6k_5)}, ft8716_ct6k_5},
};

struct lcd_effect_cmds ft8716_effect_ct_cmd[] = {
	{{ft8716_ct_head0, ARRAY_SIZE(ft8716_ct_head0)}, {ft8716_effect_ct_head0, ARRAY_SIZE(ft8716_effect_ct_head0)}},
	{{ft8716_ct_head1, ARRAY_SIZE(ft8716_ct_head1)}, {ft8716_effect_ct_head1, ARRAY_SIZE(ft8716_effect_ct_head1)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct_head2, ARRAY_SIZE(ft8716_ct_head2)}, {ft8716_effect_ct_head2, ARRAY_SIZE(ft8716_effect_ct_head2)}},

	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct0, ARRAY_SIZE(ft8716_ct0)}, {ft8716_effect_ct0, ARRAY_SIZE(ft8716_effect_ct0)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct1, ARRAY_SIZE(ft8716_ct1)}, {ft8716_effect_ct1, ARRAY_SIZE(ft8716_effect_ct1)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct2, ARRAY_SIZE(ft8716_ct2)}, {ft8716_effect_ct2, ARRAY_SIZE(ft8716_effect_ct2)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct3, ARRAY_SIZE(ft8716_ct3)}, {ft8716_effect_ct3, ARRAY_SIZE(ft8716_effect_ct3)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct4, ARRAY_SIZE(ft8716_ct4)}, {ft8716_effect_ct4, ARRAY_SIZE(ft8716_effect_ct4)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct5, ARRAY_SIZE(ft8716_ct5)}, {ft8716_effect_ct5, ARRAY_SIZE(ft8716_effect_ct5)}},

	{{ft8716_ct_head0, ARRAY_SIZE(ft8716_ct_head0)}, {ft8716_effect_ct_head0, ARRAY_SIZE(ft8716_effect_ct_head0)}},
	{{ft8716_ct_tail0, ARRAY_SIZE(ft8716_ct_tail0)}, {ft8716_effect_ct_tail0, ARRAY_SIZE(ft8716_effect_ct_tail0)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct_head2, ARRAY_SIZE(ft8716_ct_head2)}, {ft8716_effect_ct_head2, ARRAY_SIZE(ft8716_effect_ct_head2)}},
	{{ft8716_ct_tail1, ARRAY_SIZE(ft8716_ct_tail1)}, {ft8716_effect_ct_tail1, ARRAY_SIZE(ft8716_effect_ct_tail1)}},

};
#else
static char ft8716_ct_page[] = {0x00,0x00};
static char ft8716_ct_0[] = {0x84,0xe6};
static char ft8716_ct_1[] = {0x84,0x00};
static char ft8716_ct_2[] = {0x84,0xb7};
char *ft8716_ct0[] = {
	ft8716_ct_0,
	ft8716_ct_1,
	ft8716_ct_2,
};

char *ft8716_ct[] = {
	ft8716_ct_page,
};

static struct dsi_cmd_desc ft8716_effect_ct[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_page)}, ft8716_ct_page},
};

static struct dsi_cmd_desc ft8716_effect_ct0[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_ct_0)}, ft8716_ct_0},
};

struct lcd_effect_cmds ft8716_effect_ct_cmd[] = {
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct0, ARRAY_SIZE(ft8716_ct0)}, {ft8716_effect_ct0, ARRAY_SIZE(ft8716_effect_ct0)}},
};
#endif
/************************************ cabc ***********************************/
static char ft8716_cabc0[] = {0x55, 0x00};
static char ft8716_cabc1[] = {0x55, 0x01};
static char ft8716_cabc2[] = {0x55, 0x02};
static char ft8716_cabc3[] = {0x55, 0x03};

char *ft8716_cabc[] = {
	ft8716_cabc0,
	ft8716_cabc1,
	ft8716_cabc2,
	ft8716_cabc3,
};

static struct dsi_cmd_desc ft8716_effect_cabc[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ft8716_cabc0)}, ft8716_cabc0},
};
struct lcd_effect_cmds ft8716_effect_cabc_cmd[] = {
	{{ft8716_cabc, ARRAY_SIZE(ft8716_cabc)}, {ft8716_effect_cabc, ARRAY_SIZE(ft8716_effect_cabc)}},
};

/************************************** normal mode **************************************/
static struct dsi_cmd_desc ft8716_normal_mode[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ft8716_normal_mode_cmd)}, ft8716_normal_mode_cmd},
};

/*********************************** all effect ************************************/
struct lcd_effect ft8716_effect[] = {
	{"ce", ARRAY_SIZE(ft8716_ce), 0, {ft8716_effect_ce_cmd, ARRAY_SIZE(ft8716_effect_ce_cmd)}},
	{"ct", ARRAY_SIZE(ft8716_ct0), 0, {ft8716_effect_ct_cmd, ARRAY_SIZE(ft8716_effect_ct_cmd)}},
	{"cabc", ARRAY_SIZE(ft8716_cabc), 0, {ft8716_effect_cabc_cmd, ARRAY_SIZE(ft8716_effect_cabc_cmd)}},
};
/**************************************************************************************/

/************************************** all mode **************************************/
struct lcd_mode ft8716_mode[] = {
	{"custom_mode", 0, {ft8716_normal_mode, ARRAY_SIZE(ft8716_normal_mode)}},
	{"ct_warm_mode", 0, {ft8716_ct_warm_cmd, ARRAY_SIZE(ft8716_ct_warm_cmd)}},
	{"ct_default_mode", 0, {ft8716_ct_default_cmd, ARRAY_SIZE(ft8716_ct_default_cmd)}},
	{"ct_cold_mode", 0, {ft8716_ct_cold_cmd, ARRAY_SIZE(ft8716_ct_cold_cmd)}},
	{"ce_soft_mode", 0, {ft8716_ce_soft_cmd, ARRAY_SIZE(ft8716_ce_soft_cmd)}},
	{"ce_default_mode", 0, {ft8716_ce_default_cmd, ARRAY_SIZE(ft8716_ce_default_cmd)}},
	{"ce_bright_mode", 0, {ft8716_ce_bright_cmd, ARRAY_SIZE(ft8716_ce_bright_cmd)}},
};
/**************************************************************************************/
static struct lcd_cmds ft8716_head_cmds =
	{ft8716_packet_head_cmds, ARRAY_SIZE(ft8716_packet_head_cmds)};

static struct lcd_effect_data ft8716_effect_data =
	{ft8716_effect, &ft8716_head_cmds, ARRAY_SIZE(ft8716_effect)};

static struct lcd_mode_data ft8716_mode_data =
	{ft8716_mode, &ft8716_head_cmds, ARRAY_SIZE(ft8716_mode), 0};

/**************************************************************************************/
struct panel_effect_data lcd_ft8716_data = {
	&ft8716_effect_data,
	&ft8716_mode_data,
};

#endif

