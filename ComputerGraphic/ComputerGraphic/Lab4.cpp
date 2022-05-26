#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm\glm.hpp>
#include <glm\matrix.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <math.h>
#include <vector>
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")

using namespace std;

GLFWwindow *g_window;

GLuint g_shaderProgramP, g_shaderProgramC;
GLint g_uMV;

class Model
{
public:
	GLuint vbo;
	GLuint ibo;
	GLuint vao;
	GLsizei indexCount;
};

Model g_modelPoint, g_modelCurve;

//константа дл€ сравнени€ с нулем
#define EPSILON 1.0e-5

//проверить, равно ли значение нулю
#define IS_ZERO(v) (abs(v) < EPSILON)

//функци€, возращающа€ знак параметра sigh(x)
#define SIGN(v) (int)(((v) > EPSILON) - ((v) < -EPSILON))

//количество линий в каждом сегмента кривой Ѕезье
#define RESOLUTION 64

//параметр, вли€ющий на кривизну (должен быть >= 2)
#define C 2.0

//класс дл€ хранени€ и обработки точек на плоскости
class Point
{
public:
	double x, y;
	Point()
	{
		x = y = 0.0;
	};
	Point(double xCoord, double yCoord)
	{
		x = xCoord;
		y = yCoord;
	};
	//возвращает сумму координат двух точек
	Point operator +(const Point &p) const { return Point(x + p.x, y + p.y); };

	//возвращает разность координат двух точек
	Point operator -(const Point &p) const { return Point(x - p.x, y - p.y); };
	
	//возращает произведение координат точки на число
	Point operator *(double v) const { return Point(x * v, y * v); };

	//нормализаци€ координат точки
	void normalize()
	{
		double l = sqrt(x * x + y * y);
		if (IS_ZERO(l)) x = y = 0.0;
		else
		{
			x /= l;
			y /= l;
		}
	};

	//возвращает абсолютный минимум среди координат двух точек 
	static Point absMin(const Point &p1, const Point &p2)
	{
		return Point(abs(p1.x) < abs(p2.x) ? p1.x : p2.x, abs(p1.y) < abs(p2.y) ? p1.y : p2.y);
	};
};

//класс дл€ хранени€ и обработки сегментов кубической кривой Ѕезье
class Segment
{
public:
	//контрольные точки Ѕезье
	Point points[4];

	//расчет промежуточных точек кривой
	//t - параметр кривой [0; 1]
	//возращает промежуточную точку кривой Ѕезье на основе заданного параметра
	Point calc(double t) const
	{
		double t2 = t * t;
		double t3 = t2 * t;
		double nt = 1.0 - t;
		double nt2 = nt * nt;
		double nt3 = nt2 * nt;
		return Point(nt3 * points[0].x + 3.0 * t * nt2 * points[1].x + 3.0 * t2 * nt * points[2].x + t3 * points[3].x,
					 nt3 * points[0].y + 3.0 * t * nt2 * points[1].y + 3.0 * t2 * nt * points[2].y + t3 * points[3].y);
	};
};

vector<Point> points;
vector<Segment> curves;
vector<Point> pointsCurves;

//строит интерфол€ционную кривую с пор€дком гладкости 1 на основе кубической кривой Ѕезье по заданному набору точек
bool tbezierSO1(const vector<Point> &values, vector<Segment> &curve)
{
	int n = values.size() - 1;

	if (n < 2)
		return false;

	curve.resize(n);

	Point cur, next, tgL, tgR, deltaL, deltaC, deltaR;
	double l1, l2;

	next = values[1] - values[0];
	next.normalize();

	for (int i = 0; i < n; ++i)
	{
		tgL = tgR;
		cur = next;

		deltaC = values[i + 1] - values[i];

		if (i > 0)
			deltaL = Point::absMin(deltaC, values[i] - values[i - 1]);
		else
			deltaL = deltaC;

		if (i < n - 1)
		{
			next = values[i + 2] - values[i + 1];
			next.normalize();
			if (IS_ZERO(cur.x) || IS_ZERO(cur.y))
				tgR = cur;
			else if (IS_ZERO(next.x) || IS_ZERO(next.y))
				tgR = next;
			else
				tgR = cur + next;
			tgR.normalize();
			deltaR = Point::absMin(deltaC, values[i + 2] - values[i + 1]);
		}
		else
		{
			tgR = Point();
			deltaR = deltaC;
		}

		l1 = IS_ZERO(tgL.x) ? 0.0 : deltaL.x / (C * tgL.x);
		l2 = IS_ZERO(tgR.x) ? 0.0 : deltaR.x / (C * tgR.x);

		if (abs(l1 * tgL.y) > abs(deltaL.y))
			l1 = IS_ZERO(tgL.y) ? 0.0 : deltaL.y / tgL.y;
		if (abs(l2 * tgR.y) > abs(deltaR.y))
			l2 = IS_ZERO(tgR.y) ? 0.0 : deltaR.y / tgR.y;

		curve[i].points[0] = values[i];
		curve[i].points[1] = curve[i].points[0] + tgL * l1;
		curve[i].points[3] = values[i + 1];
		curve[i].points[2] = curve[i].points[3] - tgR * l2;
	}

	return true;
}

GLuint createShader(const GLchar *code, GLenum type)
{
	GLuint result = glCreateShader(type);

	glShaderSource(result, 1, &code, NULL);
	glCompileShader(result);

	GLint compiled;
	glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint infoLen = 0;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 0)
		{
			char *infoLog = (char *)alloca(infoLen);
			glGetShaderInfoLog(result, infoLen, NULL, infoLog);
			cout << "Shader compilation error" << endl << infoLog << endl;
		}
		glDeleteShader(result);
		return 0;
	}

	return result;
}

GLuint createProgram(GLuint vsh, GLuint fsh)
{
	GLuint result = glCreateProgram();

	glAttachShader(result, vsh);
	glAttachShader(result, fsh);

	glLinkProgram(result);

	GLint linked;
	glGetProgramiv(result, GL_LINK_STATUS, &linked);

	if (!linked)
	{
		GLint infoLen = 0;
		glGetProgramiv(result, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 0)
		{
			char *infoLog = (char *)alloca(infoLen);
			glGetProgramInfoLog(result, infoLen, NULL, infoLog);
			cout << "Shader program linking error" << endl << infoLog << endl;
		}
		glDeleteProgram(result);
		return 0;
	}

	return result;
}

bool createShaderProgram()
{
	g_shaderProgramP = 0;
	g_shaderProgramC = 0;

	const GLchar vshP[] =
		"#version 330\n"
		""
		"layout(location = 0) in vec2 a_position;"
		"uniform mat4 u_MV;"
		""
		"void main()"
		"{"
		"    gl_Position = u_MV * vec4(a_position, 0.0, 1.0);"
		"}"
		;

	const GLchar fsh[] =
		"#version 330\n"
		""
		"layout(location = 0) out vec4 o_color;"
		""
		"void main()"
		"{"
		"   o_color = vec4(0.0, 0.0, 0.0, 1.0);"
		"}"
		;

	const GLchar vshC[] =
		"#version 330\n"
		""
		"layout(location = 0) in vec2 a_position;"
		"uniform mat4 u_MV;"
		""
		"void main()"
		"{"
		"    gl_Position = vec4(a_position, 0.0, 1.0);"
		"}"
		;

	GLuint vertexShader, fragmentShader;

	//вершинные шейдеры дл€ точек и кривых отличаютс€, а фрагметные нет

	//вершинный шейдер дл€ построение точек
	vertexShader = createShader(vshP, GL_VERTEX_SHADER);

	//фрагметный шейдер
	fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

	g_shaderProgramP = createProgram(vertexShader, fragmentShader);

	glDeleteShader(vertexShader);

	//вершинный шейдер дл€ построени€ кривых
	vertexShader = createShader(vshC, GL_VERTEX_SHADER);
	g_shaderProgramC = createProgram(vertexShader, fragmentShader);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return g_shaderProgramP != 0 && g_shaderProgramC != 0;
}

bool createModels(bool init = false)
{
	const GLfloat vertices[] =
	{
		-0.5f, -0.5f,
		 0.5f, -0.5f,
		 0.5f,  0.5f,
		-0.5f,  0.5f,
	};

	const GLuint indices[] =
	{
		0, 1, 2, 2, 3, 0
	};

	glGenVertexArrays(1, &g_modelPoint.vao);
	glBindVertexArray(g_modelPoint.vao);

	glGenBuffers(1, &g_modelPoint.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_modelPoint.vbo);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &g_modelPoint.ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_modelPoint.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint), indices, GL_STATIC_DRAW);

	g_modelPoint.indexCount = 6;

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid *)0);

	if (!init)
	{
		int size = pointsCurves.size();
		GLfloat* verteces1 = new GLfloat[size * 2];
		GLuint* indices1 = new GLuint[size];

		int step = 0;
		for (int i = 0; i < size; i++)
		{
			verteces1[step] = pointsCurves[i].x - 1.0;
			verteces1[step + 1] = 1.0 - pointsCurves[i].y;
			indices1[i] = i;
			step += 2;
		}

		glGenVertexArrays(1, &g_modelCurve.vao);
		glBindVertexArray(g_modelCurve.vao);

		glGenBuffers(1, &g_modelCurve.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_modelCurve.vbo);
		glBufferData(GL_ARRAY_BUFFER, 2 * size * sizeof(GLfloat), verteces1, GL_STATIC_DRAW);

		glGenBuffers(1, &g_modelCurve.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_modelCurve.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(GLuint), indices1, GL_STATIC_DRAW);
		g_modelCurve.indexCount = size;

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid*)0);

		delete[] verteces1;
		delete[] indices1;
	}
	if (!init)
	{
		return g_modelPoint.vbo != 0 && g_modelPoint.ibo != 0 && g_modelPoint.vao != 0 && g_modelCurve.vbo != 0 && g_modelCurve.ibo != 0 && g_modelCurve.vao != 0;
	}
	else
	{
		return g_modelPoint.vbo != 0 && g_modelPoint.ibo != 0 && g_modelPoint.vao != 0;
	}
}

bool init()
{
	// Set initial color of color buffer to white.
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	//включает тест глубины (очень важно)
	//буфер глубины создает библиотека GLFW
	glEnable(GL_DEPTH_TEST);

	return createShaderProgram() && createModels(true);
}

void reshape(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
}

//ќбработчик нажати€ мышки дл€ отрисовки точек
void mouse_click(GLFWwindow * window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		double x_coord, y_coord;
		glfwGetCursorPos(g_window, &x_coord, &y_coord);

		int width, height;
		glfwGetWindowSize(g_window, &width, &height);

		double norm_x = 2.0 * x_coord / width;
		double norm_y = 2.0 * y_coord / height;

		points.push_back(Point(norm_x, norm_y));

		pointsCurves.clear();
		tbezierSO1(points, curves);

		for (Segment c : curves)
		{
			for (int i = 0; i < RESOLUTION; ++i)
			{
				Point p = c.calc((double)i / (double)RESOLUTION);
				pointsCurves.push_back(p);
			}
		}
		createModels();
	}
}

void draw()
{
	// Clear color buffer.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(g_shaderProgramP);
	glBindVertexArray(g_modelPoint.vao);

	for (Point p : points)
	{
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(float(p.x) - 1.0, 1.0 - float(p.y), 0.0f));
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.02f));
		glm::mat4 mv = translation * scale;
		glUniformMatrix4fv(g_uMV, 1, GL_FALSE, glm::value_ptr(mv));
		glDrawElements(GL_TRIANGLES, g_modelPoint.indexCount, GL_UNSIGNED_INT, NULL);
	}
	glUseProgram(g_shaderProgramC);
	glBindVertexArray(g_modelCurve.vao);
	glDrawElements(GL_LINE_STRIP, g_modelCurve.indexCount, GL_UNSIGNED_INT, NULL);
}

void cleanup(GLuint g_shaderProgram, Model g_model)
{
	if (g_shaderProgram != 0)
		glDeleteProgram(g_shaderProgram);
	if (g_model.vbo != 0)
		glDeleteBuffers(1, &g_model.vbo);
	if (g_model.ibo != 0)
		glDeleteBuffers(1, &g_model.ibo);
	if (g_model.vao != 0)
		glDeleteVertexArrays(1, &g_model.vao);
}

bool initOpenGL()
{
	// Initialize GLFW functions.
	if (!glfwInit())
	{
		cout << "Failed to initialize GLFW" << endl;
		return false;
	}

	// Request OpenGL 3.3 without obsoleted functions.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create window.
	g_window = glfwCreateWindow(800, 600, "OpenGL Test", NULL, NULL);
	if (g_window == NULL)
	{
		cout << "Failed to open GLFW window" << endl;
		glfwTerminate();
		return false;
	}

	// Initialize OpenGL context with.
	glfwMakeContextCurrent(g_window);

	// Set internal GLEW variable to activate OpenGL core profile.
	glewExperimental = true;

	// Initialize GLEW functions.
	if (glewInit() != GLEW_OK)
	{
		cout << "Failed to initialize GLEW" << endl;
		return false;
	}

	// Ensure we can capture the escape key being pressed.
	glfwSetInputMode(g_window, GLFW_STICKY_KEYS, GL_TRUE);

	// Set callback for framebuffer resizing event.
	glfwSetFramebufferSizeCallback(g_window, reshape);

	//ќбработчик нажати€ мышки
	glfwSetMouseButtonCallback(g_window, mouse_click);

	return true;
}

void tearDownOpenGL()
{
	// Terminate GLFW.
	glfwTerminate();
}

int main()
{
	// Initialize OpenGL
	if (!initOpenGL())
		return -1;

	// Initialize graphical resources.
	bool isOk = init();

	if (isOk)
	{
		// Main loop until window closed or escape pressed.
		while (glfwGetKey(g_window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(g_window) == 0)
		{
			// Draw scene.
			draw();

			// Swap buffers.
			glfwSwapBuffers(g_window);
			// Poll window events.
			glfwPollEvents();
		}
	}

	// Cleanup graphical resources.
	cleanup(g_shaderProgramP, g_modelPoint);
	cleanup(g_shaderProgramC, g_modelCurve);

	// Tear down OpenGL.
	tearDownOpenGL();

	return isOk ? 0 : -1;
}