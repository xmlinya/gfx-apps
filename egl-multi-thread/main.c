#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "esUtil.h"
#include "render_thread.h"
#include "gl_kmscube.h"

#ifndef USE_WAYLAND
#include "drm_gbm.h"
#else
#include "wayland_window.h"
#endif

#ifndef USE_WAYLAND
int CONNECTOR_ID = (24);
#endif

#define FRAME_W (960)
#define FRAME_H (540)

#define MAX_NUM_THREADS (8)

int num_threads = 3;

#ifndef USE_WAYLAND
static uint32_t gettime_msec()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint32_t)(t.tv_sec * 1000) + (uint32_t)(t.tv_nsec / 1000000);
}

static uint32_t gettime_usec()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint32_t)(t.tv_sec * 1000000) + (uint32_t)(t.tv_nsec / 1000);
}

unsigned long long gettime_nsec()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return ((unsigned long long)t.tv_sec) * 1000000000 + t.tv_nsec;
}

unsigned long long __gettime()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return ((unsigned long long)t.tv_sec) * 1000000000 + t.tv_nsec;
}
#endif

#ifndef USE_WAYLAND
void print_usage(char *app)
{
	printf("./%s --connector <CONNECTOR ID>\n", app); 
	printf("You can get the CONNECTOR_ID by running modetest on the \n \
			target. For example our board shows the following:\n \
		Connectors: \n \
		id      encoder status          name            size (mm) modes encoders \n \
		26      25      connected       HDMI-A-1        160x90    40 25 \n \
		  modes: \n \
		          name refresh (Hz) hdisp hss hse htot vdisp vss vse vtot) \n \
		  1920x1080 60 1920 2008 2052 2200 1080 1084 1089 1125 flags: phsync, pvsync; type: preferred, driver \n \
		  1920x1080 60 1920 2008 2052 2200 1080 1084 1089 1125 flags: phsync, pvsync; type: driver \n \
		  \n \
		  On the above board, the CONNECTOR_ID will be set to 26.\n \
	\n");
}
#else
void print_usage(char *app)
{
	printf("./%s\n", app);
}
#endif

int main(int argc, char **argv)
{
	int ret;
	int count;
	
#ifndef USE_WAYLAND
	struct drm_data *dev;
#else
	struct wayland_data *dev;
#endif

	pthread_t threadid[MAX_NUM_THREADS];
	struct render_thread_param threadparams[MAX_NUM_THREADS];

#ifndef USE_WAYLAND
	float fps;
	int frame_num = 0;
	unsigned long long starttime = 0;
#endif

	for(count = 0; count < argc; count++) {
#ifndef USE_WAYLAND
		if(strcmp(argv[count], "--connector") == 0)
			if(count + 1 < argc)
				CONNECTOR_ID = atoi(argv[count+1]);
#endif

		if (strcmp(argv[count], "--help") == 0)
			print_usage(argv[0]);
	}
	
	srand(time(0));

#ifndef USE_WAYLAND
	dev = init_drm_gbm(CONNECTOR_ID);
#else
	dev = init_wayland_display();
#endif
	if(!dev) {
		print_usage(argv[0]);
		return -1;
	}


	for(count = 0; count < num_threads; count++) {
#ifndef USE_WAYLAND
		struct plane_data *pdata = get_new_surface(dev, count * FRAME_W, count * FRAME_H, FRAME_W, FRAME_H);
#else
		struct wayland_window_data *pdata = get_new_surface(dev, count * FRAME_W, count * FRAME_H, FRAME_W, FRAME_H);
#endif
		if(!pdata) {
			break;
		}
#ifndef USE_WAYLAND
		threadparams[count].dev = pdata->gbm_dev;
		threadparams[count].surf = pdata->gbm_surf;
#else
		threadparams[count].dev = dev->display;
		threadparams[count].surf = pdata->surf;
#endif
		threadparams[count].frame_width = FRAME_W;
		threadparams[count].frame_height = FRAME_H;
		threadparams[count].render_priv_setup = setup_kmscube;
		threadparams[count].render_priv_render = render_kmscube;
	}

	printf("requested %d instances, rendering %d instances\n", num_threads, count);
	num_threads = count;
	

	/*
	 * Create the different GBM surfaces and start the draw threads
	 */

	for(count = 0; count < num_threads; count++) {

		int ret = setup_render_thread(&threadparams[count]);
		if(ret != 0) {
			printf("render_thread setup failed\n");
			return -1;
		}
	}

	for(count = 0; count < num_threads; count++) {
		start_render_thread(&threadparams[count]);
	}


#ifndef USE_WAYLAND
	starttime = __gettime();
#endif

	while(1) {

		update_all_surfaces(dev);

#ifndef USE_WAYLAND
		unsigned long long __time;
		frame_num++;
		if(frame_num == 60) {
			__time = __gettime();
			fps = (60.0 * 1000000000) / (__time - starttime);
			frame_num = 0;
			starttime = __time;
			printf("FPS = %f\n", fps);
		
		}
#endif

	}

}

