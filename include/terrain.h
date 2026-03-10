/**
 * flexible DEM to STL terrain generator
 * public API
 */

#ifndef TERRAIN_H
#define TERRAIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enums

typedef enum {
    DEM_AUTO = 0,
    DEM_SRTM_1AS,
    DEM_SRTM_3AS,
    DEM_DTM_1M
} dem_resolution_t;

typedef enum {
    MODEL_RECT_140x190 = 0,
    MODEL_SQUARE_150
} model_format_t;

typedef enum {
    TERRAIN_OK = 0,
    TERRAIN_ERR_INVALID_CONFIG,
    TERRAIN_ERR_NO_DEM_DATA,
    TERRAIN_ERR_FILE_OPEN,
    TERRAIN_ERR_MEMORY,
    TERRAIN_ERR_BOUNDS
} terrain_error_t;

/* Structs */

/** 
 * configuration for terrain generation
 */
typedef struct {
    // Geographic bounds (WGS84)
    double nw_lat;          // Northwest corner latitude
    double nw_lng;          // Northwest corner longitude  
    double se_lat;          // Southeast corner latitude
    double se_lng;          // Southeast corner longitude
    
    // Output format
    model_format_t format;          // Physical output dimensions
    dem_resolution_t resolution;    // DEM source preference (or AUTO)
    
    // Scaling parameters
    float z_scale;              // Vertical exaggeration (default: 1.0)
    float base_height_mm;       // Base plate thickness in mm (default: 2.0)
    float water_drop_mm;        // Drop sea level by this amount (default: 2.0)
    
    // Rotation (degrees, clockwise from north)
    float rotation_deg;
    
    // Data paths
    const char* dem_path;       // Path to DEM data directory
    const char* dtm_path;       // Path to DTM data directory (optional)
    
} terrain_config_t;

/**
 * Result information from generation
 */
typedef struct {
    int triangle_count;         // Number of triangles in STL
    size_t file_size_bytes;     // Output file size
    
    // Actual values used (may differ from config if AUTO)
    dem_resolution_t resolution_used;
    float meters_per_sample;    // Actual ground resolution
    
    // Model dimensions
    int samples_x;              // Samples in X direction
    int samples_y;              // Samples in Y direction
    float mm_per_sample;        // Average physical size per sample (compat)
    float mm_per_sample_x;      // Physical size per sample in X (longitude)
    float mm_per_sample_y;      // Physical size per sample in Y (latitude)
    float model_width_mm;       // Actual model width used
    float model_height_mm;      // Actual model height used
    
} terrain_result_t;

/**
 * DEM format specification (for custom formats)
 */
typedef struct {
    int samples_per_degree;     // Samples per degree of lat/lng
    int tile_size;              // Samples per tile edge
    float meters_per_sample;    // Ground resolution in meters
    const char* file_pattern;   // Filename pattern (e.g., "%c%02d%c%03d.hgt")
    const char* extension;      // File extension
} dem_format_spec_t;

/* Functions */

/**
 * Init default config
 */
void terrain_config_init(terrain_config_t* config);

/** 
 * generate STL from terrain data
 * 
 * @param config    Generation configuration
 * @param output    Output STL file path
 * @param result    Optional result info (can be NULL)
 * @return          TERRAIN_OK on success, error code otherwise
 */
terrain_error_t terrain_generate_stl(
    const terrain_config_t* config,
    const char* output_path,
    terrain_result_t* result
);
/**
 * Auto-select optimal DEM resolution based on bounding box and format
 * 
 * @param bbox_width_deg    Width of bounding box in degrees
 * @param bbox_height_deg   Height of bounding box in degrees
 * @param format            Target output format
 * @param dtm_available     Whether 1m DTM data is available for this region
 * @return                  Recommended resolution
 */
dem_resolution_t terrain_select_resolution(
    double bbox_width_deg,
    double bbox_height_deg,
    model_format_t format,
    int dtm_available
);

/**
 * Check if DEM data is available for a given bounding box
 * 
 * @param nw_lat, nw_lng    Northwest corner
 * @param se_lat, se_lng    Southeast corner
 * @param resolution        Resolution to check
 * @param dem_path          Path to DEM files
 * @return                  1 if data available, 0 otherwise
 */
int terrain_check_coverage(
    double nw_lat, double nw_lng,
    double se_lat, double se_lng,
    dem_resolution_t resolution,
    const char* dem_path
);

/**
 * Get human-readable error message
 */
const char* terrain_error_string(terrain_error_t error);

/**
 * Get format specification for a resolution type
 */
const dem_format_spec_t* terrain_get_format_spec(dem_resolution_t resolution);

#ifdef __cplusplus
}
#endif

#endif /* TERRAIN_H */
