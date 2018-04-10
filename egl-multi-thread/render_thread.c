#include <stdio.h>
#include <pthread.h>

#include <EGL/egl.h>

#include "render_thread.h"

int setup_render_thread (struct render_thread_param *prm)
{
	EGLConfig config;
	EGLint major, minor, n;
	int ret;

	EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_DEPTH_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	prm->display = eglGetDisplay((EGLNativeDisplayType)prm->dev);

	if (!eglInitialize(prm->display, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglChooseConfig(prm->display, config_attribs, &config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}

	prm->context = eglCreateContext(prm->display, config,
				EGL_NO_CONTEXT, context_attribs);
	if (prm->context == NULL) {
		printf("failed to create context\n");
		return -1;
	}

	prm->surface = eglCreateWindowSurface(prm->display, config, prm->surf, NULL);
	if (prm->surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return -1;
	}

	eglMakeCurrent(prm->display, prm->surface, prm->surface, prm->context);
	
	prm->render_priv_data = prm->render_priv_setup(prm);
	if(!prm->render_priv_data) {
		printf("failed to setup renderpriv\n");
		return -1;
	}

	eglMakeCurrent(prm->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	return 0;
}

static void *render_thread (void *arg)
{
	struct render_thread_param *prm = arg;

	eglMakeCurrent(prm->display, prm->surface, prm->surface, prm->context);

	while(1) {
		int ret = prm->render_priv_render(prm->render_priv_data);

		if(ret != 0)
			printf("renderpriv render returned %d\n", ret);

		eglSwapBuffers(prm->display, prm->surface);

	}
}

pthread_t start_render_thread (struct render_thread_param *prm)
{
	pthread_t threadid;

	int ret = pthread_create(&threadid, NULL, render_thread, prm); 

	return ret == 0 ? threadid : -1;
}

