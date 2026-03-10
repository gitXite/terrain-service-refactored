/**
 * Main terrain generation implementation
 */

#include "../include/terrain.h"
#include "../include/dem_source.h"
#include "../include/stl_writer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============== Model Dimensions ============== */

typedef struct {
    float width_mm;
    float height_mm;
} model_dims_t;

static const model_dims_t MODEL_DIMS[] = {
    [MODEL_RECT_140x190] = { 140.0f, 190.0f },
    [MODEL_SQUARE_150]   = { 150.0f, 150.0f }
};

/* ============== Internal Helpers ============== */

static void compute_sampling_params(
    const terrain_config_t* cfg,
    const dem_format_spec_t* fmt,
    int* out_samples_x,
    int* out_samples_y,
    float* out_mm_per_sample_x,
    float* out_mm_per_sample_y,
    double* out_step_lat,
    double* out_step_lng
) {
    const model_dims_t* dims = &MODEL_DIMS[cfg->format];
    
    // Geographic extent in degrees
    double width_deg = fabs(cfg->se_lng - cfg->nw_lng);
    double height_deg = fabs(cfg->nw_lat - cfg->se_lat);
    
    // Sample counts from DEM resolution
    int samples_x = (int)(width_deg * fmt->samples_per_degree);
    int samples_y = (int)(height_deg * fmt->samples_per_degree);
    if (samples_x < 10) samples_x = 10;
    if (samples_y < 10) samples_y = 10;
    
    // Always fill the full model dimensions.
    // The bounding box is expected to already have the correct aspect ratio
    // (14:19 or 1:1) adjusted for latitude in the UI/caller.
    *out_mm_per_sample_x = dims->width_mm / (float)(samples_x - 1);
    *out_mm_per_sample_y = dims->height_mm / (float)(samples_y - 1);
    
    *out_samples_x = samples_x;
    *out_samples_y = samples_y;
    *out_step_lat = height_deg / (double)(samples_y - 1);
    *out_step_lng = width_deg / (double)(samples_x - 1);
}

static int write_terrain_surface(
    stl_writer_t* stl,
    dem_source_t* dem,
    const terrain_config_t* cfg,
    int samples_x,
    int samples_y,
    float mm_per_sample_x,
    float mm_per_sample_y,
    double step_lat,
    double step_lng
) {
    float* prev_line = calloc(samples_x, sizeof(float));
    float* curr_line = calloc(samples_x, sizeof(float));
    
    if (!prev_line || !curr_line) {
        free(prev_line);
        free(curr_line);
        return -1;
    }
    
    float rot_rad = cfg->rotation_deg * M_PI / 180.0f;
    // Z scale: convert elevation (meters) to mm
    // Model height covers height_deg degrees of latitude (~111km per degree)
    double height_deg = fabs(cfg->nw_lat - cfg->se_lat);
    float z_scale = cfg->z_scale * (mm_per_sample_y * samples_y) / (float)(height_deg * 111000.0);
    float base_h = cfg->base_height_mm;
    float water_drop = cfg->water_drop_mm;
    
    // Get first line
    for (int x = 0; x < samples_x; x++) {
        double lat = cfg->se_lat;
        double lng = cfg->nw_lng + x * step_lng;
        
        // Apply rotation around center
        if (rot_rad != 0.0f) {
            double cx = cfg->nw_lng + (cfg->se_lng - cfg->nw_lng) / 2.0;
            double cy = cfg->nw_lat + (cfg->se_lat - cfg->nw_lat) / 2.0;
            double dx = lng - cx;
            double dy = lat - cy;
            lng = cx + dx * cos(rot_rad) - dy * sin(rot_rad);
            lat = cy + dx * sin(rot_rad) + dy * cos(rot_rad);
        }
        
        float elev = dem_get_elevation_interpolated(dem, lat, lng);
        if (elev == 0) {
            elev = -water_drop / z_scale;
        }
        prev_line[x] = elev * z_scale + base_h;
    }
    
    // Write first wall (south edge at y=0)
    for (int x = 1; x < samples_x; x++) {
        stl_vec3_t a = { x * mm_per_sample_x, 0, prev_line[x] };
        stl_vec3_t b = { (x-1) * mm_per_sample_x, 0, prev_line[x-1] };
        stl_vec3_t c = { (x-1) * mm_per_sample_x, 0, 0 };
        stl_vec3_t d = { x * mm_per_sample_x, 0, 0 };
        stl_add_quad(stl, b, a, d, c);
    }
    
    // Process each row
    for (int y = 1; y < samples_y; y++) {
        float y_mm = y * mm_per_sample_y;
        float y_prev_mm = (y - 1) * mm_per_sample_y;
        
        // Get current line
        for (int x = 0; x < samples_x; x++) {
            double lat = cfg->se_lat + y * step_lat;
            double lng = cfg->nw_lng + x * step_lng;
            
            if (rot_rad != 0.0f) {
                double cx = cfg->nw_lng + (cfg->se_lng - cfg->nw_lng) / 2.0;
                double cy = cfg->nw_lat + (cfg->se_lat - cfg->nw_lat) / 2.0;
                double dx = lng - cx;
                double dy = lat - cy;
                lng = cx + dx * cos(rot_rad) - dy * sin(rot_rad);
                lat = cy + dx * sin(rot_rad) + dy * cos(rot_rad);
            }
            
            float elev = dem_get_elevation_interpolated(dem, lat, lng);
            if (elev == 0) {
                elev = -water_drop / z_scale;
            }
            curr_line[x] = elev * z_scale + base_h;
        }
        
        // Write left wall
        {
            stl_vec3_t a = { 0, y_mm, curr_line[0] };
            stl_vec3_t b = { 0, y_prev_mm, prev_line[0] };
            stl_vec3_t c = { 0, y_prev_mm, 0 };
            stl_vec3_t d = { 0, y_mm, 0 };
            stl_add_quad(stl, a, b, c, d);
        }
        
        // Write right wall
        {
            int x = samples_x - 1;
            stl_vec3_t a = { x * mm_per_sample_x, y_prev_mm, prev_line[x] };
            stl_vec3_t b = { x * mm_per_sample_x, y_mm, curr_line[x] };
            stl_vec3_t c = { x * mm_per_sample_x, y_mm, 0 };
            stl_vec3_t d = { x * mm_per_sample_x, y_prev_mm, 0 };
            stl_add_quad(stl, a, b, c, d);
        }
        
        // Write terrain strip
        for (int x = 1; x < samples_x; x++) {
            float ha = curr_line[x];      // d---a (current row)
            float hb = prev_line[x];      // |   |
            float hc = prev_line[x-1];    // c---b (previous row)
            float hd = curr_line[x-1];
            
            stl_vec3_t a = { x * mm_per_sample_x, y_mm, ha };
            stl_vec3_t b = { x * mm_per_sample_x, y_prev_mm, hb };
            stl_vec3_t c = { (x-1) * mm_per_sample_x, y_prev_mm, hc };
            stl_vec3_t d = { (x-1) * mm_per_sample_x, y_mm, hd };
            
            // Choose diagonal split based on curvature
            if (fabs(hd - hb) < fabs(ha - hc)) {
                stl_add_triangle_verts(stl, a, d, b);
                stl_add_triangle_verts(stl, c, b, d);
            } else {
                stl_add_triangle_verts(stl, a, d, c);
                stl_add_triangle_verts(stl, a, c, b);
            }
        }
        
        // Swap lines
        float* tmp = prev_line;
        prev_line = curr_line;
        curr_line = tmp;
    }
    
    // Write last wall (north edge)
    float y_last = (samples_y - 1) * mm_per_sample_y;
    for (int x = 1; x < samples_x; x++) {
        stl_vec3_t a = { (x-1) * mm_per_sample_x, y_last, prev_line[x-1] };
        stl_vec3_t b = { x * mm_per_sample_x, y_last, prev_line[x] };
        stl_vec3_t c = { x * mm_per_sample_x, y_last, 0 };
        stl_vec3_t d = { (x-1) * mm_per_sample_x, y_last, 0 };
        stl_add_quad(stl, a, b, c, d);
    }
    
    free(prev_line);
    free(curr_line);
    return 0;
}

static void write_bottom(
    stl_writer_t* stl,
    int samples_x,
    int samples_y,
    float mm_per_sample_x,
    float mm_per_sample_y
) {
    // Bottom must be a grid matching the wall vertices to avoid T-junctions
    // (non-manifold edges). Each quad shares edges with adjacent wall segments.
    for (int y = 1; y < samples_y; y++) {
        float y0 = (y - 1) * mm_per_sample_y;
        float y1 = y * mm_per_sample_y;
        
        for (int x = 1; x < samples_x; x++) {
            float x0 = (x - 1) * mm_per_sample_x;
            float x1 = x * mm_per_sample_x;
            
            stl_vec3_t a = { x0, y0, 0 };
            stl_vec3_t b = { x1, y0, 0 };
            stl_vec3_t c = { x1, y1, 0 };
            stl_vec3_t d = { x0, y1, 0 };
            
            // Bottom faces down (negative Z normal) — reverse winding
            stl_add_triangle_verts(stl, a, c, b);
            stl_add_triangle_verts(stl, a, d, c);
        }
    }
}

/* ============== Public API ============== */

void terrain_config_init(terrain_config_t* config) {
    memset(config, 0, sizeof(*config));
    config->format = MODEL_RECT_140x190;
    config->resolution = DEM_AUTO;
    config->z_scale = 1.0f;
    config->base_height_mm = 3.0f;
    config->water_drop_mm = 1.0f;
    config->rotation_deg = 0.0f;
}

terrain_error_t terrain_generate_stl(
    const terrain_config_t* config,
    const char* output_path,
    terrain_result_t* result
) {
    // Validate config
    if (!config || !output_path) {
        return TERRAIN_ERR_INVALID_CONFIG;
    }
    if (config->nw_lat <= config->se_lat || config->se_lng <= config->nw_lng) {
        return TERRAIN_ERR_BOUNDS;
    }
    
    // Select resolution
    dem_resolution_t res = config->resolution;
    if (res == DEM_AUTO) {
        double w = fabs(config->se_lng - config->nw_lng);
        double h = fabs(config->nw_lat - config->se_lat);
        res = terrain_select_resolution(w, h, config->format, config->dtm_path != NULL);
    }
    
    // Get format spec
    const dem_format_spec_t* fmt = terrain_get_format_spec(res);
    if (!fmt) {
        return TERRAIN_ERR_INVALID_CONFIG;
    }
    
    // Compute sampling parameters
    int samples_x, samples_y;
    float mm_per_sample_x, mm_per_sample_y;
    double step_lat, step_lng;
    
    compute_sampling_params(
        config, fmt,
        &samples_x, &samples_y,
        &mm_per_sample_x, &mm_per_sample_y,
        &step_lat, &step_lng
    );
    
    // Open DEM source
    const char* dem_path = (res == DEM_DTM_1M && config->dtm_path) 
                          ? config->dtm_path 
                          : config->dem_path;
    dem_source_t* dem = dem_source_open(res, dem_path);
    if (!dem) {
        return TERRAIN_ERR_NO_DEM_DATA;
    }
    
    // Open STL writer
    stl_writer_t* stl = stl_writer_open(output_path);
    if (!stl) {
        dem_source_close(dem);
        return TERRAIN_ERR_FILE_OPEN;
    }
    
    // Generate terrain surface
    int err = write_terrain_surface(
        stl, dem, config,
        samples_x, samples_y,
        mm_per_sample_x, mm_per_sample_y,
        step_lat, step_lng
    );
    
    if (err != 0) {
        stl_writer_close(stl);
        dem_source_close(dem);
        return TERRAIN_ERR_MEMORY;
    }
    
    // Write bottom
    write_bottom(stl, samples_x, samples_y, mm_per_sample_x, mm_per_sample_y);
    
    // Fill result
    if (result) {
        result->triangle_count = stl_get_triangle_count(stl);
        result->resolution_used = res;
        result->meters_per_sample = fmt->meters_per_sample;
        result->samples_x = samples_x;
        result->samples_y = samples_y;
        result->mm_per_sample_x = mm_per_sample_x;
        result->mm_per_sample_y = mm_per_sample_y;
        result->mm_per_sample = (mm_per_sample_x + mm_per_sample_y) / 2.0f;
        result->model_width_mm = (samples_x - 1) * mm_per_sample_x;
        result->model_height_mm = (samples_y - 1) * mm_per_sample_y;
    }
    
    // Cleanup
    int close_ok = stl_writer_close(stl);
    dem_source_close(dem);
    
    if (result && close_ok == 0) {
        // Get file size
        FILE* f = fopen(output_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            result->file_size_bytes = ftell(f);
            fclose(f);
        }
    }
    
    return TERRAIN_OK;
}

dem_resolution_t terrain_select_resolution(
    double bbox_width_deg,
    double bbox_height_deg,
    model_format_t format,
    int dtm_available
) {
    const model_dims_t* dims = &MODEL_DIMS[format];
    
    // Target ~0.5mm per sample for good 3D print detail
    float target_mm_per_sample = 0.5f;
    int target_samples = (int)(dims->width_mm / target_mm_per_sample);
    
    // Calculate meters per degree (approximate at mid-latitudes)
    double meters_per_deg = 111000.0;  // ~111km per degree
    double extent_m = bbox_width_deg * meters_per_deg;
    
    // What resolution do we need?
    float ideal_m_per_sample = extent_m / target_samples;
    
    // Select best available
    if (ideal_m_per_sample <= 3.0f && dtm_available) {
        return DEM_DTM_1M;
    } else if (ideal_m_per_sample <= 50.0f) {
        return DEM_SRTM_1AS;
    } else {
        return DEM_SRTM_3AS;
    }
}

int terrain_check_coverage(
    double nw_lat, double nw_lng,
    double se_lat, double se_lng,
    dem_resolution_t resolution,
    const char* dem_path
) {
    // Check if we can open tiles for all corners
    dem_source_t* src = dem_source_open(resolution, dem_path);
    if (!src) return 0;
    
    // Try to get elevation at corners
    float e1 = dem_get_elevation(src, nw_lat, nw_lng);
    float e2 = dem_get_elevation(src, nw_lat, se_lng);
    float e3 = dem_get_elevation(src, se_lat, nw_lng);
    float e4 = dem_get_elevation(src, se_lat, se_lng);
    
    dem_source_close(src);
    
    // Check if any corner has valid data
    return (e1 > DEM_VOID_CUTOFF || e2 > DEM_VOID_CUTOFF || 
            e3 > DEM_VOID_CUTOFF || e4 > DEM_VOID_CUTOFF);
}

const char* terrain_error_string(terrain_error_t error) {
    switch (error) {
        case TERRAIN_OK: return "Success";
        case TERRAIN_ERR_INVALID_CONFIG: return "Invalid configuration";
        case TERRAIN_ERR_NO_DEM_DATA: return "No DEM data available";
        case TERRAIN_ERR_FILE_OPEN: return "Failed to open output file";
        case TERRAIN_ERR_MEMORY: return "Memory allocation failed";
        case TERRAIN_ERR_BOUNDS: return "Invalid geographic bounds";
        default: return "Unknown error";
    }
}

const dem_format_spec_t* terrain_get_format_spec(dem_resolution_t resolution) {
    switch (resolution) {
        case DEM_SRTM_1AS: return &DEM_FORMAT_SRTM_1AS;
        case DEM_SRTM_3AS: return &DEM_FORMAT_SRTM_3AS;
        case DEM_DTM_1M:   return &DEM_FORMAT_DTM_1M;
        default: return &DEM_FORMAT_SRTM_3AS;
    }
}