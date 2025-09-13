#include <glad/glad.h>
#include <main_state.h>
#include <math.h>

#include <rafgl.h>

static int w, h;

void main_state_init(GLFWwindow *window, void *args, int width, int height) {
  w = width;
  h = height;
}

void main_state_update(GLFWwindow *window, float delta_time,
                       rafgl_game_data_t *game_data, void *args) {}

void main_state_render(GLFWwindow *window, void *args) {}

void main_state_cleanup(GLFWwindow *window, void *args) {}
