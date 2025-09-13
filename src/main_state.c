#include <glad/glad.h>
#include <main_state.h>
#include <tavern_renderer.h>
#include <math.h>

#include <rafgl.h>

static int w, h;
static Camera camera;
static GBuffer gbuffer;
static PointLight lights[8];
static int num_lights = 4;

static rafgl_meshPUN_t floor_mesh, wall_mesh, table_mesh;
static GLuint gbuffer_program, lighting_program;
static FullscreenQuad quad;

// Tavern objects
static rafgl_meshPUN_t cube_mesh;

void main_state_init(GLFWwindow *window, void *args, int width, int height) {
    w = width;
    h = height;
    
    // Initialize camera
    camera_init(&camera);
    
    // Initialize G-Buffer
    gbuffer_init(&gbuffer, width, height);
    
    // Create shaders
    gbuffer_program = rafgl_program_create_from_name("gbuffer");
    lighting_program = rafgl_program_create_from_name("deferred");
    
    // Initialize fullscreen quad
    fullscreen_quad_init(&quad);
    
    // Create tavern geometry
    rafgl_meshPUN_init(&floor_mesh);
    rafgl_meshPUN_load_plane(&floor_mesh, 20.0f, 20.0f, 1, 1);
    
    rafgl_meshPUN_init(&cube_mesh);
    rafgl_meshPUN_load_cube(&cube_mesh, 1.0f);
    
    // Setup tavern lights
    lights[0] = (PointLight){vec3(2.0f, 2.5f, 0.0f), vec3(1.0f, 0.8f, 0.4f), 8.0f};   // Warm fireplace
    lights[1] = (PointLight){vec3(-3.0f, 2.0f, -2.0f), vec3(0.9f, 0.9f, 0.7f), 5.0f}; // Candle
    lights[2] = (PointLight){vec3(1.0f, 2.2f, -4.0f), vec3(0.8f, 0.9f, 0.6f), 6.0f};  // Lantern
    lights[3] = (PointLight){vec3(-1.0f, 1.8f, 3.0f), vec3(1.0f, 0.7f, 0.3f), 4.0f};  // Table candle
    
    glEnable(GL_DEPTH_TEST);
}

void main_state_update(GLFWwindow *window, float delta_time,
                       rafgl_game_data_t *game_data, void *args) {
    camera_update(&camera, window, delta_time);
}

void main_state_render(GLFWwindow *window, void *args) {
    // Geometry pass - render to G-Buffer
    gbuffer_bind_for_writing(&gbuffer);
    
    glUseProgram(gbuffer_program);
    
    mat4_t view = camera_get_view_matrix(&camera);
    mat4_t projection = m4_perspective(45.0f, (float)w/(float)h, 0.1f, 100.0f);
    mat4_t model = m4_identity();
    
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "view"), 1, GL_FALSE, (float*)view.m);
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "projection"), 1, GL_FALSE, (float*)projection.m);
    
    // Render floor
    model = m4_translation(vec3(0.0f, -0.5f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(floor_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);
    
    // Render some cubes for tavern objects
    for(int i = 0; i < 6; i++) {
        model = m4_translation(vec3(-4.0f + i * 1.5f, 0.0f, -2.0f));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    }
    
    // Lighting pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(lighting_program);
    gbuffer_bind_for_reading(&gbuffer);
    
    glUniform1i(glGetUniformLocation(lighting_program, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lighting_program, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lighting_program, "gAlbedoSpec"), 2);
    
    // Send lights to shader
    glUniform1i(glGetUniformLocation(lighting_program, "numLights"), num_lights);
    for(int i = 0; i < num_lights; i++) {
        char uniform_name[64];
        sprintf(uniform_name, "lights[%d].Position", i);
        glUniform3f(glGetUniformLocation(lighting_program, uniform_name), lights[i].position.x, lights[i].position.y, lights[i].position.z);
        sprintf(uniform_name, "lights[%d].Color", i);
        glUniform3f(glGetUniformLocation(lighting_program, uniform_name), lights[i].color.x, lights[i].color.y, lights[i].color.z);
        sprintf(uniform_name, "lights[%d].Radius", i);
        glUniform1f(glGetUniformLocation(lighting_program, uniform_name), lights[i].radius);
    }
    
    glUniform3f(glGetUniformLocation(lighting_program, "viewPos"), camera.position.x, camera.position.y, camera.position.z);
    
    fullscreen_quad_render(&quad);
}

void main_state_cleanup(GLFWwindow *window, void *args) {}
