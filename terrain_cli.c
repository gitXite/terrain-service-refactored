/**
 * Terrain CLI - Command line interface
 * 
 * Usage:
 *   terrain_cli --nw LAT,LNG --se LAT,LNG [options] --output FILE
 * 
 * Options:
 *   --nw LAT,LNG        Northwest corner (required)
 *   --se LAT,LNG        Southeast corner (required)
 *   --bbox LAT1,LNG1,LAT2,LNG2   Alternative: specify all 4 coords
 *   --format rect|square   Output format (default: rect = 140x190mm)
 *   --resolution auto|1as|3as|1m   DEM resolution (default: auto)
 *   --zscale FLOAT      Vertical exaggeration (default: 1.0)
 *   --base FLOAT        Base height in mm (default: 2.0)
 *   --waterdrop FLOAT   Water level drop in mm (default: 2.0)
 *   --rotation FLOAT    Rotation in degrees (default: 0)
 *   --dem-path PATH     Path to HGT files (default: ./hgt_files)
 *   --dtm-path PATH     Path to DTM files (optional)
 *   --output FILE       Output STL file (required)
 *   --quiet             Suppress progress output
 *   --help              Show this help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/terrain.h"

static void print_usage(const char* prog) {
    printf("Usage: %s --nw LAT,LNG --se LAT,LNG [options] --output FILE\n\n", prog);
    printf("Options:\n");
    printf("  --nw LAT,LNG              Northwest corner\n");
    printf("  --se LAT,LNG              Southeast corner\n");
    printf("  --bbox LAT1,LNG1,LAT2,LNG2  Alternative bounding box format\n");
    printf("  --format rect|square      Output format (rect=140x190mm, square=150x150mm)\n");
    printf("  --resolution auto|1as|3as|1m  DEM resolution\n");
    printf("  --zscale FLOAT            Vertical exaggeration (default: 1.0)\n");
    printf("  --base FLOAT              Base height in mm (default: 2.0)\n");
    printf("  --waterdrop FLOAT         Water level drop in mm (default: 2.0)\n");
    printf("  --rotation FLOAT          Rotation in degrees (default: 0)\n");
    printf("  --dem-path PATH           Path to HGT files (default: ./hgt_files)\n");
    printf("  --dtm-path PATH           Path to DTM files\n");
    printf("  --output FILE             Output STL file\n");
    printf("  --quiet                   Suppress progress output\n");
    printf("  --help                    Show this help\n");
}

static int parse_coord(const char* str, double* lat, double* lng) {
    return sscanf(str, "%lf,%lf", lat, lng) == 2;
}

static dem_resolution_t parse_resolution(const char* str) {
    if (strcmp(str, "1as") == 0) return DEM_SRTM_1AS;
    if (strcmp(str, "3as") == 0) return DEM_SRTM_3AS;
    if (strcmp(str, "1m") == 0) return DEM_DTM_1M;
    return DEM_AUTO;
}

int main(int argc, char** argv) {
    terrain_config_t config;
    terrain_config_init(&config);
    
    const char* output_path = NULL;
    const char* dem_path = "./hgt_files";
    const char* dtm_path = NULL;
    int quiet = 0;
    int have_nw = 0, have_se = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--nw") == 0 && i + 1 < argc) {
            if (!parse_coord(argv[++i], &config.nw_lat, &config.nw_lng)) {
                fprintf(stderr, "Invalid --nw format. Use: LAT,LNG\n");
                return 1;
            }
            have_nw = 1;
        }
        else if (strcmp(argv[i], "--se") == 0 && i + 1 < argc) {
            if (!parse_coord(argv[++i], &config.se_lat, &config.se_lng)) {
                fprintf(stderr, "Invalid --se format. Use: LAT,LNG\n");
                return 1;
            }
            have_se = 1;
        }
        else if (strcmp(argv[i], "--bbox") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%lf,%lf,%lf,%lf", 
                       &config.nw_lat, &config.nw_lng,
                       &config.se_lat, &config.se_lng) != 4) {
                fprintf(stderr, "Invalid --bbox format. Use: LAT1,LNG1,LAT2,LNG2\n");
                return 1;
            }
            have_nw = have_se = 1;
        }
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            const char* fmt = argv[++i];
            if (strcmp(fmt, "square") == 0) {
                config.format = MODEL_SQUARE_150;
            } else {
                config.format = MODEL_RECT_140x190;
            }
        }
        else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            config.resolution = parse_resolution(argv[++i]);
        }
        else if (strcmp(argv[i], "--zscale") == 0 && i + 1 < argc) {
            config.z_scale = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--base") == 0 && i + 1 < argc) {
            config.base_height_mm = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--waterdrop") == 0 && i + 1 < argc) {
            config.water_drop_mm = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--rotation") == 0 && i + 1 < argc) {
            config.rotation_deg = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "--dem-path") == 0 && i + 1 < argc) {
            dem_path = argv[++i];
        }
        else if (strcmp(argv[i], "--dtm-path") == 0 && i + 1 < argc) {
            dtm_path = argv[++i];
        }
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = 1;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate required args
    if (!have_nw || !have_se) {
        fprintf(stderr, "Error: Must specify --nw and --se (or --bbox)\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (!output_path) {
        fprintf(stderr, "Error: Must specify --output\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Set paths
    config.dem_path = dem_path;
    config.dtm_path = dtm_path;
    
    // Print config
    if (!quiet) {
        printf("Terrain Generation\n");
        printf("==================\n");
        printf("NW Corner: %.6f, %.6f\n", config.nw_lat, config.nw_lng);
        printf("SE Corner: %.6f, %.6f\n", config.se_lat, config.se_lng);
        printf("Format: %s\n", config.format == MODEL_SQUARE_150 ? "150x150mm square" : "140x190mm rect");
        printf("Resolution: %s\n", 
            config.resolution == DEM_AUTO ? "auto" :
            config.resolution == DEM_SRTM_1AS ? "1 arc-second" :
            config.resolution == DEM_SRTM_3AS ? "3 arc-second" : "1m DTM");
        printf("Z-Scale: %.2f\n", config.z_scale);
        printf("Base Height: %.1fmm\n", config.base_height_mm);
        printf("Water Drop: %.1fmm\n", config.water_drop_mm);
        printf("DEM Path: %s\n", dem_path);
        printf("Output: %s\n", output_path);
        printf("\nGenerating...\n");
    }
    
    // Generate!
    terrain_result_t result;
    terrain_error_t err = terrain_generate_stl(&config, output_path, &result);
    
    if (err != TERRAIN_OK) {
        fprintf(stderr, "Error: %s\n", terrain_error_string(err));
        return 1;
    }
    
    if (!quiet) {
        printf("\nComplete!\n");
        printf("  Triangles: %d\n", result.triangle_count);
        printf("  File size: %.2f KB\n", result.file_size_bytes / 1024.0);
        printf("  Resolution used: %s (%.0fm/sample)\n",
            result.resolution_used == DEM_SRTM_1AS ? "1 arc-second" :
            result.resolution_used == DEM_SRTM_3AS ? "3 arc-second" : "1m DTM",
            result.meters_per_sample);
        printf("  Grid: %d x %d samples\n", result.samples_x, result.samples_y);
        printf("  Scale: %.3fmm per sample\n", result.mm_per_sample);
    }
    
    return 0;
}