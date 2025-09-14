#include <glad/glad.h>
#include <main_state.h>
#include <tavern_renderer.h>
#include <math.h>

#include <rafgl.h>

static int w, h;
static Camera camera;
static GBuffer gbuffer;
static PointLight lights[8];
static int num_lights = 0;
static int base_num_lights = 0; // No fixed tavern lights - flashlight only
static int flashlight_active = 0;
static float flashlight_distance = 0.2f; // Distance from camera
static TextureManager texture_manager;

static rafgl_meshPUN_t floor_mesh, wall_mesh, table_mesh;
static GLuint gbuffer_program, lighting_program, shadow_program, postprocess_program, ssao_program;
static FullscreenQuad quad;

// Post-processing framebuffer
static GLuint postprocessFBO, colorTexture;

// SSAO
static GLuint ssaoFBO, ssaoColorBuffer;

// Tavern objects - loaded from .obj files
static rafgl_meshPUN_t barrel_mesh, table_round_mesh, bench_mesh, stool_mesh;
static rafgl_meshPUN_t beer_mug_mesh, green_bottle_mesh, wall_candle_mesh, food_plate_mesh;
// Keep basic cube for fallback/debugging
static rafgl_meshPUN_t cube_mesh;

// Scroll wheel callback for flashlight distance control
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Only adjust flashlight distance when F key is held and flashlight is active
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && flashlight_active) {
        // Scroll up = move light forward, scroll down = move light backward
        flashlight_distance += (float)yoffset * 0.5f;
        
        // Clamp distance to reasonable range
        if (flashlight_distance < -5.0f) flashlight_distance = -5.0f; // Behind camera
        if (flashlight_distance > 10.0f) flashlight_distance = 10.0f;  // Far into scene
        
        printf("Flashlight distance: %.2f\n", flashlight_distance);
    }
}

void main_state_init(GLFWwindow *window, void *args, int width, int height) {
    w = width;
    h = height;
    
    // Capture mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Set up scroll wheel callback for flashlight control
    glfwSetScrollCallback(window, scroll_callback);
    
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
    
    // Create detailed floor geometry for better shadow receiving
    rafgl_meshPUN_init(&floor_mesh);
    rafgl_meshPUN_load_plane(&floor_mesh, 20.0f, 20.0f, 50, 50); // High subdivision for shadows
    
    // Load tavern models from .obj files
    rafgl_meshPUN_init(&barrel_mesh);
    rafgl_meshPUN_load_from_OBJ(&barrel_mesh, "res/models/Wooden barrel with metal bands/base.obj");
    
    rafgl_meshPUN_init(&table_round_mesh);
    rafgl_meshPUN_load_from_OBJ(&table_round_mesh, "res/models/Round wooden table with pedestal base/base.obj");
    
    rafgl_meshPUN_init(&bench_mesh);
    rafgl_meshPUN_load_from_OBJ(&bench_mesh, "res/models/Wooden bench with panels/base.obj");
    
    rafgl_meshPUN_init(&stool_mesh);
    rafgl_meshPUN_load_from_OBJ(&stool_mesh, "res/models/Wooden stool with ocagonal seat/base.obj");
    
    rafgl_meshPUN_init(&beer_mug_mesh);
    rafgl_meshPUN_load_from_OBJ(&beer_mug_mesh, "res/models/Wooden beer mug with foam/base.obj");
    
    rafgl_meshPUN_init(&green_bottle_mesh);
    rafgl_meshPUN_load_from_OBJ(&green_bottle_mesh, "res/models/Green bottle with cork stopper/base.obj");
    
    rafgl_meshPUN_init(&wall_candle_mesh);
    rafgl_meshPUN_load_from_OBJ(&wall_candle_mesh, "res/models/Wall-mounted candle with flame/base.obj");
    
    rafgl_meshPUN_init(&food_plate_mesh);
    rafgl_meshPUN_load_from_OBJ(&food_plate_mesh, "res/models/Plate with steak and drumstick/base.obj");
    
    // Keep basic cube for debugging
    rafgl_meshPUN_init(&cube_mesh);
    rafgl_meshPUN_load_cube(&cube_mesh, 1.0f);
    
    // No static lights - only the flashlight will illuminate the scene
    
    // Setup shadow maps for lights
    for(int i = 0; i < num_lights; i++) {
        setup_point_light_shadows(&lights[i], 512, 512);
    }
    
    // Initialize texture manager
    texture_manager_init(&texture_manager);
    
    glEnable(GL_DEPTH_TEST);
}

void main_state_update(GLFWwindow *window, float delta_time,
                       rafgl_game_data_t *game_data, void *args) {
    camera_update(&camera, window, delta_time);
    
    // Handle flashlight toggle with F key
    static int f_key_pressed = 0;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!f_key_pressed) {
            // F key just pressed - toggle flashlight
            if (!flashlight_active) {
                // Add flashlight (positioned at camera, pointing forward)
                lights[base_num_lights] = (PointLight){
                    .position = camera.position,
                    .color = vec3(1.0f, 0.9f, 0.7f), // Warm white flashlight
                    .radius = 15.0f, // Strong light
                    .shadowFBO = 0,
                    .shadowCubeMap = 0
                };
                
                // Setup shadow mapping for flashlight
                setup_point_light_shadows(&lights[base_num_lights], 512, 512);
                
                flashlight_active = 1;
                num_lights = base_num_lights + 1;
                printf("Flashlight ON\n");
            }
            f_key_pressed = 1;
        }
    } else {
        if (f_key_pressed && flashlight_active) {
            // F key released - turn off flashlight
            flashlight_active = 0;
            num_lights = base_num_lights;
            printf("Flashlight OFF\n");
        }
        f_key_pressed = 0;
    }
    
    // Update flashlight position to follow camera at controlled distance
    if (flashlight_active) {
        lights[base_num_lights].position = v3_add(camera.position, v3_muls(camera.front, flashlight_distance));
    }
}

void main_state_render(GLFWwindow *window, void *args) {
    // Shadow pass - render depth from flashlight ONLY when active
    int shadow_light_index = base_num_lights; // Always use flashlight index
    
    if(flashlight_active && num_lights > 0 && shadow_light_index < num_lights) {
        glBindFramebuffer(GL_FRAMEBUFFER, lights[shadow_light_index].shadowFBO);
        glViewport(0, 0, 512, 512);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shadow_program);
        mat4_t lightProjection = m4_perspective(60.0f, 1.0f, 0.1f, 25.0f); // Increased range for flashlight
        
        vec3_t lightTarget;
        if (flashlight_active) {
            // Flashlight points in camera direction
            lightTarget = v3_add(lights[shadow_light_index].position, camera.front);
        } else {
            // Candle light looks DOWN from its position
            lightTarget = vec3(lights[shadow_light_index].position.x, 
                              lights[shadow_light_index].position.y - 2.0f, 
                              lights[shadow_light_index].position.z);
        }
        
        mat4_t lightView = m4_look_at(lights[shadow_light_index].position, lightTarget, vec3(0.0f, 1.0f, 0.0f));
        mat4_t lightSpaceMatrix = m4_mul(lightProjection, lightView);
        
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "lightSpaceMatrix"), 1, GL_FALSE, (float*)lightSpaceMatrix.m);
        
        // Render ALL scene geometry for shadows (must match main rendering)
        
        // Floor - at exact ground level for clean shadows
        mat4_t model = m4_translation(vec3(0.0f, 0.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(floor_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);
        
        // Walls
        model = m4_mul(m4_translation(vec3(0.0f, 2.0f, -5.0f)), m4_scaling(vec3(10.0f, 4.0f, 0.2f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Bar table and dining tables (must match main rendering exactly)
        
        // Bar table
        model = m4_mul(m4_translation(vec3(4.0f, 0.0f, 0.0f)), 
                       m4_scaling(vec3(2.5f, 0.8f, 0.8f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(bench_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, bench_mesh.vertex_count);
        
        // Beer mugs on bar
        for(int i = 0; i < 4; i++) {
            float bar_x = 3.0f + (i * 0.5f) - 1.0f;
            model = m4_mul(m4_translation(vec3(bar_x, 0.65f, 0.0f)), 
                           m4_scaling(vec3(0.12f, 0.12f, 0.12f)));
            glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(beer_mug_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
        }
        
        // Bottles on bar
        for(int i = 0; i < 2; i++) {
            float bottle_x = 4.5f + (i * 0.8f) - 0.4f;
            model = m4_mul(m4_translation(vec3(bottle_x, 0.65f, -0.2f)), 
                           m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
            glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(green_bottle_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
        }
        
        // 3 dining tables with stools (must match main rendering exactly)
        float table_positions[][2] = {{-3.0f, -1.5f}, {0.5f, -4.0f}, {3.5f, -2.0f}};
        for(int i = 0; i < 3; i++) {
            // Round table
            model = m4_mul(m4_translation(vec3(table_positions[i][0], 0.0f, table_positions[i][1])), 
                           m4_scaling(vec3(0.7f, 0.7f, 0.7f)));
            glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(table_round_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, table_round_mesh.vertex_count);
            
            // 3 octagonal stools around each table
            for(int stool = 0; stool < 3; stool++) {
                float angle = stool * (2.0f * M_PIf / 3.0f);
                float stool_x = table_positions[i][0] + cosf(angle) * 1.5f;
                float stool_z = table_positions[i][1] + sinf(angle) * 1.5f;
                
                model = m4_mul(m4_translation(vec3(stool_x, 0.0f, stool_z)), 
                               m4_scaling(vec3(0.4f, 0.4f, 0.4f)));
                glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
                glBindVertexArray(stool_mesh.vao_id);
                glDrawArrays(GL_TRIANGLES, 0, stool_mesh.vertex_count);
            }
        }
        
        // Table items (selective)
        // Table 0: beer mug only
        model = m4_mul(m4_translation(vec3(table_positions[0][0], 0.6f, table_positions[0][1])), 
                       m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(beer_mug_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
        
        // Table 1: food plate and bottle
        model = m4_mul(m4_translation(vec3(table_positions[1][0], 0.6f, table_positions[1][1])), 
                       m4_scaling(vec3(0.15f, 0.15f, 0.15f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(food_plate_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, food_plate_mesh.vertex_count);
        
        model = m4_mul(m4_translation(vec3(table_positions[1][0] + 0.3f, 0.6f, table_positions[1][1] + 0.2f)), 
                       m4_scaling(vec3(0.08f, 0.08f, 0.08f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(green_bottle_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
        
        // Table 2: nothing (empty)
        
        // Fireplace
        model = m4_mul(m4_translation(vec3(4.0f, 1.0f, -2.0f)), m4_scaling(vec3(1.0f, 2.0f, 1.0f)));
        glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(cube_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
        
        // Real barrel models (must match main rendering exactly)
        float barrel_positions[][2] = {{5.0f, 2.0f}, {-5.0f, 1.5f}, {3.0f, -3.0f}, {-2.0f, -4.0f}};
        for(int i = 0; i < 4; i++) {
            model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.0f, barrel_positions[i][1])), 
                           m4_scaling(vec3(0.8f, 0.8f, 0.8f)));
            glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(barrel_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, barrel_mesh.vertex_count);
        }
        
        // Clean shadow pass - no old procedural decorations
        
        // No static candles in shadow pass either
        
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
    
    // Render detailed wooden floor at ground level
    model = m4_translation(vec3(0.0f, 0.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.35f, 0.2f); // Rich wood floor
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    glBindVertexArray(floor_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);
    
    
    // Render tavern walls
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.3f, 0.2f); // Dark wood
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    model = m4_mul(m4_translation(vec3(0.0f, 2.0f, -5.0f)), m4_scaling(vec3(10.0f, 4.0f, 0.2f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Create bar table (one big bench acting as bar)
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.25f, 0.15f); // Bar wood
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    model = m4_mul(m4_translation(vec3(4.0f, 0.0f, 0.0f)), 
                   m4_scaling(vec3(2.5f, 0.8f, 0.8f))); // Long bar table
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(bench_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, bench_mesh.vertex_count);
    
    // Create 3 round dining tables with better spacing
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.3f, 0.2f); // Table wood
    float table_positions[][2] = {{-3.0f, -1.5f}, {0.5f, -4.0f}, {3.5f, -2.0f}}; // x, z positions - more spread out
    for(int i = 0; i < 3; i++) {
        // Round table
        model = m4_mul(m4_translation(vec3(table_positions[i][0], 0.0f, table_positions[i][1])), 
                       m4_scaling(vec3(0.7f, 0.7f, 0.7f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(table_round_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, table_round_mesh.vertex_count);
        
        // 3 octagonal stools around each table with better spacing
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.2f, 0.1f); // Stool wood
        for(int stool = 0; stool < 3; stool++) {
            float angle = stool * (2.0f * M_PIf / 3.0f); // 120 degrees apart
            float stool_x = table_positions[i][0] + cosf(angle) * 1.5f; // Increased radius from 1.2f to 1.5f
            float stool_z = table_positions[i][1] + sinf(angle) * 1.5f;
            
            model = m4_mul(m4_translation(vec3(stool_x, 0.0f, stool_z)), 
                           m4_scaling(vec3(0.4f, 0.4f, 0.4f)));
            glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
            glBindVertexArray(stool_mesh.vao_id);
            glDrawArrays(GL_TRIANGLES, 0, stool_mesh.vertex_count);
        }
        // Reset color for next table
        glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f, 0.3f, 0.2f);
    }
    
    // Add beer mugs and bottles on the bar table
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.3f, 0.2f); // Wood mug color
    for(int i = 0; i < 4; i++) { // 4 beer mugs across the bar
        float bar_x = 3.0f + (i * 0.5f) - 1.0f; // Spread across bar length
        model = m4_mul(m4_translation(vec3(bar_x, 0.65f, 0.0f)), 
                       m4_scaling(vec3(0.12f, 0.12f, 0.12f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(beer_mug_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
    }
    
    // Add bottles on bar
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.1f, 0.4f, 0.1f); // Green glass
    for(int i = 0; i < 2; i++) {
        float bottle_x = 4.5f + (i * 0.8f) - 0.4f;
        model = m4_mul(m4_translation(vec3(bottle_x, 0.65f, -0.2f)), 
                       m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(green_bottle_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
    }
    
    // Add items on dining tables (selective)
    // Table 0: beer mug only
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.4f, 0.3f, 0.2f);
    model = m4_mul(m4_translation(vec3(table_positions[0][0], 0.6f, table_positions[0][1])), 
                   m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(beer_mug_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
    
    // Table 1: food plate and bottle
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.8f, 0.7f, 0.6f); // Ceramic plate
    model = m4_mul(m4_translation(vec3(table_positions[1][0], 0.6f, table_positions[1][1])), 
                   m4_scaling(vec3(0.15f, 0.15f, 0.15f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(food_plate_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, food_plate_mesh.vertex_count);
    
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.1f, 0.4f, 0.1f); // Green bottle
    model = m4_mul(m4_translation(vec3(table_positions[1][0] + 0.3f, 0.6f, table_positions[1][1] + 0.2f)), 
                   m4_scaling(vec3(0.08f, 0.08f, 0.08f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(green_bottle_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
    
    // Table 2: nothing (empty table)
    
    // Render fireplace
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f, 0.3f, 0.3f); // Stone
    model = m4_mul(m4_translation(vec3(4.0f, 1.0f, -2.0f)), m4_scaling(vec3(1.0f, 2.0f, 1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
    glBindVertexArray(cube_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);
    
    // Render real barrel models around the tavern
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.4f, 0.2f); // Natural wood
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    float barrel_positions[][2] = {{5.0f, 2.0f}, {-5.0f, 1.5f}, {3.0f, -3.0f}, {-2.0f, -4.0f}};
    for(int i = 0; i < 4; i++) {
        model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.0f, barrel_positions[i][1])), 
                       m4_scaling(vec3(0.8f, 0.8f, 0.8f)));
        glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1, GL_FALSE, (float*)model.m);
        glBindVertexArray(barrel_mesh.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, barrel_mesh.vertex_count);
    }
    
    // Add stone corbels to walls (stepped architecture creates excellent SSAO)
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.6f, 0.5f, 0.4f); // Light stone
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    float corbel_positions[][2] = {{-3.0f, -4.8f}, {0.0f, -4.8f}, {3.0f, -4.8f}};
    for(int i = 0; i < 3; i++) {
        // Create stepped corbel geometry (creates deep crevices for SSAO)
        float steps[] = {0.5f, 0.4f, 0.3f, 0.2f};
        for(int step = 0; step < 4; step++) {
            float step_size = steps[step];
            float y = 2.2f + step * 0.15f;
            
            model = m4_mul(m4_translation(vec3(corbel_positions[i][0], y, corbel_positions[i][1] + step_size * 0.5f)), 
                           m4_scaling(vec3(0.6f, 0.12f, step_size)));
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
    
    // Clean tavern scene - only real 3D models
    
    // No static candles - flashlight only
    
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
    
    // Bind shadow maps - only when flashlight is active
    if(flashlight_active && num_lights > 0 && shadow_light_index < num_lights) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, lights[shadow_light_index].shadowCubeMap);
        glUniform1i(glGetUniformLocation(lighting_program, "shadowMap0"), 4);
        
        mat4_t lightProjection = m4_perspective(60.0f, 1.0f, 0.1f, 25.0f); // Match shadow pass range
        
        vec3_t lightTarget;
        if (flashlight_active) {
            // Flashlight points in camera direction
            lightTarget = v3_add(lights[shadow_light_index].position, camera.front);
        } else {
            // Candle light looks DOWN from its position
            lightTarget = vec3(lights[shadow_light_index].position.x, 
                              lights[shadow_light_index].position.y - 2.0f, 
                              lights[shadow_light_index].position.z);
        }
        
        mat4_t lightView = m4_look_at(lights[shadow_light_index].position, lightTarget, vec3(0.0f, 1.0f, 0.0f));
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

void main_state_cleanup(GLFWwindow *window, void *args) {
    texture_manager_cleanup(&texture_manager);
}

// Texture management implementation
void material_init(Material *mat) {
    rafgl_texture_init(&mat->diffuse);
    rafgl_texture_init(&mat->normal);
    rafgl_texture_init(&mat->specular);
    mat->has_normal_map = 0;
    mat->has_specular_map = 0;
    mat->roughness = 0.8f;
    mat->metallic = 0.0f;
}

void material_load_diffuse(Material *mat, const char *diffuse_path) {
    if (rafgl_texture_load_basic(diffuse_path, &mat->diffuse) == 0) {
        printf("Successfully loaded diffuse texture: %s\n", diffuse_path);
    } else {
        printf("Failed to load diffuse texture: %s\n", diffuse_path);
    }
}

void material_load_normal(Material *mat, const char *normal_path) {
    if (rafgl_texture_load_basic(normal_path, &mat->normal) == 0) {
        mat->has_normal_map = 1;
        printf("Successfully loaded normal texture: %s\n", normal_path);
    } else {
        printf("Failed to load normal texture: %s\n", normal_path);
        mat->has_normal_map = 0;
    }
}

void material_load_specular(Material *mat, const char *specular_path) {
    if (rafgl_texture_load_basic(specular_path, &mat->specular) == 0) {
        mat->has_specular_map = 1;
        printf("Successfully loaded specular texture: %s\n", specular_path);
    } else {
        printf("Failed to load specular texture: %s\n", specular_path);
        mat->has_specular_map = 0;
    }
}

void material_bind(Material *mat, GLuint shader_program) {
    // Bind diffuse texture
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, mat->diffuse.tex_id);
    glUniform1i(glGetUniformLocation(shader_program, "texture_diffuse1"), 5);
    
    // Bind normal texture if available
    if (mat->has_normal_map) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, mat->normal.tex_id);
        glUniform1i(glGetUniformLocation(shader_program, "texture_normal1"), 6);
        glUniform1f(glGetUniformLocation(shader_program, "hasNormalMap"), 1.0f);
    } else {
        glUniform1f(glGetUniformLocation(shader_program, "hasNormalMap"), 0.0f);
    }
    
    // Bind specular texture if available
    if (mat->has_specular_map) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, mat->specular.tex_id);
        glUniform1i(glGetUniformLocation(shader_program, "texture_specular1"), 7);
    }
    
    // Set material properties
    glUniform1f(glGetUniformLocation(shader_program, "material.roughness"), mat->roughness);
    glUniform1f(glGetUniformLocation(shader_program, "material.metallic"), mat->metallic);
    glUniform1f(glGetUniformLocation(shader_program, "hasTexture"), 1.0f);
}

void texture_manager_init(TextureManager *tm) {
    // Initialize all materials
    material_init(&tm->wood_planks);
    material_init(&tm->oak_table);
    material_init(&tm->dark_wood);
    material_init(&tm->medieval_stone);
    material_init(&tm->brick_wall);
    material_init(&tm->iron_metal);
    material_init(&tm->rusty_metal);
    material_init(&tm->ceramic);
    material_init(&tm->leather);
    
    // Try to load textures (will work when files are present)
    
    // Wood materials
    material_load_diffuse(&tm->wood_planks, "res/textures/wood_planks_diffuse.png");
    material_load_normal(&tm->wood_planks, "res/textures/wood_planks_normal.png");
    tm->wood_planks.roughness = 0.8f;
    tm->wood_planks.metallic = 0.0f;
    
    material_load_diffuse(&tm->oak_table, "res/textures/oak_table_diffuse.png");
    material_load_normal(&tm->oak_table, "res/textures/oak_table_normal.png");
    tm->oak_table.roughness = 0.6f;
    tm->oak_table.metallic = 0.0f;
    
    material_load_diffuse(&tm->dark_wood, "res/textures/dark_wood_diffuse.png");
    material_load_normal(&tm->dark_wood, "res/textures/dark_wood_normal.png");
    tm->dark_wood.roughness = 0.7f;
    tm->dark_wood.metallic = 0.0f;
    
    // Stone materials  
    material_load_diffuse(&tm->medieval_stone, "res/textures/medieval_stone_diffuse.png");
    material_load_normal(&tm->medieval_stone, "res/textures/medieval_stone_normal.png");
    tm->medieval_stone.roughness = 0.9f;
    tm->medieval_stone.metallic = 0.0f;
    
    material_load_diffuse(&tm->brick_wall, "res/textures/brick_diffuse.png");
    material_load_normal(&tm->brick_wall, "res/textures/brick_normal.png");
    tm->brick_wall.roughness = 0.8f;
    tm->brick_wall.metallic = 0.0f;
    
    // Metal materials
    material_load_diffuse(&tm->iron_metal, "res/textures/iron_metal_diffuse.png");
    material_load_normal(&tm->iron_metal, "res/textures/iron_metal_normal.png");
    tm->iron_metal.roughness = 0.3f;
    tm->iron_metal.metallic = 0.9f;
    
    material_load_diffuse(&tm->rusty_metal, "res/textures/rusty_metal_diffuse.png");
    material_load_normal(&tm->rusty_metal, "res/textures/rusty_metal_normal.png");
    tm->rusty_metal.roughness = 0.7f;
    tm->rusty_metal.metallic = 0.6f;
    
    // Other materials
    material_load_diffuse(&tm->ceramic, "res/textures/ceramic_diffuse.png");
    material_load_normal(&tm->ceramic, "res/textures/ceramic_normal.png");
    tm->ceramic.roughness = 0.2f;
    tm->ceramic.metallic = 0.0f;
    
    material_load_diffuse(&tm->leather, "res/textures/leather_diffuse.png");
    material_load_normal(&tm->leather, "res/textures/leather_normal.png");
    tm->leather.roughness = 0.8f;
    tm->leather.metallic = 0.0f;
}

void texture_manager_cleanup(TextureManager *tm) {
    rafgl_texture_cleanup(&tm->wood_planks.diffuse);
    rafgl_texture_cleanup(&tm->wood_planks.normal);
    rafgl_texture_cleanup(&tm->oak_table.diffuse);
    rafgl_texture_cleanup(&tm->oak_table.normal);
    rafgl_texture_cleanup(&tm->dark_wood.diffuse);
    rafgl_texture_cleanup(&tm->dark_wood.normal);
    rafgl_texture_cleanup(&tm->medieval_stone.diffuse);
    rafgl_texture_cleanup(&tm->medieval_stone.normal);
    rafgl_texture_cleanup(&tm->brick_wall.diffuse);
    rafgl_texture_cleanup(&tm->brick_wall.normal);
    rafgl_texture_cleanup(&tm->iron_metal.diffuse);
    rafgl_texture_cleanup(&tm->iron_metal.normal);
    rafgl_texture_cleanup(&tm->rusty_metal.diffuse);
    rafgl_texture_cleanup(&tm->rusty_metal.normal);
    rafgl_texture_cleanup(&tm->ceramic.diffuse);
    rafgl_texture_cleanup(&tm->ceramic.normal);
    rafgl_texture_cleanup(&tm->leather.diffuse);
    rafgl_texture_cleanup(&tm->leather.normal);
}

// Simplified approach using existing RAFGL functions creatively
void create_detailed_barrel_mesh(rafgl_meshPUN_t *mesh, float radius, float height) {
    // Use a subdivided plane wrapped around to approximate a barrel
    // For now, just use a cube as a placeholder - we'll enhance visuals with multiple objects
    rafgl_meshPUN_load_cube(mesh, radius);
}

void create_stone_corbel_mesh(rafgl_meshPUN_t *mesh) {
    // Create a stepped corbel using a cube base - we'll add detail with multiple renders
    rafgl_meshPUN_load_cube(mesh, 0.5f);
}
