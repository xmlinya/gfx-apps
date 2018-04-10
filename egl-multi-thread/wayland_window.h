#ifndef __WAYLAND_WINDOW_H__
#define __WAYLAND_WINDOW_H__

#include <wayland-client.h>
#include <wayland-egl.h>

struct wayland_data {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
};

struct wayland_window_data {
	struct wl_egl_window *surf;
};

struct wayland_data *init_wayland_display (void);
struct wayland_window_data *get_new_surface(struct wayland_data *wayland, int posx, int posy, int width, int height);
int update_all_surfaces(struct wayland_data *drm);

#endif /*__WAYLAND_WINDOW_H__*/
