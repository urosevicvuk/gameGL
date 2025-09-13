#ifndef TAVERN_RENDERER_H
#define TAVERN_RENDERER_H

#include <rafgl.h>

typedef struct {
    GLuint framebuffer;
    GLuint gPosition, gNormal, gAlbedoSpec;
    GLuint depthBuffer;
    int width, height;
} GBuffer;

typedef struct {
    vec3_t position;
    vec3_t color;
    float radius;
} PointLight;

typedef struct {
    vec3_t position;
    vec3_t front;
    vec3_t up;
    vec3_t right;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
} Camera;

// G-Buffer functions
void gbuffer_init(GBuffer *gb, int width, int height);
void gbuffer_bind_for_writing(GBuffer *gb);
void gbuffer_bind_for_reading(GBuffer *gb);

// Camera functions
void camera_init(Camera *cam);
void camera_update(Camera *cam, GLFWwindow *window, float deltaTime);
mat4_t camera_get_view_matrix(Camera *cam);

// Fullscreen quad
typedef struct {
    GLuint VAO, VBO;
} FullscreenQuad;

void fullscreen_quad_init(FullscreenQuad *quad);
void fullscreen_quad_render(FullscreenQuad *quad);

// Lighting
void render_point_lights(PointLight *lights, int count, Camera *cam, GBuffer *gb);

#endif