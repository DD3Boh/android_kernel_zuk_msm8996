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
	ft8716_ce0,
	ft8716_ce0,
	ft8716_ce0,
};

static struct dsi_cmd_desc ft8716_effect_ce[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ce0)}, ft8716_ce0},
};

struct lcd_effect_cmds ft8716_effect_ce_cmd[] = {
	{{ft8716_ce, ARRAY_SIZE(ft8716_ce)}, {ft8716_effect_ce, ARRAY_SIZE(ft8716_effect_ce)}},
};
/*********************************** ct *************************************/
#if 1

static char ft8716_ct_page[] = {0x00,0x00};
static char ft8716_ct_page1[] = {0x00,0x80};
static char ft8716_cmd2_enable0[] = {0xff,0x87,0x16,0x01};
static char ft8716_cmd2_enable2[] = {0xff,0x87,0x16};
static char ft8716_cmd2_disable0[] = {0xff,0x00,0x00,0x00};
static char ft8716_cmd2_disable2[] = {0xff,0x00,0x00};

//4000k,use for night mode
static char ft8716_ct4k_0[] = {0xEC,0x00,0x04,0x08,0x0c,0x00,0x10,0x14,0x18,0x1c,0x00,0x20,0x28,0x30,0x38,0x00,0x00,
									0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0x10,
									0xa0,0xa8,0xb0,0xb8,0x00,0xc0,0xc8,0xd0,0xd8,0x00,0xe0,0xe8,0xf0,0xf8,0x00,0x20,
									0xfc,0xfe,0xff,0x00,};
static char ft8716_ct4k_1[] = {0xED,0x00,0x03,0x06,0x09,0xa4,0x0c,0x10,0x13,0x16,0x53,0x19,0x20,0x26,0x2c,0xd2,0x10,
									0x33,0x39,0x3f,0x46,0x78,0x4c,0x53,0x59,0x5f,0xd2,0x66,0x6c,0x72,0x79,0x78,0x20,
									0x7f,0x86,0x8c,0x92,0xd2,0x99,0x9f,0xa5,0xac,0x78,0xb2,0xb9,0xbf,0xc5,0xd2,0x30,
									0xc8,0xca,0xcb,0x1b,};
static char ft8716_ct4k_2[] = {0xEE,0x00,0x02,0x04,0x07,0x34,0x09,0x0b,0x0e,0x0f,0x89,0x11,0x16,0x1b,0x1f,0xcb,0x10,
									0x24,0x29,0x2d,0x32,0x61,0x36,0x3b,0x40,0x45,0x1f,0x49,0x4e,0x52,0x58,0x36,0x20,
									0x5c,0x61,0x65,0x6a,0xb6,0x6f,0x73,0x78,0x7d,0x2d,0x81,0x86,0x8b,0x8f,0x87,0x30,
									0x91,0x93,0x93,0x23,};

static char ft8716_ct6k_0[] = {0xEC,0x00,0x04,0x08,0x0c,0x00,0x10,0x14,0x18,0x1c,0x00,0x20,0x28,0x30,0x38,0x00,0x10,
									0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0x20,
									0xa0,0xa8,0xb0,0xb8,0x00,0xc0,0xc8,0xd0,0xd8,0x00,0xe0,0xe8,0xf0,0xf8,0x00,0x30,
									0xfc,0xfe,0xff,0x00,};
static char ft8716_ct6k_1[] = {0xED,0x00,0x03,0x07,0x0b,0x1c,0x0e,0x12,0x15,0x19,0xb6,0x1d,0x24,0x2b,0x33,0x39,0x10,
									0x3a,0x41,0x48,0x50,0x79,0x57,0x5e,0x66,0x6d,0x4e,0x74,0x7c,0x83,0x8a,0x92,0x20,
									0x91,0x99,0xa0,0xa7,0x93,0xaf,0xb6,0xbd,0xc4,0xe4,0xcc,0xd3,0xda,0xe2,0x24,0x30,
									0xe5,0xe7,0xe8,0x16,};
static char ft8716_ct6k_2[] = {0xEE,0x00,0x03,0x06,0x09,0xe4,0x0d,0x10,0x13,0x16,0x24,0x19,0x1f,0x26,0x2d,0x2d,0x10,
									0x33,0x3a,0x40,0x47,0x22,0x4d,0x54,0x5a,0x61,0x76,0x67,0x6e,0x75,0x7c,0x07,0x20,
									0x82,0x89,0x8f,0x96,0x22,0x9c,0xa3,0xa9,0xb0,0x22,0xb6,0xbd,0xc3,0xca,0x72,0x30,
									0xcd,0xcf,0xcf,0x32,};


static char ft8716_ct7k3_0[] = {0xEC,0x00,0x04,0x08,0x0c,0x00,0x10,0x14,0x18,0x1c,0x00,0x20,0x28,0x30,0x38,0x00,0x10,
									0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0x20,
									0xa0,0xa8,0xb0,0xb8,0x00,0xc0,0xc8,0xd0,0xd8,0x00,0xe0,0xe8,0xf0,0xf8,0x00,0x30,
									0xfc,0xfe,0xff,0x00,};
static char ft8716_ct7k3_1[] = {0xED,0x00,0x04,0x08,0x0c,0x00,0x0f,0x13,0x17,0x1b,0xff,0x1f,0x27,0x2f,0x37,0xaf,0x10,
									0x3f,0x47,0x4f,0x57,0x56,0x5f,0x67,0x6f,0x77,0x00,0x7e,0x86,0x8e,0x96,0xbf,0x20,
									0x9e,0xa6,0xae,0xb6,0x5a,0xbe,0xc6,0xce,0xd6,0x05,0xde,0xe5,0xed,0xf5,0xfc,0x30,
									0xf9,0xfb,0xfc,0x2b,};
static char ft8716_ct7k3_2[] = {0xEE,0x00,0x03,0x07,0x0b,0xbc,0x0f,0x13,0x17,0x1a,0x56,0x1e,0x25,0x2d,0x35,0x6d,0x10,
									0x3d,0x44,0x4c,0x54,0x6c,0x5c,0x63,0x6b,0x73,0x6c,0x7b,0x82,0x8a,0x92,0xec,0x20,
									0x9a,0xa2,0xa9,0xb1,0xb2,0xb9,0xc1,0xc8,0xd0,0xb1,0xd8,0xe0,0xe7,0xef,0x61,0x30,
									0xf3,0xf5,0xf6,0x01,};

static char ft8716_ct9k_0[] = {0xEC,0x00,0x03,0x07,0x0b,0xbc,0x0f,0x13,0x17,0x1a,0xc1,0x1e,0x26,0x2e,0x35,0x86,0x10,
									0x3d,0x44,0x4c,0x54,0x2d,0x5b,0x63,0x6b,0x72,0xcb,0x7a,0x82,0x89,0x91,0x71,0x20,
									0x99,0xa0,0xa8,0xb0,0x18,0xb7,0xbf,0xc6,0xce,0xb6,0xd6,0xdd,0xe5,0xed,0x2c,0x30,
									0xf0,0xf2,0xf3,0x3f,};
static char ft8716_ct9k_1[] = {0xED,0x00,0x03,0x07,0x0b,0x6c,0x0f,0x12,0x16,0x1a,0x6c,0x1e,0x25,0x2d,0x34,0x88,0x10,
									0x3c,0x43,0x4b,0x52,0x88,0x5a,0x61,0x69,0x70,0x88,0x78,0x7f,0x87,0x8e,0x88,0x20,
									0x96,0x9d,0xa5,0xac,0x88,0xb4,0xbb,0xc3,0xca,0x88,0xd2,0xd9,0xe1,0xe8,0x88,0x30,
									0xec,0xee,0xef,0x01,};
static char ft8716_ct9k_2[] = {0xEE,0x00,0x04,0x08,0x0c,0x00,0x10,0x14,0x18,0x1b,0x40,0x1f,0x27,0x2f,0x37,0x55,0x10,
									0x3f,0x47,0x4f,0x57,0x55,0x5f,0x67,0x6f,0x77,0xa9,0x7f,0x87,0x8f,0x98,0x2a,0x20,
									0xa0,0xa8,0xb0,0xb8,0x00,0xc0,0xc8,0xd0,0xd8,0x00,0xe0,0xe8,0xf0,0xf8,0x00,0x30,
									0xfc,0xfe,0xff,0x00,};

char *ft8716_ct[] = {
	ft8716_ct_page,
	ft8716_ct_page,
	ft8716_ct_page,
	ft8716_ct_page,
};
char *ft8716_ctpage1[] = {
	ft8716_ct_page1,
};

char *ft8716_cmd2_en0[] = {
	ft8716_cmd2_enable0,
};
char *ft8716_cmd2_en2[] = {
	ft8716_cmd2_enable2,
};
char *ft8716_cmd2_disb0[] = {
	ft8716_cmd2_disable0,
};
char *ft8716_cmd2_disb2[] = {
	ft8716_cmd2_disable2,
};

char *ft8716_ct0[] = {
	ft8716_ct6k_0,
	ft8716_ct7k3_0,
	ft8716_ct9k_0,
	ft8716_ct4k_0,
};
char *ft8716_ct1[] = {
	ft8716_ct6k_1,
	ft8716_ct7k3_1,
	ft8716_ct9k_1,
	ft8716_ct4k_1,
};
char *ft8716_ct2[] = {
	ft8716_ct6k_2,
	ft8716_ct7k3_2,
	ft8716_ct9k_2,
	ft8716_ct4k_2,
};

static struct dsi_cmd_desc ft8716_effect_ct[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_page)}, ft8716_ct_page},
};
static struct dsi_cmd_desc ft8716_effect_ct_page1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_ct_page1)}, ft8716_ct_page1},
};
static struct dsi_cmd_desc ft8716_effect_ct_enable0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_cmd2_enable0)}, ft8716_cmd2_enable0},
};
static struct dsi_cmd_desc ft8716_effect_ct_enable2[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 5, sizeof(ft8716_cmd2_enable2)}, ft8716_cmd2_enable2},
};
static struct dsi_cmd_desc ft8716_effect_ct_disable0[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_cmd2_disable0)}, ft8716_cmd2_disable0},
};
static struct dsi_cmd_desc ft8716_effect_ct_disable2[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_cmd2_disable2)}, ft8716_cmd2_disable2},
};

static struct dsi_cmd_desc ft8716_effect_ct0[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_ct6k_0)}, ft8716_ct6k_0},
};
static struct dsi_cmd_desc ft8716_effect_ct1[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_ct6k_1)}, ft8716_ct6k_1},
};
static struct dsi_cmd_desc ft8716_effect_ct2[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_ct6k_2)}, ft8716_ct6k_2},
};

struct lcd_effect_cmds ft8716_effect_ct_cmd[] = {
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_cmd2_en0, ARRAY_SIZE(ft8716_cmd2_en0)}, {ft8716_effect_ct_enable0, ARRAY_SIZE(ft8716_effect_ct_enable0)}},
	{{ft8716_ctpage1, ARRAY_SIZE(ft8716_ctpage1)}, {ft8716_effect_ct_page1, ARRAY_SIZE(ft8716_effect_ct_page1)}},
	{{ft8716_cmd2_en2, ARRAY_SIZE(ft8716_cmd2_en2)}, {ft8716_effect_ct_enable2, ARRAY_SIZE(ft8716_effect_ct_enable2)}},

	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct0, ARRAY_SIZE(ft8716_ct0)}, {ft8716_effect_ct0, ARRAY_SIZE(ft8716_effect_ct0)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct1, ARRAY_SIZE(ft8716_ct1)}, {ft8716_effect_ct1, ARRAY_SIZE(ft8716_effect_ct1)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_ct2, ARRAY_SIZE(ft8716_ct2)}, {ft8716_effect_ct2, ARRAY_SIZE(ft8716_effect_ct2)}},

	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_cmd2_disb0, ARRAY_SIZE(ft8716_cmd2_disb0)}, {ft8716_effect_ct_disable0, ARRAY_SIZE(ft8716_effect_ct_disable0)}},
	{{ft8716_ctpage1, ARRAY_SIZE(ft8716_ctpage1)}, {ft8716_effect_ct_page1, ARRAY_SIZE(ft8716_effect_ct_page1)}},
	{{ft8716_cmd2_disb0, ARRAY_SIZE(ft8716_cmd2_disb0)}, {ft8716_effect_ct_disable2, ARRAY_SIZE(ft8716_effect_ct_disable2)}},

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
	ft8716_cabc3,
	ft8716_cabc2,
	ft8716_cabc1,
};

static struct dsi_cmd_desc ft8716_effect_cabc[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(ft8716_cabc0)}, ft8716_cabc0},
};
struct lcd_effect_cmds ft8716_effect_cabc_cmd[] = {
	{{ft8716_cabc, ARRAY_SIZE(ft8716_cabc)}, {ft8716_effect_cabc, ARRAY_SIZE(ft8716_effect_cabc)}},
};
/*************************************  hbm sunlight mode***********************************/

static char ft8716_hbm_page[] = {0x00,0x00};
static char ft8716_hbm_page1[] = {0x00,0xa0};
static char ft8716_hbm_page2[] = {0x00,0xb0};
static char ft8716_hbm_page3[] = {0x00,0xc0};
static char ft8716_hbm_page4[] = {0x00,0xd0};
static char ft8716_hbm_page5[] = {0xfb,0x01};
static char ft8716_hbm_enable[] = {0xd7,0x05};
static char ft8716_hbm_disable[] = {0xd7,0x04};

//4000k,use for night mode
static char ft8716_g22_0[] = {0xE1,0x00,0x0b,0x22,0x35,0x41,0x4c,0x5e,0x6b,0x70,0x78,0x7e,0x83,
								0x76,0x71,0x6f,0x69,0x5b,0x4f,0x3f,0x35,0x2c,0x1c,0x07,0x03};
static char ft8716_g22_1[] = {0xE2,0x00,0x0b,0x22,0x35,0x41,0x4c,0x5e,0x6b,0x70,0x78,0x7e,0x83,
								0x76,0x71,0x6f,0x69,0x5b,0x4f,0x3f,0x35,0x2c,0x1c,0x07,0x03};
static char ft8716_g22_ce1[] = {0xd6,0x0e,0x06,0x02,0x05,0x07,0x09,0x11,0x15,0x00,0x17,0x17,0x15};
static char ft8716_g22_ce2[] = {0xd6,0x9c,0x9e,0x86,0x69,0x53,0x44,0x49,0x4e,0x63,0x7a,0x8b,0x9c};
static char ft8716_g22_ce3[] = {0xd6,0x89,0x87,0x73,0x64,0x5e,0x5a,0x5e,0x66,0x6c,0x70,0x7a,0x87};
static char ft8716_g22_ce4[] = {0xd6,0x78,0x74,0x5d,0x56,0x45,0x44,0x51,0x66,0x5f,0x60,0x67,0x74};
static char ft8716_g22_ce5[] = {0x91,0x80};

static char ft8716_g145_0[] = {0xE1,0x00,0x07,0x18,0x28,0x32,0x3d,0x4c,0x58,0x5e,0x65,0x6c,0x72,
								0x88,0x86,0x87,0x83,0x7a,0x71,0x65,0x5c,0x54,0x42,0x26,0x03};
static char ft8716_g145_1[] = {0xE2,0x00,0x07,0x18,0x28,0x32,0x3d,0x4c,0x58,0x5e,0x65,0x6c,0x72,
								0x88,0x86,0x87,0x83,0x7a,0x71,0x65,0x5c,0x54,0x42,0x26,0x03};
static char ft8716_g145_ce1[] = {0xd6,0x12,0x06,0x00,0x1e,0x05,0x06,0x10,0x18,0x00,0x19,0x1c,0x15};
static char ft8716_g145_ce2[] = {0xd6,0xff,0xff,0xe1,0xbc,0x79,0x66,0x64,0x5e,0x8b,0xc4,0xe8,0xff};
static char ft8716_g145_ce3[] = {0xd6,0xd4,0xd1,0x95,0x7d,0x76,0x68,0x6f,0x7d,0x95,0x9a,0xa4,0xbf};
static char ft8716_g145_ce4[] = {0xd6,0xaa,0x9d,0x6b,0x62,0x5a,0x59,0x5c,0x81,0x9d,0x87,0x88,0xa5};
static char ft8716_g145_ce5[] = {0x91,0xbf};

char *ft8716_hbm[] = {
	ft8716_hbm_page,
	ft8716_hbm_page,
};
char *ft8716_hbmpage1[] = {
	ft8716_hbm_page1,
	ft8716_hbm_page1,
};
char *ft8716_hbmpage2[] = {
	ft8716_hbm_page2,
	ft8716_hbm_page2,
};
char *ft8716_hbmpage3[] = {
	ft8716_hbm_page3,
	ft8716_hbm_page3,
};
char *ft8716_hbmpage4[] = {
	ft8716_hbm_page4,
	ft8716_hbm_page4,
};
char *ft8716_hbmpage5[] = {
	ft8716_hbm_page5,
	ft8716_hbm_page5,
};

char *ft8716_hbm_switch[] = {
	ft8716_hbm_disable,
	ft8716_hbm_enable,
};

char *ft8716_hbm0[] = {
	ft8716_g22_0,
	ft8716_g145_0,
};
char *ft8716_hbm1[] = {
	ft8716_g22_1,
	ft8716_g145_1,
};

char *ft8716_hbmce1[] = {
	ft8716_g22_ce1,
	ft8716_g145_ce1,
};
char *ft8716_hbmce2[] = {
	ft8716_g22_ce2,
	ft8716_g145_ce2,
};
char *ft8716_hbmce3[] = {
	ft8716_g22_ce3,
	ft8716_g145_ce3,
};
char *ft8716_hbmce4[] = {
	ft8716_g22_ce4,
	ft8716_g145_ce4,
};
char *ft8716_hbmce5[] = {
	ft8716_g22_ce5,
	ft8716_g145_ce5,
};


static struct dsi_cmd_desc ft8716_effect_hbm[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page)}, ft8716_hbm_page},
};
static struct dsi_cmd_desc ft8716_effect_hbm_page1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page1)}, ft8716_hbm_page1},
};
static struct dsi_cmd_desc ft8716_effect_hbm_page2[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page2)}, ft8716_hbm_page2},
};
static struct dsi_cmd_desc ft8716_effect_hbm_page3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page3)}, ft8716_hbm_page3},
};
static struct dsi_cmd_desc ft8716_effect_hbm_page4[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page4)}, ft8716_hbm_page4},
};
static struct dsi_cmd_desc ft8716_effect_hbm_page5[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_hbm_page5)}, ft8716_hbm_page5},
};

static struct dsi_cmd_desc ft8716_effect_hbm_switch[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_hbm_disable)}, ft8716_hbm_disable},
};
static struct dsi_cmd_desc ft8716_effect_hbm0[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_g22_0)}, ft8716_g22_0},
};
static struct dsi_cmd_desc ft8716_effect_hbm1[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_g22_1)}, ft8716_g22_1},
};
static struct dsi_cmd_desc ft8716_effect_hbmce1[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_g22_ce1)}, ft8716_g22_ce1},
};
static struct dsi_cmd_desc ft8716_effect_hbmce2[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(ft8716_g22_ce2)}, ft8716_g22_ce2},
};
static struct dsi_cmd_desc ft8716_effect_hbmce3[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_g22_ce3)}, ft8716_g22_ce3},
};
static struct dsi_cmd_desc ft8716_effect_hbmce4[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(ft8716_g22_ce4)}, ft8716_g22_ce4},
};
static struct dsi_cmd_desc ft8716_effect_hbmce5[] = {
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ft8716_g22_ce5)}, ft8716_g22_ce5},
};

struct lcd_effect_cmds ft8716_effect_hbm_cmd[] = {
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_cmd2_en0, ARRAY_SIZE(ft8716_cmd2_en0)}, {ft8716_effect_ct_enable0, ARRAY_SIZE(ft8716_effect_ct_enable0)}},
	{{ft8716_ctpage1, ARRAY_SIZE(ft8716_ctpage1)}, {ft8716_effect_ct_page1, ARRAY_SIZE(ft8716_effect_ct_page1)}},
	{{ft8716_cmd2_en2, ARRAY_SIZE(ft8716_cmd2_en2)}, {ft8716_effect_ct_enable2, ARRAY_SIZE(ft8716_effect_ct_enable2)}},

	{{ft8716_hbm, ARRAY_SIZE(ft8716_hbm)}, {ft8716_effect_hbm, ARRAY_SIZE(ft8716_effect_hbm)}},
	{{ft8716_hbm0, ARRAY_SIZE(ft8716_hbm0)}, {ft8716_effect_hbm0, ARRAY_SIZE(ft8716_effect_hbm0)}},
	{{ft8716_hbm, ARRAY_SIZE(ft8716_hbm)}, {ft8716_effect_hbm, ARRAY_SIZE(ft8716_effect_hbm)}},
	{{ft8716_hbm1, ARRAY_SIZE(ft8716_hbm1)}, {ft8716_effect_hbm1, ARRAY_SIZE(ft8716_effect_hbm1)}},
	{{ft8716_hbmpage1, ARRAY_SIZE(ft8716_hbmpage1)}, {ft8716_effect_hbm_page1, ARRAY_SIZE(ft8716_effect_hbm_page1)}},
	{{ft8716_hbm_switch, ARRAY_SIZE(ft8716_hbm_switch)}, {ft8716_effect_hbm_switch, ARRAY_SIZE(ft8716_effect_hbm_switch)}},

	{{ft8716_hbmpage1, ARRAY_SIZE(ft8716_hbmpage1)}, {ft8716_effect_hbm_page1, ARRAY_SIZE(ft8716_effect_hbm_page1)}},
	{{ft8716_hbmce1, ARRAY_SIZE(ft8716_hbmce1)}, {ft8716_effect_hbmce1, ARRAY_SIZE(ft8716_effect_hbmce1)}},
	{{ft8716_hbmpage2, ARRAY_SIZE(ft8716_hbmpage2)}, {ft8716_effect_hbm_page2, ARRAY_SIZE(ft8716_effect_hbm_page2)}},
	{{ft8716_hbmce2, ARRAY_SIZE(ft8716_hbmce2)}, {ft8716_effect_hbmce2, ARRAY_SIZE(ft8716_effect_hbmce2)}},

	{{ft8716_hbmpage3, ARRAY_SIZE(ft8716_hbmpage3)}, {ft8716_effect_hbm_page3, ARRAY_SIZE(ft8716_effect_hbm_page3)}},
	{{ft8716_hbmce3, ARRAY_SIZE(ft8716_hbmce3)}, {ft8716_effect_hbmce3, ARRAY_SIZE(ft8716_effect_hbmce3)}},
	{{ft8716_hbmpage4, ARRAY_SIZE(ft8716_hbmpage4)}, {ft8716_effect_hbm_page4, ARRAY_SIZE(ft8716_effect_hbm_page4)}},
	{{ft8716_hbmce4, ARRAY_SIZE(ft8716_hbmce4)}, {ft8716_effect_hbmce4, ARRAY_SIZE(ft8716_effect_hbmce4)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_hbmce5, ARRAY_SIZE(ft8716_hbmce5)}, {ft8716_effect_hbmce5, ARRAY_SIZE(ft8716_effect_hbmce5)}},
	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_hbmpage5, ARRAY_SIZE(ft8716_hbmpage5)}, {ft8716_effect_hbm_page5, ARRAY_SIZE(ft8716_effect_hbm_page5)}},

	{{ft8716_ct, ARRAY_SIZE(ft8716_ct)}, {ft8716_effect_ct, ARRAY_SIZE(ft8716_effect_ct)}},
	{{ft8716_cmd2_disb0, ARRAY_SIZE(ft8716_cmd2_disb0)}, {ft8716_effect_ct_disable0, ARRAY_SIZE(ft8716_effect_ct_disable0)}},
	{{ft8716_ctpage1, ARRAY_SIZE(ft8716_ctpage1)}, {ft8716_effect_ct_page1, ARRAY_SIZE(ft8716_effect_ct_page1)}},
	{{ft8716_cmd2_disb0, ARRAY_SIZE(ft8716_cmd2_disb0)}, {ft8716_effect_ct_disable2, ARRAY_SIZE(ft8716_effect_ct_disable2)}},

};
/************************************** normal mode **************************************/
static struct dsi_cmd_desc ft8716_normal_mode[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ft8716_normal_mode_cmd)}, ft8716_normal_mode_cmd},
};

/*********************************** all effect ************************************/
struct lcd_effect ft8716_effect[] = {
	{"ce", ARRAY_SIZE(ft8716_ce), 0, {ft8716_effect_ce_cmd, ARRAY_SIZE(ft8716_effect_ce_cmd)}},
	{"ct", ARRAY_SIZE(ft8716_ct), 0, {ft8716_effect_ct_cmd, ARRAY_SIZE(ft8716_effect_ct_cmd)}},
	{"cabc", ARRAY_SIZE(ft8716_cabc), 0, {ft8716_effect_cabc_cmd, ARRAY_SIZE(ft8716_effect_cabc_cmd)}},
	{"hbm", ARRAY_SIZE(ft8716_hbm), 0, {ft8716_effect_hbm_cmd, ARRAY_SIZE(ft8716_effect_hbm_cmd)}},
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

