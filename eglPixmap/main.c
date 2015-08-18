/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2013 Anand Balagopalakrishnan <anandb@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gbm/gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <omap/omap_drm.h>
#include <math.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define HDISPLAYSIZE (640)
#define VDISPLAYSIZE (480)

int outfile;

static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	GLuint program;
	GLuint positionoffset, coloroffset;
	GLuint vertex_shader, fragment_shader;
	GLuint rotation_uniform;
} gl;

static struct {
	struct gbm_device *dev;
	struct gbm_bo *bo;
	void *vaddr;
} gbm;

static struct {
	int fd;
} drm;

static const char *vert_shader_text =
	"uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = rotation * pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static int init_drm(void)
{
	int i;

	static const char *modules[] = {
			"omapdrm", "i915", "radeon", "nouveau", "vmwgfx", "exynos"
	};

	for (i = 0; i < ARRAY_SIZE(modules); i++) {
		printf("trying to load module %s...", modules[i]);
		drm.fd = drmOpen(modules[i], NULL);
		if (drm.fd < 0) {
			printf("failed.\n");
		} else {
			printf("success.\n");
			break;
		}
	}

	if (drm.fd < 0) {
		printf("could not open drm device\n");
		return -1;
	}

	return 0;
}

static int init_gbm(void)
{
	union gbm_bo_handle handle;

	gbm.dev = gbm_create_device(drm.fd);

	gbm.bo = gbm_bo_create(gbm.dev,
			HDISPLAYSIZE, VDISPLAYSIZE,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm.bo) {
		printf("failed to create gbm bo\n");
		return -1;
	}

	handle = gbm_bo_get_handle(gbm.bo);

	/* This is a hacky way to mmap the GBM BO.		*/
	/* Ideally there should be GBM API to mmap the buffer	*/
	/* NOTE: The address obtained is virtual address.	*/

	struct drm_omap_gem_info req = {
			.handle = handle.s32,
	};
	int ret = drmCommandWriteRead(drm.fd, DRM_OMAP_GEM_INFO,
			&req, sizeof(req));
	if (ret) {
		return ret;
	}

	gbm.vaddr = mmap(0, req.size, PROT_READ | PROT_WRITE,
				MAP_SHARED, drm.fd, req.offset);
	if (gbm.vaddr == MAP_FAILED) {
		return (int)gbm.vaddr;
	}

	return 0;
}

static GLuint
create_shader(const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	if (shader == 0) {
		exit(1);
	}

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

static int init_gl(void)
{
	EGLint major, minor, n;
	GLint ret;
	GLint status;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_PIXMAP_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};


	gl.display = eglGetDisplay(gbm.dev);

	if (!eglInitialize(gl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}

	printf("Using display %p with EGL version %d.%d\n",
			gl.display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}

	gl.context = eglCreateContext(gl.display, gl.config,
			EGL_NO_CONTEXT, context_attribs);
	if (gl.context == NULL) {
		printf("failed to create context\n");
		return -1;
	}

	gl.surface = eglCreatePixmapSurface(gl.display, gl.config, gbm.bo, NULL);
	if (gl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return -1;
	}

	/* connect the context to the surface */
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl.context);

	gl.fragment_shader = create_shader(frag_shader_text, GL_FRAGMENT_SHADER);
	gl.vertex_shader = create_shader(vert_shader_text, GL_VERTEX_SHADER);

	gl.program = glCreateProgram();
	glAttachShader(gl.program, gl.fragment_shader);
	glAttachShader(gl.program, gl.vertex_shader);
	glLinkProgram(gl.program);

	glGetProgramiv(gl.program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(gl.program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(gl.program);

	gl.positionoffset = 0;
	gl.coloroffset = 1;

	glBindAttribLocation(gl.program, gl.positionoffset, "pos");
	glBindAttribLocation(gl.program, gl.coloroffset, "color");
	gl.rotation_uniform = glGetUniformLocation(gl.program, "rotation");
	glEnableVertexAttribArray(gl.positionoffset);
	glEnableVertexAttribArray(gl.coloroffset);

	glViewport(0, 0, HDISPLAYSIZE, VDISPLAYSIZE);

	return 0;
}

static void exit_gbm(void)
{
        gbm_bo_destroy(gbm.bo);
        gbm_device_destroy(gbm.dev);
        return;
}

static void exit_gl(void)
{
        glDeleteProgram(gl.program);
        glDeleteShader(gl.fragment_shader);
        glDeleteShader(gl.vertex_shader);
        eglDestroySurface(gl.display, gl.surface);
        eglDestroyContext(gl.display, gl.context);
        eglTerminate(gl.display);
        return;
}

static void exit_drm(void)
{

        drmClose(drm.fd);
        return;
}

void cleanup_kmscube(void)
{
	exit_gl();
	exit_gbm();
	exit_drm();
	printf("Cleanup of GL, GBM and DRM completed\n");
	return;
}

static void draw(uint32_t i)
{
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5},
		{  0.5, -0.5},
		{  0,    0.5}
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	};
	GLfloat angle;
	GLfloat rotation[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};

	angle = (i * 36)% 360 * M_PI / 180.0;

	rotation[0][0] =  cos(angle);
	rotation[0][2] =  sin(angle);
	rotation[2][0] = -sin(angle);
	rotation[2][2] =  cos(angle);

	glUniformMatrix4fv(gl.rotation_uniform, 1, GL_FALSE, (GLfloat *) rotation);
	
	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(gl.positionoffset, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(gl.coloroffset, 3, GL_FLOAT, GL_FALSE, 0, colors);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	eglWaitGL();

	//glDisableVertexAttribArray(gl.positionoffset);
	//glDisableVertexAttribArray(gl.coloroffset);

}

int frame_count = 0;

int kms_signalhandler(int signum)
{
	switch(signum) {
	case SIGINT:
        case SIGTERM:
                /* Allow the pending page flip requests to be completed before
                 * the teardown sequence */
                sleep(1);
                printf("Handling signal number = %d\n", signum);
		cleanup_kmscube();
		break;
	case SIGALRM:
                printf("frames rendered in 1 sec = %d\n", frame_count);
		frame_count = 0;
		alarm(1);
		return 0;
	default:
		printf("Unknown signal\n");
		break;
	}
	exit(1);
}

int main(int argc, char *argv[])
{
	int i = 0, ret;
	int write_count = 0;

	signal(SIGINT, kms_signalhandler);
	signal(SIGTERM, kms_signalhandler);
	signal(SIGALRM, kms_signalhandler);

	outfile = open("pixmap-output-file-argb", O_WRONLY|O_TRUNC|O_CREAT, 0666);
	if(outfile < 0) {
		printf("failed to open file for writing\n");
		return outfile;
	}

	ret = init_drm();
	if (ret) {
		printf("failed to initialize DRM\n");
		return ret;
	}

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	ret = init_gl();
	if (ret) {
		printf("failed to initialize EGL\n");
		return ret;
	}

	alarm(1);

	while (true) {

		draw(i++);
		
		/* Lets write the first 10 frames to a file */
		if(write_count < 10)
			write(outfile, gbm.vaddr, HDISPLAYSIZE * VDISPLAYSIZE * 4);

		write_count++;
		frame_count++;
	}

	cleanup_kmscube();
	printf("\n Exiting kmscube \n");

	return ret;
}
