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
static GLuint gbuffer_program, lighting_program, shadow_program, postprocess_program, ssao_program;
static FullscreenQuad quad;

// Post-processing framebuffer
static GLuint postprocessFBO, colorTexture;

// SSAO
static GLuint ssaoFBO, ssaoColorBuffer;

// Tavern objects
static rafgl_meshPUN_t cube_mesh, cylinder_mesh;

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
    shadow_program = rafgl_program_create_from_name("shadows");
    ssao_program = rafgl_program_create_from_name("ssao");
    
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
    
    // Setup SSAO framebuffer  
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Create tavern geometry
    rafgl_meshPUN_init(&floor_mesh);
    rafgl_meshPUN_load_plane(&floor_mesh, 20.0f, 20.0f, 1, 1);
    
    rafgl_meshPUN_init(&cube_mesh);
    rafgl_meshPUN_load_cube(&cube_mesh, 1.0f);
    
    // Create cylinder mesh for barrels (using multiple cubes stacked)
    rafgl_meshPUN_init(&cylinder_mesh);
    rafgl_meshPUN_load_cube(&cylinder_mesh, 1.0f);
    
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
    // Shadow pass - render depth from first light's perspective
    if(num_lights > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, lights[0].shadowFBO);
        glViewport(0, 0, 512, 512);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shadow_program);
        mat4_t lightProjection = m4_perspective(90.0f, 1.0f, 1.0f, 25.0f);
        mat4_t lightView = m4_look_at(lights[0].position, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4_t lightSpaceMatrix = m4_mul(lightProjection, lightView);
        
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "lightSpaceMatrix"), 1, GL_FALSE, (float*)lightSpaceMatrix.m);
        
        // Render scene geometry for shadows (simplified)
        mat4_t model = m4_translation(vec3(0.0f, -0.5f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(floor_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);
        
        glViewport(0, 0, w, h);
    }
    
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
    
    // Render barrels around the tavern (made from stacked cubes for cylindrical look)
    float barrel_positions[][2] = {{5.0f, 2.0f}, {-5.0f, 1.5f}, {3.0f, -3.0f}, {-2.0f, -4.0f}};
    for(int i = 0; i < 4; i++) {
        // Main barrel body
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.2f, 0.1f); // Dark wood
        model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.4f, barrel_positions[i][1])), 
                       m4_scaling(vec3(0.6f, 0.8f, 0.6f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Barrel hoops (metal bands)
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.3f, 0.3f); // Metal
        for(int j = 0; j < 3; j++) {
            model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.2f + j * 0.2f, barrel_positions[i][1])), 
                           m4_scaling(vec3(0.65f, 0.05f, 0.65f)));
            glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(cube_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        }
    }
    
    // Render weapon rack with hanging weapons
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.2f, 0.1f); // Dark wood
    // Rack frame
    model = m4_mul(m4_translation(vec3(-4.5f, 1.0f, -4.5f)), m4_scaling(vec3(0.2f, 2.0f, 1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Hanging weapons/tools
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.7f, 0.7f, 0.7f); // Metal
    for(int i = 0; i < 3; i++) {
        model = m4_mul(m4_translation(vec3(-4.4f, 1.5f - i * 0.3f, -4.8f + i * 0.3f)), 
                       m4_scaling(vec3(0.05f, 0.4f, 0.05f))); // Sword/tool handles
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    }
    
    // Add detailed ale mugs and plates on tables
    for(int i = 0; i < 3; i++) {
        // Ale mug
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.7f, 0.6f, 0.4f); // Clay/ceramic
        model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f + 0.3f, 0.6f, -1.0f + 0.2f)), 
                       m4_scaling(vec3(0.15f, 0.2f, 0.15f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Mug handle
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.5f, 0.3f);
        model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f + 0.45f, 0.65f, -1.0f + 0.2f)), 
                       m4_scaling(vec3(0.05f, 0.1f, 0.05f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Wooden plate
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.3f, 0.2f);
        model = m4_mul(m4_translation(vec3(-3.0f + i * 3.0f - 0.3f, 0.52f, -1.0f - 0.2f)), 
                       m4_scaling(vec3(0.25f, 0.02f, 0.25f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    }
    
    // Add ceiling beams
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.2f, 0.1f); // Dark wood
    for(int i = 0; i < 3; i++) {
        model = m4_mul(m4_translation(vec3(-6.0f + i * 6.0f, 3.5f, 0.0f)), 
                       m4_scaling(vec3(8.0f, 0.3f, 0.4f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    }
    
    // Render realistic light sources
    
    // Light 0: Fireplace (2.0f, 2.5f, 0.0f)
    // Fireplace base
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.3f, 0.3f); // Stone
    model = m4_mul(m4_translation(vec3(2.0f, 0.5f, 0.0f)), m4_scaling(vec3(1.5f, 1.0f, 0.8f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Fireplace back wall
    model = m4_mul(m4_translation(vec3(2.0f, 1.5f, -0.3f)), m4_scaling(vec3(1.5f, 2.0f, 0.2f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Fire (glowing)
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 
               lights[0].color.x * 1.5f, lights[0].color.y * 1.2f, lights[0].color.z * 0.3f);
    model = m4_mul(m4_translation(vec3(2.0f, 1.2f, 0.0f)), m4_scaling(vec3(0.3f, 0.4f, 0.3f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Light 1: Table Candle (-3.0f, 2.0f, -2.0f)
    // Candle base
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.4f, 0.2f); // Bronze
    model = m4_mul(m4_translation(vec3(-3.0f, 0.52f, -2.0f)), m4_scaling(vec3(0.15f, 0.05f, 0.15f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Candle wax
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.9f, 0.9f, 0.8f); // Wax
    model = m4_mul(m4_translation(vec3(-3.0f, 0.8f, -2.0f)), m4_scaling(vec3(0.08f, 0.5f, 0.08f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Candle flame
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 
               lights[1].color.x * 2.0f, lights[1].color.y * 1.8f, lights[1].color.z * 0.8f);
    model = m4_mul(m4_translation(vec3(-3.0f, 1.1f, -2.0f)), m4_scaling(vec3(0.05f, 0.1f, 0.05f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Light 2: Hanging Lantern (1.0f, 2.2f, -4.0f)
    // Lantern body
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.3f, 0.2f); // Dark metal
    model = m4_mul(m4_translation(vec3(1.0f, 2.2f, -4.0f)), m4_scaling(vec3(0.25f, 0.4f, 0.25f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Lantern chain
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.5f, 0.5f); // Chain metal
    for(int j = 0; j < 3; j++) {
        model = m4_mul(m4_translation(vec3(1.0f, 2.6f + j * 0.15f, -4.0f)), m4_scaling(vec3(0.03f, 0.1f, 0.03f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    }
    
    // Lantern light
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 
               lights[2].color.x * 1.5f, lights[2].color.y * 1.5f, lights[2].color.z * 1.0f);
    model = m4_mul(m4_translation(vec3(1.0f, 2.2f, -4.0f)), m4_scaling(vec3(0.15f, 0.2f, 0.15f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Light 3: Another table candle (-1.0f, 1.8f, 3.0f)
    // Candle base  
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.4f, 0.2f); // Bronze
    model = m4_mul(m4_translation(vec3(-1.0f, 0.52f, 3.0f)), m4_scaling(vec3(0.15f, 0.05f, 0.15f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Candle wax
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.8f, 0.6f, 0.5f); // Red wax
    model = m4_mul(m4_translation(vec3(-1.0f, 0.8f, 3.0f)), m4_scaling(vec3(0.08f, 0.5f, 0.08f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Candle flame
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 
               lights[3].color.x * 2.0f, lights[3].color.y * 1.5f, lights[3].color.z * 0.5f);
    model = m4_mul(m4_translation(vec3(-1.0f, 1.1f, 3.0f)), m4_scaling(vec3(0.05f, 0.1f, 0.05f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // SSAO pass
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(ssao_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gbuffer.gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gbuffer.gNormal);
    
    glUniform1i(glGetUniformLocation(ssao_program, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(ssao_program, "gNormal"), 1);
    
    glUniformMatrix4fv(glGetUniformLocation(ssao_program, "projection"), 1, GL_FALSE, (float*)projection.m);
    
    fullscreen_quad_render(&quad);
    
    // Lighting pass - render directly to screen for now
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(lighting_program);
    gbuffer_bind_for_reading(&gbuffer);
    
    glUniform1i(glGetUniformLocation(lighting_program, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lighting_program, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lighting_program, "gAlbedoSpec"), 2);
    
    // Bind SSAO texture
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glUniform1i(glGetUniformLocation(lighting_program, "ssaoTexture"), 3);
    
    // Bind shadow maps
    if(num_lights > 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, lights[0].shadowCubeMap);
        glUniform1i(glGetUniformLocation(lighting_program, "shadowMap0"), 4);
        
        mat4_t lightProjection = m4_perspective(90.0f, 1.0f, 1.0f, 25.0f);
        mat4_t lightView = m4_look_at(lights[0].position, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4_t lightSpaceMatrix = m4_mul(lightProjection, lightView);
        glUniformMatrix4fv(glGetUniformLocation(lighting_program, "lightSpaceMatrix0"), 1, GL_FALSE, (float*)lightSpaceMatrix.m);
    }
    
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
