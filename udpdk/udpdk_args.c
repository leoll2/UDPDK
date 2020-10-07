//
// Created by leoll2 on 10/6/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
#include <arpa/inet.h>  // for inet_addr

#include <rte_ether.h>

#include "ini.h"

#include "udpdk_args.h"
#include "udpdk_types.h"

extern configuration config;
extern int primary_argc;
extern int secondary_argc;
extern char *primary_argv[MAX_ARGC];
extern char *secondary_argv[MAX_ARGC];
static char *progname;

static int parse_handler(void* configuration, const char* section, const char* name, const char* value) {
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("port0", "mac_addr")) {
        if (rte_ether_unformat_addr(value, &config.src_mac_addr) < 0) {
            fprintf(stderr, "Can't parse MAC address: %s\n", value);
            return 0;
        }
    } else if (MATCH("port0", "ip_addr")) {
        config.src_ip_addr.s_addr = inet_addr(value);
        if (config.src_ip_addr.s_addr == (in_addr_t)(-1)) {
            fprintf(stderr, "Can't parse IPv4 address: %s\n", value);
        }
    }else if (MATCH("port0_dst", "mac_addr")) {
        if (rte_ether_unformat_addr(value, &config.dst_mac_addr) < 0) {
            fprintf(stderr, "Can't parse MAC address: %s\n", value);
            return 0;
        }
    } else if (MATCH("dpdk", "lcores_primary")) {
        strncpy(config.lcores_primary, value, MAX_ARG_LEN);
    } else if (MATCH("dpdk", "lcores_secondary")) {
        strncpy(config.lcores_secondary, value, MAX_ARG_LEN);
    } else if (MATCH("dpdk", "n_mem_channels")) {
        config.n_mem_channels = atoi(value);
    } else {
        fprintf(stderr, "Do not know how to parse section:%s name:%s\n", section, name);
        return 0;   // unknown section/name
    }
    return 1;
}

static int setup_primary_secondary_args(int argc, char *argv[])
{
    // Build primary args
    primary_argc = 0;
    primary_argv[primary_argc] = malloc(strlen(progname)+1);
    snprintf(primary_argv[primary_argc], MAX_ARG_LEN, "%s", progname);
    primary_argc++;
    primary_argv[primary_argc] = malloc(3);
    snprintf(primary_argv[primary_argc], 3, "-l");
    primary_argc++;
    primary_argv[primary_argc] = malloc(strlen(config.lcores_primary)+1);
    snprintf(primary_argv[primary_argc], MAX_ARG_LEN, "%s", config.lcores_primary);
    primary_argc++;
    primary_argv[primary_argc] = malloc(3);
    snprintf(primary_argv[primary_argc], 3, "-n");
    primary_argc++;
    primary_argv[primary_argc] = malloc(8);
    snprintf(primary_argv[primary_argc], MAX_ARG_LEN, "%d", config.n_mem_channels);
    primary_argc++;
    primary_argv[primary_argc] = malloc(strlen("--proc-type=primary")+1);
    snprintf(primary_argv[primary_argc], MAX_ARG_LEN, "--proc-type=primary");
    primary_argc++;

    // Build secondary args
    secondary_argc = 0;
    secondary_argv[secondary_argc] = malloc(strlen(progname)+1);
    snprintf(secondary_argv[secondary_argc], MAX_ARG_LEN, "%s", progname);
    secondary_argc++;
    secondary_argv[secondary_argc] = malloc(3);
    snprintf(secondary_argv[secondary_argc], 3, "-l");
    secondary_argc++;
    secondary_argv[secondary_argc] = malloc(strlen(config.lcores_secondary)+1);
    snprintf(secondary_argv[secondary_argc], MAX_ARG_LEN, "%s", config.lcores_secondary);
    secondary_argc++;
    secondary_argv[secondary_argc] = malloc(3);
    snprintf(secondary_argv[secondary_argc], 3, "-n");
    secondary_argc++;
    secondary_argv[secondary_argc] = malloc(8);
    snprintf(secondary_argv[secondary_argc], MAX_ARG_LEN, "%d", config.n_mem_channels);
    secondary_argc++;
    secondary_argv[secondary_argc] = malloc(strlen("--proc-type=secondary")+1);
    snprintf(secondary_argv[secondary_argc], MAX_ARG_LEN, "--proc-type=secondary");
    secondary_argc++;

    if (primary_argc + argc >= MAX_ARGC) {
        return -1;
    }

    // Append app arguments to primary after --
    primary_argv[primary_argc] = malloc(3);
    snprintf(primary_argv[primary_argc], 3, "--");
    primary_argc++;
    for (int i = 0; i < argc; i++) {
        primary_argv[primary_argc] = malloc(strlen(argv[i])+1);
        snprintf(primary_argv[primary_argc], MAX_ARG_LEN, "%s", argv[i]);
        primary_argc++;
    }

    printf("Application args: ");
    for (int i = 0; i < primary_argc; i++)
        printf("%s ", primary_argv[i]);
    printf("\n");

    printf("Poller args: ");
    for (int i = 0; i < secondary_argc; i++)
        printf("%s ", secondary_argv[i]);
    printf("\n");

    return 0;
}

int udpdk_parse_args(int argc, char *argv[])
{
    int c;
    char *cfg_filename;

    if (argc < 3) {
        fprintf(stderr, "Too few arguments (must specify at least the configuration file\n");
        return -1;
    }

    progname = argv[0];

    // Get the name of the .ini config file
    c = getopt (argc, argv, "c:");
    if (c == 'c') {
        cfg_filename = optarg;
        if (strlen(optarg) <= 4 || strcmp(optarg + strlen(optarg) - 4, ".ini")) {
            fprintf(stderr, "The configuration file (%s) is not a .ini file apparently!\n", cfg_filename);
            return -1;
        }
        argc -= 2;
        argv += 2;
    } else {
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        return -1;
    }
    argc--;
    argv++;

    if (ini_parse(cfg_filename, parse_handler, NULL) < 0) {
        fprintf(stderr, "Can not parse configuration file %s\n", cfg_filename);
        return -1;
    }

    // Initialize global arrays of arguments for primary and secondary
    if (setup_primary_secondary_args(argc, argv) < 0) {
        fprintf(stderr, "Failed to initialize primary/secondary arguments\n");
        return -1;
    }

    return 0;
}