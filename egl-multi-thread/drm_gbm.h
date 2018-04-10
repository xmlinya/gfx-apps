#ifndef __DRM_GBM_H__
#define __DRM_GBM_H__

#include <gbm/gbm.h>

struct plane_data {
	int plane;
	int fb_id_property;
	int zorder;
	int primary;

	struct gbm_device *gbm_dev;
	struct gbm_surface *gbm_surf;

	struct gbm_bo *current_bo;

	int width;
	int height;
	int posx;
	int posy;

	int occupied;
};

struct drm_data;

struct drm_data *init_drm_gbm (int conn_id);
struct plane_data *get_new_surface(struct drm_data *drm, int posx, int posy, int width, int height);
int update_all_surfaces(struct drm_data *drm);

#endif /*__DRM_GBM_H__*/
