/**
 * Binary STL writer
 * Internal header
 */

#ifndef STL_WRITER_H
#define STL_WRITER_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== Types ============== */

typedef struct {
    float x, y, z;
} stl_vec3_t;

typedef struct {
    stl_vec3_t normal;
    stl_vec3_t v1, v2, v3;
    uint16_t attr;  // Attribute byte count (usually 0)
} stl_triangle_t;

typedef struct {
    FILE* file;
    uint32_t triangle_count;
    long header_pos;  // Position to write final count
} stl_writer_t;

/* ============== Functions ============== */

/**
 * Open STL file for writing
 * 
 * @param path      Output file path
 * @return          Writer handle or NULL on error
 */
stl_writer_t* stl_writer_open(const char* path);

/**
 * Close STL file and finalize header
 */
int stl_writer_close(stl_writer_t* writer);

/**
 * Add a triangle to the STL file
 */
int stl_add_triangle(stl_writer_t* writer, const stl_triangle_t* tri);

/**
 * Add a triangle from 3 vertices (auto-compute normal)
 */
int stl_add_triangle_verts(
    stl_writer_t* writer,
    stl_vec3_t v1,
    stl_vec3_t v2,
    stl_vec3_t v3
);

/**
 * Compute normal vector for a triangle
 */
stl_vec3_t stl_compute_normal(stl_vec3_t v1, stl_vec3_t v2, stl_vec3_t v3);

/**
 * Get current triangle count
 */
uint32_t stl_get_triangle_count(const stl_writer_t* writer);

/* ============== Mesh Building Helpers ============== */

/**
 * Write a quad as two triangles
 * Vertices in order: a--b
 *                    |  |
 *                    d--c
 */
int stl_add_quad(
    stl_writer_t* writer,
    stl_vec3_t a, stl_vec3_t b,
    stl_vec3_t c, stl_vec3_t d
);

/**
 * Write a wall segment between two height lines
 */
int stl_add_wall_segment(
    stl_writer_t* writer,
    float x, float y,
    float h_top, float h_bottom,
    float x_prev, float h_top_prev, float h_bottom_prev,
    int flip_normal
);

#ifdef __cplusplus
}
#endif

#endif /* STL_WRITER_H */
