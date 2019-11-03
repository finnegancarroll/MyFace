//Face.cpp: Render my face with phong shading

#include <glad.h>
#include <glfw3.h>
#include <stdio.h>
#include "GLXtras.h"
#include <vector>
#include <time.h>
#include <cmath>
#include "VecMat.h"
#include "Camera.h"

void Keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
void MouseScroll(GLFWwindow* window, double xoffset, double yoffset);
void MouseButton(GLFWwindow *w, int butn, int action, int mods);
void MouseMove(GLFWwindow *w, double x, double y);
void ErrorGFLW(int id, const char *reason);
void normalizeAll(vec3 n[], int size);
void initNorms(vec3 n[], int size);
void InitVertexBuffer();
void initBuffers();
void Display();
void update();
void Close();

GLuint vBuffer = 0;	//GPU vertex buffer ID, valid if > 0
GLuint program = 0; //GLSL program ID, valid if > 0

//Phong vertex + pixel shaders
const char *vertexPhongShader = "\
	#version 130														\n\
	in vec3 point;														\n\
	in vec3 normal;														\n\
	uniform mat4 modelview;												\n\
	uniform mat4 persp;													\n\
	out vec3 vPoint;													\n\
	out vec3 vNormal;													\n\
	void main() {														\n\
		gl_Position = persp * modelview * vec4(point, 1);				\n\
		vPoint = (modelview * vec4(normal, 1)).xyz;						\n\
		vNormal = (modelview * vec4(point, 1)).xyz;						\n\
	}\n";

//N - Normalized Normal at that point
//L - Vector from point to light source
//d - Angle between N and L, 1 means parallel, 0 means perpendicular
//R - Reflection of light source vector around normal, results in direction light is bouncing towards
//E - Direction to camera
//h - Angle between reflected light and direction towards the camera
//s - A shiny spot should darken exponentially quickly, so we take h to a power

//TOOK CLAMP() OFF INTENSITY SO THINGS CAN BE BRIGHTER THAN DEFAULT
//ADDED ABS() IN H CALCULATION SO BOTH SIDES OF FACE ARE SHINY 
const char *pixelPhongShader = "\
	#version 130								\n\
	in vec3 vPoint;								\n\
	in vec3 vNormal;							\n\
	out vec4 pColor;							\n\
	uniform vec3 lightPos = vec3(0,0,-1.f);		\n\
	uniform float a = .1f;						\n\
	void main()	{								\n\
		vec3 color = vec3(.35, .35, .35);		\n\
		vec3 N = normalize(vNormal);			\n\
		vec3 L = normalize(lightPos - vPoint);	\n\
        float d = abs(dot(N, L));				\n\
		vec3 R = reflect(L, N);					\n\
		vec3 E = normalize(vPoint);				\n\
		float h = max(0, abs(dot(R, E)));		\n\
		float s = pow(h, 100);					\n\
		float intensity = a+d+s;				\n\
		pColor = vec4(intensity * color, 1);	\n\
	}\n";

//General max value for each direction guidline
//Right half of face, so left mustr be 0
float l = 0, r = 8, b = -8, t = 8, n = -1.f, f = 1.f;

//Points in xyz
//Origin added so I can count index 1
float points[][3] = { {0,0,0},
					  {l, t, n-1.25f}, {1.75f, t, n - 1.25f}, {3.5f, t-1.f, n - 1.25f}, {6.f, t-2.5f, n - 1.25f}, {6.85f, t-5.3f, n - 1.45f},//5
					  {7.f, t- 7.45f, n - 1.45f}, {7.f, t-9.6f, n - 1.4f}, {6.f, t - 12.1f, n - 1.4f}, {5.f, t-14.1f,n - 1.25f}, {2.7f, t - 3.1f, n},//10
					  {}, {}, {l, t- 10.5f, n + .3f}, {1.3f, t -9.4f, n}, {2.1f, t- 7.1f, n},//15 
					  {}, {}, {}, {4.9f, t- 9.5f, n - .1f}, {5.f, t-7.4f, n -.15f},//20 
					  {6.f, t-5.3f, n - .85f}, {5.f, t-3.5f, n - .15f}, {4.6f, t- 5.3f, n - .12f}, {2.9f, t-5.f, n -.11f}, {l, t - 1.25f, n - .15f},//25
					  {3.f, t-2.3f, n - .15f}, {1.25f, t - 1.25f, n - .15f}, {l, t-4.4f, n}, {1.2f, t-3.f, n}, {1.2f,t-4.4f, n -.2f},//30
					  {l, t- 3.f, n}, {l, t- 5.5f, n}, {l, t-7.f, n + .5f}, {l, t- 8.f, n + 1.f}, {1.1f ,t- 5.4f, n -.13f},//35
					  {1.f, t -7.f, n}, {1.2f, t - 8.1f, n}, {2.5f, t - 10.5f, n}, {l, t-9.6f, n + 1.f}, {2.7f, t - 8.f, n-.1f},//40
					  {}, {l, t - 11.8f, n - .2f}, {1.9f, t - 11.9f, n}, {3.8f, t-12.f, n}, {},//45
					  {2.f, t- 14.9f, n - .5f}, {2, t - 13.2f, n}, {l, t - 13.7f, n + .3f}, {l, t - 14.9f, n - .6f}, {4.f, t - 13.2f, n},//50
					};

//4 points to each face (connect the dots) 
int faces[][3] = { {1, 25, 27}, {1, 27, 2},{2, 27, 26}, {2, 26, 3}, {3, 26, 22}, {3, 22, 4}, {25, 31, 29}, {25, 29 , 27},
				   {27, 29, 10}, {27, 10, 26}, {26, 10, 22}, {31, 28, 30}, {31, 30, 29}, {29, 30, 24}, {29, 24, 10},
				   {22, 23, 21}, {10, 24, 23}, {10, 23, 22}, {22, 21, 5}, {4, 22, 5}, {23, 20, 21}, {21, 20, 5}, {5, 20, 6}, {28, 32, 35},
				   {28, 35, 30}, {32, 33, 36}, {32, 36, 35}, {35, 36, 15}, {33, 34, 37}, {33, 37, 36}, {36, 37, 40},
				   {36, 40, 15}, {40, 19, 20}, {20, 19, 7}, {20, 7, 6}, {40, 38, 19}, {34, 39, 14}, {34, 14, 37},
				   {39, 13, 38}, {39, 38, 14}, {37, 14, 38}, {37, 38, 40}, {13, 42, 43}, {13, 43, 38}, {38, 43, 44},
				   {19, 44, 8}, {19, 8, 7}, {42, 48, 47}, {42, 47, 43}, {43, 47, 50}, {43, 50, 44}, {44, 50, 8},
				   {48, 49, 46}, {48, 46, 47}, {47, 46, 9}, {47, 9, 50}, {50, 9, 8}, {38, 44, 19} };

//Cumulative Normals for each point
vec3 norms[sizeof(points)/sizeof(float[3])];

//Global constants
const float POLYSIZE = .02f;
const float ZFAR = 1.0f, ZNEAR = 0.0f;
const float ROTSPEED = .3f;
const int VSPERFACE = 3;
//Near and far plane values (frustum values)
const float N = .001F, F = 500;

//Just some public vars hanging out

int halfWidth;
int vsize = sizeof(points), nQuads = sizeof(faces) / (sizeof(int));
int screenWidth, screenHeight;
float fieldOfView = 30;

Camera cam(0, 0, vec3(0, 0, 0), vec3(0, 0, -1), fieldOfView, N, F, true);

//GPU CODE

void Display() {
	//Pixel/Vertex shader stored in program in main
	glUseProgram(program);

	//Where/How I pick up my points/colors/normals
	VertexAttribPointer(program, "point", 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
	VertexAttribPointer(program, "normal", 3, GL_FLOAT, GL_FALSE, 0, (void *)sizeof(points));

	//Initialize depth buffer and color buffer
	initBuffers();

	//Calculations that will be done for each frame
	update();

	//Resizing and stretching to make my values look a bit better
	mat4 scale = Scale(POLYSIZE, POLYSIZE * 1.5f, POLYSIZE);
	
	//Use camera to retrieve final matrix
	SetUniform(program, "modelview", cam.modelview * scale);
	SetUniform(program, "persp", cam.persp);

	//Useful for resizing window
	glViewport(0, 0, screenWidth, screenHeight);

	//Draw the triangles
	glDrawElements(GL_TRIANGLES, sizeof(faces)/sizeof(int), GL_UNSIGNED_INT, &faces[0]);

	//Reset buffer
	glFlush();
}

void update() {
	//Recalculate half window
	halfWidth = (screenWidth) / 2;
	//Set field of view
	cam.SetFOV(fieldOfView);
	//Update cam dimensions each frame
	cam.Resize(halfWidth, screenHeight);
}

void InitVertexBuffer() {
	//Calculate vertice number and faces number
	int nvertices = sizeof(faces) / sizeof(int), nfaces = nvertices / VSPERFACE;
	//Initialize all normals to zero vector
	initNorms(norms, sizeof(points)/sizeof(float[VSPERFACE]));
	for (int i = 0; i < nfaces; i++) {
		int* f = faces[i];
		//Store the first three points of the current face 
		vec3 p1(points[f[0]][0], points[f[0]][1], points[f[0]][2]),
		   	p2(points[f[1]][0], points[f[1]][1], points[f[1]][2]),
			p3(points[f[2]][0], points[f[2]][1], points[f[2]][2]);
		//Crossing two non equal vectors to find a vector perpendicular to the plane
		//Adding vector for phong shading average of normals
		vec3 n = normalize(cross(p2-p1, p3 - p2));
		for (int k = 0; k < VSPERFACE; k++) {
			int normNumber = f[k];
			//Contribute to norm average for that vertex
			norms[normNumber] += n;
		}
	}

	//Make sure all normals are unit length
	normalizeAll(norms, sizeof(points) / sizeof(float[VSPERFACE]));

	//Setting up our buffer
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	//Feeding in our points
	glBufferData(GL_ARRAY_BUFFER, sizeof(points) + sizeof(norms), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(points), points);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(points), sizeof(norms), norms);
}

//Normalize all vectors in an array
void normalizeAll(vec3 n[], int size) {
	for (int i = 0; i < size; i++) {
		n[i] = normalize(n[i]);
	}
}

//Set all norms to origin
void initNorms(vec3 n[], int size) {
	for (int i = 0; i < size; i++) {
		n[i] = vec3(0, 0, 0);
	}
}

//Clearing and enabling for depth and color buffers
void initBuffers() {
	//Clear screen to grey
	glClearColor(.5f, .5f, .5f, 1);
	//Depth is by default at max
	glClearDepth(ZFAR);
	//Clear color/depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//Enables various gl stuff
	glEnable(GL_BLEND);
	//How combined rgb values are computed
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	//Enable depth test
	glEnable(GL_DEPTH_TEST);
	//Enable writing to depth buffer
	glDepthMask(GL_TRUE);
	//Policy used to compare depth values
	glDepthFunc(GL_LEQUAL);
	//Depth range
	glDepthRange(ZNEAR, ZFAR);
}

// APPLICATION CODE

int main() {
  	glfwSetErrorCallback(ErrorGFLW);
  	if (!glfwInit())
      		return 1;
  	GLFWwindow *window = glfwCreateWindow(800, 800, "Faceted Shading", NULL, NULL);
  	if (!window) {
      		glfwTerminate();
      		return 1;
  	}
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	printf("GL version: %s\n", glGetString(GL_VERSION));
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertexPhongShader, &pixelPhongShader)))
		return 0;
	
 	 InitVertexBuffer();

	//Register events for polling
	//Passing in functions
  	glfwSetKeyCallback(window, Keyboard);//Keys
	glfwSetMouseButtonCallback(window, MouseButton);//Clicks 
	glfwSetCursorPosCallback(window, MouseMove);//Moves
	glfwSetScrollCallback(window, MouseScroll);//Scroll

	//Aka vsync
	//One buffer to render, one to display 
	//Setting number of frames between swaps
	//Expensive without - Why render 400fps on 60fps monitor?
  	glfwSwapInterval(1);

	while (!glfwWindowShouldClose(window)) {
		//Store window size before each frame is displayed
		glfwGetWindowSize(window, &screenWidth, &screenHeight);
		Display();
		//Swaps render and display buffers
		glfwSwapBuffers(window);
		//Processes recieved pending events
		glfwPollEvents();
	}

	//Cleanup
	Close();
	glfwDestroyWindow(window);
	glfwTerminate();
}

//New keyboard code for adjusting stretch and field of view
//Note: (expression)? evalutates to boolean, the following (value1) : (value2)
//acts like the body of an if statement. If true (value1), else (value2)
//Sometimes they are nested. Evaluate inside.outside
void Keyboard(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		bool shift = mods & GLFW_MOD_SHIFT;
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		fieldOfView += key == 'F' ? shift ? -5 : 5 : 0;
		fieldOfView = fieldOfView < 5 ? 5 : fieldOfView > 150 ? 150 : fieldOfView;
	}
}

//Called on each mouse press
void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
	//Function used to poll for certain mouse events
	if (action == GLFW_PRESS) {//While mouse is left clicking
		double x, y;
		glfwGetCursorPos(w, &x, &y); //Get the coords		
		cam.MouseDown((int)x, (int)y);
	}
	if (action == GLFW_RELEASE) {
		cam.MouseUp();
	}
}

//Tracks mouse coordinates
void MouseMove(GLFWwindow *w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		cam.MouseDrag((int)x, (int)y, false);
	}
}

//Tracks scroll wheel position
//NOTE:Needs a NET positive scroll to elongate
void MouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
	cam.MouseWheel((int)yoffset, true);
}

void Close() {
	// unbind vertex buffer and free GPU memory
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (vBuffer >= 0)
		glDeleteBuffers(1, &vBuffer);
}

void ErrorGFLW(int id, const char *reason) {
	printf("GFLW error %i: %s\n", id, reason);
}
