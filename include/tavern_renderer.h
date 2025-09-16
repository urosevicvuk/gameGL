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
    GLuint shadowCubeMap;
    GLuint shadowFBO;
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

// Shadow mapping
void setup_point_light_shadows(PointLight *light, int shadowWidth, int shadowHeight);
void render_shadow_map(PointLight *light, rafgl_meshPUN_t *meshes, int meshCount, GLuint shadowProgram);
void render_cube_shadow_map(PointLight *light, GLuint shadowProgram, void (*render_scene_func)(GLuint program));

// SSAO
typedef struct {
    GLuint framebuffer;
    GLuint colorBuffer;
    GLuint blurFramebuffer;
    GLuint blurColorBuffer;
    GLuint noiseTexture;
} SSAOBuffer;

void ssao_init(SSAOBuffer *ssao, int width, int height);

// Texture management
typedef struct {
    rafgl_texture_t diffuse;
    rafgl_texture_t normal;
    rafgl_texture_t specular;
    int has_normal_map;
    int has_specular_map;
    float roughness;
    float metallic;
} Material;

typedef struct {
    Material wood_planks;
    Material oak_table;
    Material dark_wood;
    Material medieval_stone;
    Material brick_wall;
    Material iron_metal;
    Material rusty_metal;
    Material ceramic;
    Material leather;
} TextureManager;

// Material & texture functions
void texture_manager_init(TextureManager *tm);
void texture_manager_cleanup(TextureManager *tm);
void material_bind(Material *mat, GLuint shader_program);

// Procedural geometry generation
void create_cylinder_mesh(rafgl_meshPUN_t *mesh, float radius, float height, int segments);
void create_detailed_barrel_mesh(rafgl_meshPUN_t *mesh, float radius, float height);
void create_ornate_chair_mesh(rafgl_meshPUN_t *mesh);
void create_stone_corbel_mesh(rafgl_meshPUN_t *mesh);

// Lighting
void render_point_lights(PointLight *lights, int count, Camera *cam, GBuffer *gb);

#endif