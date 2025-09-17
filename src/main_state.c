#include <glad/glad.h>
#include <main_state.h>
#include <math.h>
#include <tavern_renderer.h>

#include <rafgl.h>

static int w, h;
static Camera camera;
static GBuffer gbuffer;
static PointLight lights[8];
static int num_lights = 0;
static int base_num_lights =
    0; // Will be set to candle count during initialization
static int flashlight_active = 0;
static float flashlight_distance =
    0.0f; // Distance from camera (in 0.5 increments)
static float global_light_radius = 8.0f; // Adjustable radius for all lights
static int flashlight_only_shadows = 1;  // Only flashlight casts shadows
static TextureManager texture_manager;

static rafgl_meshPUN_t floor_mesh;
static GLuint gbuffer_program, lighting_program, shadow_program,
    postprocess_program, ssao_program;
static FullscreenQuad quad;

// Post-processing framebuffer
static GLuint postprocessFBO, colorTexture;

// SSAO
static GLuint ssaoFBO, ssaoColorBuffer;

// Tavern objects - loaded from .obj files
static rafgl_meshPUN_t barrel_mesh, table_round_mesh, bench_mesh, stool_mesh;
static rafgl_meshPUN_t beer_mug_mesh, green_bottle_mesh, wall_candle_mesh,
    food_plate_mesh;
// Keep basic cube for fallback/debugging
static rafgl_meshPUN_t cube_mesh;
// Procedural candle geometry
static rafgl_meshPUN_t candle_base_mesh, candle_flame_mesh;

// Wall candles - Static models with flickering lights only
typedef struct {
  vec3_t position;     // Fixed wall position
  float intensity;     // Animated light intensity (0.7-1.0)
  float flicker_speed; // Animation speed
  float time_offset;   // Phase offset for variety
  int light_index;     // Index in lights array
} WallCandle;

// Table candles - Object hierarchy with programmatic animation (Parent: base,
// Child: animated flame)
typedef struct {
  vec3_t base_position; // Parent: candle base position (static)
  vec3_t flame_offset;  // Child: flame offset relative to base (animated)
  float intensity;      // Animated light intensity
  float flicker_speed;  // Animation speed
  float time_offset;    // Phase offset for variety
  int light_index;      // Index in lights array
} TableCandle;

static WallCandle wall_candles[3];
static int num_wall_candles = 3; // Re-enable wall candles
static TableCandle table_candles[3];
static int num_table_candles =
    3; // Re-enable table candles - they work correctly
static float animation_time = 0.0f;
static float startup_flashlight_timer = 0.0f;

// Scroll wheel callback for flashlight distance control
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  // Only adjust flashlight distance when F key is held and flashlight is active
  if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && flashlight_active) {
    // Scroll up = move light forward, scroll down = move light backward
    // Use discrete 0.5 increments for easy return to 0
    if (yoffset > 0) {
      flashlight_distance += 0.5f; // Forward by 0.5
    } else if (yoffset < 0) {
      flashlight_distance -= 0.5f; // Backward by 0.5
    }

    // Clamp distance to reasonable range
    if (flashlight_distance < -5.0f)
      flashlight_distance = -5.0f; // Behind camera
    if (flashlight_distance > 10.0f)
      flashlight_distance = 10.0f; // Far into scene

    printf("Flashlight distance: %.1f\n", flashlight_distance);
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

  // Create procedural candle geometry using available RAFGL functions
  rafgl_meshPUN_init(&candle_base_mesh);
  rafgl_meshPUN_load_cube(&candle_base_mesh,
                          0.1f); // Simple cube for candle base

  rafgl_meshPUN_init(&candle_flame_mesh);
  rafgl_meshPUN_load_cube(&candle_flame_mesh, 0.05f); // Small cube for flame

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTexture, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Setup SSAO framebuffer
  glGenFramebuffers(1, &ssaoFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

  glGenTextures(1, &ssaoColorBuffer);
  glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         ssaoColorBuffer, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Create detailed floor geometry for better shadow receiving
  rafgl_meshPUN_init(&floor_mesh);
  rafgl_meshPUN_load_plane(&floor_mesh, 20.0f, 20.0f, 50,
                           50); // High subdivision for shadows

  // Load tavern models from .obj files
  rafgl_meshPUN_init(&barrel_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &barrel_mesh, "res/models/Wooden barrel with metal bands/base.obj");

  rafgl_meshPUN_init(&table_round_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &table_round_mesh,
      "res/models/Round wooden table with pedestal base/base.obj");

  rafgl_meshPUN_init(&bench_mesh);
  rafgl_meshPUN_load_from_OBJ(&bench_mesh,
                              "res/models/Wooden bench with panels/base.obj");

  rafgl_meshPUN_init(&stool_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &stool_mesh, "res/models/Wooden stool with ocagonal seat/base.obj");

  rafgl_meshPUN_init(&beer_mug_mesh);
  rafgl_meshPUN_load_from_OBJ(&beer_mug_mesh,
                              "res/models/Wooden beer mug with foam/base.obj");

  rafgl_meshPUN_init(&green_bottle_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &green_bottle_mesh, "res/models/Green bottle with cork stopper/base.obj");

  rafgl_meshPUN_init(&wall_candle_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &wall_candle_mesh, "res/models/Wall-mounted candle with flame/base.obj");

  rafgl_meshPUN_init(&food_plate_mesh);
  rafgl_meshPUN_load_from_OBJ(
      &food_plate_mesh, "res/models/Plate with steak and drumstick/base.obj");

  // Keep basic cube for debugging
  rafgl_meshPUN_init(&cube_mesh);
  rafgl_meshPUN_load_cube(&cube_mesh, 1.0f);

  // Initialize wall candles - visual position close to walls
  wall_candles[0] = (WallCandle){
      .position =
          vec3(0.0f, 1.8f, -5.15f), // Back wall - visual position close to wall
      .intensity = 1.0f,
      .flicker_speed = 3.0f,
      .time_offset = 0.0f,
      .light_index = 0};

  wall_candles[1] = (WallCandle){
      .position =
          vec3(-5.15f, 1.5f, 2.0f), // Left wall - visual position close to wall
      .intensity = 1.0f,
      .flicker_speed = 2.5f,
      .time_offset = 1.0f,
      .light_index = 1};

  wall_candles[2] = (WallCandle){
      .position = vec3(5.15f, 1.5f,
                       -1.0f), // Right wall - visual position close to wall
      .intensity = 1.0f,
      .flicker_speed = 2.8f,
      .time_offset = 2.0f,
      .light_index = 2};

  // Initialize table candles - Object hierarchy with programmatic animation
  // Table model scaled by 0.7f, need to calculate actual table surface height
  // Assuming original table height ~2.0f, scaled = 1.4f surface height
  float table_positions[][2] = {{-3.5f, 1.0f}, {-1.0f, 3.5f}, {1.5f, 0.5f}};
  for (int i = 0; i < num_table_candles; i++) {
    table_candles[i] = (TableCandle){
        .base_position =
            vec3(table_positions[i][0], 1.4f,
                 table_positions[i][1]),        // Adjusted table surface height
        .flame_offset = vec3(0.0f, 0.0f, 0.0f), // Will be animated
        .intensity = 1.0f,
        .flicker_speed = 2.5f + i * 0.3f,
        .time_offset = i * 0.8f,
        .light_index = num_wall_candles + i};
  }

  // Create lights for all candles
  // Wall candles - calculate light position based on candle position and wall orientation
  for (int i = 0; i < num_wall_candles; i++) {
    // Calculate light offset from wall surface toward room center
    vec3_t light_offset = vec3(0.0f, 0.15f, 0.0f); // Default: above candle
    
    if (i == 0) {
      // Back wall candle - offset forward (positive Z) toward room
      light_offset = vec3(0.0f, 0.15f, 0.5f);
    } else if (i == 1) {
      // Left wall candle - offset right (positive X) toward room
      light_offset = vec3(0.5f, 0.15f, 0.0f);
    } else if (i == 2) {
      // Right wall candle - offset left (negative X) toward room
      light_offset = vec3(-0.5f, 0.15f, 0.0f);
    }
    
    vec3_t flame_pos = v3_add(wall_candles[i].position, light_offset);
    lights[i] =
        (PointLight){.position = flame_pos,
                     .color = vec3(1.0f, 0.6f, 0.3f), // Warm candle light
                     .radius = global_light_radius,
                     .shadowFBO = 0,
                     .shadowCubeMap = 0};
    setup_point_light_shadows(&lights[i], 512, 512);
  }

  // Table candles - position lights at flame location (will be updated in
  // animation loop)
  for (int i = 0; i < num_table_candles; i++) {
    vec3_t initial_flame_pos = v3_add(table_candles[i].base_position,
                                      vec3(0.0f, 0.12f, 0.0f)); // Above base

    lights[num_wall_candles + i] =
        (PointLight){.position = initial_flame_pos,
                     .color = vec3(1.0f, 0.6f, 0.3f), // Warm candle light
                     .radius = global_light_radius, // Use global radius for all
                     .shadowFBO = 0,
                     .shadowCubeMap = 0};
    setup_point_light_shadows(&lights[num_wall_candles + i], 512, 512);
  }

  num_lights = num_wall_candles + num_table_candles;
  base_num_lights = num_lights; // All candles are now base lights

  printf("INITIALIZATION: %d candle lights created (%d wall + %d table)\n",
         num_lights, num_wall_candles, num_table_candles);

  // Initialize texture manager
  texture_manager_init(&texture_manager);

  // Auto-activate flashlight at startup (so lights are visible immediately)
  flashlight_active = 1;
  lights[base_num_lights] = (PointLight){
      .position =
          v3_add(camera.position, v3_muls(camera.front, flashlight_distance)),
      .color = vec3(1.0f, 1.0f, 1.0f), // White flashlight
      .radius = global_light_radius,
      .shadowFBO = 0,
      .shadowCubeMap = 0};
  setup_point_light_shadows(&lights[base_num_lights], 512, 512);
  num_lights = base_num_lights + 1;
  // Flashlight auto-activated to initialize lighting

  glEnable(GL_DEPTH_TEST);
}

void main_state_update(GLFWwindow *window, float delta_time,
                       rafgl_game_data_t *game_data, void *args) {
  camera_update(&camera, window, delta_time);

  // Update animation time
  animation_time += delta_time;

  // Auto-deactivate startup flashlight after brief delay
  if (flashlight_active && startup_flashlight_timer >= 0.0f) {
    startup_flashlight_timer += delta_time;
    if (startup_flashlight_timer > 0.5f) { // 0.5 second delay
      flashlight_active = 0;
      num_lights = base_num_lights;     // Back to just candle lights
      startup_flashlight_timer = -1.0f; // Mark as completed
      // Flashlight auto-deactivated, candle lights now visible
    }
  }

  // Animate wall candle lights only (no geometry movement)
  for (int i = 0; i < num_wall_candles; i++) {
    WallCandle *candle = &wall_candles[i];

    // Calculate animation phase for this candle
    float phase = animation_time * candle->flicker_speed + candle->time_offset;

    // Animate flame intensity (0.7 to 1.0 range for realistic flicker)
    candle->intensity = 0.85f + 0.15f * sinf(phase) * sinf(phase * 1.3f);

    // Calculate light position based on candle position and wall orientation
    vec3_t flame_flicker =
        vec3(0.005f * sinf(phase * 2.1f), 0.01f * sinf(phase * 1.7f),
             0.005f * sinf(phase * 2.3f));

    // Calculate light offset from wall surface toward room center
    vec3_t light_offset = vec3(0.0f, 0.15f, 0.0f); // Default: above candle
    
    if (i == 0) {
      // Back wall candle - offset forward (positive Z) toward room
      light_offset = vec3(0.0f, 0.15f, 0.5f);
    } else if (i == 1) {
      // Left wall candle - offset right (positive X) toward room
      light_offset = vec3(0.5f, 0.15f, 0.0f);
    } else if (i == 2) {
      // Right wall candle - offset left (negative X) toward room
      light_offset = vec3(-0.5f, 0.15f, 0.0f);
    }

    lights[candle->light_index].position =
        v3_add(candle->position, v3_add(light_offset, flame_flicker));

    // Update light color
    lights[candle->light_index].color =
        vec3(candle->intensity * 1.0f, // Red channel
             candle->intensity * 0.6f, // Green channel
             candle->intensity * 0.3f  // Blue channel (warm orange)
        );
  }

  // Animate table candle flames - Object hierarchy with programmatic movement
  for (int i = 0; i < num_table_candles; i++) {
    TableCandle *candle = &table_candles[i];

    // Calculate animation phase for this candle
    float phase = animation_time * candle->flicker_speed + candle->time_offset;

    // Animate flame intensity (0.7 to 1.0 range for realistic flicker)
    candle->intensity = 0.85f + 0.15f * sinf(phase) * sinf(phase * 1.3f);

    // Animate flame position (subtle wobble - child movement relative to
    // parent)
    candle->flame_offset.x = 0.01f * sinf(phase * 2.1f);
    candle->flame_offset.y =
        0.02f * sinf(phase * 1.7f); // More vertical movement
    candle->flame_offset.z = 0.01f * sinf(phase * 2.3f);

    // Update light properties - Position light at animated flame location
    // (Parent + Child + flame height)
    vec3_t flame_light_pos =
        v3_add(candle->base_position,
               v3_add(candle->flame_offset, vec3(0.0f, 0.12f, 0.0f)));
    lights[candle->light_index].position = flame_light_pos;
    lights[candle->light_index].color =
        vec3(candle->intensity * 1.0f, // Red channel
             candle->intensity * 0.6f, // Green channel
             candle->intensity * 0.3f  // Blue channel (warm orange)
        );
  }

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
            .radius = 50.0f,                 // Very strong flashlight
            .shadowFBO = 0,
            .shadowCubeMap = 0};

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

  // Handle light radius adjustment with Q/E keys
  static int q_key_pressed = 0;
  static int e_key_pressed = 0;

  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
    if (!q_key_pressed) {
      // Q key pressed - decrease radius
      global_light_radius -= 1.0f;
      if (global_light_radius < 1.0f)
        global_light_radius = 1.0f; // Min radius

      // Update all active lights
      for (int i = 0; i < base_num_lights; i++) {
        lights[i].radius = global_light_radius;
      }
      printf("Light radius decreased to: %.1f\n", global_light_radius);
    }
    q_key_pressed = 1;
  } else {
    q_key_pressed = 0;
  }

  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    if (!e_key_pressed) {
      // E key pressed - increase radius
      global_light_radius += 1.0f;
      if (global_light_radius > 20.0f)
        global_light_radius = 20.0f; // Max radius

      // Update all active lights
      for (int i = 0; i < base_num_lights; i++) {
        lights[i].radius = global_light_radius;
      }
      printf("Light radius increased to: %.1f\n", global_light_radius);
    }
    e_key_pressed = 1;
  } else {
    e_key_pressed = 0;
  }

  // Handle shadow mode toggle with TAB key
  static int tab_key_pressed = 0;
  if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
    if (!tab_key_pressed) {
      // TAB key pressed - toggle shadow mode
      flashlight_only_shadows = !flashlight_only_shadows;
      if (flashlight_only_shadows) {
        printf("Shadow Mode: FLASHLIGHT ONLY\n");
      } else {
        printf("Shadow Mode: ALL LIGHTS\n");
      }
    }
    tab_key_pressed = 1;
  } else {
    tab_key_pressed = 0;
  }

  // Handle flashlight distance reset with R key
  static int r_key_pressed = 0;
  if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
    if (!r_key_pressed && flashlight_active) {
      // R key pressed - reset flashlight distance to 0
      flashlight_distance = 0.0f;
      printf("Flashlight distance reset to: %.1f\n", flashlight_distance);
    }
    r_key_pressed = 1;
  } else {
    r_key_pressed = 0;
  }

  // Update flashlight position to follow camera at controlled distance
  if (flashlight_active) {
    lights[base_num_lights].position =
        v3_add(camera.position, v3_muls(camera.front, flashlight_distance));
  }
}

// Render all scene geometry for shadows
void render_scene_geometry(GLuint shadow_program) {
  // Floor - at exact ground level for clean shadows
  mat4_t model = m4_translation(vec3(0.0f, 0.0f, 0.0f));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(floor_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);

  // Complete wall system for shadows
  // Back wall
  model = m4_mul(m4_translation(vec3(0.0f, 2.0f, -5.5f)),
                 m4_scaling(vec3(11.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Left wall
  model = m4_mul(m4_translation(vec3(-5.5f, 2.0f, 0.0f)),
                 m4_scaling(vec3(0.2f, 4.0f, 11.0f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Right wall
  model = m4_mul(m4_translation(vec3(5.5f, 2.0f, 0.0f)),
                 m4_scaling(vec3(0.2f, 4.0f, 11.0f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Front wall segments
  model = m4_mul(m4_translation(vec3(-3.0f, 2.0f, 5.5f)),
                 m4_scaling(vec3(5.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  model = m4_mul(m4_translation(vec3(3.0f, 2.0f, 5.5f)),
                 m4_scaling(vec3(5.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  model = m4_mul(m4_translation(vec3(0.0f, 3.0f, 5.5f)),
                 m4_scaling(vec3(2.0f, 2.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Massive bar counter
  model = m4_mul(m4_translation(vec3(3.5f, 0.0f, -2.0f)),
                 m4_scaling(vec3(4.5f, 1.2f, 1.5f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(bench_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, bench_mesh.vertex_count);

  // Beer mugs on massive bar counter
  for (int i = 0; i < 4; i++) {
    float bar_x = 2.0f + (i * 0.8f) - 1.0f;
    model = m4_mul(m4_translation(vec3(bar_x, 0.95f, -2.0f)),
                   m4_scaling(vec3(0.12f, 0.12f, 0.12f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(beer_mug_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
  }

  // Bottles on bar counter
  for (int i = 0; i < 2; i++) {
    float bottle_x = 4.0f + (i * 0.8f) - 0.4f;
    model = m4_mul(m4_translation(vec3(bottle_x, 0.62f, -1.8f)),
                   m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(green_bottle_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
  }

  // 3 dining tables with stools - repositioned
  float table_positions[][2] = {{-3.5f, 1.0f}, {-1.0f, 3.5f}, {1.5f, 0.5f}};
  for (int i = 0; i < 3; i++) {
    // Round table
    model = m4_mul(m4_translation(vec3(table_positions[i][0], 0.0f,
                                       table_positions[i][1])),
                   m4_scaling(vec3(0.7f, 0.7f, 0.7f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(table_round_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, table_round_mesh.vertex_count);

    // 3 octagonal stools around each table
    for (int stool = 0; stool < 3; stool++) {
      float angle = stool * (2.0f * M_PIf / 3.0f);
      float stool_x = table_positions[i][0] + cosf(angle) * 1.5f;
      float stool_z = table_positions[i][1] + sinf(angle) * 1.5f;

      model = m4_mul(m4_translation(vec3(stool_x, 0.0f, stool_z)),
                     m4_scaling(vec3(0.4f, 0.4f, 0.4f)));
      glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                         GL_FALSE, (float *)model.m);
      glBindVertexArray(stool_mesh.vao_id);
      glDrawArrays(GL_TRIANGLES, 0, stool_mesh.vertex_count);
    }
  }

  // Barrels - repositioned, removed fireplace barrel
  float barrel_positions[][2] = {
      {4.5f, 4.0f}, {-4.5f, 4.0f}, {-4.5f, -1.0f}, {2.0f, 4.0f}};
  for (int i = 0; i < 4; i++) {
    model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.0f,
                                       barrel_positions[i][1])),
                   m4_scaling(vec3(0.8f, 0.8f, 0.8f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(barrel_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, barrel_mesh.vertex_count);
  }

  // Fireplace - repositioned to back corner
  model = m4_mul(m4_translation(vec3(-4.5f, 1.0f, -4.0f)),
                 m4_scaling(vec3(1.0f, 2.0f, 1.0f)));
  glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1, GL_FALSE,
                     (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Wall candles with proper rotations
  for (int i = 0; i < num_wall_candles; i++) {
    WallCandle *candle = &wall_candles[i];

    // Apply wall-specific rotations to face tavern center
    mat4_t rotation = m4_identity();
    if (i == 0) {
      // Back wall candle (no rotation needed - faces forward)
      rotation = m4_identity();
    } else if (i == 1) {
      // Left wall candle - rotate 90° to face right (toward center)
      rotation = m4_rotation_y(M_PIf / 2.0f);
    } else if (i == 2) {
      // Right wall candle - rotate -90° to face left (toward center)
      rotation = m4_rotation_y(-M_PIf / 2.0f);
    }

    model = m4_mul(m4_translation(candle->position),
                   m4_mul(rotation, m4_scaling(vec3(0.4f, 0.4f, 0.4f))));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(wall_candle_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, wall_candle_mesh.vertex_count);
  }

  // Table candles
  for (int i = 0; i < num_table_candles; i++) {
    TableCandle *candle = &table_candles[i];

    // Candle base
    model = m4_mul(m4_translation(candle->base_position),
                   m4_scaling(vec3(0.3f, 0.8f, 0.3f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(candle_base_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, candle_base_mesh.vertex_count);

    // Candle flame
    vec3_t flame_pos =
        v3_add(candle->base_position,
               v3_add(candle->flame_offset, vec3(0.0f, 0.12f, 0.0f)));
    model =
        m4_mul(m4_translation(flame_pos), m4_scaling(vec3(0.2f, 0.4f, 0.2f)));
    glUniformMatrix4fv(glGetUniformLocation(shadow_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(candle_flame_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, candle_flame_mesh.vertex_count);
  }
}

void main_state_render(GLFWwindow *window, void *args) {
  // Shadow pass - render depth from active lights (candles + flashlight if
  // active)
  int num_shadow_lights = base_num_lights; // Start with candle lights
  if (flashlight_active) {
    num_shadow_lights = num_lights; // Include flashlight if active
  }

  // Render cube map shadows for all active lights using omnidirectional system
  for (int shadow_light_index = 0; shadow_light_index < num_shadow_lights;
       shadow_light_index++) {
    render_cube_shadow_map(&lights[shadow_light_index], shadow_program,
                           render_scene_geometry);
  }

  // Geometry pass - render to G-Buffer
  gbuffer_bind_for_writing(&gbuffer);

  glUseProgram(gbuffer_program);

  mat4_t view = camera_get_view_matrix(&camera);
  mat4_t projection = m4_perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
  mat4_t model = m4_identity();

  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "view"), 1, GL_FALSE,
                     (float *)view.m);
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "projection"), 1,
                     GL_FALSE, (float *)projection.m);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);

  // Render detailed wooden floor at ground level
  model = m4_translation(vec3(0.0f, 0.0f, 0.0f));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f,
              0.35f, 0.2f); // Rich wood floor
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
  glBindVertexArray(floor_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, floor_mesh.vertex_count);

  // Render complete tavern wall system
  glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.5f,
              0.3f, 0.2f); // Dark wood
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);

  // Back wall
  model = m4_mul(m4_translation(vec3(0.0f, 2.0f, -5.5f)),
                 m4_scaling(vec3(11.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Left wall
  model = m4_mul(m4_translation(vec3(-5.5f, 2.0f, 0.0f)),
                 m4_scaling(vec3(0.2f, 4.0f, 11.0f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Right wall
  model = m4_mul(m4_translation(vec3(5.5f, 2.0f, 0.0f)),
                 m4_scaling(vec3(0.2f, 4.0f, 11.0f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Front wall with door opening (two segments)
  // Left segment
  model = m4_mul(m4_translation(vec3(-3.0f, 2.0f, 5.5f)),
                 m4_scaling(vec3(5.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Right segment
  model = m4_mul(m4_translation(vec3(3.0f, 2.0f, 5.5f)),
                 m4_scaling(vec3(5.0f, 4.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Door lintel (top of door opening)
  model = m4_mul(m4_translation(vec3(0.0f, 3.0f, 5.5f)),
                 m4_scaling(vec3(2.0f, 2.0f, 0.2f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Create massive bar counter (Serbian šank)
  material_bind(&texture_manager.wooden_bench, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  model = m4_mul(m4_translation(vec3(3.5f, 0.0f, -2.0f)),
                 m4_scaling(vec3(4.5f, 1.2f, 1.5f))); // Massive bar counter
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(bench_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, bench_mesh.vertex_count);

  // Create 3 round dining tables with repositioned layout
  material_bind(&texture_manager.round_table, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  float table_positions[][2] = {{-3.5f, 1.0f},
                                {-1.0f, 3.5f},
                                {1.5f, 0.5f}}; // x, z positions - repositioned
  for (int i = 0; i < 3; i++) {
    // Round table
    model = m4_mul(m4_translation(vec3(table_positions[i][0], 0.0f,
                                       table_positions[i][1])),
                   m4_scaling(vec3(0.7f, 0.7f, 0.7f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(table_round_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, table_round_mesh.vertex_count);

    // 3 octagonal stools around each table with better spacing
    material_bind(&texture_manager.wooden_stool, gbuffer_program);
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
    for (int stool = 0; stool < 3; stool++) {
      float angle = stool * (2.0f * M_PIf / 3.0f); // 120 degrees apart
      float stool_x = table_positions[i][0] +
                      cosf(angle) * 1.5f; // Increased radius from 1.2f to 1.5f
      float stool_z = table_positions[i][1] + sinf(angle) * 1.5f;

      model = m4_mul(m4_translation(vec3(stool_x, 0.0f, stool_z)),
                     m4_scaling(vec3(0.4f, 0.4f, 0.4f)));
      glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                         GL_FALSE, (float *)model.m);
      glBindVertexArray(stool_mesh.vao_id);
      glDrawArrays(GL_TRIANGLES, 0, stool_mesh.vertex_count);
    }
    // Reset to table material for next table
    material_bind(&texture_manager.round_table, gbuffer_program);
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  }

  // Add beer mugs and bottles on the massive bar counter
  material_bind(&texture_manager.beer_mug, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  for (int i = 0; i < 4; i++) {             // 4 beer mugs across the bar
    float bar_x = 2.0f + (i * 0.8f) - 1.0f; // Spread across massive bar length
    model = m4_mul(m4_translation(vec3(bar_x, 0.95f, -2.0f)), // Raised by 0.3f
                   m4_scaling(vec3(0.12f, 0.12f, 0.12f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(beer_mug_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);
  }

  // Add bottles on bar counter - repositioned and lowered
  material_bind(&texture_manager.green_bottle, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  for (int i = 0; i < 2; i++) {
    float bottle_x = 4.0f + (i * 0.8f) - 0.4f; // Repositioned to bar center
    model = m4_mul(
        m4_translation(
            vec3(bottle_x, 0.62f, -1.8f)), // Lowered by 0.3f, positioned on bar
        m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(green_bottle_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);
  }

  // Add items on dining tables (selective) - raised by 0.8f
  // Table 0: beer mug only
  material_bind(&texture_manager.beer_mug, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  model = m4_mul(m4_translation(vec3(table_positions[0][0], 1.4f,
                                     table_positions[0][1])), // Raised by 0.8f
                 m4_scaling(vec3(0.1f, 0.1f, 0.1f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(beer_mug_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, beer_mug_mesh.vertex_count);

  // Table 1: food plate and bottle
  material_bind(&texture_manager.food_plate, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  model = m4_mul(m4_translation(vec3(table_positions[1][0], 1.4f,
                                     table_positions[1][1])), // Raised by 0.8f
                 m4_scaling(vec3(0.15f, 0.15f, 0.15f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(food_plate_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, food_plate_mesh.vertex_count);

  material_bind(&texture_manager.green_bottle, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  model = m4_mul(
      m4_translation(vec3(table_positions[1][0] + 0.3f, 1.4f, // Raised by 0.8f
                          table_positions[1][1] + 0.2f)),
      m4_scaling(vec3(0.08f, 0.08f, 0.08f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(green_bottle_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, green_bottle_mesh.vertex_count);

  // Table 2: nothing (empty table)

  // Render fireplace - repositioned to back corner
  glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.3f,
              0.3f, 0.3f); // Stone
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
  model = m4_mul(m4_translation(vec3(-4.5f, 1.0f, -4.0f)), // Back corner
                 m4_scaling(vec3(1.0f, 2.0f, 1.0f)));
  glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                     GL_FALSE, (float *)model.m);
  glBindVertexArray(cube_mesh.vao_id);
  glDrawArrays(GL_TRIANGLES, 0, cube_mesh.vertex_count);

  // Render real barrel models around the tavern - repositioned layout
  material_bind(&texture_manager.wooden_barrel, gbuffer_program);
  glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
  float barrel_positions[][2] = {
      {4.5f, 4.0f},
      {-4.5f, 4.0f},
      {-4.5f, -1.0f},
      {2.0f, 4.0f}}; // Repositioned, removed fireplace barrel
  for (int i = 0; i < 4; i++) {
    model = m4_mul(m4_translation(vec3(barrel_positions[i][0], 0.0f,
                                       barrel_positions[i][1])),
                   m4_scaling(vec3(0.8f, 0.8f, 0.8f)));
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(barrel_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, barrel_mesh.vertex_count);
  }

  // Render wall candles (static .obj models on walls) with proper rotations
  if (num_wall_candles > 0) {
    material_bind(&texture_manager.wall_candle, gbuffer_program);
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 1.0f);
    for (int i = 0; i < num_wall_candles; i++) {
      WallCandle *candle = &wall_candles[i];

      // Apply wall-specific rotations to face tavern center
      mat4_t rotation = m4_identity();
      if (i == 0) {
        // Back wall candle (no rotation needed - faces forward)
        rotation = m4_identity();
      } else if (i == 1) {
        // Left wall candle - rotate 90° to face right (toward center)
        rotation = m4_rotation_y(M_PIf / 2.0f);
      } else if (i == 2) {
        // Right wall candle - rotate -90° to face left (toward center)
        rotation = m4_rotation_y(-M_PIf / 2.0f);
      }

      model = m4_mul(m4_translation(candle->position),
                     m4_mul(rotation, m4_scaling(vec3(0.4f, 0.4f, 0.4f))));
      glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                         GL_FALSE, (float *)model.m);
      glBindVertexArray(wall_candle_mesh.vao_id);
      glDrawArrays(GL_TRIANGLES, 0, wall_candle_mesh.vertex_count);
    }
  }

  // Render table candles (Object hierarchy - Parent: base, Child: animated
  // flame)
  for (int i = 0; i < num_table_candles; i++) {
    TableCandle *candle = &table_candles[i];

    // Render candle base (parent - static, white wax color)
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 0.95f,
                0.95f, 0.9f); // White candle wax
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    model = m4_mul(m4_translation(candle->base_position),
                   m4_scaling(vec3(0.3f, 0.8f, 0.3f))); // Thin and tall
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(candle_base_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, candle_base_mesh.vertex_count);

    // Render candle flame (child - animated position, bright orange)
    glUniform3f(glGetUniformLocation(gbuffer_program, "materialColor"), 1.0f,
                0.7f, 0.2f); // Bright flame
    glUniform1f(glGetUniformLocation(gbuffer_program, "hasTexture"), 0.0f);
    vec3_t flame_pos =
        v3_add(candle->base_position,
               v3_add(candle->flame_offset,
                      vec3(0.0f, 0.12f, 0.0f))); // Just above base
    model = m4_mul(m4_translation(flame_pos),
                   m4_scaling(vec3(0.2f, 0.4f, 0.2f))); // Small flame
    glUniformMatrix4fv(glGetUniformLocation(gbuffer_program, "model"), 1,
                       GL_FALSE, (float *)model.m);
    glBindVertexArray(candle_flame_mesh.vao_id);
    glDrawArrays(GL_TRIANGLES, 0, candle_flame_mesh.vertex_count);
  }

  // Stone corbels removed - cleaner wall appearance

  // Wall weapon racks removed - replaced with wall candles for better medieval
  // ambiance

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

  glUniformMatrix4fv(glGetUniformLocation(ssao_program, "projection"), 1,
                     GL_FALSE, (float *)projection.m);

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

  // Bind shadow maps for all active lights (up to 4 supported by shader)
  int active_shadow_lights = base_num_lights; // Always bind candle shadows
  if (flashlight_active) {
    active_shadow_lights = num_lights; // Include flashlight shadow when active
  }

  // Bind shadow maps for all active lights (max 8)
  const char *shadowMapUniforms[] = {"shadowMap0", "shadowMap1", "shadowMap2",
                                     "shadowMap3", "shadowMap4", "shadowMap5",
                                     "shadowMap6", "shadowMap7"};

  for (int i = 0; i < active_shadow_lights && i < 8; i++) {
    // Bind shadow cube map texture
    glActiveTexture(GL_TEXTURE4 + i);
    glBindTexture(GL_TEXTURE_CUBE_MAP, lights[i].shadowCubeMap);
    glUniform1i(glGetUniformLocation(lighting_program, shadowMapUniforms[i]),
                4 + i);
  }

  // Send far_plane uniform for cube map shadow calculations
  glUniform1f(glGetUniformLocation(lighting_program, "far_plane"), 25.0f);

  // Send shadow mode toggle
  glUniform1i(glGetUniformLocation(lighting_program, "flashlightOnlyShadows"),
              flashlight_only_shadows);

  // Send lights to shader
  glUniform1i(glGetUniformLocation(lighting_program, "numLights"), num_lights);
  for (int i = 0; i < num_lights; i++) {
    char uniform_name[64];
    sprintf(uniform_name, "lights[%d].Position", i);
    glUniform3f(glGetUniformLocation(lighting_program, uniform_name),
                lights[i].position.x, lights[i].position.y,
                lights[i].position.z);
    sprintf(uniform_name, "lights[%d].Color", i);
    glUniform3f(glGetUniformLocation(lighting_program, uniform_name),
                lights[i].color.x, lights[i].color.y, lights[i].color.z);
    sprintf(uniform_name, "lights[%d].Radius", i);
    glUniform1f(glGetUniformLocation(lighting_program, uniform_name),
                lights[i].radius);
  }

  glUniform3f(glGetUniformLocation(lighting_program, "viewPos"),
              camera.position.x, camera.position.y, camera.position.z);

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
  rafgl_raster_t raster;
  if (rafgl_raster_load_from_image(&raster, diffuse_path) == 0) {
    glGenTextures(1, &mat->diffuse.tex_id);
    rafgl_texture_load_from_raster(&mat->diffuse, &raster);
    printf("Successfully loaded diffuse texture: %s\n", diffuse_path);
  } else {
    printf("Failed to load diffuse texture: %s\n", diffuse_path);
  }
}

void material_load_normal(Material *mat, const char *normal_path) {
  rafgl_raster_t raster;
  if (rafgl_raster_load_from_image(&raster, normal_path) == 0) {
    glGenTextures(1, &mat->normal.tex_id);
    rafgl_texture_load_from_raster(&mat->normal, &raster);
    mat->has_normal_map = 1;
    printf("Successfully loaded normal texture: %s\n", normal_path);
  } else {
    printf("Failed to load normal texture: %s\n", normal_path);
    mat->has_normal_map = 0;
  }
}

void material_load_specular(Material *mat, const char *specular_path) {
  rafgl_raster_t raster;
  if (rafgl_raster_load_from_image(&raster, specular_path) == 0) {
    glGenTextures(1, &mat->specular.tex_id);
    rafgl_texture_load_from_raster(&mat->specular, &raster);
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
  glUniform1f(glGetUniformLocation(shader_program, "material.roughness"),
              mat->roughness);
  glUniform1f(glGetUniformLocation(shader_program, "material.metallic"),
              mat->metallic);
  glUniform1f(glGetUniformLocation(shader_program, "hasTexture"), 1.0f);
}

void texture_manager_init(TextureManager *tm) {
  // Initialize all materials - each object gets its own dedicated material
  material_init(&tm->wooden_barrel);
  material_init(&tm->round_table);
  material_init(&tm->wooden_bench);
  material_init(&tm->wall_candle);
  material_init(&tm->beer_mug);
  material_init(&tm->green_bottle);
  material_init(&tm->food_plate);
  material_init(&tm->wooden_stool);
  material_init(&tm->floor_material);

  // Load EACH object's OWN specific textures

  // 1. Wooden barrel - uses SHADED texture with metal bands, wood, etc.
  material_load_diffuse(&tm->wooden_barrel,
                        "res/textures/wooden_barrel_shaded.png");
  material_load_normal(&tm->wooden_barrel,
                       "res/textures/wooden_barrel_normal.png");
  tm->wooden_barrel.roughness = 0.8f;
  tm->wooden_barrel.metallic = 0.0f;

  // 2. Round table - uses SHADED texture with full detail
  material_load_diffuse(&tm->round_table,
                        "res/textures/round_table_shaded.png");
  material_load_normal(&tm->round_table, "res/textures/round_table_normal.png");
  tm->round_table.roughness = 0.6f;
  tm->round_table.metallic = 0.0f;

  // 3. Wooden bench - uses SHADED texture with panels and details
  material_load_diffuse(&tm->wooden_bench,
                        "res/textures/wooden_bench_shaded.png");
  material_load_normal(&tm->wooden_bench,
                       "res/textures/wooden_bench_normal.png");
  tm->wooden_bench.roughness = 0.7f;
  tm->wooden_bench.metallic = 0.0f;

  // 4. Wall candle - uses SHADED texture with wax, holder, flame colors
  material_load_diffuse(&tm->wall_candle,
                        "res/textures/wall_candle_shaded.png");
  material_load_normal(&tm->wall_candle, "res/textures/wall_candle_normal.png");
  tm->wall_candle.roughness = 0.9f;
  tm->wall_candle.metallic = 0.0f;

  // 5. Beer mug - uses its own beer mug textures (already working!)
  material_load_diffuse(&tm->beer_mug, "res/textures/beer_mug_diffuse.png");
  material_load_normal(&tm->beer_mug, "res/textures/beer_mug_normal.png");
  tm->beer_mug.roughness = 0.8f;
  tm->beer_mug.metallic = 0.0f;

  // 6. Green bottle - uses SHADED texture with glass and cork
  material_load_diffuse(&tm->green_bottle,
                        "res/textures/green_bottle_shaded.png");
  material_load_normal(&tm->green_bottle,
                       "res/textures/green_bottle_normal.png");
  tm->green_bottle.roughness = 0.1f;
  tm->green_bottle.metallic = 0.0f;

  // 7. Food plate - uses SHADED texture with food and plate colors
  material_load_diffuse(&tm->food_plate, "res/textures/food_plate_shaded.png");
  material_load_normal(&tm->food_plate, "res/textures/food_plate_normal.png");
  tm->food_plate.roughness = 0.2f;
  tm->food_plate.metallic = 0.0f;

  // 8. Wooden stool - uses SHADED texture with full wood detail
  material_load_diffuse(&tm->wooden_stool,
                        "res/textures/wooden_stool_shaded.png");
  material_load_normal(&tm->wooden_stool,
                       "res/textures/wooden_stool_normal.png");
  tm->wooden_stool.roughness = 0.6f;
  tm->wooden_stool.metallic = 0.0f;
}

void texture_manager_cleanup(TextureManager *tm) {
  rafgl_texture_cleanup(&tm->wooden_barrel.diffuse);
  rafgl_texture_cleanup(&tm->wooden_barrel.normal);
  rafgl_texture_cleanup(&tm->round_table.diffuse);
  rafgl_texture_cleanup(&tm->round_table.normal);
  rafgl_texture_cleanup(&tm->wooden_bench.diffuse);
  rafgl_texture_cleanup(&tm->wooden_bench.normal);
  rafgl_texture_cleanup(&tm->wall_candle.diffuse);
  rafgl_texture_cleanup(&tm->wall_candle.normal);
  rafgl_texture_cleanup(&tm->beer_mug.diffuse);
  rafgl_texture_cleanup(&tm->beer_mug.normal);
  rafgl_texture_cleanup(&tm->green_bottle.diffuse);
  rafgl_texture_cleanup(&tm->green_bottle.normal);
  rafgl_texture_cleanup(&tm->food_plate.diffuse);
  rafgl_texture_cleanup(&tm->food_plate.normal);
  rafgl_texture_cleanup(&tm->wooden_stool.diffuse);
  rafgl_texture_cleanup(&tm->wooden_stool.normal);
  rafgl_texture_cleanup(&tm->floor_material.diffuse);
  rafgl_texture_cleanup(&tm->floor_material.normal);
}

// Simplified approach using existing RAFGL functions creatively
void create_detailed_barrel_mesh(rafgl_meshPUN_t *mesh, float radius,
                                 float height) {
  // Use a subdivided plane wrapped around to approximate a barrel
  // For now, just use a cube as a placeholder - we'll enhance visuals with
  // multiple objects
  rafgl_meshPUN_load_cube(mesh, radius);
}

void create_stone_corbel_mesh(rafgl_meshPUN_t *mesh) {
  // Create a stepped corbel using a cube base - we'll add detail with multiple
  // renders
  rafgl_meshPUN_load_cube(mesh, 0.5f);
}
