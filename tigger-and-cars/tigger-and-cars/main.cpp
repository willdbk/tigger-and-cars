#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <time.h>

#if defined(__APPLE__)
#  include <GLUT/GLUT.h>
#  include <OpenGL/gl3.h>
#  include <OpenGL/glu.h>
#else
#  if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#    include <windows.h>
#  endif
#  include <GL/glew.h>
#  include <GL/freeglut.h>
#endif

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
const unsigned int windowWidth = 512, windowHeight = 512;

int majorVersion = 3, minorVersion = 0;

bool keyboardState[256];
bool arrowKeyState[4];
double trackingT;
int best_score;
int score;
int lives;
bool invincible;
int visible;
bool game_over;

enum OBJECT_TYPE {
  TIGGER, GROUND, OBSTACLE, HEART, NONE
};

void
getErrorInfo(unsigned int handle) {
  int logLen;
  glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
  if (logLen > 0) {
    char * log = new char[logLen];
    int written;
    glGetShaderInfoLog(handle, logLen, &written, log);
    printf("Shader log:\n%s", log);
    delete log;
  }
}
void
checkShader(unsigned int shader, const char * message) {
  int OK;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
  if (!OK) {
    printf("%s!\n", message);
    getErrorInfo(shader);
  }
}

void
checkLinking(unsigned int program) {
  int OK;
  glGetProgramiv(program, GL_LINK_STATUS, &OK);
  if (!OK) {
    printf("Failed to link shader program!\n");
    getErrorInfo(program);
  }
}

// row-major matrix 4x4

struct mat4 {
  float m[4][4];
public:

  mat4() { }

  mat4(float m00, float m01, float m02, float m03,
       float m10, float m11, float m12, float m13,
       float m20, float m21, float m22, float m23,
       float m30, float m31, float m32, float m33) {
    m[0][0] = m00;
    m[0][1] = m01;
    m[0][2] = m02;
    m[0][3] = m03;
    m[1][0] = m10;
    m[1][1] = m11;
    m[1][2] = m12;
    m[1][3] = m13;
    m[2][0] = m20;
    m[2][1] = m21;
    m[2][2] = m22;
    m[2][3] = m23;
    m[3][0] = m30;
    m[3][1] = m31;
    m[3][2] = m32;
    m[3][3] = m33;
  }

  mat4 operator *(const mat4& right) {
    mat4 result;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        result.m[i][j] = 0;
        for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
      }
    }
    return result;
  }

  operator float*() {
    return &m[0][0];
  }
};


// 3D point in homogeneous coordinates

struct vec4 {
  float v[4];

  vec4(float x = 0, float y = 0, float z = 0, float w = 1) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
  }

  vec4 operator *(const mat4& mat) {
    vec4 result;
    for (int j = 0; j < 4; j++) {
      result.v[j] = 0;
      for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
    }
    return result;
  }

  vec4 operator +(const vec4& vec) {
    vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
    return result;
  }
};

// 2D point in Cartesian coordinates

struct vec2 {
  float x, y;

  vec2(float x = 0.0, float y = 0.0) : x(x), y(y) { }

  vec2 operator +(const vec2& v) {
    return vec2(x + v.x, y + v.y);
  }

  vec2 operator *(float s) {
    return vec2(x * s, y * s);
  }

};

// 3D point in Cartesian coordinates

struct vec3 {
  float x, y, z;

  vec3(float x = 0.0, float y = 0.0, float z = 0.0) : x(x), y(y), z(z) { }

  static vec3
  random() {
    return vec3(((float) rand() / RAND_MAX) * 2 - 1, ((float) rand() / RAND_MAX) * 2 - 1, ((float) rand() / RAND_MAX) * 2 - 1);
  }

  vec3 operator +(const vec3& v) {
    return vec3(x + v.x, y + v.y, z + v.z);
  }

  vec3 operator -(const vec3& v) {
    return vec3(x - v.x, y - v.y, z - v.z);
  }

  vec3 operator *(float s) {
    return vec3(x * s, y * s, z * s);
  }

  vec3 operator /(float s) {
    return vec3(x / s, y / s, z / s);
  }

  float
  length() {
    return sqrt(x * x + y * y + z * z);
  }

  vec3
  normalize() {
    return *this / length();
  }

  void
  print() {
    printf("%f \t %f \t %f \n", x, y, z);
  }
};

vec3
cross(const vec3& a, const vec3& b) {
  return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float
dot(const vec3& a, const vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3
elementWise(const vec3& a, const vec3& b) {
  return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

class Geometry {
protected:
  unsigned int vao;

public:

  Geometry() {
    glGenVertexArrays(1, &vao);
  }

  virtual void Draw() = 0;
};

class PolygonalMesh : public Geometry {

  struct Face {
    int positionIndices[4];
    int normalIndices[4];
    int texcoordIndices[4];
    bool isQuad;
  };

  std::vector<std::string*> rows;
  std::vector<vec3*> positions;
  std::vector<std::vector<Face*>> submeshFaces;
  std::vector<vec3*> normals;
  std::vector<vec2*> texcoords;

  int nTriangles;

public:
  PolygonalMesh(const char *filename);
  ~PolygonalMesh();

  void Draw();
};

class TexturedQuad : public Geometry {
  unsigned int vbo[3];

public:

  TexturedQuad() {
    glBindVertexArray(vao);
    glGenBuffers(3, vbo);

    // define the texture coordinates here
    // assign the array of texture coordinates to
    // vertex attribute array 1
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    static float vertexCoords[] = {0, -1, 0, -1, -1, -1, -1, -1, 1, 1, -1, 1, 1, -1, -1, -1, -1, -1};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexCoords), vertexCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);


    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    static float vertexTextureCoords[] = {0.5, 0.5, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexTextureCoords), vertexTextureCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);


    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    static float vertexNormalCoords[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexNormalCoords), vertexNormalCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
  }

  void
  Draw() {
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
    glDisable(GL_DEPTH_TEST);
  }
};

class InfiniteTexturedQuad : public Geometry {
  unsigned int vbo[3];

public:

  InfiniteTexturedQuad() {
    glBindVertexArray(vao);
    glGenBuffers(3, vbo);

    // define the texture coordinates here
    // assign the array of texture coordinates to
    // vertex attribute array 1
    float y = 0;
    float w = 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    static float vertexCoords[] = {0, y, 0, 1, -1, y, -1, w, -1, y, 1, w, 1, y, 1, w, 1, y, -1, w, -1, y, -1, w};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexCoords), vertexCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);


    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    static float vertexTextureCoords[] = {0.5, 0.5, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexTextureCoords), vertexTextureCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);


    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    static float vertexNormalCoords[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    glBufferData(GL_ARRAY_BUFFER, sizeof (vertexNormalCoords), vertexNormalCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
  }

  void
  Draw() {
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
    glDisable(GL_DEPTH_TEST);
  }
};

PolygonalMesh::PolygonalMesh(const char *filename) {
  std::fstream file(filename);
  if (!file.is_open()) {
    return;
  }

  char buffer[256];
  while (!file.eof()) {
    file.getline(buffer, 256);
    rows.push_back(new std::string(buffer));
  }

  submeshFaces.push_back(std::vector<Face*>());
  std::vector<Face*>* faces = &submeshFaces.at(submeshFaces.size() - 1);

  for (std::string *row : rows) {
    if (row->empty() || (*row)[0] == '#')
      continue;
    else if ((*row)[0] == 'v' && (*row)[1] == ' ') {
      float tmpx, tmpy, tmpz;
      sscanf(row->c_str(), "v %f %f %f", &tmpx, &tmpy, &tmpz);
      positions.push_back(new vec3(tmpx, tmpy, tmpz));
    } else if ((*row)[0] == 'v' && (*row)[1] == 'n') {
      float tmpx, tmpy, tmpz;
      sscanf(row->c_str(), "vn %f %f %f", &tmpx, &tmpy, &tmpz);
      normals.push_back(new vec3(tmpx, tmpy, tmpz));
    } else if ((*row)[0] == 'v' && (*row)[1] == 't') {
      float tmpx, tmpy;
      sscanf(row->c_str(), "vt %f %f", &tmpx, &tmpy);
      texcoords.push_back(new vec2(tmpx, tmpy));
    } else if ((*row)[0] == 'f') {
      if (count(row->begin(), row->end(), ' ') == 3) {
        Face* f = new Face();
        f->isQuad = false;
        sscanf(row->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d",
               &f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
               &f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
               &f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2]);
        faces->push_back(f);
      } else {
        Face* f = new Face();
        f->isQuad = true;
        sscanf(row->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
               &f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
               &f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
               &f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2],
               &f->positionIndices[3], &f->texcoordIndices[3], &f->normalIndices[3]);
        faces->push_back(f);
      }
    } else if ((*row)[0] == 'g') {
      if (faces->size() > 0) {
        submeshFaces.push_back(std::vector<Face*>());
        faces = &submeshFaces.at(submeshFaces.size() - 1);
      }
    }
  }

  int numberOfTriangles = 0;
  for (int iSubmesh = 0; iSubmesh < submeshFaces.size(); iSubmesh++) {
    std::vector<Face*>& faces = submeshFaces.at(iSubmesh);

    for (int i = 0; i < faces.size(); i++) {
      if (faces[i]->isQuad) numberOfTriangles += 2;
      else numberOfTriangles += 1;
    }
  }

  nTriangles = numberOfTriangles;

  float *vertexCoords = new float[numberOfTriangles * 9];
  float *vertexTexCoords = new float[numberOfTriangles * 6];
  float *vertexNormalCoords = new float[numberOfTriangles * 9];


  int triangleIndex = 0;
  for (int iSubmesh = 0; iSubmesh < submeshFaces.size(); iSubmesh++) {
    std::vector<Face*>& faces = submeshFaces.at(iSubmesh);

    for (int i = 0; i < faces.size(); i++) {
      if (faces[i]->isQuad) {
        vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[0] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[0] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;


        vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[0] - 1]->x;
        vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0] - 1]->y;
        vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0] - 1]->z;

        vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1] - 1]->x;
        vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1] - 1]->y;
        vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1] - 1]->z;

        vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2] - 1]->x;
        vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2] - 1]->y;
        vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2] - 1]->z;


        vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[0] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2] - 1]->z;

        triangleIndex++;


        vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[3] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[3] - 1]->y;


        vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[1] - 1]->x;
        vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[1] - 1]->y;
        vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[1] - 1]->z;

        vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[2] - 1]->x;
        vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[2] - 1]->y;
        vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[2] - 1]->z;

        vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[3] - 1]->x;
        vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[3] - 1]->y;
        vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[3] - 1]->z;


        vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[1] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[1] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[1] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[2] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[2] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[2] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[3] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[3] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[3] - 1]->z;

        triangleIndex++;
      } else {
        vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[0] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[0] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;

        vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
        vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;

        vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[0] - 1]->x;
        vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0] - 1]->y;
        vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0] - 1]->z;

        vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1] - 1]->x;
        vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1] - 1]->y;
        vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1] - 1]->z;

        vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2] - 1]->x;
        vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2] - 1]->y;
        vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2] - 1]->z;


        vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[0] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1] - 1]->z;

        vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2] - 1]->x;
        vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2] - 1]->y;
        vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2] - 1]->z;

        triangleIndex++;
      }
    }
  }

  glBindVertexArray(vao);

  unsigned int vbo[3];
  glGenBuffers(3, &vbo[0]);

  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
  glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof (float), vertexCoords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glBufferData(GL_ARRAY_BUFFER, nTriangles * 6 * sizeof (float), vertexTexCoords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

  glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
  glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof (float), vertexNormalCoords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  delete vertexCoords;
  delete vertexTexCoords;
  delete vertexNormalCoords;
}

void
PolygonalMesh::Draw() {
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, nTriangles * 3);
  glDisable(GL_DEPTH_TEST);
}

PolygonalMesh::~PolygonalMesh() {
  for (unsigned int i = 0; i < rows.size(); i++) delete rows[i];
  for (unsigned int i = 0; i < positions.size(); i++) delete positions[i];
  for (unsigned int i = 0; i < submeshFaces.size(); i++)
    for (unsigned int j = 0; j < submeshFaces.at(i).size(); j++)
      delete submeshFaces.at(i).at(j);
  for (unsigned int i = 0; i < normals.size(); i++) delete normals[i];
  for (unsigned int i = 0; i < texcoords.size(); i++) delete texcoords[i];
}

class Shader {
protected:
  unsigned int shaderProgram;

public:

  Shader() {
    shaderProgram = 0;
  }

  ~Shader() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
  }

  void
  Run() {
    if (shaderProgram) glUseProgram(shaderProgram);
  }

  virtual void
  UploadInvM(mat4& InvM) { }

  virtual void
  UploadMVP(mat4& MVP) { }

  virtual void
  UploadVP(mat4& VP) { }

  virtual void
  UploadM(mat4& M) { }

  virtual void
  UploadMaterialAttributes(vec3 ka, vec3 kd, vec3 ks, float shininess) { }

  virtual void
  UploadLightAttributes(vec4 worldLightPosition, vec3 La, vec3 Le) { }

  virtual void
  UploadEyePosition(vec3 wEye) { }

  virtual void
  UploadSamplerID() { }

  virtual void
  UploadColor(vec3 colorRaw) { }
};

class MeshShader : public Shader {
public:

  MeshShader() {
    const char *vertexSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        in vec3 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        in vec3 vertexNormal; \n\
        uniform mat4 M, InvM, MVP; \n\
        uniform vec3 worldEyePosition; \n\
        uniform vec4 worldLightPosition; \n\
        out vec2 texCoord; \n\
        out vec3 worldNormal; \n\
        out vec3 worldView; \n\
        out vec3 worldLight; \n\
        \n\
        void main() { \n\
        texCoord = vertexTexCoord; \n\
        vec4 worldPosition = vec4(vertexPosition, 1) * M; \n\
        worldLight  = worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w; \n\
        worldView = worldEyePosition - worldPosition.xyz; \n\
        worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz; \n\
        gl_Position = vec4(vertexPosition, 1) * MVP; \n\
        } \n\
        ";

    const char *fragmentSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        uniform sampler2D samplerUnit; \n\
        uniform vec3 La, Le; \n\
        uniform vec3 ka, kd, ks; \n\
        uniform float shininess; \n\
        in vec2 texCoord; \n\
        in vec3 worldNormal; \n\
        in vec3 worldView; \n\
        in vec3 worldLight; \n\
        out vec4 fragmentColor; \n\
        \n\
        void main() { \n\
            vec3 N = normalize(worldNormal); \n\
            vec3 V = normalize(worldView); \n\
            vec3 L = normalize(worldLight); \n\
            vec3 H = normalize(V + L); \n\
            vec3 texel = texture(samplerUnit, texCoord).xyz; \n\
            vec3 color = \n\
                La * ka + \n\
                Le * kd * texel * max(0.0, dot(L, N)) + \n\
                Le * ks * pow(max(0.0, dot(H, N)), shininess); \n\
            fragmentColor = vec4(color, 1); \n\
        } \n\
        ";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!vertexShader) {
      printf("Error in vertex shader creation\n");
      exit(1);
    }

    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    checkShader(vertexShader, "Vertex shader error");

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fragmentShader) {
      printf("Error in fragment shader creation\n");
      exit(1);
    }

    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    checkShader(fragmentShader, "Fragment shader error");

    shaderProgram = glCreateProgram();
    if (!shaderProgram) {
      printf("Error in shader program creation\n");
      exit(1);
    }

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glBindAttribLocation(shaderProgram, 0, "vertexPosition");
    glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
    glBindAttribLocation(shaderProgram, 2, "vertexNormal");

    glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

    glLinkProgram(shaderProgram);
    checkLinking(shaderProgram);
  }

  void
  UploadSamplerID() {
    int samplerUnit = 0;
    int location = glGetUniformLocation(shaderProgram, "samplerUnit");
    glUniform1i(location, samplerUnit);
    glActiveTexture(GL_TEXTURE0 + samplerUnit);
  }

  void
  UploadInvM(mat4& InvM) {
    int location = glGetUniformLocation(shaderProgram, "InvM");
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, InvM);
    else printf("uniform InvM cannot be set\n");
  }

  void
  UploadMVP(mat4& MVP) {
    int location = glGetUniformLocation(shaderProgram, "MVP");
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVP);
    else printf("uniform MVP cannot be set\n");
  }

  void
  UploadM(mat4& M) {
    int location = glGetUniformLocation(shaderProgram, "M");
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
    else printf("uniform M cannot be set\n");
  }

  void
  UploadMaterialAttributes(vec3 ka, vec3 kd, vec3 ks, float shininess) {
    int location = glGetUniformLocation(shaderProgram, "ka");
    if (location >= 0) glUniform3fv(location, 1, &ka.x);
    else printf("uniform ka cannot be set\n");

    location = glGetUniformLocation(shaderProgram, "kd");
    if (location >= 0) glUniform3fv(location, 1, &kd.x);
    else printf("uniform kd cannot be set\n");

    location = glGetUniformLocation(shaderProgram, "ks");
    if (location >= 0) glUniform3fv(location, 1, &ks.x);
    else printf("uniform ks cannot be set\n");

    location = glGetUniformLocation(shaderProgram, "shininess");
    if (location >= 0) glUniform1f(location, shininess);
    else printf("uniform shininess cannot be set\n");
  }

  void
  UploadLightAttributes(vec4 worldLightPosition, vec3 La, vec3 Le) {
    int location = glGetUniformLocation(shaderProgram, "worldLightPosition");
    if (location >= 0) glUniform4fv(location, 1, &worldLightPosition.v[0]);
    else printf("uniform worldLightPosition cannot be set\n");

    location = glGetUniformLocation(shaderProgram, "La");
    if (location >= 0) glUniform3fv(location, 1, &La.x);
    else printf("uniform La cannot be set\n");

    location = glGetUniformLocation(shaderProgram, "Le");
    if (location >= 0) glUniform3fv(location, 1, &Le.x);
    else printf("uniform Le cannot be set\n");
  }

  void
  UploadEyePosition(vec3 wEye) {
    int location = glGetUniformLocation(shaderProgram, "worldEyePosition");
    if (location >= 0) glUniform3fv(location, 1, &wEye.x);
    else printf("uniform worldEyePosition cannot be set\n");
  }
};

class InfiniteMeshShader : public MeshShader {
public:

  InfiniteMeshShader() {
    // shader program for rendering the ground as an infinite quad

    const char *vertexSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        in vec4 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        in vec3 vertexNormal; \n\
        uniform mat4 M, InvM, MVP; \n\
        \n\
        out vec2 texCoord; \n\
        out vec4 worldPosition; \n\
        out vec3 worldNormal; \n\
        \n\
        void main() { \n\
        texCoord = vertexTexCoord; \n\
        worldPosition = vertexPosition * M; \n\
        worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz; \n\
        gl_Position = vertexPosition * MVP; \n\
        } \n\
        ";

    const char *fragmentSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        uniform sampler2D samplerUnit; \n\
        uniform vec3 La, Le; \n\
        uniform vec3 ka, kd, ks; \n\
        uniform float shininess; \n\
        uniform vec3 worldEyePosition; \n\
        uniform vec4 worldLightPosition; \n\
        in vec2 texCoord; \n\
        in vec4 worldPosition; \n\
        in vec3 worldNormal; \n\
        out vec4 fragmentColor; \n\
        void main() { \n\
        vec3 N = normalize(worldNormal); \n\
        vec3 V = normalize(worldEyePosition * worldPosition.w - worldPosition.xyz);\n\
        vec3 L = normalize(worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w);\n\
        vec3 H = normalize(V + L); \n\
        vec2 position = worldPosition.xz / worldPosition.w; \n\
        vec2 tex = position.xy - floor(position.xy); \n\
        vec3 texel = texture(samplerUnit, tex).xyz; \n\
        vec3 color = La * ka + Le * kd * texel* max(0.0, dot(L, N)) + Le * ks * pow(max(0.0, dot(H, N)), shininess); \n\
        fragmentColor = vec4(color, 1); \n\
        } \n\
        ";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!vertexShader) {
      printf("Error in vertex shader creation\n");
      exit(1);
    }

    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    checkShader(vertexShader, "Vertex shader error");

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fragmentShader) {
      printf("Error in fragment shader creation\n");
      exit(1);
    }

    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    checkShader(fragmentShader, "Fragment shader error");

    shaderProgram = glCreateProgram();
    if (!shaderProgram) {
      printf("Error in shader program creation\n");
      exit(1);
    }

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glBindAttribLocation(shaderProgram, 0, "vertexPosition");
    glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
    glBindAttribLocation(shaderProgram, 2, "vertexNormal");

    glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

    glLinkProgram(shaderProgram);
    checkLinking(shaderProgram);
  }

};

class ShadowShader : public Shader {
public:

  ShadowShader() {
    // shader program for rendering plane-projected shadows

    const char *vertexSource = " \n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        in vec3 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        in vec3 vertexNormal; \n\
        uniform mat4 M, VP; \n\
        uniform vec4 worldLightPosition; \n\
        \n\
        void main() { \n\
        vec4 p = vec4(vertexPosition, 1) * M; \n\
        vec3 s; \n\
        s.y = -0.999; \n\
        s.x = (p.x - worldLightPosition.x) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.x; \n\
        s.z = (p.z - worldLightPosition.z) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.z; \n\
        gl_Position = vec4(s, 1) * VP; \n\
        } \n\
        ";

    const char *fragmentSource = " \n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        out vec4 fragmentColor; \n\
        \n\
        void main() { \n\
        fragmentColor = vec4(0.0, 0.1, 0.0, 1); \n\
        } \n\
        ";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!vertexShader) {
      printf("Error in vertex shader creation\n");
      exit(1);
    }

    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    checkShader(vertexShader, "Vertex shader error");

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fragmentShader) {
      printf("Error in fragment shader creation\n");
      exit(1);
    }

    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    checkShader(fragmentShader, "Fragment shader error");

    shaderProgram = glCreateProgram();
    if (!shaderProgram) {
      printf("Error in shader program creation\n");
      exit(1);
    }

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glBindAttribLocation(shaderProgram, 0, "vertexPosition");
    glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
    glBindAttribLocation(shaderProgram, 2, "vertexNormal");

    glBindFragDataLocation(shaderProgram, 0, "fragmentColor");

    glLinkProgram(shaderProgram);
    checkLinking(shaderProgram);
  }

  void
  UploadVP(mat4& VP) {
    int location = glGetUniformLocation(shaderProgram, "VP");
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, VP);
    else printf("uniform VP cannot be set\n");
  }

  void
  UploadM(mat4& M) {
    int location = glGetUniformLocation(shaderProgram, "M");
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
    else printf("uniform M cannot be set\n");
  }

  void
  UploadLightAttributes(vec4 worldLightPosition, vec3 La, vec3 Le) {
    int location = glGetUniformLocation(shaderProgram, "worldLightPosition");
    if (location >= 0) glUniform4fv(location, 1, &worldLightPosition.v[0]);
    else printf("uniform worldLightPosition cannot be set\n");
  }
};



extern "C" unsigned char*
stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class Texture {
  unsigned int textureId;

public:

  Texture(const std::string& inputFileName) {
    unsigned char* data;
    int width;
    int height;
    int nComponents = 4;

    data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);

    if (data == NULL) {
      return;
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    if (nComponents == 3) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    if (nComponents == 4) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    delete data;
  }

  void
  Bind() {
    glBindTexture(GL_TEXTURE_2D, textureId);
  }
};

class Light {
  vec3 La, Le;
  vec4 worldLightPosition;

public:

  Light(vec4 _worldLightPosition, vec3 _La = vec3(1, 1, 1), vec3 _Le = vec3(1, 1, 1)) {
    worldLightPosition = _worldLightPosition;
    La = _La;
    Le = _Le;
  }

  void
  UploadAttributes(Shader* s) {
    s->UploadLightAttributes(worldLightPosition, La, Le);
  }

  void
  SetPointLightSource(vec3& pos) {
    worldLightPosition = vec4(pos.x, pos.y, pos.z, 1);
  }

  void
  SetDirectionalLightSource(vec3& dir) {
    worldLightPosition = vec4(dir.x, dir.y, dir.z, 0);
  }
};

Light
light(vec4(0, 2, 2, 1));

class Material {
  Shader* shader;
  Texture* texture;
  vec3 ka, kd, ks;
  float shininess;

public:

  Material(Shader* s, vec3 _ka, vec3 _kd, vec3 _ks, float _shininess, Texture* t = 0) {
    shader = s;
    ka = _ka;
    kd = _kd;
    ks = _ks;
    shininess = _shininess;
    texture = t;
  }

  Shader*
  GetShader() {
    return shader;
  }

  void
  UploadAttributes() {
    if (texture) {
      shader->UploadSamplerID();
      texture->Bind();
      shader->UploadMaterialAttributes(ka, kd, ks, shininess);
    } else
      shader->UploadMaterialAttributes(ka, kd, ks, shininess);
  }
};

class Mesh {
  Geometry* geometry;
  Material* material;

public:

  Mesh(Geometry* g, Material* m) {
    geometry = g;
    material = m;
  }

  Shader*
  GetShader() {
    return material->GetShader();
  }

  void
  Draw() {
    material->UploadAttributes();
    geometry->Draw();
  }
};

class Camera {
  vec3 wEye, wLookat, wVup, velocity;
  float fov, asp, fp, bp, speed, angularVelocity;

public:

  Camera() {
    wEye = vec3(1.0, 0.0, 3.0);
    wLookat = vec3(0.5, 0.0, 0.0);
    wVup = vec3(0.0, 1.0, 0.0);
    velocity = vec3(0.0, 0.0, 0.0);
    angularVelocity = 0;
    fov = M_PI / 4.0;
    asp = 1.0;
    fp = 0.01;
    bp = 20.0;
  }

  vec3
  getEyePosition() {
    return wEye;
  }

  void
  SetAspectRatio(float a) {
    asp = a;
  }

  void
  Move() {
    wEye = wEye + velocity;
    wLookat = wLookat + velocity;

    vec3 r = cross(GetAhead(), wVup.normalize());
    float d = wLookat.length();
    if (d != 0) {
      wLookat = wLookat.normalize();
    }
    wLookat = (wLookat * cos(angularVelocity) + r * sin(angularVelocity)) * d;
  }

  void
  Control(float dt) {
    if (keyboardState['d']) {
      angularVelocity = 2.0 * dt;
    } else if (keyboardState['a']) {
      angularVelocity = -2.0 * dt;
    } else {
      angularVelocity = 0;
    }

    if (keyboardState['w']) {
      velocity = GetAhead() * 1.5 * dt;
    } else if (keyboardState['s']) {
      velocity = GetAhead() * -1.5 * dt;
    }      //        else if(arrowKeyState[0]) {
      //            velocity.x = -2.0*dt;
      //        }
      //        else if(arrowKeyState[2]) {
      //            velocity.x = 2.0*dt;
      //        }
      //        else if(arrowKeyState[1]) {
      //            velocity.z = -2.0*dt;
      //        }
      //        else if(arrowKeyState[3]) {
      //            velocity.z = 2.0*dt;
      //        }
    else {
      velocity = vec3(0.0, 0.0, 0.0);
    }

    if (keyboardState['T'] || keyboardState['t']) {
      double theta = trackingT;
      // derivative of the heart curve
      velocity.x = 48 * cos(theta) * pow(sin(theta), 2) / 10.0 * dt;
      velocity.z = -1 * (-13 * sin(theta) + 10 * sin(2 * theta) + 6 * sin(3 * theta) + 4 * sin(4 * theta)) / 10.0 * dt;
      trackingT += dt;
    }
  }

  void
  UploadAttributes(Shader* s) {
    s->UploadEyePosition(wEye);
  }

  vec3
  GetAhead() {
    return (wLookat - wEye).normalize();
  }

  mat4
  GetViewMatrix() {
    vec3 w = (wEye - wLookat).normalize();
    vec3 u = cross(wVup, w).normalize();
    vec3 v = cross(w, u);

    return
    mat4(
         1.0f, 0.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 1.0f, 0.0f,
         -wEye.x, -wEye.y, -wEye.z, 1.0f) *
            mat4(
                 u.x, v.x, w.x, 0.0f,
                 u.y, v.y, w.y, 0.0f,
                 u.z, v.z, w.z, 0.0f,
                 0.0f, 0.0f, 0.0f, 1.0f);
  }

  mat4
  GetProjectionMatrix() {
    float sy = 1 / tan(fov / 2);
    return mat4(
                sy / asp, 0.0f, 0.0f, 0.0f,
                0.0f, sy, 0.0f, 0.0f,
                0.0f, 0.0f, -(fp + bp) / (bp - fp), -1.0f,
                0.0f, 0.0f, -2 * fp * bp / (bp - fp), 0.0f);
  }
};

Camera camera;

class Object {
  Shader* shader;
  ShadowShader* shadowShader;
  Mesh *mesh;
  Object *parent;

  bool isAvatar;
  vec3 sunPos;
  vec3 scaling;

  vec3 orientation;
  vec3 rotationRate;

  vec3 velocity;
  vec3 acceleration;


  bool can_jump;


public:
  OBJECT_TYPE obj_type;
  vec3 position;

  Object(Mesh *m, vec3 position = vec3(0.0, 0.0, 0.0), vec3 scaling = vec3(1.0, 1.0, 1.0), vec3 orientation = vec3(0.0, 0.0, 0.0), vec3 rotationRate = vec3(0.0, 0.0, 0.0), Object* parent = nullptr, vec3 acceleration = vec3(0, 0, 0), OBJECT_TYPE obj_type = NONE, bool isAvatar = false) : position(position), scaling(scaling), orientation(orientation), rotationRate(rotationRate), parent(parent), acceleration(acceleration), obj_type(obj_type), isAvatar(isAvatar) {
    shader = m->GetShader();
    mesh = m;
    sunPos = vec3(0.0, 400.0, 200.0);
  }

  void
  setObjType(OBJECT_TYPE _obj_type) {
    obj_type = _obj_type;
  }

  void
  Draw() {
    shader->Run();
    UploadAttributes();

    if (isAvatar || (parent && parent->isAvatar)) {
      vec3 spotlightPos = camera.getEyePosition() + vec3(0, 2.0, 0);
      light.SetPointLightSource(spotlightPos);
    } else {
      light.SetDirectionalLightSource(sunPos);
    }
    light.UploadAttributes(shader);

    camera.UploadAttributes(shader);


    mesh->Draw();
  }

  void
  DrawShadow(Shader* shadowShader) {
    shadowShader->Run();

    UploadAttributes(shadowShader);


    light.SetDirectionalLightSource(sunPos);
    light.UploadAttributes(shadowShader);

    camera.UploadAttributes(shadowShader);

    mesh->Draw();
  }

  void
  UploadAttributes() {

    mat4 T = mat4(
                  1.0, 0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  position.x, position.y, position.z, 1.0);

    mat4 InvT = mat4(
                     1.0, 0.0, 0.0, 0.0,
                     0.0, 1.0, 0.0, 0.0,
                     0.0, 0.0, 1.0, 0.0,
                     -position.x, -position.y, -position.z, 1.0);

    mat4 S = mat4(
                  scaling.x, 0.0, 0.0, 0.0,
                  0.0, scaling.y, 0.0, 0.0,
                  0.0, 0.0, scaling.z, 0.0,
                  0.0, 0.0, 0.0, 1.0);

    mat4 InvS = mat4(
                     1.0 / scaling.x, 0.0, 0.0, 0.0,
                     0.0, 1.0 / scaling.y, 0.0, 0.0,
                     0.0, 0.0, 1.0 / scaling.z, 0.0,
                     0.0, 0.0, 0.0, 1.0);

    mat4 R[3];
    mat4 InvR[3];

    float alpha = orientation.x / 180.0 * M_PI;
    R[0] = mat4(
                1.0, 0.0, 0.0, 0.0,
                0.0, cos(alpha), sin(alpha), 0.0,
                0.0, -sin(alpha), cos(alpha), 0.0,
                0.0, 0.0, 0.0, 1.0);

    InvR[0] = mat4(
                   1.0, 0.0, 0.0, 0.0,
                   0.0, cos(alpha), -sin(alpha), 0.0,
                   0.0, sin(alpha), cos(alpha), 0.0,
                   0.0, 0.0, 0.0, 1.0);

    alpha = orientation.y / 180.0 * M_PI;
    R[1] = mat4(
                cos(alpha), 0.0, sin(alpha), 0.0,
                0.0, 1.0, 0.0, 0.0,
                -sin(alpha), 0.0, cos(alpha), 0.0,
                0.0, 0.0, 0.0, 1.0);

    InvR[1] = mat4(
                   cos(alpha), 0.0, -sin(alpha), 0.0,
                   0.0, 1.0, 0.0, 0.0,
                   sin(alpha), 0.0, cos(alpha), 0.0,
                   0.0, 0.0, 0.0, 1.0);

    alpha = orientation.z / 180.0 * M_PI;
    R[2] = mat4(
                cos(alpha), sin(alpha), 0.0, 0.0,
                -sin(alpha), cos(alpha), 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);

    InvR[2] = mat4(
                   cos(alpha), -sin(alpha), 0.0, 0.0,
                   sin(alpha), cos(alpha), 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0,
                   0.0, 0.0, 0.0, 1.0);

    mat4 M = S * R[0] * R[1] * R[2] * T;
    mat4 InvM = InvT * InvR[2] * InvR[1] * InvR[0] * InvS;


    if (parent) {
      T = mat4(
               1.0, 0.0, 0.0, 0.0,
               0.0, 1.0, 0.0, 0.0,
               0.0, 0.0, 1.0, 0.0,
               parent->position.x, parent->position.y, parent->position.z, 1.0);

      InvT = mat4(
                  1.0, 0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  -parent->position.x, -parent->position.y, -parent->position.z, 1.0);

      S = mat4(
               parent->scaling.x, 0.0, 0.0, 0.0,
               0.0, parent->scaling.y, 0.0, 0.0,
               0.0, 0.0, parent->scaling.z, 0.0,
               0.0, 0.0, 0.0, 1.0);

      InvS = mat4(
                  1.0 / parent->scaling.x, 0.0, 0.0, 0.0,
                  0.0, 1.0 / parent->scaling.y, 0.0, 0.0,
                  0.0, 0.0, 1.0 / parent->scaling.z, 0.0,
                  0.0, 0.0, 0.0, 1.0);


      alpha = parent->orientation.x / 180.0 * M_PI;
      R[0] = mat4(
                  1.0, 0.0, 0.0, 0.0,
                  0.0, cos(alpha), sin(alpha), 0.0,
                  0.0, -sin(alpha), cos(alpha), 0.0,
                  0.0, 0.0, 0.0, 1.0);

      InvR[0] = mat4(
                     1.0, 0.0, 0.0, 0.0,
                     0.0, cos(alpha), -sin(alpha), 0.0,
                     0.0, sin(alpha), cos(alpha), 0.0,
                     0.0, 0.0, 0.0, 1.0);

      alpha = parent->orientation.y / 180.0 * M_PI;
      R[1] = mat4(
                  cos(alpha), 0.0, sin(alpha), 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  -sin(alpha), 0.0, cos(alpha), 0.0,
                  0.0, 0.0, 0.0, 1.0);

      InvR[1] = mat4(
                     cos(alpha), 0.0, -sin(alpha), 0.0,
                     0.0, 1.0, 0.0, 0.0,
                     sin(alpha), 0.0, cos(alpha), 0.0,
                     0.0, 0.0, 0.0, 1.0);

      alpha = parent->orientation.z / 180.0 * M_PI;
      R[2] = mat4(
                  cos(alpha), sin(alpha), 0.0, 0.0,
                  -sin(alpha), cos(alpha), 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  0.0, 0.0, 0.0, 1.0);

      InvR[2] = mat4(
                     cos(alpha), -sin(alpha), 0.0, 0.0,
                     sin(alpha), cos(alpha), 0.0, 0.0,
                     0.0, 0.0, 1.0, 0.0,
                     0.0, 0.0, 0.0, 1.0);

      mat4 parentM = S * R[0] * R[1] * R[2] * T;
      mat4 parentInvM = InvT * InvR[2] * InvR[1] * InvR[0] * InvS;

      M = M * parentM;
      InvM = parentInvM * InvM;
    }



    mat4 MVP = M * camera.GetViewMatrix() * camera.GetProjectionMatrix();

    shader->UploadInvM(InvM);
    shader->UploadMVP(MVP);
    shader->UploadM(M);
  }

  void
  UploadAttributes(Shader* shadowShader) {
    mat4 T = mat4(
                  1.0, 0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  position.x, position.y, position.z, 1.0);

    mat4 S = mat4(
                  scaling.x, 0.0, 0.0, 0.0,
                  0.0, scaling.y, 0.0, 0.0,
                  0.0, 0.0, scaling.z, 0.0,
                  0.0, 0.0, 0.0, 1.0);

    mat4 R[3];

    float alpha = orientation.x / 180.0 * M_PI;
    R[0] = mat4(
                1.0, 0.0, 0.0, 0.0,
                0.0, cos(alpha), sin(alpha), 0.0,
                0.0, -sin(alpha), cos(alpha), 0.0,
                0.0, 0.0, 0.0, 1.0);


    alpha = orientation.y / 180.0 * M_PI;
    R[1] = mat4(
                cos(alpha), 0.0, sin(alpha), 0.0,
                0.0, 1.0, 0.0, 0.0,
                -sin(alpha), 0.0, cos(alpha), 0.0,
                0.0, 0.0, 0.0, 1.0);

    alpha = orientation.z / 180.0 * M_PI;
    R[2] = mat4(
                cos(alpha), sin(alpha), 0.0, 0.0,
                -sin(alpha), cos(alpha), 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);


    mat4 M = S * R[0] * R[1] * R[2] * T;
    mat4 V = camera.GetViewMatrix();
    mat4 P = camera.GetProjectionMatrix();

    if (parent) {
      T = mat4(
               1.0, 0.0, 0.0, 0.0,
               0.0, 1.0, 0.0, 0.0,
               0.0, 0.0, 1.0, 0.0,
               parent->position.x, parent->position.y, parent->position.z, 1.0);

      S = mat4(
               parent->scaling.x, 0.0, 0.0, 0.0,
               0.0, parent->scaling.y, 0.0, 0.0,
               0.0, 0.0, parent->scaling.z, 0.0,
               0.0, 0.0, 0.0, 1.0);


      alpha = parent->orientation.x / 180.0 * M_PI;
      R[0] = mat4(
                  1.0, 0.0, 0.0, 0.0,
                  0.0, cos(alpha), sin(alpha), 0.0,
                  0.0, -sin(alpha), cos(alpha), 0.0,
                  0.0, 0.0, 0.0, 1.0);

      alpha = parent->orientation.y / 180.0 * M_PI;
      R[1] = mat4(
                  cos(alpha), 0.0, sin(alpha), 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  -sin(alpha), 0.0, cos(alpha), 0.0,
                  0.0, 0.0, 0.0, 1.0);

      alpha = parent->orientation.z / 180.0 * M_PI;
      R[2] = mat4(
                  cos(alpha), sin(alpha), 0.0, 0.0,
                  -sin(alpha), cos(alpha), 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  0.0, 0.0, 0.0, 1.0);

      mat4 parentM = S * R[0] * R[1] * R[2] * T;

      M = M * parentM;
    }

    mat4 VP = V*P;

    shadowShader->UploadM(M);
    shadowShader->UploadVP(VP);
  }

  void
  Control(double dt) {
    if (isAvatar) {
      if (arrowKeyState[0]) {
        velocity.x += -0.3 * dt;
      } else if (arrowKeyState[2]) {
        velocity.x += 0.3 * dt;
      } else if (arrowKeyState[1]) {
        velocity.z += -0.3 * dt;
      } else if (arrowKeyState[3]) {
        velocity.z += 0.3 * dt;
      }
      else if (keyboardState[32] && can_jump) {
        velocity.y += 3 * dt;
      } else {
        velocity.x = velocity.x * .92;
        velocity.y = velocity.y * .99;
        velocity.z = velocity.z * .92;
      }
      velocity = velocity + acceleration*dt;
    }

    if (obj_type == OBSTACLE) {
      if (velocity.z == 0) {
        double speed = (rand() % 4) / 20.0 + 1 / 20;
        velocity.z = speed;
      }
      if (position.z >= 3) {
        double start_z = -6 - rand() % 14;
        position.z = start_z;

        double speed = (rand() % 4) / 40.0 + 1 / 20;
        velocity.z = speed;
      }
    }

  }

  void
  Move() {
    position = position + velocity;
    if (obj_type == TIGGER) {
      if (fabs(position.x) > 2) {
        velocity.x = velocity.x * (-0.5);
      }
      if (fabs(position.z + 2) > 2) {
        velocity.z = velocity.z * (-0.5);
      }
    }


    if (rotationRate.x != 0) {
      orientation.x += rotationRate.x;
      while (orientation.x >= 360) {
        orientation.x -= 360;
      }
    }
    if (rotationRate.y != 0) {
      orientation.y += rotationRate.y;
      while (orientation.y >= 360) {
        orientation.y -= 360;
      }
    }
    if (rotationRate.z != 0) {
      orientation.z += rotationRate.z;
      while (orientation.z >= 360) {
        orientation.z -= 360;
      }
    }
  }

  void
  Interact(Object* object) {
    switch (obj_type) {
      case TIGGER:
        switch (object->obj_type) {
          case GROUND:
            if (position.y <= object->position.y) {
              velocity.y = velocity.y * (-1);
              position.y = object->position.y;
            }
            can_jump = position.y <= object->position.y + 0.2;

            break;
          case OBSTACLE:
            if (position.y <= 0.4 && fabs(position.x - object->position.x) <= 0.5 && fabs(position.z - object->position.z) <= 0.8 && !invincible) {
              lives--;
              if (lives == 0) {
                game_over = true;
              } else {
                invincible = true;
              }
            }

          default:
            break;
        }
        break;

      default:
        break;
    }
  }
};

class Scene {
  MeshShader *meshShader;
  InfiniteMeshShader *infiniteMeshShader;
  ShadowShader *shadowShader;

  std::vector<Texture*> textures;
  std::vector<Material*> materials;
  std::vector<Geometry*> geometries;
  std::vector<Mesh*> meshes;
  std::vector<Object*> objects;

public:

  Scene() {
    meshShader = 0;
    infiniteMeshShader = 0;
    shadowShader = 0;
  }

  void
  Initialize() {
    meshShader = new MeshShader();
    infiniteMeshShader = new InfiniteMeshShader();
    shadowShader = new ShadowShader();

    vec3 ka = vec3(0.2, 0.2, 0.2);
    vec3 kd = vec3(0.6, 0.6, 0.6);
    vec3 ks = vec3(0.3, 0.3, 0.3);
    float shininess = 50.0;

    textures.push_back(new Texture("Meshes/tigger.png"));
    textures.push_back(new Texture("Meshes/chevy/chevy.png"));
    textures.push_back(new Texture("Meshes/chevy/chevy.png"));
    textures.push_back(new Texture("Meshes/heart/red1.png"));
    textures.push_back(new Texture("Meshes/rainbow.png"));



    for (uint i = 0; i < textures.size() - 1; i++) {
      materials.push_back(new Material(meshShader, ka, kd, ks, shininess, textures[i]));
    }

    materials.push_back(new Material(infiniteMeshShader, ka, kd, ks, shininess, textures[textures.size() - 1]));

    geometries.push_back(new PolygonalMesh("Meshes/tigger.obj"));
    geometries.push_back(new PolygonalMesh("Meshes/chevy/chevy.obj"));
    geometries.push_back(new PolygonalMesh("Meshes/chevy/wheel.obj"));
    geometries.push_back(new PolygonalMesh("Meshes/heart/heart.obj"));
    geometries.push_back(new InfiniteTexturedQuad());

    for (uint i = 0; i < geometries.size() - 1 && i < materials.size() - 1; i++) {
      meshes.push_back(new Mesh(geometries[i], materials[i]));
    }
    meshes.push_back(new Mesh(geometries[geometries.size() - 1], materials[materials.size() - 1]));


    Object* tigger = new Object(meshes[0], vec3(0.0, 0.0, -2.0), vec3(0.04, 0.04, 0.04), vec3(0, 90.0, 0), vec3(0.0, 0.0, 0.0), nullptr, vec3(0, -.2, 0), TIGGER, true);
    objects.push_back(tigger);


    for (int i = 0; i < 4; i++) {
      Object* chevy = new Object(meshes[1], vec3(-1.5 + i, -0.7, 10), vec3(0.04, 0.04, 0.04), vec3(0.0, 0.0, 0), vec3(0.0, 0.0, 0.0), nullptr, false);
      chevy->setObjType(OBSTACLE);
      objects.push_back(chevy);

      vec3 xOffset = vec3(6.5, 0.0, 0.0);
      vec3 yOffset = vec3(0.0, 4.0, 0.0);
      vec3 zOffset = vec3(0.0, 0.0, 11.15);
      Object* wheel = new Object(meshes[2], xOffset - yOffset + zOffset * (1.25), vec3(1, 1, 1), vec3(0, 0, 0), vec3(3.0, 0.0, 0.0), chevy);
      objects.push_back(wheel);
      wheel = new Object(meshes[2], xOffset * (-1) - yOffset + zOffset * (1.25), vec3(1, 1, 1), vec3(0, 0, 0), vec3(3.0, 0.0, 0.0), chevy);
      objects.push_back(wheel);
      wheel = new Object(meshes[2], xOffset - yOffset - zOffset, vec3(1, 1, 1), vec3(0, 0, 0), vec3(3.0, 0.0, 0.0), chevy);
      objects.push_back(wheel);
      wheel = new Object(meshes[2], xOffset * (-1) - yOffset - zOffset, vec3(1, 1, 1), vec3(0, 0, 0), vec3(3.0, 0.0, 0.0), chevy);
      objects.push_back(wheel);
    }

    Object* heart = new Object(meshes[3], vec3(0, 2, -3), vec3(.02, .02, .02), vec3(-90, 0, 0));
    heart->setObjType(HEART);
    objects.push_back(heart);


    //ground always must be last object so it's shadow isn't drawn
    Object* ground = new Object(meshes[meshes.size() - 1], vec3(0.0, -1.0, 0));
    ground->setObjType(GROUND);
    objects.push_back(ground);
  }

  ~Scene() {
    for (Texture *texture : textures) delete texture;
    for (Material *material : materials) delete material;
    for (Geometry *geometry : geometries) delete geometry;
    for (Mesh *mesh : meshes) delete mesh;
    for (Object *object : objects) delete object;

    if (meshShader) delete meshShader;
  }

void
  Draw() {
    for (Object *object : objects) {
      switch (object->obj_type) {
        case HEART:
          for (int j = 0; j < lives; j++) {
            object->position.x = -2.3 + 0.5 * j;
            object->Draw();
          }
          break;

        case TIGGER:
          if (!invincible || visible <= 3 || game_over) {
            object->Draw();
            object->DrawShadow(shadowShader);
          }
          break;

        case GROUND:
          object->Draw();
          break;

        default:
          if (!game_over) {
            object->Draw();
            object->DrawShadow(shadowShader);
          }
          break;
      }
    }
  }

  void
  Interact() {
    for (uint i = 0; i < objects.size(); i++) {
      for (uint j = 0; j < objects.size(); j++) {
        if (i != j) {
          objects[i]->Interact(objects[j]);
        }
      }
    }
  }

  void
  Control(double dt) {
    for (Object *object : objects) object->Control(dt);
  }

void
  Move() {
    for (Object *object : objects) object->Move();
  }
};

Scene scene;

void
onInitialization() {
  glViewport(0, 0, windowWidth, windowHeight);

  scene.Initialize();
}

void
onExit() {
  printf("exit");
}

void
onDisplay() {

  glClearColor(0, 0, 1.0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  scene.Draw();

  glutSwapBuffers();

}

void
onArrowKey(int key, int x, int y) {
  arrowKeyState[key - 100] = true;
}

void
onArrowKeyUp(int key, int x, int y) {
  arrowKeyState[key - 100] = false;
}

void
onKeyboard(unsigned char key, int x, int y) {
  keyboardState[key] = true;
}

void
onKeyboardUp(unsigned char key, int x, int y) {
  keyboardState[key] = false;
}

void
onReshape(int winWidth, int winHeight) {
  camera.SetAspectRatio((float) winWidth / winHeight);
  glViewport(0, 0, winWidth, winHeight);
}

void
onIdle() {
  double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
  static double lastTime = 0.0;
  double dt = t - lastTime;
  lastTime = t;

  if (!game_over) {
    score = int(t);
  } else if (score > best_score) {
    std::string data;
    std::ofstream myfile("best_score.txt");
    myfile << std::to_string(score);
    myfile.close();
  }

  camera.Control(dt);
  camera.Move();

  static double invicible_start_t = 0.0;
  if (t - invicible_start_t > 3.0) {
    invincible = false;
  }
  if (invincible) {
    visible = (visible + 1) % 5;
  }
  if (!invincible) {
    invicible_start_t = t;
  }


  scene.Interact();
  scene.Control(dt);
  scene.Move();

  std::string title = "SCORE: " + std::to_string(score) + ", BEST SCORE: " + std::to_string(best_score);
  glutSetWindowTitle(title.c_str());
  glutPostRedisplay();
}

int
main(int argc, char * argv[]) {
  std::string data;
  std::ifstream myfile("best_score.txt");
  myfile >> data;
  best_score = stoi(data);
  myfile.close();


  invincible = true;
  lives = 6;
  srand(time(NULL));
  glutInit(&argc, argv);
#if !defined(__APPLE__)
  glutInitContextVersion(majorVersion, minorVersion);
#endif
  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(50, 50);
#if defined(__APPLE__)
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);
#else
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
  glutCreateWindow("3D Mesh Rendering");

#if !defined(__APPLE__)
  glewExperimental = true;
  glewInit();
#endif
  printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
  printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
  printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
  printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
  printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

  onInitialization();

  glutDisplayFunc(onDisplay);
  glutIdleFunc(onIdle);
  glutKeyboardFunc(onKeyboard);
  glutKeyboardUpFunc(onKeyboardUp);
  glutReshapeFunc(onReshape);
  glutSpecialFunc(onArrowKey);
  glutSpecialUpFunc(onArrowKeyUp);

  glutMainLoop();
  onExit();
  return 1;
}
