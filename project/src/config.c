#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>


/* ===================================================== */
/* Remove leading/trailing spaces                        */
/* ===================================================== */
static void trim_whitespace(char *str)
{
    char *start = str;
    char *end;

    /*
     * Skip leading spaces
     */
    while (isspace((unsigned char)*start))
    {
        start++;
    }

    /*
     * Shift string left
     */
    if (start != str)
    {
        memmove(str,
                start,
                strlen(start) + 1);
    }

    /*
     * Find end of string
     */
    end = str + strlen(str) - 1;

    /*
     * Remove trailing spaces
     */
    while (end > str &&
           isspace((unsigned char)*end))
    {
        end--;
    }

    *(end + 1) = '\0';
}

/* ===================================================== */
/* Safe integer parser                                   */
/* ===================================================== */
static int parse_int(const char *value, int *result)
{
    char *endptr;
    long number;

    errno = 0;

    number = strtol(value, &endptr, 10);

    /* No digits found */
    if (value == endptr)
        return -1;

    /* Extra characters after number */
    while (isspace((unsigned char)*endptr))
        endptr++;

    if (*endptr != '\0')
        return -1;

    /* Overflow / underflow */
    if ((errno == ERANGE) ||
        (number > INT_MAX) ||
        (number < INT_MIN))
        return -1;

    *result = (int)number;

    return 0;
}


/* ===================================================== */
/* Read configuration file                               */
/* ===================================================== */
int load_config(const char *filename, Config *config)
{
    FILE *file;
    char line[256];
    char *key;
    char *value;
    int line_number = 0;
    int found_data = 0;

    /* Duplicate detection flags */
    int green_found = 0;
    int yellow_found = 0;
    int all_red_found = 0;
    int pedestrian_found = 0;
    int emergency_found = 0;
    int vehicle_min_found = 0;
    int vehicle_max_found = 0;
    int emergency_min_found = 0;
    int emergency_max_found = 0;
    int max_vehicle_found = 0;
    int max_ped_wait_found = 0;
    int max_emergency_found = 0;
    int log_found = 0;
    int max_green_found = 0;
    int ipc_key_found = 0;

    file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening config file");
        return -1;
    }

    /* Initialize everything */
    memset(config, 0, sizeof(Config));

    while (fgets(line, sizeof(line), file) != NULL)
    {
        line_number++;

        /* Remove newline */
        line[strcspn(line, "\n")] = '\0';

        /* Ignore comments and empty lines */
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0')
            continue;

        key = strtok(line, "=");
        value = strtok(NULL, "=");

        if (key == NULL || value == NULL)
        {
            printf("Invalid line format at line %d\n", line_number);
            fclose(file);
            return -1;
        }

        trim_whitespace(key);
        trim_whitespace(value);

        if (strlen(value) == 0)
        {
            printf("Missing value at line %d\n", line_number);
            fclose(file);
            return -1;
        }

        found_data = 1;

        if (strcmp(key, "GREEN_TIME") == 0)
        {
            if (green_found) { printf("Duplicate GREEN_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->green_time) != 0) { printf("Invalid GREEN_TIME\n"); fclose(file); return -1; }
            green_found = 1;
        }
        else if (strcmp(key, "YELLOW_TIME") == 0)
        {
            if (yellow_found) { printf("Duplicate YELLOW_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->yellow_time) != 0) { printf("Invalid YELLOW_TIME\n"); fclose(file); return -1; }
            yellow_found = 1;
        }
        else if (strcmp(key, "ALL_RED_TIME") == 0)
        {
            if (all_red_found) { printf("Duplicate ALL_RED_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->all_red_time) != 0) { printf("Invalid ALL_RED_TIME\n"); fclose(file); return -1; }
            all_red_found = 1;
        }
        else if (strcmp(key, "PEDESTRIAN_TIME") == 0)
        {
            if (pedestrian_found) { printf("Duplicate PEDESTRIAN_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->pedestrian_time) != 0) { printf("Invalid PEDESTRIAN_TIME\n"); fclose(file); return -1; }
            pedestrian_found = 1;
        }
        else if (strcmp(key, "EMERGENCY_TIME") == 0)
        {
            if (emergency_found) { printf("Duplicate EMERGENCY_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->emergency_time) != 0) { printf("Invalid EMERGENCY_TIME\n"); fclose(file); return -1; }
            emergency_found = 1;
        }
        else if (strcmp(key, "VEHICLE_MIN_INTERVAL") == 0)
        {
            if (vehicle_min_found) { printf("Duplicate VEHICLE_MIN_INTERVAL\n"); fclose(file); return -1; }
            if (parse_int(value, &config->vehicle_min_interval) != 0) { printf("Invalid VEHICLE_MIN_INTERVAL\n"); fclose(file); return -1; }
            vehicle_min_found = 1;
        }
        else if (strcmp(key, "VEHICLE_MAX_INTERVAL") == 0)
        {
            if (vehicle_max_found) { printf("Duplicate VEHICLE_MAX_INTERVAL\n"); fclose(file); return -1; }
            if (parse_int(value, &config->vehicle_max_interval) != 0) { printf("Invalid VEHICLE_MAX_INTERVAL\n"); fclose(file); return -1; }
            vehicle_max_found = 1;
        }
        else if (strcmp(key, "EMERGENCY_MIN_INTERVAL") == 0)
        {
            if (emergency_min_found) { printf("Duplicate EMERGENCY_MIN_INTERVAL\n"); fclose(file); return -1; }
            if (parse_int(value, &config->emergency_min_interval) != 0) { printf("Invalid EMERGENCY_MIN_INTERVAL\n"); fclose(file); return -1; }
            emergency_min_found = 1;
        }
        else if (strcmp(key, "EMERGENCY_MAX_INTERVAL") == 0)
        {
            if (emergency_max_found) { printf("Duplicate EMERGENCY_MAX_INTERVAL\n"); fclose(file); return -1; }
            if (parse_int(value, &config->emergency_max_interval) != 0) { printf("Invalid EMERGENCY_MAX_INTERVAL\n"); fclose(file); return -1; }
            emergency_max_found = 1;
        }
        else if (strcmp(key, "MAX_VEHICLES_PER_DIRECTION") == 0)
        {
            if (max_vehicle_found) { printf("Duplicate MAX_VEHICLES_PER_DIRECTION\n"); fclose(file); return -1; }
            if (parse_int(value, &config->max_vehicles_per_direction) != 0) { printf("Invalid MAX_VEHICLES_PER_DIRECTION\n"); fclose(file); return -1; }
            max_vehicle_found = 1;
        }
        else if (strcmp(key, "MAX_PEDESTRIAN_WAIT") == 0)
        {
            if (max_ped_wait_found) { printf("Duplicate MAX_PEDESTRIAN_WAIT\n"); fclose(file); return -1; }
            if (parse_int(value, &config->max_pedestrian_wait) != 0) { printf("Invalid MAX_PEDESTRIAN_WAIT\n"); fclose(file); return -1; }
            max_ped_wait_found = 1;
        }
        else if (strcmp(key, "MAX_EMERGENCY_RESPONSE") == 0)
        {
            if (max_emergency_found) { printf("Duplicate MAX_EMERGENCY_RESPONSE\n"); fclose(file); return -1; }
            if (parse_int(value, &config->max_emergency_response) != 0) { printf("Invalid MAX_EMERGENCY_RESPONSE\n"); fclose(file); return -1; }
            max_emergency_found = 1;
        }
        else if (strcmp(key, "LOG_FILE") == 0)
        {
            if (log_found) { printf("Duplicate LOG_FILE\n"); fclose(file); return -1; }
            strncpy(config->log_file, value, sizeof(config->log_file) - 1);
            log_found = 1;
        }
        else if (strcmp(key, "MAX_GREEN_TIME") == 0)
        {
            if (max_green_found) { printf("Duplicate MAX_GREEN_TIME\n"); fclose(file); return -1; }
            if (parse_int(value, &config->max_green_time) != 0) { printf("Invalid MAX_GREEN_TIME\n"); fclose(file); return -1; }
            max_green_found = 1;
        }
        else if (strcmp(key, "IPC_KEY_BASE") == 0)
        {
            if (ipc_key_found) { printf("Duplicate IPC_KEY_BASE\n"); fclose(file); return -1; }
            if (parse_int(value, &config->ipc_key_base) != 0) { printf("Invalid IPC_KEY_BASE\n"); fclose(file); return -1; }
            ipc_key_found = 1;
        }
        else
        {
            printf("Unknown config key at line %d: %s\n", line_number, key);
            fclose(file);
            return -1;
        }
    }

    fclose(file);

    if (!found_data)
    {
        printf("Configuration file is empty\n");
        return -1;
    }

    /* Check for missing required values */
    if (!green_found || !yellow_found || !all_red_found || !pedestrian_found || !emergency_found ||
        !vehicle_min_found || !vehicle_max_found || !emergency_min_found || !emergency_max_found ||
        !max_vehicle_found || !max_ped_wait_found || !max_emergency_found || !log_found ||
        !max_green_found || !ipc_key_found)
    {
        printf("Missing one or more required configuration values.\n");
        return -1;
    }

    return 0;
}


/* ===================================================== */
/* Validate configuration values                         */
/* ===================================================== */
int validate_config(const Config *config)
{
    /* Positive values and basic safety constraints */
    if (config->green_time <= 0) { printf("GREEN_TIME must be > 0\n"); return -1; }
    if (config->yellow_time < 2) { printf("YELLOW_TIME must be >= 2 for safety\n"); return -1; }
    if (config->all_red_time < 0) { printf("ALL_RED_TIME must be >= 0\n"); return -1; }
    if (config->pedestrian_time <= 0) { printf("PEDESTRIAN_TIME must be > 0\n"); return -1; }
    if (config->emergency_time <= 0) { printf("EMERGENCY_TIME must be > 0\n"); return -1; }
    if (config->max_green_time <= 0) { printf("MAX_GREEN_TIME must be > 0\n"); return -1; }
    if (config->ipc_key_base == 0) { printf("IPC_KEY_BASE must be != 0\n"); return -1; }

    if (config->vehicle_min_interval <= 0 || config->vehicle_max_interval <= 0)
    { printf("Vehicle intervals must be > 0\n"); return -1; }

    if (config->emergency_min_interval <= 0 || config->emergency_max_interval <= 0)
    { printf("Emergency intervals must be > 0\n"); return -1; }

    if (config->max_vehicles_per_direction < 0)
    { printf("MAX_VEHICLES_PER_DIRECTION must be >= 0\n"); return -1; }

    if (config->max_pedestrian_wait <= 0)
    { printf("MAX_PEDESTRIAN_WAIT must be > 0\n"); return -1; }

    if (config->max_emergency_response <= 0)
    { printf("MAX_EMERGENCY_RESPONSE must be > 0\n"); return -1; }

    /* Logical constraints */
    if (config->yellow_time >= config->green_time)
    { printf("YELLOW_TIME must be smaller than GREEN_TIME\n"); return -1; }

    if (config->max_green_time < config->green_time)
    { printf("MAX_GREEN_TIME must be >= GREEN_TIME\n"); return -1; }

    if (config->vehicle_min_interval >= config->vehicle_max_interval)
    { printf("VEHICLE_MIN_INTERVAL must be smaller than VEHICLE_MAX_INTERVAL\n"); return -1; }

    if (config->emergency_min_interval >= config->emergency_max_interval)
    { printf("EMERGENCY_MIN_INTERVAL must be smaller than EMERGENCY_MAX_INTERVAL\n"); return -1; }

    if (config->max_emergency_response > config->emergency_time)
    { printf("MAX_EMERGENCY_RESPONSE must be <= EMERGENCY_TIME\n"); return -1; }

    if (config->max_pedestrian_wait < config->pedestrian_time)
    { printf("MAX_PEDESTRIAN_WAIT must be >= PEDESTRIAN_TIME\n"); return -1; }

    if (strlen(config->log_file) == 0)
    { printf("LOG_FILE path is empty\n"); return -1; }

    if (config->green_time < config->pedestrian_time)
    {
        printf("GREEN_TIME should be >= PEDESTRIAN_TIME\n");
        return -1;
    }

    return 0;
}


/* ===================================================== */
/* Print configuration                                   */
/* ===================================================== */
void print_config(const Config *config)
{
    printf("\n========== CONFIG VALUES ==========\n");
    printf("GREEN_TIME = %d\n", config->green_time);
    printf("YELLOW_TIME = %d\n", config->yellow_time);
    printf("ALL_RED_TIME = %d\n", config->all_red_time);
    printf("PEDESTRIAN_TIME = %d\n", config->pedestrian_time);
    printf("EMERGENCY_TIME = %d\n", config->emergency_time);
    printf("MAX_GREEN_TIME = %d\n", config->max_green_time);
    printf("IPC_KEY_BASE = %d\n", config->ipc_key_base);
    printf("\nVehicle Interval = [%d - %d]\n", config->vehicle_min_interval, config->vehicle_max_interval);
    printf("Emergency Interval = [%d - %d]\n", config->emergency_min_interval, config->emergency_max_interval);
    printf("\nMAX_VEHICLES_PER_DIRECTION = %d\n", config->max_vehicles_per_direction);
    printf("MAX_PEDESTRIAN_WAIT = %d\n", config->max_pedestrian_wait);
    printf("MAX_EMERGENCY_RESPONSE = %d\n", config->max_emergency_response);
    printf("LOG_FILE = %s\n", config->log_file);
    printf("===================================\n");
}
