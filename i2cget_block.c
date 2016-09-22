/*
    i2cget.c - A user-space program to read an I2C register.
    Copyright (C) 2001-2012  Jean Delvare <jdelvare@suse.de>
                             Frodo Looijaard <frodol@dds.nl>, and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
                             Alexei Fedotov <alexei.fedotov@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

#include "i2cbusses.h"
#define VERSION "0.1"

#define MIN_BLOCK_SIZE 1 // change to 3
#define MAX_BLOCK_SIZE 32

static void help(void) __attribute__ ((noreturn));
static void help(void)
{
    fprintf(stderr,
        "Usage: i2cget_block [-f] I2CBUS CHIP-ADDRESS [DATA-ADDRESS [SIZE]]\n"
        "  I2CBUS is an integer or an I2C bus name\n"
        "  ADDRESS is an integer (0x03 - 0x77)\n"
        "  SIZE is a number (%i .. %i) of bytes to read\n",
        MIN_BLOCK_SIZE, MAX_BLOCK_SIZE);
    exit(1);
}

static int check_funcs(int file, int daddress, int pec)
{
    unsigned long funcs;
    /* check adapter functionality */
    if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
        fprintf(stderr, "Error: Could not get the adapter "
            "functionality matrix: %s\n", strerror(errno));
        return -1;
    }

    if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
        fprintf(stderr, MISSING_FUNC_FMT, "SMBus read block");
        return -1;
    }

    if (pec
     && !(funcs & (I2C_FUNC_SMBUS_PEC | I2C_FUNC_I2C))) {
        fprintf(stderr, "Warning: Adapter does "
            "not seem to support PEC\n");
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char *end;
    int res, i2cbus, address, file;
    int blocksize = 4;
    int daddress;
    char filename[20];
    int pec = 0;
    int flags = 0;
    int force = 0;
    /* handle (optional) flags first */
    while (1 + flags < argc && argv[1 + flags][0] == '-') {
        switch (argv[1 + flags][1]) {
        case 'V':
            fprintf(stderr, "i2cget_block version %s\n", VERSION);
            return 0;
        case 'f': force = 1; break;
        default:
            fprintf(stderr, "Error: Unsupported option "
                "\"%s\"!\n", argv[1 + flags]);
            help();
            return 1;
        }
        flags++;
    }
    if (argc < flags + 3)
        help();
    i2cbus = lookup_i2c_bus(argv[flags+1]);
    if (i2cbus < 0)
        help();
    address = parse_i2c_address(argv[flags+2]);
    if (address < 0)
        help();
    if (argc > flags + 3) {
        daddress = strtol(argv[flags+3], &end, 0);
        if (*end || daddress < 0 || daddress > 0xff) {
            fprintf(stderr, "Error: Data address invalid!\n");
            help();
        }
    } else {
        daddress = -1;
    }
    if (argc > flags + 4) {
        blocksize = strtol(argv[flags + 4], &end, 0);
        if (blocksize < MIN_BLOCK_SIZE || blocksize > MAX_BLOCK_SIZE) {
            fprintf(stderr, "Error: Invalid mode!\n");
            help();
        }
        pec = argv[flags + 4][1] == 'p';
    }
    file = open_i2c_dev(i2cbus, filename, sizeof(filename), 0);
    if (file < 0
        || check_funcs(file, daddress, pec)
        || set_slave_addr(file, address, force)) exit(1);

    if (pec && ioctl(file, I2C_PEC, 1) < 0) {
        fprintf(stderr, "Error: Could not set PEC: %s\n",
            strerror(errno));
        close(file);
        exit(1);
    }


    unsigned char values[MAX_BLOCK_SIZE]; 
    res = i2c_smbus_read_i2c_block_data(file, daddress, blocksize, values);

    close(file);

    if (res < 0) {
        fprintf(stderr, "Error: Read failed\n");
        exit(2);
    }

    printf("0x");

    int i;
    for (i = 0; i < res; i++) {
        printf("%2x", values[i]);
    }

    printf("\n");
    exit(0);
}

