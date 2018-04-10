#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include "wayland_window.h"

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct wayland_data *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_registry_bind(registry, name,
					    &wl_shell_interface, 1);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};


struct wayland_window_data *get_new_surface(struct wayland_data *wayland, int posx, int posy, int width, int height)
{
	struct wayland_window_data *window = calloc(sizeof(struct wayland_window_data), 1);
	if(!window) {
		printf("wayland window data alloc failed\n");
		return NULL;
	}

	struct wl_surface *surface = wl_compositor_create_surface(wayland->compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(wayland->shell, surface);

	wl_shell_surface_set_toplevel(shell_surface);

	struct wl_egl_window *egl_window = wl_egl_window_create(surface,
				width, height);

	window->surf = egl_window;

	return window;
}


struct wayland_data *init_wayland_display () {
	struct wayland_data *wayland = calloc(sizeof(struct wayland_data), 1);
	if(!wayland) {
		printf("wayland data alloc failed\n");
		return NULL;
	}

	wayland->display = wl_display_connect(NULL);
	if(!wayland->display) {
		free(wayland);
		printf("wl_display_connect failed\n");		
		return NULL;
	}

	wayland->registry = wl_display_get_registry(wayland->display);
	wl_registry_add_listener(wayland->registry,
				 &registry_listener, wayland);
	wl_display_dispatch(wayland->display);

	while(1) {
		if(wayland->compositor && wayland->shell)
			break;
		wl_display_roundtrip(wayland->display);
	}

	return wayland;

}

int update_all_surfaces(struct wayland_data *wayland) {
}
