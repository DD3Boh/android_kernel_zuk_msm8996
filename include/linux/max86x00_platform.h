/*
 * Copyright (c) 2014 Maxim Integrated Products, Inc.
 * Authors: Ismail Kose <Ismail.Kose@maximintegrated.com>,
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _MAX86X00_PLATFORM_H_
#define _MAX86X00_PLATFORM_H_


#define MAX86X00_CHIP_NAME		"max86x00"

#define MAX86000_SLAVE_ADDR         0x51
#define MAX86000A_SLAVE_ADDR        0x57

struct max86x00_pdata {
	int gpio;
	struct regulator *vdd_supply;
	const char *vdd_supply_name;
};
#endif
