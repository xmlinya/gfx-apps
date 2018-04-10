#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>

#include "render_thread.h"
#include "gl_kmscube.h"
#include "esUtil.h"

static const GLfloat vVertices[] = {
			// front
	-1.0f, -1.0f, +1.0f, // point blue
	+1.0f, -1.0f, +1.0f, // point magenta
	-1.0f, +1.0f, +1.0f, // point cyan
	+1.0f, +1.0f, +1.0f, // point white
	// back
	+1.0f, -1.0f, -1.0f, // point red
	-1.0f, -1.0f, -1.0f, // point black
	+1.0f, +1.0f, -1.0f, // point yellow
	-1.0f, +1.0f, -1.0f, // point green
	// right
	+1.0f, -1.0f, +1.0f, // point magenta
	+1.0f, -1.0f, -1.0f, // point red
	+1.0f, +1.0f, +1.0f, // point white
	+1.0f, +1.0f, -1.0f, // point yellow
	// left
	-1.0f, -1.0f, -1.0f, // point black
	-1.0f, -1.0f, +1.0f, // point blue
	-1.0f, +1.0f, -1.0f, // point green
	-1.0f, +1.0f, +1.0f, // point cyan
	// top
	-1.0f, +1.0f, +1.0f, // point cyan
	+1.0f, +1.0f, +1.0f, // point white
	-1.0f, +1.0f, -1.0f, // point green
	+1.0f, +1.0f, -1.0f, // point yellow
	// bottom
	-1.0f, -1.0f, -1.0f, // point black
	+1.0f, -1.0f, -1.0f, // point red
	-1.0f, -1.0f, +1.0f, // point blue
	+1.0f, -1.0f, +1.0f  // point magenta
};

static const GLfloat vColors[] = {
	// front
	0.0f,  0.0f,  1.0f, // blue
	1.0f,  0.0f,  1.0f, // magenta
	0.0f,  1.0f,  1.0f, // cyan
	1.0f,  1.0f,  1.0f, // white
	// back
	1.0f,  0.0f,  0.0f, // red
	0.0f,  0.0f,  0.0f, // black
	1.0f,  1.0f,  0.0f, // yellow
	0.0f,  1.0f,  0.0f, // green
	// right
	1.0f,  0.0f,  1.0f, // magenta
	1.0f,  0.0f,  0.0f, // red
	1.0f,  1.0f,  1.0f, // white
	1.0f,  1.0f,  0.0f, // yellow
	// left
	0.0f,  0.0f,  0.0f, // black
	0.0f,  0.0f,  1.0f, // blue
	0.0f,  1.0f,  0.0f, // green
	0.0f,  1.0f,  1.0f, // cyan
	// top
	0.0f,  1.0f,  1.0f, // cyan
	1.0f,  1.0f,  1.0f, // white
	0.0f,  1.0f,  0.0f, // green
	1.0f,  1.0f,  0.0f, // yellow
	// bottom
	0.0f,  0.0f,  0.0f, // black
	1.0f,  0.0f,  0.0f, // red
	0.0f,  0.0f,  1.0f, // blue
	1.0f,  0.0f,  1.0f  // magenta
};

static const GLfloat vNormals[] = {
	// front
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	// back
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	+0.0f, +0.0f, -1.0f, // backbard
	// right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	+1.0f, +0.0f, +0.0f, // right
	// left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	-1.0f, +0.0f, +0.0f, // left
	// top
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	+0.0f, +1.0f, +0.0f, // up
	// bottom
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f, // down
	+0.0f, -1.0f, +0.0f  // down
};

static const char *vertex_shader_source =
	"uniform mat4 modelviewMatrix;      \n"
	"uniform mat4 modelviewprojectionMatrix;\n"
	"uniform mat3 normalMatrix;         \n"
	"                                   \n"
	"attribute vec4 in_position;        \n"
	"attribute vec3 in_normal;          \n"
	"attribute vec4 in_color;           \n"
	"\n"
	"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
	"                                   \n"
	"varying vec4 vVaryingColor;        \n"
	"                                   \n"
	"void main()                        \n"
	"{                                  \n"
	"    gl_Position = modelviewprojectionMatrix * in_position;\n"
	"    vec3 vEyeNormal = normalMatrix * in_normal;\n"
	"    vec4 vPosition4 = modelviewMatrix * in_position;\n"
	"    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
	"    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
	"    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
	"    vVaryingColor = vec4(diff * in_color.rgb, 1.0);\n"
	"}                                  \n";

static const char *fragment_shader_source =
	"precision mediump float;           \n"
	"                                   \n"
	"varying vec4 vVaryingColor;        \n"
	"                                   \n"
	"void main()                        \n"
	"{                                  \n"
	"    gl_FragColor = vVaryingColor;  \n"
	"}                                  \n";

struct gl_kmscube_data {
	GLuint program;
	GLint modelviewmatrix;
	GLint modelviewprojectionmatrix;
	GLint normalmatrix;
	GLuint vbo;
	GLuint positionsoffset;
	GLuint colorsoffset;
	GLuint normalsoffset;
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint bgcolor;
	GLuint alpha;
	GLuint width;
	GLuint height;
};


/*
 * kmscube setup
 * NOTE:  Must be called after eglMakeCurrent returns successfully.
 *
 * NOTE: Caller can safely call eglMakeCurrent(disp, 
 * 			EGL_NO_SURFACE,
 *			EGL_NO_SURFACE,
 *			EGL_NO_CONTEXT);
 * after this function returns a valid pointer.
 *
 * NOTE: If this function fails, it returns NULL, but DOES NOT free the
 * resources allocated prior to the failure.
 */
void *setup_kmscube (struct render_thread_param *prm)
{

	int ret;

	struct gl_kmscube_data *priv = calloc(sizeof(struct gl_kmscube_data), 1);
	if(!priv) {
		printf("kmscube: could not allocate priv data\n");
		return NULL;
	}

	priv->vertex_shader = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(priv->vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(priv->vertex_shader);

	glGetShaderiv(priv->vertex_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("kmscube : vertex shader compilation failed!:\n");
		glGetShaderiv(priv->vertex_shader, GL_INFO_LOG_LENGTH, &ret);
		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(priv->vertex_shader, ret, NULL, log);
			printf("kmscube : %s", log);
		}

		return NULL;
	}

	priv->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(priv->fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(priv->fragment_shader);

	glGetShaderiv(priv->fragment_shader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("kmscube : fragment shader compilation failed!:\n");
		glGetShaderiv(priv->fragment_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(priv->fragment_shader, ret, NULL, log);
			printf("kmscube : %s", log);
		}

		return NULL;
	}

	priv->program = glCreateProgram();

	glAttachShader(priv->program, priv->vertex_shader);
	glAttachShader(priv->program, priv->fragment_shader);

	glBindAttribLocation(priv->program, 0, "in_position");
	glBindAttribLocation(priv->program, 1, "in_normal");
	glBindAttribLocation(priv->program, 2, "in_color");

	glLinkProgram(priv->program);

	glGetProgramiv(priv->program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("kmscube : program linking failed!:\n");
		glGetProgramiv(priv->program, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(priv->program, ret, NULL, log);
			printf("kmscube : %s", log);
		}

		return NULL;
	}

	glUseProgram(priv->program);

	priv->modelviewmatrix = glGetUniformLocation(priv->program, "modelviewMatrix");
	priv->modelviewprojectionmatrix = glGetUniformLocation(priv->program, "modelviewprojectionMatrix");
	priv->normalmatrix = glGetUniformLocation(priv->program, "normalMatrix");


	priv->positionsoffset = 0;
	priv->colorsoffset = sizeof(vVertices);
	priv->normalsoffset = sizeof(vVertices) + sizeof(vColors);
	glGenBuffers(1, &priv->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, priv->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vColors) + sizeof(vNormals), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, priv->positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, priv->colorsoffset, sizeof(vColors), &vColors[0]);
	glBufferSubData(GL_ARRAY_BUFFER, priv->normalsoffset, sizeof(vNormals), &vNormals[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)priv->positionsoffset);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)priv->normalsoffset);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)priv->colorsoffset);
	glEnableVertexAttribArray(2);

	priv->bgcolor = rand();
	priv->alpha = (priv->bgcolor & 0xff000000) >> 24;
	priv->width = prm->frame_width;
	priv->height = prm->frame_height;

	return (void *)priv;
}

/*
 * kmscube render
 * NOTE:  Must be called after eglMakeCurrent returns successfully.
 * NOTE: Caller thread must be current
 *
 * NOTE: Caller can safely call eglMakeCurrent(disp, 
 * 			EGL_NO_SURFACE,
 *			EGL_NO_SURFACE,
 *			EGL_NO_CONTEXT);
 * after this function returns a valid pointer.
 */
int render_kmscube (void *priv)
{
	static int j = 0;
	int r, g, b;
	struct gl_kmscube_data *prm = priv;
	/* connect the context to the surface */

	r = (((prm->bgcolor & 0x00ff0000) >> 16) + j) % 512;
	g = (((prm->bgcolor & 0x0000ff00) >> 8) + j) % 512;
	b = ((prm->bgcolor & 0x000000ff) + j) % 512;

	if(r >= 256) 
		r = 511 - r;
	if(g >= 256) 
		g = 511 - g;
	if(b >= 256) 
		b = 511 - b;

	glViewport(0, 0, prm->width, prm->height);
	glEnable(GL_CULL_FACE);

	/*
	 * Different color every frame
	 */
	glClearColor(r/256.0, g/256.0, b/256.0, prm->alpha/256.0);
	glClear(GL_COLOR_BUFFER_BIT);

	ESMatrix modelview;

	/* clear the color buffer */

	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f + (0.25f * j), 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f - (0.5f * j), 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f + (0.15f * j), 0.0f, 0.0f, 1.0f);

	GLfloat aspect = (GLfloat)(prm->height) / (GLfloat)(prm->width);

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	float normal[9];
	normal[0] = modelview.m[0][0];
	normal[1] = modelview.m[0][1];
	normal[2] = modelview.m[0][2];
	normal[3] = modelview.m[1][0];
	normal[4] = modelview.m[1][1];
	normal[5] = modelview.m[1][2];
	normal[6] = modelview.m[2][0];
	normal[7] = modelview.m[2][1];
	normal[8] = modelview.m[2][2];

	glUniformMatrix4fv(prm->modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
	glUniformMatrix4fv(prm->modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	glUniformMatrix3fv(prm->normalmatrix, 1, GL_FALSE, normal);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);

	glDisable(GL_CULL_FACE);

	j++;

	return 0;
}
