#include <tavern_renderer.h>
#include <math.h>

void gbuffer_init(GBuffer *gb, int width, int height) {
    gb->width = width;
    gb->height = height;
    
    glGenFramebuffers(1, &gb->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gb->framebuffer);
    
    // Position texture
    glGenTextures(1, &gb->gPosition);
    glBindTexture(GL_TEXTURE_2D, gb->gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gb->gPosition, 0);
    
    // Normal texture
    glGenTextures(1, &gb->gNormal);
    glBindTexture(GL_TEXTURE_2D, gb->gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gb->gNormal, 0);
    
    // Albedo + Specular texture
    glGenTextures(1, &gb->gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gb->gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gb->gAlbedoSpec, 0);
    
    // Depth buffer
    glGenTextures(1, &gb->depthBuffer);
    glBindTexture(GL_TEXTURE_2D, gb->depthBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gb->depthBuffer, 0);
    
    GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gbuffer_bind_for_writing(GBuffer *gb) {
    glBindFramebuffer(GL_FRAMEBUFFER, gb->framebuffer);
    glViewport(0, 0, gb->width, gb->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void gbuffer_bind_for_reading(GBuffer *gb) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gb->gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gb->gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gb->gAlbedoSpec);
}

void camera_init(Camera *cam) {
    cam->position = vec3(0.0f, 1.6f, 5.0f);  // Player height
    cam->front = vec3(0.0f, 0.0f, -1.0f);
    cam->up = vec3(0.0f, 1.0f, 0.0f);
    cam->yaw = -90.0f;
    cam->pitch = 0.0f;
    cam->speed = 5.0f;
    cam->sensitivity = 0.1f;
}

void camera_update(Camera *cam, GLFWwindow *window, float deltaTime) {
    // Keyboard input
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cam->position = v3_add(cam->position, v3_muls(cam->front, cam->speed * deltaTime));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cam->position = v3_sub(cam->position, v3_muls(cam->front, cam->speed * deltaTime));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cam->position = v3_sub(cam->position, v3_muls(v3_norm(v3_cross(cam->front, cam->up)), cam->speed * deltaTime));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cam->position = v3_add(cam->position, v3_muls(v3_norm(v3_cross(cam->front, cam->up)), cam->speed * deltaTime));
    
    // Mouse look - simplified for now
    static double lastX = 400, lastY = 300;
    static int firstMouse = 1;
    
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = 0;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Y coordinates are flipped
    lastX = xpos;
    lastY = ypos;
    
    xoffset *= cam->sensitivity;
    yoffset *= cam->sensitivity;
    
    cam->yaw += xoffset;
    cam->pitch += yoffset;
    
    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;
    
    // Update front vector
    vec3_t direction;
    direction.x = cos(cam->yaw * M_PIf / 180.0f) * cos(cam->pitch * M_PIf / 180.0f);
    direction.y = sin(cam->pitch * M_PIf / 180.0f);
    direction.z = sin(cam->yaw * M_PIf / 180.0f) * cos(cam->pitch * M_PIf / 180.0f);
    cam->front = v3_norm(direction);
    
    cam->right = v3_norm(v3_cross(cam->front, vec3(0.0f, 1.0f, 0.0f)));
    cam->up = v3_norm(v3_cross(cam->right, cam->front));
}

mat4_t camera_get_view_matrix(Camera *cam) {
    return m4_look_at(cam->position, v3_add(cam->position, cam->front), cam->up);
}

void fullscreen_quad_init(FullscreenQuad *quad) {
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &quad->VAO);
    glGenBuffers(1, &quad->VBO);
    glBindVertexArray(quad->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, quad->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

void fullscreen_quad_render(FullscreenQuad *quad) {
    glBindVertexArray(quad->VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void setup_point_light_shadows(PointLight *light, int shadowWidth, int shadowHeight) {
    glGenFramebuffers(1, &light->shadowFBO);
    
    // Create a proper cube map texture for omnidirectional shadows
    glGenTextures(1, &light->shadowCubeMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, light->shadowCubeMap);
    
    // Create all 6 faces of the cube map
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, 
                     shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }
    
    // Set cube map parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, light->shadowCubeMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_cube_shadow_map(PointLight *light, GLuint shadowProgram, void (*render_scene_func)(GLuint program)) {
    // The 6 view directions for a cube map (from a point light's perspective)
    vec3_t directions[6] = {
        vec3( 1.0f,  0.0f,  0.0f), // +X
        vec3(-1.0f,  0.0f,  0.0f), // -X
        vec3( 0.0f,  1.0f,  0.0f), // +Y
        vec3( 0.0f, -1.0f,  0.0f), // -Y
        vec3( 0.0f,  0.0f,  1.0f), // +Z
        vec3( 0.0f,  0.0f, -1.0f)  // -Z
    };
    
    vec3_t ups[6] = {
        vec3(0.0f, -1.0f,  0.0f), // +X
        vec3(0.0f, -1.0f,  0.0f), // -X
        vec3(0.0f,  0.0f,  1.0f), // +Y
        vec3(0.0f,  0.0f, -1.0f), // -Y
        vec3(0.0f, -1.0f,  0.0f), // +Z
        vec3(0.0f, -1.0f,  0.0f)  // -Z
    };
    
    glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
    glViewport(0, 0, 512, 512);
    
    glUseProgram(shadowProgram);
    
    // Set up projection matrix for 90 degree FOV (cube faces)
    mat4_t lightProjection = m4_perspective(90.0f, 1.0f, 0.1f, 25.0f);
    glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "lightProjection"), 1, GL_FALSE, (float*)lightProjection.m);
    
    // Pass light position to shader for distance calculation
    glUniform3f(glGetUniformLocation(shadowProgram, "lightPos"), light->position.x, light->position.y, light->position.z);
    glUniform1f(glGetUniformLocation(shadowProgram, "far_plane"), 25.0f);
    
    // Render to each face of the cube map
    for (int face = 0; face < 6; ++face) {
        // Calculate view matrix for this face
        vec3_t target = v3_add(light->position, directions[face]);
        mat4_t lightView = m4_look_at(light->position, target, ups[face]);
        
        // Attach the specific face of the cube map to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, light->shadowCubeMap, 0);
        
        glClear(GL_DEPTH_BUFFER_BIT);
        
        // Send view matrix to shader
        glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "lightView"), 1, GL_FALSE, (float*)lightView.m);
        
        // Render the scene for this face
        render_scene_func(shadowProgram);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}