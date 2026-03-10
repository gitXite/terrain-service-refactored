/** 
 * DEM data source abstraction
 * Internal header
*/

#ifndef DEM_SOURCE_H
#define DEM_SOURCE_H

#include "terrain.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// DEM source interface

typedef struct dem_source dem_source_t;

/**
 * Open a DEM source for reading
 * 
 * @param resolution    DEM resolution type
 * @param base_path     base path to DEM files
 * @return              source handle or null on error
 */
dem_source_t* dem_source_open(dem_resolution_t resolution, const char* base_path);

 /**
  * close and free DEM source
  */
 void dem_source_close(dem_source_t* src);

/**
 * Get elevation at a specific coordinate
 * 
 * @param src       DEM source handle
 * @param lat       Latitude (WGS84)
 * @param lng       Longitude (WGS84)
 * @return          Elevation in meters, or DEM_VOID_VALUE if no data
 */
float dem_get_elevation(dem_source_t* src, double lat, double lng);

/**
 * Get elevation with bilinear interpolation
 */
float dem_get_elevation_interpolated(dem_source_t* src, double lat, double lng);

/**
 * Get a line of elevations (optimized batch read)
 * 
 * @param src           DEM source
 * @param heights       Output buffer (must be width floats)
 * @param width         Number of samples
 * @param start_lat     Starting latitude
 * @param start_lng     Starting longitude
 * @param step_lat      Latitude step per sample
 * @param step_lng      Longitude step per sample
 * @param rotation      Rotation in radians
 * @return              Number of valid samples
 */
int dem_get_elevation_line(
    dem_source_t* src, 
    float* heights, 
    int width, 
    double start_lat,
    double start_lng,
    double step_lat,
    double step_lng,
    float rotation
);

/**
 * Get the format specification for this source
 */
const dem_format_spec_t* dem_source_get_format(const dem_source_t* src);

/**
 * Preload tiles for a bounding box (optional optimization)
 */
int dem_source_preload(
    dem_source_t* src, 
    double nw_lat, 
    double nw_lng,
    double se_lat,
    double se_lng
);

#define DEM_VOID_VALUE (-32768.0f)
#define DEM_VOID_CUTOFF (-1000.0f)

// srtm 1 arc-second resolution
static const dem_format_spec_t DEM_FORMAT_SRTM_1AS = {
    .samples_per_degree = 3600,
    .tile_size = 3601,
    .meters_per_sample = 30.0f,
    .file_pattern = "%c%02d%c%03d.hgt",
    .extension = ".hgt"
};

// srtm 3 arc-second resolution
static const dem_format_spec_t DEM_FORMAT_SRTM_3AS = {
    .samples_per_degree = 1200,
    .tile_size = 1201,
    .meters_per_sample = 90.0f,
    .file_pattern = "%c%02d%c%03d.hgt",
    .extension = ".hgt"
};

// 1m DTM if needed
static const dem_format_spec_t DEM_FORMAT_DTM_1M = {
    .samples_per_degree = 0,  // Not degree-based
    .tile_size = 0,           // Variable
    .meters_per_sample = 1.0f,
    .file_pattern = "dtm_%d_%d.tif",  // Adjust for your format
    .extension = ".tif"
};

#ifdef __cplusplus
}
#endif

#endif /* DEM_SOURCE_H */