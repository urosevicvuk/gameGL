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
static GLuint gbuffer_program, lighting_program, shadow_program, postprocess_program;
static FullscreenQuad quad;

// Post-processing framebuffer
static GLuint postprocessFBO, colorTexture;

// Tavern objects
static rafgl_meshPUN_t cube_mesh;

void main_state_init(GLFWwindow *window, void *args, int width, int height) {
    w = width;
    h = height;
    
    // Capture mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Initialize camera
    camera_init(&camera);
    
    // Initialize G-Buffer
    gbuffer_init(&gbuffer, width, height);
    
    // Create shaders
    gbuffer_program = rafgl_program_create_from_name("gbuffer");
    lighting_program = rafgl_program_create_from_name("deferred");
    postprocess_program = rafgl_program_create_from_name("postprocess");
    
    // Initialize fullscreen quad
    fullscreen_quad_init(&quad);
    
    // Setup post-processing framebuffer
    glGenFramebuffers(1, &postprocessFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, postprocessFBO);
    
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Create tavern geometry
    rafgl_meshPUN_init(&floor_mesh);
    rafgl_meshPUN_load_plane(&floor_mesh, 20.0f, 20.0f, 1, 1);
    
    rafgl_meshPUN_init(&cube_mesh);
    rafgl_meshPUN_load_cube(&cube_mesh, 1.0f);
    
    // Setup tavern lights
    lights[0] = (PointLight){vec3(2.0f, 2.5f, 0.0f), vec3(1.0f, 0.8f, 0.4f), 8.0f, 0, 0};   // Warm fireplace
    lights[1] = (PointLight){vec3(-3.0f, 2.0f, -2.0f), vec3(0.9f, 0.9f, 0.7f), 5.0f, 0, 0}; // Candle
    lights[2] = (PointLight){vec3(1.0f, 2.2f, -4.0f), vec3(0.8f, 0.9f, 0.6f), 6.0f, 0, 0};  // Lantern
    lights[3] = (PointLight){vec3(-1.0f, 1.8f, 3.0f), vec3(1.0f, 0.7f, 0.3f), 4.0f, 0, 0};  // Table candle
    
    // Setup shadow maps for lights
    for(int i = 0; i < num_lights; i++) {
        setup_point_light_shadows(&lights[i], 512, 512);
    }
    
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
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    
    // Render floor
    model = m4_translation(vec3(0.0f, -0.5f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.4f, 0.2f); // Wood floor
    glBindVertexArray(floor_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);
    
    // Render tavern walls
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.3f, 0.2f); // Dark wood
    model = m4_mul(m4_translation(vec3(0.0f, 2.0f, -5.0f)), m4_scaling(vec3(10.0f, 4.0f, 0.2f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Render tavern tables and chairs
    for(int i = 0; i < 3; i++) {
        // Table
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.25f, 0.15f); // Table wood
        model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f, 0.4f, -1.0f)), m4_scaling(vec3(1.5f, 0.1f, 1.0f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Table legs
        for(int j = 0; j < 4; j++) {
            float x_offset = (j % 2) ? 0.6f : -0.6f;
            float z_offset = (j < 2) ? 0.4f : -0.4f;
            model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f + x_offset, 0.2f, -1.0f + z_offset)), m4_scaling(vec3(0.1f, 0.4f, 0.1f)));
            glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(cube_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        }
        
        // Chairs
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.2f, 0.1f); // Chair wood
        for(int chair = 0; chair < 2; chair++) {
            float chair_z = -1.0f + (chair ? 1.2f : -1.2f);
            model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f, 0.25f, chair_z)), m4_scaling(vec3(0.4f, 0.5f, 0.4f)));
            glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(cube_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        }
    }
    
    // Render fireplace
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.3f, 0.3f); // Stone
    model = m4_mul(m4_translation(vec3(4.0f, 1.0f, -2.0f)), m4_scaling(vec3(1.0f, 2.0f, 1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Lighting pass - render directly to screen for now
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
