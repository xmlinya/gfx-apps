#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <gbm/gbm.h>

#include "drm_gbm.h"

struct drm_data {
	int fd;
	int conn_id;
	int crtc_id;
	int width;
	int height;

	int count_planes;
	struct plane_data *pdata;
	int primary_planes;
	int nonprimary_planes;

	struct gbm_device *gbm_dev;
};

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
	int dmabuf_fd;
	int fd;
};

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;
	struct gbm_device *gbm = gbm_bo_get_device(bo);

	if (fb->fb_id)
		drmModeRmFB(fb->fd, fb->fb_id);

	free(fb);
}

static struct drm_fb * drm_fb_get_from_bo(int fd, struct gbm_bo *bo)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, stride, handle;
	int ret;

	if (fb)
		return fb;

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;
	fb->fd = fd;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;

	ret = drmModeAddFB(fd, width, height, 32, 32, stride, handle, &fb->fb_id);
	if (ret) {
		free(fb);
		return NULL;
	}

	fb->dmabuf_fd = gbm_bo_get_fd(bo);

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static void page_flip_handler(int fd, unsigned int frame,
			      unsigned int sec, unsigned int usec,
			      void *data)
{
	int *flips = data;
	*flips= *flips - 1;
}


struct drm_data *init_drm_gbm (int conn_id) {
	int ret;
	int count;
	struct drm_set_client_cap req;

	struct drm_data *drm = calloc(sizeof(struct drm_data), 1);
	if(!drm) {
		printf("drm data alloc failed\n");
		return NULL;
	}

	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	drm->fd = fd;
	if(fd < 0) {
		printf("drm open failed\n");
		return NULL;
	}

	ret = drmSetMaster(fd);	
	if(ret < 0) {
		printf("drm set master failed\n");
		return NULL;
	}

	req.capability = DRM_CLIENT_CAP_ATOMIC;
	req.value = 1;
	ret = ioctl(fd, DRM_IOCTL_SET_CLIENT_CAP, &req);
	if(ret < 0) {
		printf("drm set atomic cap failed\n");
		return NULL;
	}

	int conn_found = 0;
	drm->conn_id = conn_id;
	drmModeResPtr res = drmModeGetResources(fd);
	for(count = 0; count < res->count_connectors; count++) {
		if(res->connectors[count]== drm->conn_id) {
			drmModeConnectorPtr connector = drmModeGetConnector(fd, drm->conn_id);
			drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoder_id);
			drm->crtc_id = encoder->crtc_id;
			drmModeCrtcPtr crtc = drmModeGetCrtc(fd, encoder->crtc_id);
			drm->width = crtc->width;
			drm->height = crtc->height;
			conn_found = 1;
			break;
		}
	}
	if(!conn_found) {
		printf("drm connector %d not found\n", drm->conn_id);
		return NULL;
	}

	drm->gbm_dev = gbm_create_device(fd);

	drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
	drm->count_planes = planes->count_planes;
	drm->pdata = calloc(sizeof(struct plane_data), planes->count_planes);
	for(count = 0; count < planes->count_planes; count++) {
		drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, planes->planes[count], DRM_MODE_OBJECT_PLANE);
		int propc;
		for(propc = 0; propc < props->count_props; propc++) {
			drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[propc]);
			if(strcmp(prop->name, "type") == 0) {
				unsigned long long prop_value = props->prop_values[propc];
				if(prop_value) {
					drm->pdata[count].primary = 1;
					drm->primary_planes++;
				} else {
					drm->pdata[count].primary = 0;
					drm->nonprimary_planes++;
				}
			}
			if(strcmp(prop->name, "zorder") == 0) {
				if(drm->pdata[count].primary) {
					drm->pdata[count].zorder = drm->primary_planes -1 ;
				} else {
					drm->pdata[count].zorder = 4 - drm->nonprimary_planes;
				}
				drmModeObjectSetProperty(fd, planes->planes[count],
						DRM_MODE_OBJECT_PLANE,
						props->props[propc], drm->pdata[count].zorder);
			}
			if(strcmp(prop->name, "FB_ID") == 0)
				drm->pdata[count].fb_id_property = props->props[propc];
		}
		drm->pdata[count].plane = planes->planes[count];
		drm->pdata[count].occupied = 0;
		drm->pdata[count].gbm_dev = drm->gbm_dev;
		
	}

	return drm;

}

struct plane_data *get_new_surface(struct drm_data *drm, int posx, int posy, int width, int height)
{
	int count;

	if(posx < 0 || posx + width > drm->width || posy < 0 || posy + height > drm->height) {
		printf("surface dimensions exceed crtc dimensions\n");
		return NULL;
	}

	for(count = 0; count < drm->count_planes; count++) {
		if(drm->pdata[count].primary)
			continue;
		if(drm->pdata[count].occupied)
			continue;
		break;
	}

	if(count == drm->count_planes) {
		printf("no more planes\n");
		return NULL;
	}

	drm->pdata[count].gbm_surf = gbm_surface_create(drm->gbm_dev,
			width, height,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	drm->pdata[count].posx = posx;
	drm->pdata[count].posy = posy;
	drm->pdata[count].width = width;
	drm->pdata[count].height = height;
	drm->pdata[count].occupied = 1;

	return &drm->pdata[count];
}

int update_all_surfaces(struct drm_data *drm)
{
	int count;
	int ret;
	static int firsttime = 1;
	static int flip_pending = 0;

	if(firsttime) {
		firsttime = 0;
		for(count = 0; count < drm->count_planes; count++) {	
			struct plane_data *pdata = &drm->pdata[count];
			if(drm->pdata[count].occupied == 0)
				continue;
			struct gbm_surface *surf = drm->pdata[count].gbm_surf;
			struct gbm_bo *bo = gbm_surface_lock_front_buffer(surf);
			struct drm_fb *fb = drm_fb_get_from_bo(drm->fd, bo);

			ret = drmModeSetPlane(drm->fd,
					drm->pdata[count].plane,
					drm->crtc_id,
					fb->fb_id,
					0,
					drm->pdata[count].posx,
					drm->pdata[count].posy,
					drm->pdata[count].width,
					drm->pdata[count].height,
					0,
					0,
					drm->pdata[count].width << 16,
					drm->pdata[count].height << 16);
		}

	}

	drmModeAtomicReqPtr m_req = drmModeAtomicAlloc();

	for(count = 0; count < drm->count_planes; count++) {	
		struct plane_data *pdata = &drm->pdata[count];
		if(drm->pdata[count].occupied == 0)
			continue;
		struct gbm_surface *surf = drm->pdata[count].gbm_surf;
		struct gbm_bo *bo = gbm_surface_lock_front_buffer(surf);
		struct drm_fb *fb = drm_fb_get_from_bo(drm->fd, bo);

		drmModeAtomicAddProperty(m_req,
				drm->pdata[count].plane,
				drm->pdata[count].fb_id_property,
				fb->fb_id);

		drm->pdata[count].current_bo = bo;
	}


	flip_pending++;
	ret = drmModeAtomicCommit(drm->fd, m_req, DRM_MODE_ATOMIC_TEST_ONLY, 0);
	ret = drmModeAtomicCommit(drm->fd, m_req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, &flip_pending);

	drmModeAtomicFree(m_req);

	while (flip_pending) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(drm->fd, &fds);

		ret = select(drm->fd + 1, &fds, NULL, NULL, NULL);

		if(ret <= 0) {
			printf("failing %d\n", ret);
			return 0;
		}

		if (FD_ISSET(drm->fd, &fds)) {
			drmEventContext ev = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.vblank_handler = 0,
				.page_flip_handler = page_flip_handler,
			};

			drmHandleEvent(drm->fd, &ev);
		}
	}

	for(count = 0; count < drm->count_planes; count++) {	
		struct plane_data *pdata = &drm->pdata[count];
		if(drm->pdata[count].occupied == 0)
			continue;
		struct gbm_surface *surf = drm->pdata[count].gbm_surf;
		struct gbm_bo *bo = drm->pdata[count].current_bo;	
		gbm_surface_release_buffer(surf, bo);
		drm->pdata[count].current_bo = NULL;
		
	}

	return 0;

}
