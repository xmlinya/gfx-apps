#ifndef __RENDER_THREAD__
#define __RENDER_THREAD__

#include <EGL/egl.h>
#include <gbm/gbm.h>
#include <pthread.h>

struct render_thread_param {
	struct gbm_device *dev;
	struct gbm_surface *surf;
	EGLDisplay display;		
	EGLContext context;
	EGLSurface surface;


	unsigned int frame_width;
	unsigned int frame_height;

	void *render_priv_data;
	void *(*render_priv_setup) (struct render_thread_param *prm);
	int (*render_priv_render) (void *priv);

};

int setup_render_thread (struct render_thread_param *prm);

pthread_t start_render_thread (struct render_thread_param *prm);

#endif /*__RENDER_THREAD__*/
