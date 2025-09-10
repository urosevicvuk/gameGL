#include <main_state.h>
#include <glad/glad.h>
#include <math.h>

#include <rafgl.h>

static int w, h;
static rafgl_raster_t fnaf_flashlight, doge;
static rafgl_texture_t texture;

void main_state_init(GLFWwindow *window, void *args, int width, int height)
{
    w = width;
    h = height;
    rafgl_raster_init(&fnaf_flashlight, w, h);
    rafgl_raster_load_from_image(&doge, "res/images/doge.png");
    rafgl_texture_init(&texture);

}

int c = 0;

void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args)
{

    fnaf_flashlight_update();

}

void main_state_render(GLFWwindow *window, void *args)
{
    rafgl_texture_load_from_raster(&texture, &doge);
    rafgl_texture_show(&texture, 0);

}

void main_state_cleanup(GLFWwindow *window, void *args)
{

}

void fnaf_flashlight_update(){
    int x, y;
    int cr = 100;
    int cx = game_data->mouse_pos_x, cy = game_data->mouse_pos_y;

    int grad;
    float dist;


    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            grad = 3*x;
            grad = grad%256;
            dist = rafgl_distance2D(cx,cy, x,y);
            if (dist < cr && game_data->is_lmb_down) {
                pixel_at_m(fnaf_flashlight, x, y).rgba = rafgl_RGB(grad, grad, grad);
            }
            else {
                pixel_at_m(fnaf_flashlight, x, y).rgba = rafgl_RGB(0, 0, 0);
            }
        }
    }
}
