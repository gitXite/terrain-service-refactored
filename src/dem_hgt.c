/**
 * SRTM HGT file loader
 * Supports both 1" (3601x3601) and 3" (1201x1201) formats
 */

#include "../include/dem_source.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

/* ============== Types ============== */

typedef struct {
    int tile_lat;       // Floor of latitude
    int tile_lng;       // Floor of longitude
    int16_t* data;      // Elevation data (big-endian converted)
    int size;           // Tile size (1201 or 3601)
} hgt_tile_t;

struct dem_source {
    dem_resolution_t resolution;
    const dem_format_spec_t* format;
    char base_path[512];
    
    // Tile cache (simple single-tile cache)
    hgt_tile_t* cached_tile;
    
    // Current open file
    FILE* current_file;
    int current_tile_lat;
    int current_tile_lng;
};

/* ============== Internal Helpers ============== */

static void get_tile_filename(
    char* buf, size_t len,
    const char* base_path,
    int tile_lat, int tile_lng
) {
    char ns = tile_lat >= 0 ? 'N' : 'S';
    char ew = tile_lng >= 0 ? 'E' : 'W';
    
    snprintf(buf, len, "%s/%c%02d%c%03d.hgt",
        base_path, ns, abs(tile_lat), ew, abs(tile_lng));
}

static hgt_tile_t* load_tile(
    dem_source_t* src,
    int tile_lat, int tile_lng
) {
    // Check cache
    if (src->cached_tile && 
        src->cached_tile->tile_lat == tile_lat &&
        src->cached_tile->tile_lng == tile_lng) {
        return src->cached_tile;
    }
    
    // Free old cached tile
    if (src->cached_tile) {
        free(src->cached_tile->data);
        free(src->cached_tile);
        src->cached_tile = NULL;
    }
    
    // Build filename
    char filename[1024];
    get_tile_filename(filename, sizeof(filename), src->base_path, tile_lat, tile_lng);
    
    // Open file
    FILE* f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }
    
    // Determine tile size from file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    int tile_size;
    if (file_size == 3601 * 3601 * 2) {
        tile_size = 3601;  // 1 arc-second
    } else if (file_size == 1201 * 1201 * 2) {
        tile_size = 1201;  // 3 arc-second
    } else {
        fclose(f);
        return NULL;  // Unknown format
    }
    
    // Allocate tile
    hgt_tile_t* tile = malloc(sizeof(hgt_tile_t));
    if (!tile) {
        fclose(f);
        return NULL;
    }
    
    tile->tile_lat = tile_lat;
    tile->tile_lng = tile_lng;
    tile->size = tile_size;
    tile->data = malloc(tile_size * tile_size * sizeof(int16_t));
    
    if (!tile->data) {
        free(tile);
        fclose(f);
        return NULL;
    }
    
    // Read data
    size_t read = fread(tile->data, sizeof(int16_t), tile_size * tile_size, f);
    fclose(f);
    
    if (read != (size_t)(tile_size * tile_size)) {
        free(tile->data);
        free(tile);
        return NULL;
    }
    
    // Convert from big-endian
    for (int i = 0; i < tile_size * tile_size; i++) {
        tile->data[i] = ntohs(tile->data[i]);
    }
    
    src->cached_tile = tile;
    return tile;
}

static float get_elevation_at_point(
    hgt_tile_t* tile,
    double lat, double lng
) {
    if (!tile) return DEM_VOID_VALUE;
    
    // Calculate position within tile
    double frac_lat = lat - floor(lat);
    double frac_lng = lng - floor(lng);
    
    // For southern/western tiles, adjust
    if (tile->tile_lat < 0) frac_lat = 1.0 - frac_lat;
    if (tile->tile_lng < 0) frac_lng = 1.0 - frac_lng;
    
    // Convert to pixel coordinates
    // Note: HGT files are stored top-to-bottom (north to south)
    int px = (int)(frac_lng * (tile->size - 1));
    int py = (int)((1.0 - frac_lat) * (tile->size - 1));
    
    // Clamp
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= tile->size) px = tile->size - 1;
    if (py >= tile->size) py = tile->size - 1;
    
    int idx = py * tile->size + px;
    int16_t elev = tile->data[idx];
    
    // Check for void
    if (elev == -32768) return DEM_VOID_VALUE;
    
    return (float)elev;
}

static float get_elevation_interpolated_internal(
    hgt_tile_t* tile,
    double lat, double lng
) {
    if (!tile) return DEM_VOID_VALUE;
    
    double frac_lat = lat - floor(lat);
    double frac_lng = lng - floor(lng);
    
    if (tile->tile_lat < 0) frac_lat = 1.0 - frac_lat;
    if (tile->tile_lng < 0) frac_lng = 1.0 - frac_lng;
    
    // Get floating point pixel position
    double px_f = frac_lng * (tile->size - 1);
    double py_f = (1.0 - frac_lat) * (tile->size - 1);
    
    int px0 = (int)floor(px_f);
    int py0 = (int)floor(py_f);
    int px1 = px0 + 1;
    int py1 = py0 + 1;
    
    // Clamp
    if (px0 < 0) px0 = 0;
    if (py0 < 0) py0 = 0;
    if (px1 >= tile->size) px1 = tile->size - 1;
    if (py1 >= tile->size) py1 = tile->size - 1;
    
    // Get four corners
    int16_t e00 = tile->data[py0 * tile->size + px0];
    int16_t e10 = tile->data[py0 * tile->size + px1];
    int16_t e01 = tile->data[py1 * tile->size + px0];
    int16_t e11 = tile->data[py1 * tile->size + px1];
    
    // Check for voids
    if (e00 == -32768 || e10 == -32768 || e01 == -32768 || e11 == -32768) {
        // Fall back to nearest valid
        if (e00 != -32768) return (float)e00;
        if (e10 != -32768) return (float)e10;
        if (e01 != -32768) return (float)e01;
        if (e11 != -32768) return (float)e11;
        return DEM_VOID_VALUE;
    }
    
    // Bilinear interpolation
    double fx = px_f - px0;
    double fy = py_f - py0;
    
    double top = e00 * (1.0 - fx) + e10 * fx;
    double bot = e01 * (1.0 - fx) + e11 * fx;
    
    return (float)(top * (1.0 - fy) + bot * fy);
}

/* ============== Public Interface ============== */

dem_source_t* dem_source_open(dem_resolution_t resolution, const char* base_path) {
    if (!base_path) return NULL;
    
    dem_source_t* src = calloc(1, sizeof(dem_source_t));
    if (!src) return NULL;
    
    src->resolution = resolution;
    strncpy(src->base_path, base_path, sizeof(src->base_path) - 1);
    
    switch (resolution) {
        case DEM_SRTM_1AS:
            src->format = &DEM_FORMAT_SRTM_1AS;
            break;
        case DEM_SRTM_3AS:
        default:
            src->format = &DEM_FORMAT_SRTM_3AS;
            break;
    }
    
    return src;
}

void dem_source_close(dem_source_t* src) {
    if (!src) return;
    
    if (src->cached_tile) {
        free(src->cached_tile->data);
        free(src->cached_tile);
    }
    
    if (src->current_file) {
        fclose(src->current_file);
    }
    
    free(src);
}

float dem_get_elevation(dem_source_t* src, double lat, double lng) {
    if (!src) return DEM_VOID_VALUE;
    
    int tile_lat = (int)floor(lat);
    int tile_lng = (int)floor(lng);
    
    hgt_tile_t* tile = load_tile(src, tile_lat, tile_lng);
    return get_elevation_at_point(tile, lat, lng);
}

float dem_get_elevation_interpolated(dem_source_t* src, double lat, double lng) {
    if (!src) return DEM_VOID_VALUE;
    
    int tile_lat = (int)floor(lat);
    int tile_lng = (int)floor(lng);
    
    hgt_tile_t* tile = load_tile(src, tile_lat, tile_lng);
    return get_elevation_interpolated_internal(tile, lat, lng);
}

int dem_get_elevation_line(
    dem_source_t* src,
    float* heights,
    int width,
    double start_lat,
    double start_lng,
    double step_lat,
    double step_lng,
    float rotation
) {
    if (!src || !heights) return 0;
    
    int valid = 0;
    double cos_r = cos(rotation);
    double sin_r = sin(rotation);
    
    for (int x = 0; x < width; x++) {
        // Apply rotation
        double dx = x * step_lng;
        double dy = 0;  // For single line
        
        double lat = start_lat + dy * cos_r - dx * sin_r;
        double lng = start_lng + dx * cos_r + dy * sin_r;
        
        heights[x] = dem_get_elevation_interpolated(src, lat, lng);
        if (heights[x] > DEM_VOID_CUTOFF) valid++;
    }
    
    return valid;
}

const dem_format_spec_t* dem_source_get_format(const dem_source_t* src) {
    return src ? src->format : NULL;
}

int dem_source_preload(
    dem_source_t* src,
    double nw_lat, double nw_lng,
    double se_lat, double se_lng
) {
    // For now, just verify tiles exist
    // Could expand to load multiple tiles into cache
    int min_lat = (int)floor(se_lat);
    int max_lat = (int)floor(nw_lat);
    int min_lng = (int)floor(nw_lng);
    int max_lng = (int)floor(se_lng);
    
    int count = 0;
    for (int lat = min_lat; lat <= max_lat; lat++) {
        for (int lng = min_lng; lng <= max_lng; lng++) {
            if (load_tile(src, lat, lng)) {
                count++;
            }
        }
    }
    
    return count;
}
