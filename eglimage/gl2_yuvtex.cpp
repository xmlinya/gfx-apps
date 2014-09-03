/* gl2_yuv.cpp
 * This is an example/sample test application to show simple use case of eglimage extension
 * This is being reused from RobClark's eglimage work.
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MAX_DISPLAYS 	(4)
#define MAX_TILES	(4)
uint8_t DISP_ID = 0;
uint8_t all_display = 0;
int8_t connector_id = -1;

int VID_WIDTH=1024;
int VID_HEIGHT=1024;
int VID_SIZE;

static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
	GLuint vertex_shader, fragment_shader;
} gl;

static struct {
	struct gbm_device *dev;
	struct gbm_surface *surface;
} gbm;

static struct {
	int fd;
	uint32_t ndisp;
	uint32_t crtc_id[MAX_DISPLAYS];
	uint32_t connector_id[MAX_DISPLAYS];
	uint32_t resource_id;
	uint32_t encoder[MAX_DISPLAYS];
	drmModeModeInfo *mode[MAX_DISPLAYS];
	drmModeConnector *connectors[MAX_DISPLAYS];
} drm;

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

static int init_drm(void)
{
	static const char *modules[] = {
			"omapdrm", "i915", "radeon", "nouveau", "vmwgfx", "exynos"
	};
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, j;
	uint32_t maxRes, curRes;

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

	resources = drmModeGetResources(drm.fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}
	drm.resource_id = (uint32_t) resources;

	/* find a connected connector: */
	for (int i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* choose the first supported mode */
			drm.mode[drm.ndisp] = &connector->modes[0];
			drm.connector_id[drm.ndisp] = connector->connector_id;

			for (j=0; j<resources->count_encoders; j++) {
				encoder = drmModeGetEncoder(drm.fd, resources->encoders[j]);
				if (encoder->encoder_id == connector->encoder_id)
					break;

				drmModeFreeEncoder(encoder);
				encoder = NULL;
			}

			if (!encoder) {
				printf("no encoder!\n");
				return -1;
			}

			drm.encoder[drm.ndisp]  = (uint32_t) encoder;
			drm.crtc_id[drm.ndisp] = encoder->crtc_id;
			drm.connectors[drm.ndisp] = connector;

			printf("### Display [%d]: CRTC = %d, Connector = %d\n", drm.ndisp, drm.crtc_id[drm.ndisp], drm.connector_id[drm.ndisp]);
			printf("\tMode chosen [%s] : Clock => %d, Vertical refresh => %d, Type => %d\n", drm.mode[drm.ndisp]->name, drm.mode[drm.ndisp]->clock, drm.mode[drm.ndisp]->vrefresh, drm.mode[drm.ndisp]->type);
			printf("\tHorizontal => %d, %d, %d, %d, %d\n", drm.mode[drm.ndisp]->hdisplay, drm.mode[drm.ndisp]->hsync_start, drm.mode[drm.ndisp]->hsync_end, drm.mode[drm.ndisp]->htotal, drm.mode[drm.ndisp]->hskew);
			printf("\tVertical => %d, %d, %d, %d, %d\n", drm.mode[drm.ndisp]->vdisplay, drm.mode[drm.ndisp]->vsync_start, drm.mode[drm.ndisp]->vsync_end, drm.mode[drm.ndisp]->vtotal, drm.mode[drm.ndisp]->vscan);

			/* If a connector_id is specified, use the corresponding display */
			if ((connector_id != -1) && (connector_id == drm.connector_id[drm.ndisp]))
				DISP_ID = drm.ndisp;

			/* If all displays are enabled, choose the connector with maximum
			* resolution as the primary display */
			if (all_display) {
				maxRes = drm.mode[DISP_ID]->vdisplay * drm.mode[DISP_ID]->hdisplay;
				curRes = drm.mode[drm.ndisp]->vdisplay * drm.mode[drm.ndisp]->hdisplay;

				if (curRes > maxRes)
					DISP_ID = drm.ndisp;
			}

			drm.ndisp++;
		} else {
			drmModeFreeConnector(connector);
		}
	}

	if (drm.ndisp == 0) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
		return -1;
	}

	return 0;
}

static int init_gbm(void)
{
	gbm.dev = gbm_create_device(drm.fd);

	gbm.surface = gbm_surface_create(gbm.dev,
			drm.mode[DISP_ID]->hdisplay, drm.mode[DISP_ID]->vdisplay,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		return -1;
	}

	return 0;
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = (struct drm_fb *)data;

	if (fb->fb_id)
		drmModeRmFB(drm.fd, fb->fb_id);

	free(fb);
}

static struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
{
	struct drm_fb *fb = (struct drm_fb *)gbm_bo_get_user_data(bo);
	uint32_t width, height, stride, handle;
	int ret;

	if (fb)
		return fb;

	fb = (struct drm_fb *)calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;

	ret = drmModeAddFB(drm.fd, width, height, 24, 32, stride, handle, &fb->fb_id);
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	int *waiting_for_flip = (int *)data;
	*waiting_for_flip = 0;
}

static void printGLString(const char *name, GLenum s) {
	// fprintf(stderr, "printGLString %s, %d\n", name, s);
	const char *v = (const char *) glGetString(s);
	// int error = glGetError();
	// fprintf(stderr, "glGetError() = %d, result of glGetString = %x\n", error,
	//        (unsigned int) v);
	// if ((v < (const char*) 0) || (v > (const char*) 0x10000))
	//    fprintf(stderr, "GL %s = %s\n", name, v);
	// else
	//    fprintf(stderr, "GL %s = (null) 0x%08x\n", name, (unsigned int) v);
	fprintf(stderr, "GL %s = %s\n", name, v);
}

static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
	if (returnVal != EGL_TRUE) {
		fprintf(stderr, "%s() returned %d\n", op, returnVal);
	}

	for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
		fprintf(stderr, "after %s() eglError 0x%x\n", op, error);
	}
}

static void checkGlError(const char* op) {
	for (GLint error = glGetError(); error; error = glGetError()) {
		fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
	}
}

static const char gVertexShader[] =
        "attribute vec4 vPosition;\n"
	"attribute vec2 texCoords;\n"
		"varying vec2 yuvTexCoords;\n"
		"void main() {\n"
		"  yuvTexCoords = texCoords;\n"
		"  gl_Position = vPosition;\n"
		"}\n";

static const char gFragmentShader[] =
        "#extension GL_OES_EGL_image_external : require\n"
		"precision mediump float;\n"
		"uniform samplerExternalOES yuvTexSampler;\n"
		"varying vec2 yuvTexCoords;\n"
		"void main() {\n"
		"  gl_FragColor = texture2D(yuvTexSampler, yuvTexCoords);\n"
		"}\n";

GLuint loadShader(GLenum shaderType, const char* pSource) {
	GLuint shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &pSource, NULL);
		glCompileShader(shader);
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen) {
				char* buf = (char*) malloc(infoLen);
				if (buf) {
					glGetShaderInfoLog(shader, infoLen, NULL, buf);
					fprintf(stderr, "Could not compile shader %d:\n%s\n",
							shaderType, buf);
					free(buf);
				}
			} else {
				fprintf(stderr, "Guessing at GL_INFO_LOG_LENGTH size\n");
				char* buf = (char*) malloc(0x1000);
				if (buf) {
					glGetShaderInfoLog(shader, 0x1000, NULL, buf);
					fprintf(stderr, "Could not compile shader %d:\n%s\n",
							shaderType, buf);
					free(buf);
				}
			}
			glDeleteShader(shader);
			shader = 0;
		}
	}
	return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
	if (!vertexShader) {
		return 0;
	}

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
	if (!pixelShader) {
		return 0;
	}

	GLuint program = glCreateProgram();
	if (program) {
		glAttachShader(program, vertexShader);
		checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		checkGlError("glAttachShader");
		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE) {
			GLint bufLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
			if (bufLength) {
				char* buf = (char*) malloc(bufLength);
				if (buf) {
					glGetProgramInfoLog(program, bufLength, NULL, buf);
					fprintf(stderr, "Could not link program:\n%s\n", buf);
					free(buf);
				}
			}
			glDeleteProgram(program);
			program = 0;
		}
	}
	return program;
}

GLuint gProgram;
GLint gvPositionHandle;
GLint gvTexHandle;
GLint gYuvTexSamplerHandle;

bool setupGraphics(int w, int h) {
	gProgram = createProgram(gVertexShader, gFragmentShader);
	if (!gProgram) {
		return false;
	}
	gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
	checkGlError("glGetAttribLocation");
	fprintf(stderr, "glGetAttribLocation(\"vPosition\") = %d\n",
			gvPositionHandle);

	gvTexHandle = glGetAttribLocation(gProgram, "texCoords");
	checkGlError("glGetAttribLocation");
	fprintf(stderr, "glGetAttribLocation(\"texCoords\") = %d\n",
			gvTexHandle);

	gYuvTexSamplerHandle = glGetUniformLocation(gProgram, "yuvTexSampler");
	checkGlError("glGetUniformLocation");
	fprintf(stderr, "glGetUniformLocation(\"yuvTexSampler\") = %d\n",
			gYuvTexSamplerHandle);

	glViewport(0, 0, w, h);
	checkGlError("glViewport");
	return true;
}

int align(int x, int a) {
	return (x + (a-1)) & (~(a-1));
}

static GLuint yuvTex[MAX_TILES];
static EGLImageKHR img[MAX_TILES];

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])

#define WIN_WIDTH  500
#define WIN_HEIGHT 500
#define PAGE_SIZE  4096

#ifndef EGL_TI_raw_video
#  define EGL_TI_raw_video 1
#  define EGL_RAW_VIDEO_TI						0x333A	/* eglCreateImageKHR target */
#  define EGL_GL_VIDEO_FOURCC_TI				0x3331	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_WIDTH_TI					0x3332	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_HEIGHT_TI				0x3333	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_BYTE_STRIDE_TI			0x3334	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_BYTE_SIZE_TI				0x3335	/* eglCreateImageKHR attribute */
#  define EGL_GL_VIDEO_YUV_FLAGS_TI				0x3336	/* eglCreateImageKHR attribute */
#endif

#ifndef EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE
#  define EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE (0 << 0)
#  define EGLIMAGE_FLAGS_YUV_FULL_RANGE       (1 << 0)
#  define EGLIMAGE_FLAGS_YUV_BT601            (0 << 1)
#  define EGLIMAGE_FLAGS_YUV_BT709            (1 << 1)
#endif

extern "C" {
/* swap these for big endian.. */
#define RED   2
#define GREEN 1
#define BLUE  0

static void fill420(unsigned char *y, unsigned char *u, unsigned char *v,
		int cs /*chroma pixel stride */,
		int n, int width, int height, int stride, unsigned int rgbvalue)
{
	int i, j;

	/* paint the buffer with colored tiles, in blocks of 2x2 */
	for (j = 0; j < height; j+=2) {
		unsigned char *y1p = y + j * stride;
		unsigned char *y2p = y1p + stride;
		unsigned char *up = u + (j/2) * stride * cs / 2;
		unsigned char *vp = v + (j/2) * stride * cs / 2;

		for (i = 0; i < width; i+=2) {
			div_t d = div(n+i+j, width);
			uint32_t rgb = 0x00130502 * (d.quot >> 6) + 0x000a1120 * (d.rem >> 6);

			if (rgbvalue != 0)
				rgb = rgbvalue;

			unsigned char *rgbp = (unsigned char *)&rgb;
			unsigned char y = (0.299 * rgbp[RED]) + (0.587 * rgbp[GREEN]) + (0.114 * rgbp[BLUE]);

			*(y2p++) = *(y1p++) = y;
			*(y2p++) = *(y1p++) = y;

			*up = (rgbp[BLUE] - y) * 0.565 + 128;
			*vp = (rgbp[RED] - y) * 0.713 + 128;
			up += cs;
			vp += cs;
		}
	}
}
}

//#define USE_TILER

#ifdef USE_TILER
extern "C" {
#  include <tiler.h>
#  include <tilermem.h>
#  include <memmgr.h>
}
#endif

void setupYuvBuffer(unsigned char *buf, char *file, unsigned int rgbvalue)
{
    unsigned char *y = buf;
    unsigned char *u = y + (VID_HEIGHT * VID_WIDTH);
    unsigned char *v = u + 1;

    if (!file) {
	fill420(y, u, v, 2, 0, VID_WIDTH, VID_HEIGHT, VID_WIDTH, rgbvalue);
    } else {
	    FILE *fp = fopen(file, "rb");
	    int n = 0;

	    if (!fp) {
		    printf("cannot open file : %s\n", file);
		    exit(1);
	    }
	    n = fread(buf, VID_SIZE, 1, fp);
	    fclose(fp);

	    printf("Read %d bytes from file %s\n", n, file);
    }
}


bool setupYuvTexSurface(EGLDisplay dpy, EGLContext context, unsigned char *ptr, int index) {
    EGLint attr[] = {
            EGL_GL_VIDEO_FOURCC_TI,      FOURCC_STR("NV12"),
            EGL_GL_VIDEO_WIDTH_TI,       VID_WIDTH,
            EGL_GL_VIDEO_HEIGHT_TI,      VID_HEIGHT,
            EGL_GL_VIDEO_BYTE_STRIDE_TI, VID_WIDTH,
            EGL_GL_VIDEO_BYTE_SIZE_TI,   VID_SIZE,
            // TODO: pick proper YUV flags..
            EGL_GL_VIDEO_YUV_FLAGS_TI,   EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE |
            EGLIMAGE_FLAGS_YUV_BT601,
            EGL_NONE
    };

    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR =
            (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
            (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    img[index] = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_RAW_VIDEO_TI, ptr, attr);
    checkEglError("eglCreateImageKHR");
    if (img[index] == EGL_NO_IMAGE_KHR) {
        printf("eglCreateImageKHR failed\n");
        return false;
    }

    glGenTextures(1, &yuvTex[index]);
    checkGlError("glGenTextures");
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, yuvTex[index]);
    checkGlError("glBindTexture");
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    checkGlError("glTexParameteri");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img[index]);
    checkGlError("glEGLImageTargetTexture2DOES");

    return true;
}

const GLfloat gTriangleVertices_fullscreen[] = {
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f
};
const GLfloat gTriangleVertices_topleft[] = {
		-1.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f
};
const GLfloat gTriangleVertices_bottomleft[] = {
		-1.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 0.0f
};
const GLfloat gTriangleVertices_bottomright[] = {
		0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f
};
const GLfloat gTriangleVertices_topright[] = {
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f
};
const GLfloat gTexCoords[] = {
		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
};

void renderFrame(const GLfloat *vertices, const GLfloat *texcoords, const GLuint tex) {
	glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	checkGlError("glVertexAttribPointer");
	glEnableVertexAttribArray(gvPositionHandle);
	checkGlError("glEnableVertexAttribArray");

	glVertexAttribPointer(gvTexHandle, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
	checkGlError("glVertexAttribPointer");
	glEnableVertexAttribArray(gvTexHandle);
	checkGlError("glEnableVertexAttribArray");

	glUniform1i(gYuvTexSamplerHandle, 0);
	checkGlError("glUniform1i");
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
	checkGlError("glBindTexture");

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	checkGlError("glDrawArrays");
}

void printEGLConfiguration(EGLDisplay dpy, EGLConfig config) {

#define X(VAL) {VAL, #VAL}
	struct {
		EGLint attribute;
		const char* name;
	} names[] = {
			X(EGL_BUFFER_SIZE),
			X(EGL_ALPHA_SIZE),
			X(EGL_BLUE_SIZE),
			X(EGL_GREEN_SIZE),
			X(EGL_RED_SIZE),
			X(EGL_DEPTH_SIZE),
			X(EGL_STENCIL_SIZE),
			X(EGL_CONFIG_CAVEAT),
			X(EGL_CONFIG_ID),
			X(EGL_LEVEL),
			X(EGL_MAX_PBUFFER_HEIGHT),
			X(EGL_MAX_PBUFFER_PIXELS),
			X(EGL_MAX_PBUFFER_WIDTH),
			X(EGL_NATIVE_RENDERABLE),
			X(EGL_NATIVE_VISUAL_ID),
			X(EGL_NATIVE_VISUAL_TYPE),
			X(EGL_SAMPLES),
			X(EGL_SAMPLE_BUFFERS),
			X(EGL_SURFACE_TYPE),
			X(EGL_TRANSPARENT_TYPE),
			X(EGL_TRANSPARENT_RED_VALUE),
			X(EGL_TRANSPARENT_GREEN_VALUE),
			X(EGL_TRANSPARENT_BLUE_VALUE),
			X(EGL_BIND_TO_TEXTURE_RGB),
			X(EGL_BIND_TO_TEXTURE_RGBA),
			X(EGL_MIN_SWAP_INTERVAL),
			X(EGL_MAX_SWAP_INTERVAL),
			X(EGL_LUMINANCE_SIZE),
			X(EGL_ALPHA_MASK_SIZE),
			X(EGL_COLOR_BUFFER_TYPE),
			X(EGL_RENDERABLE_TYPE),
			X(EGL_CONFORMANT),
	};
#undef X

	for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
		EGLint value = -1;
		EGLint returnVal = eglGetConfigAttrib(dpy, config, names[j].attribute, &value);
		EGLint error = eglGetError();
		if (returnVal && error == EGL_SUCCESS) {
			printf(" %s: ", names[j].name);
			printf("%d (0x%x)", value, value);
		}
	}
	printf("\n");
}

#define NUM_BUFS (3)
char *file = NULL;

int main(int argc, char** argv) {
	static const EGLint attribs[] = {
			EGL_RED_SIZE, 1,
			EGL_GREEN_SIZE, 1,
			EGL_BLUE_SIZE, 1,
			EGL_ALPHA_SIZE, 0,
			EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_NONE
	};
	EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLConfig config;
	EGLint num_configs;
	EGLint majorVersion;
	EGLint minorVersion;
	EGLContext context;
	EGLSurface surface;
	EGLint w, h;

	EGLDisplay edpy;

	fd_set fds;
	drmEventContext evctx;
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;

	struct gbm_bo *bo;
	struct drm_fb *fb;
	int ret;

	if (argc > 1) {
		file = argv[1];
		VID_WIDTH = atoi(argv[2]);
		VID_HEIGHT = atoi(argv[3]);
	}

	VID_SIZE = (VID_WIDTH * VID_HEIGHT * 3) / 2;
	
	unsigned char *buf[MAX_TILES];

	ret = init_drm();
	if (ret) {
		printf("failed to initialize DRM\n");
		return ret;
	}
	printf("### Primary display => ConnectorId = %d, Resolution = %dx%d\n",
			drm.connector_id[DISP_ID], drm.mode[DISP_ID]->hdisplay,
			drm.mode[DISP_ID]->vdisplay);

	FD_ZERO(&fds);
	FD_SET(drm.fd, &fds);

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	checkEglError("<init>");
	edpy = eglGetDisplay((EGLNativeDisplayType)gbm.dev);
	checkEglError("eglGetDisplay");
	if (edpy == EGL_NO_DISPLAY) {
		printf("eglGetDisplay returned EGL_NO_DISPLAY.\n");
		return 0;
	}

	ret = eglInitialize(edpy, &majorVersion, &minorVersion);
	checkEglError("eglInitialize", ret);
	fprintf(stderr, "EGL version %d.%d\n", majorVersion, minorVersion);
	if (ret != EGL_TRUE) {
		printf("eglInitialize failed\n");
		return 0;
	}

	if (!eglChooseConfig(edpy, attribs, &config, 1, &num_configs)) {
		printf("Error: couldn't get an EGL visual config\n");
		exit(1);
	}

    surface = eglCreateWindowSurface(edpy, config,
		    (EGLNativeWindowType)gbm.surface, NULL);
	checkEglError("eglCreateWindowSurface");
	if (surface == EGL_NO_SURFACE) {
		printf("gelCreateWindowSurface failed.\n");
		return 1;
	}

    context = eglCreateContext(edpy, config, EGL_NO_CONTEXT, context_attribs);
	checkEglError("eglCreateContext");
	if (context == EGL_NO_CONTEXT) {
		printf("eglCreateContext failed\n");
		return 1;
	}

	ret = eglMakeCurrent(edpy, surface, surface, context);
	checkEglError("eglMakeCurrent", ret);
	if (ret != EGL_TRUE) {
		printf("eglMakeCurrent failed\n");
		return 1;
	}

	eglQuerySurface(edpy, surface, EGL_WIDTH, &w);
	checkEglError("eglQuerySurface");
	eglQuerySurface(edpy, surface, EGL_HEIGHT, &h);
	checkEglError("eglQuerySurface");

	fprintf(stderr, "Window dimensions: %d x %d\n", w, h);

	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	unsigned int color = 0xff;
	for (int i=0; i<MAX_TILES; i++) {
		buf[i] = (unsigned char *)malloc(VID_SIZE + PAGE_SIZE);
		buf[i] = (unsigned char *)(((uint32_t)buf[i] + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

		if (i==0)
			setupYuvBuffer(buf[i], file, 0x0000FF);
		else {
			setupYuvBuffer(buf[i], NULL, color);
			color <<= 8;
		}

		if(!setupYuvTexSurface(edpy, context, buf[i], i)) {
			fprintf(stderr, "Could not set up texture surface.\n");
			return 1;
		}
	}

	if(!setupGraphics(w, h)) {
		fprintf(stderr, "Could not set up graphics.\n");
		return 1;
	}

	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(gl.display, gl.surface);
	bo = gbm_surface_lock_front_buffer(gbm.surface);
	fb = drm_fb_get_from_bo(bo);

	ret = drmModeSetCrtc(drm.fd, drm.crtc_id[DISP_ID], fb->fb_id,
			0, 0, &drm.connector_id[DISP_ID], 1, drm.mode[DISP_ID]);
	if (ret) {
		printf("display %d failed to set mode: %s\n", DISP_ID, strerror(errno));
		return ret;
	}

	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
	checkGlError("glClearColor");
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	checkGlError("glClear");

	glUseProgram(gProgram);
	checkGlError("glUseProgram");

	for (;;) {
		struct gbm_bo *next_bo;
		int waiting_for_flip = 1;

		renderFrame(gTriangleVertices_topleft, gTexCoords, yuvTex[0]);
		renderFrame(gTriangleVertices_bottomleft, gTexCoords, yuvTex[1]);
		renderFrame(gTriangleVertices_bottomright, gTexCoords, yuvTex[2]);
		renderFrame(gTriangleVertices_topright, gTexCoords, yuvTex[3]);

		eglSwapBuffers(edpy, surface);
		checkEglError("eglSwapBuffers");

		next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		fb = drm_fb_get_from_bo(next_bo);

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

		ret = drmModePageFlip(drm.fd, drm.crtc_id[DISP_ID], fb->fb_id,
				DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return -1;
		}

		while (waiting_for_flip) {
			ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return ret;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return -1;
			} else if (FD_ISSET(0, &fds)) {
				continue;
			}
			drmHandleEvent(drm.fd, &evctx);
		}

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
	}

	return 0;
}
