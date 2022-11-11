#include <cmath>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <set>
#include <time.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// ディレクトリの設定ファイル
#include "common.h"

static int WIN_WIDTH   = 500;                       // ウィンドウの幅
static int WIN_HEIGHT  = 500;                       // ウィンドウの高さ
static const char *WIN_TITLE = "Rubik's Cube";     // ウィンドウのタイトル

static const double PI = 4.0 * std::atan(1.0);

// シェーダファイル
static std::string VERT_SHADER_FILE = std::string(SHADER_DIRECTORY) + "render.vert";
static std::string FRAG_SHADER_FILE = std::string(SHADER_DIRECTORY) + "render.frag";

// 頂点オブジェクト
struct Vertex {
    Vertex(const glm::vec3 &position_, const glm::vec3 &color_, const glm::vec3 &normal_)
        : position(position_)
        , color(color_)
        , normal(normal_) {
    }

    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
};

static const glm::vec3 positions[8] = {
    glm::vec3(-1.0f, -1.0f, -1.0f),
    glm::vec3( 1.0f, -1.0f, -1.0f),
    glm::vec3(-1.0f,  1.0f, -1.0f),
    glm::vec3(-1.0f, -1.0f,  1.0f),
    glm::vec3( 1.0f,  1.0f, -1.0f),
    glm::vec3(-1.0f,  1.0f,  1.0f),
    glm::vec3( 1.0f, -1.0f,  1.0f),
    glm::vec3( 1.0f,  1.0f,  1.0f)
};

static const glm::vec3 colors[6] = {
    glm::vec3(235/255.0f, 65/255.0f, 38/255.0f),  // 赤
    glm::vec3(255/255.0f, 255/255.0f, 255/255.0f),  // 白
    glm::vec3(75/255.0f, 164/255.0f, 47/255.0f),  // 緑
    glm::vec3(35/255.0f, 103/255.0f, 246/255.0f),  // 青
    glm::vec3(254/255.0f, 244/255.0f, 86/255.0f),  // 黄
    glm::vec3(236/255.0f, 151/255.0f, 63/255.0f),  // 橙
};

static const unsigned int faces[12][3] = {
    { 1, 6, 7 }, { 1, 7, 4 },
    { 2, 5, 7 }, { 2, 7, 4 },
    { 3, 5, 7 }, { 3, 7, 6 },
    { 0, 1, 4 }, { 0, 4, 2 },
    { 0, 1, 6 }, { 0, 6, 3 },
    { 0, 2, 5 }, { 0, 5, 3 }
};

static const glm::vec3 normals[8] = {
    glm::vec3(-1.0f, -1.0f, -1.0f),
    glm::vec3( 1.0f, -1.0f, -1.0f),
    glm::vec3(-1.0f,  1.0f, -1.0f),
    glm::vec3(-1.0f, -1.0f,  1.0f),
    glm::vec3( 1.0f,  1.0f, -1.0f),
    glm::vec3(-1.0f,  1.0f,  1.0f),
    glm::vec3( 1.0f, -1.0f,  1.0f),
    glm::vec3( 1.0f,  1.0f,  1.0f)
};

// バッファを参照する番号
GLuint vaoId;
GLuint vertexBufferId;
GLuint indexBufferId;

// シェーダを参照する番号
GLuint programId;

// シェーディングのための情報
// Gold (参照: http://www.barradeau.com/nicoptere/dump/materials.html)
static const glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
static const glm::vec3 diffColor = glm::vec3(0.75164f, 0.60648f, 0.22648f);
static const glm::vec3 specColor = glm::vec3(0.628281f, 0.555802f, 0.366065f);
static const glm::vec3 ambiColor = glm::vec3(0.24725f, 0.1995f, 0.0745f);
static const float shininess = 51.2f;


// Arcballコントロールのための変数
bool isDragging = false;

enum ArcballMode {
    ARCBALL_MODE_NONE = 0x00,
    ARCBALL_MODE_TRANSLATE = 0x01,
    ARCBALL_MODE_ROTATE = 0x02,
    ARCBALL_MODE_SCALE = 0x04
};

int arcballMode = ARCBALL_MODE_NONE;
glm::mat4 modelMat, viewMat, projMat;
glm::mat4 acRotMat, acTransMat, acScaleMat;
glm::vec3 gravity;

int N = 3;
int outColorMode = 0;
int mode = 0;

/*
Cube
cubeType: コーナーキューブ、エッジキューブ、フェイスキューブ（センターキューブ）の種類を表す。
position: キューブの位置。現在は回転によって更新はせず、キューブの初期位置として利用している。
rotMat: キューブにかけられた回転行列。これによってキューブを回転させている。
transMat: キューブの位置までの並進行列。現在は回転によって更新はせず、キューブの初期位置までの並進行列として利用している。
pn: キューブの位置。0~N-1までの値を持つベクトル。
*/
struct Cube {
    Cube(const int &cubeType_, const glm::vec3 &position_, const glm::mat4 &rotMat_)
        : cubeType(cubeType_)
        , position(position_)
        , rotMat(rotMat_)
        , transMat(glm::translate(glm::mat4(1.0), position_)) {
    }
    int cubeType;
    glm::vec3 position;
    glm::mat4 rotMat;
    glm::mat4 transMat;
    std::vector<int> pn;
};
std::vector<Cube> Cubes;

/*
CubePlane
一回の操作で同時に動くキューブの集合を格納する。回転面というのが分かりやすい。
nv: 回転面の法線ベクトル
cubeIds: キューブのインデックスの集合。Cubesのインデックスが格納される。
*/
struct CubePlane {
    CubePlane(const glm::vec3 &nv_)
        : nv(nv_) {
    }
    glm::vec3 nv;
    std::set<int> cubeIds;
};

std::vector<CubePlane> xCubePlanes;
std::vector<CubePlane> yCubePlanes;
std::vector<CubePlane> zCubePlanes;

static const glm::vec3 cCubePositions[8] = {
    glm::vec3(-1.0f,  1.0f, -1.0f),
    glm::vec3( 1.0f,  1.0f, -1.0f),
    glm::vec3( 1.0f,  1.0f,  1.0f),
    glm::vec3(-1.0f,  1.0f,  1.0f),
    glm::vec3(-1.0f, -1.0f, -1.0f),
    glm::vec3( 1.0f, -1.0f, -1.0f),
    glm::vec3( 1.0f, -1.0f,  1.0f),
    glm::vec3(-1.0f, -1.0f,  1.0f)
};

static const unsigned int cubeEdgeCorners[12][2] = {
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 },
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 }
};

static const glm::vec3 eCubePositions[12] = {
    glm::vec3(-1.0f,  0.0f, -1.0f),
    glm::vec3( 1.0f,  0.0f, -1.0f),
    glm::vec3( 1.0f,  0.0f,  1.0f),
    glm::vec3(-1.0f,  0.0f,  1.0f),
    glm::vec3( 0.0f,  1.0f, -1.0f),
    glm::vec3( 1.0f,  1.0f,  0.0f),
    glm::vec3( 0.0f,  1.0f,  1.0f),
    glm::vec3(-1.0f,  1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f, -1.0f),
    glm::vec3( 1.0f, -1.0f,  0.0f),
    glm::vec3( 0.0f, -1.0f,  1.0f),
    glm::vec3(-1.0f, -1.0f,  0.0f)
};

static const unsigned int cubeFaceEdges[6][4] = {
    { 2, 10, 3, 6 }, // F
    { 1, 9, 2, 5 }, // R
    { 8, 9, 10, 11 }, // D
    { 3, 11, 0, 7 }, // L
    { 4, 5, 6, 7 }, // U
    { 0, 8, 1, 4 }  // B
};

static const glm::vec3 fCubePositions[6] = {
    glm::vec3( 0.0f,  0.0f,  1.0f), // F
    glm::vec3( 1.0f,  0.0f,  0.0f), // R
    glm::vec3( 0.0f, -1.0f,  0.0f), // D
    glm::vec3(-1.0f,  0.0f,  0.0f), // L
    glm::vec3( 0.0f,  1.0f,  0.0f), // U
    glm::vec3( 0.0f,  0.0f, -1.0f)  // B
};


std::vector<int> cubeIds2vao;

void initCube(int N) {

    for (int i = 0; i < N; i++) {
        CubePlane plane(glm::vec3(1.0f, 0.0f, 0.0f));
        xCubePlanes.push_back(plane);
    }
    for (int i = 0; i < N; i++) {
        CubePlane plane(glm::vec3(0.0f, 1.0f, 0.0f));
        yCubePlanes.push_back(plane);
    }
    for (int i = 0; i < N; i++) {
        CubePlane plane(glm::vec3(0.0f, 0.0f, 1.0f));
        zCubePlanes.push_back(plane);
    }

    if (N == 1) {
        glm::vec3 p = glm::vec3(N-1);
        Cube c(1, p, glm::mat4(1.0));
        Cubes.push_back(c);
        
    } else {

        // corner
        for (int i = 0; i < 8; i++) {
            glm::vec3 p = cCubePositions[i] * glm::vec3(N-1);
            Cube c(1, p, glm::mat4(1.0));
            Cubes.push_back(c);
        }

        // edge
        for (int i = 0; i < 12; i++) {
            glm::vec3 dir = (cCubePositions[cubeEdgeCorners[i][0]] - cCubePositions[cubeEdgeCorners[i][1]]) * glm::vec3(0.5f);
            for (int j = 0; j < N-2; j++) {
                glm::vec3 p = eCubePositions[i] * glm::vec3(N-1) + dir * glm::vec3(2*j-(N-3));
                Cube c(2, p, glm::mat4(1.0));
                Cubes.push_back(c);
            }
        }

        // ボイドキューブの場合はフェイスキューブは作成しない
        if (mode != 2) {
            // face
            for (int i = 0; i < 6; i++) {
                glm::vec3 dir1 = (eCubePositions[cubeFaceEdges[i][0]] - eCubePositions[cubeFaceEdges[i][2]]) * glm::vec3(0.5f);
                glm::vec3 dir2 = (eCubePositions[cubeFaceEdges[i][1]] - eCubePositions[cubeFaceEdges[i][3]]) * glm::vec3(0.5f);
                for (int j = 0; j < N-2; j++) {
                    glm::vec3 pj = fCubePositions[i] * glm::vec3(N-1) + dir1 * glm::vec3(2*j-(N-3));
                    for (int k = 0; k < N-2; k++) {
                        glm::vec3 p = pj + dir2 * glm::vec3(2*k-(N-3));
                        Cube c(3, p, glm::mat4(1.0));
                        Cubes.push_back(c);
                    }
                }
            }
        }
    }

    for (int i = 0; i < Cubes.size(); i++) {
        glm::vec3 pn = (Cubes[i].position + glm::vec3(N-1)) * glm::vec3(0.5f);
        int xn = (int)(pn.x);
        int yn = (int)(pn.y);
        int zn = (int)(pn.z);
        xCubePlanes[xn].cubeIds.insert(i);
        yCubePlanes[yn].cubeIds.insert(i);
        zCubePlanes[zn].cubeIds.insert(i);
        Cubes[i].pn = {xn, yn, zn};
    }

    for (int i = 0; i < Cubes.size(); i++) {
        int x = Cubes[i].pn[0];
        int y = Cubes[i].pn[1];
        int z = Cubes[i].pn[2];

        if (x < N-1 && x > 0) x = 1;
        if (y < N-1 && y > 0) y = 1;
        if (z < N-1 && z > 0) z = 1;
        if (x == N-1) x = 2;
        if (y == N-1) y = 2;
        if (z == N-1) z = 2;

        if (N > 1) {
            cubeIds2vao.push_back(9*x + 3*y + z);
        } else {
            cubeIds2vao.push_back(0);
        }
    }
}


float acScale = 1.0f;
glm::ivec2 oldPos;
glm::ivec2 newPos;

// オブジェクトを選択するためのID
bool selectMode = false;
Cube *selectedObj = &Cubes[0];

// VAOの初期化
void initVAO() {
    // Vertex配列の作成
    // ここら辺のコード冗長的になっちゃってます。
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    int idx = 0;
    gravity = glm::vec3(0.0f, 0.0f, 0.0f);

    if (N == 1) {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                Vertex v(positions[faces[i * 2 + 0][j]], colors[i], glm::normalize(normals[faces[i * 2 + 0][j]]));
                vertices.push_back(v);
                indices.push_back(idx++);
                gravity += v.position;
            }
            
            for (int j = 0; j < 3; j++) {
                Vertex v(positions[faces[i * 2 + 1][j]], colors[i], glm::normalize(normals[faces[i * 2 + 1][j]]));
                vertices.push_back(v);
                indices.push_back(idx++);
                gravity += v.position;
            }
        }
    } else {

        // 通常のルービックキューブ及びボイドキューブの場合
        if (mode == 0 || mode == 2) {
            for (float x = -1; x < 2; x++) {
                for (float y = -1; y < 2; y++) {
                    for (float z = -1; z < 2; z++) {
                        for (int i = 0; i < 6; i++) {
                            for (int j = 0; j < 3; j++) {
                                if ((x == -1 && i == 5)||(y == -1 && i == 4)||(z == -1 && i == 3)||(z == 1 && i == 2)||(y == 1 && i == 1)||(x == 1 && i == 0)) {
                                    Vertex v(positions[faces[i * 2 + 0][j]], colors[i], glm::normalize(normals[faces[i * 2 + 0][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                } else {
                                    Vertex v(positions[faces[i * 2 + 0][j]], glm::vec3( 0.0f,  0.0f,  0.0f), glm::normalize(normals[faces[i * 2 + 0][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                }
                                

                            }

                            for (int j = 0; j < 3; j++) {

                                if ((x == -1 && i == 5)||(y == -1 && i == 4)||(z == -1 && i == 3)||(z == 1 && i == 2)||(y == 1 && i == 1)||(x == 1 && i == 0)) {
                                    Vertex v(positions[faces[i * 2 + 1][j]], colors[i], glm::normalize(normals[faces[i * 2 + 1][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                } else {
                                    Vertex v(positions[faces[i * 2 + 1][j]], glm::vec3( 0.0f,  0.0f,  0.0f), glm::normalize(normals[faces[i * 2 + 1][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                }
                            }
                        }
                    }
                }
            }

        // ミラーブロックスの場合
        } else if (mode == 1) {
            for (float x = -1; x < 2; x++) {
                for (float y = -1; y < 2; y++) {
                    for (float z = -1; z < 2; z++) {
                        for (int i = 0; i < 6; i++) {
                            for (int j = 0; j < 3; j++) {

                                glm::vec3 d = glm::vec3( 0.0f,  0.0f,  0.0f);

                                if ((int)positions[faces[i * 2 + 0][j]].x == x) d += glm::vec3( 0.5f,  0.0f,  0.0f);
                                if ((int)positions[faces[i * 2 + 0][j]].y == y) d += glm::vec3( 0.0f,  0.1f,  0.0f);
                                if ((int)positions[faces[i * 2 + 0][j]].z == z) d += glm::vec3( 0.0f,  0.0f,  0.9f);
                                if ((x == -1 && i == 5)||(y == -1 && i == 4)||(z == -1 && i == 3)||(z == 1 && i == 2)||(y == 1 && i == 1)||(x == 1 && i == 0)) {
                                    Vertex v(positions[faces[i * 2 + 0][j]] + d, colors[i], glm::normalize(normals[faces[i * 2 + 0][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                } else {
                                    Vertex v(positions[faces[i * 2 + 0][j]] + d, glm::vec3( 0.0f,  0.0f,  0.0f), glm::normalize(normals[faces[i * 2 + 0][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                }
                            }

                            for (int j = 0; j < 3; j++) {

                                glm::vec3 d = glm::vec3( 0.0f,  0.0f,  0.0f);

                                if ((int)positions[faces[i * 2 + 1][j]].x == x) d += glm::vec3( 0.5f,  0.0f,  0.0f);
                                if ((int)positions[faces[i * 2 + 1][j]].y == y) d += glm::vec3( 0.0f,  0.1f,  0.0f);
                                if ((int)positions[faces[i * 2 + 1][j]].z == z) d += glm::vec3( 0.0f,  0.0f,  0.9f);

                                if ((x == -1 && i == 5)||(y == -1 && i == 4)||(z == -1 && i == 3)||(z == 1 && i == 2)||(y == 1 && i == 1)||(x == 1 && i == 0)) {
                                    Vertex v(positions[faces[i * 2 + 1][j]] + d, colors[i], glm::normalize(normals[faces[i * 2 + 1][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                } else {
                                    Vertex v(positions[faces[i * 2 + 1][j]] + d, glm::vec3( 0.0f,  0.0f,  0.0f), glm::normalize(normals[faces[i * 2 + 1][j]]));
                                    vertices.push_back(v);
                                    indices.push_back(idx++);
                                    gravity += v.position;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    gravity /= indices.size();

    // VAOの作成
    glGenVertexArrays(1, &vaoId);
    glBindVertexArray(vaoId);

    // 頂点バッファの作成
    glGenBuffers(1, &vertexBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    // 頂点バッファの有効化
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // 頂点番号バッファの作成
    glGenBuffers(1, &indexBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(),
                 indices.data(), GL_STATIC_DRAW);

    // VAOをOFFにしておく
    glBindVertexArray(0);
}

GLuint compileShader(const std::string &filename, GLuint type) {
    // シェーダの作成
    GLuint shaderId = glCreateShader(type);
    
    // ファイルの読み込み
    std::ifstream reader;
    size_t codeSize;
    std::string code;

    // ファイルを開く
    reader.open(filename.c_str(), std::ios::in);
    if (!reader.is_open()) {
        // ファイルを開けなかったらエラーを出して終了
        fprintf(stderr, "Failed to load a shader: %s\n", VERT_SHADER_FILE.c_str());
        exit(1);
    }

    // ファイルをすべて読んで変数に格納 (やや難)
    reader.seekg(0, std::ios::end);             // ファイル読み取り位置を終端に移動 
    codeSize = reader.tellg();                  // 現在の箇所(=終端)の位置がファイルサイズ
    code.resize(codeSize);                      // コードを格納する変数の大きさを設定
    reader.seekg(0);                            // ファイルの読み取り位置を先頭に移動
    reader.read(&code[0], codeSize);            // 先頭からファイルサイズ分を読んでコードの変数に格納

    // ファイルを閉じる
    reader.close();

    // コードのコンパイル
    const char *codeChars = code.c_str();
    glShaderSource(shaderId, 1, &codeChars, NULL);
    glCompileShader(shaderId);

    // コンパイルの成否を判定する
    GLint compileStatus;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_FALSE) {
        // コンパイルが失敗したらエラーメッセージとソースコードを表示して終了
        fprintf(stderr, "Failed to compile a shader!\n");

        // エラーメッセージの長さを取得する
        GLint logLength;
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            // エラーメッセージを取得する
            GLsizei length;
            std::string errMsg;
            errMsg.resize(logLength);
            glGetShaderInfoLog(shaderId, logLength, &length, &errMsg[0]);

            // エラーメッセージとソースコードの出力
            fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
            fprintf(stderr, "%s\n", code.c_str());
        }
        exit(1);
    }

    return shaderId;
}

GLuint buildShaderProgram(const std::string &vShaderFile, const std::string &fShaderFile) {
    // シェーダの作成
    GLuint vertShaderId = compileShader(vShaderFile, GL_VERTEX_SHADER);
    GLuint fragShaderId = compileShader(fShaderFile, GL_FRAGMENT_SHADER);
    
    // シェーダプログラムのリンク
    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertShaderId);
    glAttachShader(programId, fragShaderId);
    glLinkProgram(programId);
    
    // リンクの成否を判定する
    GLint linkState;
    glGetProgramiv(programId, GL_LINK_STATUS, &linkState);
    if (linkState == GL_FALSE) {
        // リンクに失敗したらエラーメッセージを表示して終了
        fprintf(stderr, "Failed to link shaders!\n");

        // エラーメッセージの長さを取得する
        GLint logLength;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            // エラーメッセージを取得する
            GLsizei length;
            std::string errMsg;
            errMsg.resize(logLength);
            glGetProgramInfoLog(programId, logLength, &length, &errMsg[0]);

            // エラーメッセージを出力する
            fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
        }
        exit(1);
    }
    
    // シェーダを無効化した後にIDを返す
    glUseProgram(0);
    return programId;
}

// シェーダの初期化
void initShaders() {
    programId = buildShaderProgram(VERT_SHADER_FILE, FRAG_SHADER_FILE);
}

// OpenGLの初期化関数
void initializeGL() {
    // 深度テストの有効化
    glEnable(GL_DEPTH_TEST);

    // 背景色の設定 (黒)
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    // キューブの初期化
    initCube(N);

    // VAOの初期化
    initVAO();

    // シェーダの用意
    initShaders();

    // カメラの初期化
    projMat = glm::perspective(45.0f, (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);

    viewMat = glm::lookAt(glm::vec3(3.0f*N*0.75, 4.0f*N*0.75, 5.0f*N*0.75),   // 視点の位置
                          glm::vec3(0.0f, 0.0f, 0.0f),   // 見ている先
                          glm::vec3(0.0f, 1.0f, 0.0f));  // 視界の上方向

    // その他の行列の初期化
    modelMat = glm::mat4(1.0);
    acRotMat = glm::mat4(1.0);
}

// OpenGLの描画関数
void paintGL() {
    // 背景色の描画
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // シェーダの有効化
    glUseProgram(programId);
        
    // VAOの有効化
    glBindVertexArray(vaoId);

    

    GLuint uid;
    uid = glGetUniformLocation(programId, "u_lightPos");
    glUniform3fv(uid, 1, glm::value_ptr(lightPos));
    uid = glGetUniformLocation(programId, "u_diffColor");
    glUniform3fv(uid, 1, glm::value_ptr(diffColor));
    uid = glGetUniformLocation(programId, "u_specColor");
    glUniform3fv(uid, 1, glm::value_ptr(specColor));
    uid = glGetUniformLocation(programId, "u_ambiColor");
    glUniform3fv(uid, 1, glm::value_ptr(ambiColor));
    uid = glGetUniformLocation(programId, "u_shininess");
    glUniform1f(uid, shininess);
    uid = glGetUniformLocation(programId, "u_outColorMode");
    glUniform1i(uid, outColorMode);

    // Cube
    for (int i = 0; i < Cubes.size(); i++) {
        glm::mat4 mvpMat = projMat * viewMat * modelMat * acRotMat * Cubes[i].rotMat * Cubes[i].transMat;

        glm::mat4 mvMat = viewMat * modelMat * acRotMat * Cubes[i].rotMat * Cubes[i].transMat;
        glm::mat4 normMat = glm::transpose(glm::inverse(mvMat));
        glm::mat4 lightMat = viewMat;

        // Uniform変数の転送
        GLuint uid;
        uid = glGetUniformLocation(programId, "u_mvpMat");
        glUniformMatrix4fv(uid, 1, GL_FALSE, glm::value_ptr(mvpMat));

        uid = glGetUniformLocation(programId, "u_mvMat");
        glUniformMatrix4fv(uid, 1, GL_FALSE, glm::value_ptr(mvMat));
        uid = glGetUniformLocation(programId, "u_normMat");
        glUniformMatrix4fv(uid, 1, GL_FALSE, glm::value_ptr(normMat));
        uid = glGetUniformLocation(programId, "u_lightMat");
        glUniformMatrix4fv(uid, 1, GL_FALSE, glm::value_ptr(lightMat));


        uid = glGetUniformLocation(programId, "u_cubeType");
        glUniform1i(uid, selectMode ? Cubes[i].cubeType : -1);
        uid = glGetUniformLocation(programId, "u_cubeID");
        glUniform1i(uid, selectMode ? i : -1);


        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(36 * sizeof(uint32_t) * cubeIds2vao[i]));
        //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    // VAOの無効化
    glBindVertexArray(0);

    // シェーダの無効化
    glUseProgram(0);
}

void resizeGL(GLFWwindow *window, int width, int height) {
    // ユーザ管理のウィンドウサイズを変更
    WIN_WIDTH = width;
    WIN_HEIGHT = height;
    
    // GLFW管理のウィンドウサイズを変更
    glfwSetWindowSize(window, WIN_WIDTH, WIN_HEIGHT);
    
    // 実際のウィンドウサイズ (ピクセル数) を取得
    int renderBufferWidth, renderBufferHeight;
    glfwGetFramebufferSize(window, &renderBufferWidth, &renderBufferHeight);
    
    // ビューポート変換の更新
    glViewport(0, 0, renderBufferWidth, renderBufferHeight);

    // 投影変換行列の初期化
    projMat = glm::perspective(45.0f, (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);
}


int pressKey = 0;
CubePlane* selectedCubePlane;
bool rotating = false;
int axis = 0;
bool rotateDir = true;

void updateCubePlane(int axis, bool dir, CubePlane* selectedCubePlane) {
    int count = 0;
    for (auto itr = selectedCubePlane->cubeIds.begin(); itr != selectedCubePlane->cubeIds.end(); ++itr) {
        count++;
        int xn = Cubes[*itr].pn[0];
        int yn = Cubes[*itr].pn[1];
        int zn = Cubes[*itr].pn[2];

        if (dir) {
            if (axis == 0) {
                yCubePlanes[yn].cubeIds.erase(*itr);
                zCubePlanes[zn].cubeIds.erase(*itr);
                yCubePlanes[N-1-zn].cubeIds.insert(*itr);
                zCubePlanes[yn].cubeIds.insert(*itr);
                Cubes[*itr].pn[1] = N-1-zn;
                Cubes[*itr].pn[2] = yn;
            } else if (axis == 1) {
                zCubePlanes[zn].cubeIds.erase(*itr);
                xCubePlanes[xn].cubeIds.erase(*itr);
                zCubePlanes[N-1-xn].cubeIds.insert(*itr);
                xCubePlanes[zn].cubeIds.insert(*itr);
                Cubes[*itr].pn[2] = N-1-xn;
                Cubes[*itr].pn[0] = zn;
            } else {
                xCubePlanes[xn].cubeIds.erase(*itr);
                yCubePlanes[yn].cubeIds.erase(*itr);
                xCubePlanes[N-1-yn].cubeIds.insert(*itr);
                yCubePlanes[xn].cubeIds.insert(*itr);
                Cubes[*itr].pn[0] = N-1-yn;
                Cubes[*itr].pn[1] = xn;
            }
        } else {
            if (axis == 0) {
                yCubePlanes[yn].cubeIds.erase(*itr);
                zCubePlanes[zn].cubeIds.erase(*itr);
                yCubePlanes[zn].cubeIds.insert(*itr);
                zCubePlanes[N-1-yn].cubeIds.insert(*itr);
                Cubes[*itr].pn[1] = zn;
                Cubes[*itr].pn[2] = N-1-yn;
            } else if (axis == 1) {
                zCubePlanes[zn].cubeIds.erase(*itr);
                xCubePlanes[xn].cubeIds.erase(*itr);
                zCubePlanes[xn].cubeIds.insert(*itr);
                xCubePlanes[N-1-zn].cubeIds.insert(*itr);
                Cubes[*itr].pn[2] = xn;
                Cubes[*itr].pn[0] = N-1-zn;
            } else {
                xCubePlanes[xn].cubeIds.erase(*itr);
                yCubePlanes[yn].cubeIds.erase(*itr);
                xCubePlanes[yn].cubeIds.insert(*itr);
                yCubePlanes[N-1-xn].cubeIds.insert(*itr);
                Cubes[*itr].pn[0] = yn;
                Cubes[*itr].pn[1] = N-1-xn;
            }
        }
    }
}

void rotateCubeByKey(int pressKey) {
    switch ((char)pressKey) {
        case 'R':
            selectedCubePlane = &xCubePlanes[N-1];
            rotating = true;
            axis = 0;
            rotateDir = true;
            break;

        case 'L':
            selectedCubePlane = &xCubePlanes[0];
            rotating = true;
            axis = 0;
            rotateDir = false;
            break;

        case 'U':
            selectedCubePlane = &yCubePlanes[N-1];
            rotating = true;
            axis = 1;
            rotateDir = true;
            break;

        case 'D':
            selectedCubePlane = &yCubePlanes[0];
            rotating = true;
            axis = 1;
            rotateDir = false;
            break;

        case 'F':
            selectedCubePlane = &zCubePlanes[N-1];
            rotating = true;
            axis = 2;
            rotateDir = true;
            break;

        case 'B':
            selectedCubePlane = &zCubePlanes[0];
            rotating = true;
            axis = 2;
            rotateDir = false;
            break;

        case 'M':
            if (mode == 2) break;
            selectedCubePlane = &xCubePlanes[N/2];
            rotating = true;
            axis = 0;
            rotateDir = true;
            break;
        
        case 'E':
            if (mode == 2) break;
            selectedCubePlane = &yCubePlanes[N/2];
            rotating = true;
            axis = 1;
            rotateDir = true;
            break;
        
        case 'S':
            if (mode == 2) break;
            selectedCubePlane = &zCubePlanes[N/2];
            rotating = true;
            axis = 2;
            rotateDir = false;
            break;

        default:
            break;
    }
}

void rotate(int axis, bool rotateDir, CubePlane* selectedCubePlane) {
    glm::vec3 dir = rotateDir ? glm::vec3(1) : glm::vec3(-1);
    for (auto itr = selectedCubePlane->cubeIds.begin(); itr != selectedCubePlane->cubeIds.end(); ++itr) {
        Cubes[*itr].rotMat = glm::rotate((float)(90.0f * PI / 180.0f), selectedCubePlane->nv * dir) * Cubes[*itr].rotMat;
    }
    updateCubePlane(axis, rotateDir, selectedCubePlane);
}

void shuffleCube() {
    for (int i = 0; i < 10; i++) {
        int axis = rand() % 3;
        bool rotateDir = rand() % 2;
        CubePlane* selectedCubePlane;
        int n = rand() % N;
        if (axis == 0) {
            selectedCubePlane = &xCubePlanes[n];
        } else if (axis == 1) {
            selectedCubePlane = &yCubePlanes[n];
        } else {
            selectedCubePlane = &zCubePlanes[n];
        }

        rotate(axis, rotateDir, selectedCubePlane);
    }
}

void resetCube() {
    for (int i = 0; i < N; i++) {
        xCubePlanes[i].cubeIds.clear();
        yCubePlanes[i].cubeIds.clear();
        zCubePlanes[i].cubeIds.clear();
    }

    for (int i = 0; i < Cubes.size(); i++) {
        Cubes[i].rotMat = glm::mat4(1.0);
        glm::vec3 pn = (Cubes[i].position + glm::vec3(N-1)) * glm::vec3(0.5f);
        int xn = (int)(pn.x);
        int yn = (int)(pn.y);
        int zn = (int)(pn.z);
        xCubePlanes[xn].cubeIds.insert(i);
        yCubePlanes[yn].cubeIds.insert(i);
        zCubePlanes[zn].cubeIds.insert(i);
        Cubes[i].pn = {xn, yn, zn};
    }
}

void changeColorMode() {
    outColorMode++;
    if (outColorMode > 2) outColorMode = 0;
}

void initData() {
    Cubes.clear();
    xCubePlanes.clear();
    yCubePlanes.clear();
    zCubePlanes.clear();
    cubeIds2vao.clear();
}

void changeMode() {
    mode++;
    if (mode > 2) mode = 0;

    initData();
    // キューブの初期化
    initCube(N);
    // VAOの初期化
    initVAO();
}

void changeN(int pressKey) {
    N = pressKey - 48;

    initData();

    initializeGL();
}

void keyboardEvent(GLFWwindow *window, int key, int scancode, int action, int mods) {
    // キーボードの状態と押されたキーを表示する
    printf("Keyboard: %s\n", action == GLFW_PRESS ? "Press" : action == GLFW_REPEAT ? "Repeat" : "Release");
    printf("Key: %c\n", (char)key);

    if(action == GLFW_PRESS) {
        pressKey = key;

        // 回転操作
        if (!rotating) {
            rotateCubeByKey(pressKey);
        }

        if ((char)pressKey == ' ') shuffleCube();

        if ((char)pressKey == 'Q') resetCube();

        if ((char)pressKey == 'C') changeColorMode();

        if ((char)pressKey == 'P') changeMode();

    } else if (action == GLFW_RELEASE) {
        pressKey = 0;
    }
    
    // 特殊キーが押されているかの判定
    int specialKeys[] = { GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT, GLFW_MOD_SUPER };
    const char *specialKeyNames[] = { "Shift", "Ctrl", "Alt", "Super" };
    
    printf("Special Keys: ");
    for (int i = 0; i < 4; i++) {
        if ((mods & specialKeys[i]) != 0) {
            printf("%s ", specialKeyNames[i]);

            if (i == 3 && pressKey >= 49 && pressKey <= 57) changeN(pressKey);
        }
    }
    printf("\n");
}

void mouseEvent(GLFWwindow *window, int button, int action, int mods) {
    // クリックしたボタンで処理を切り替える
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        arcballMode = ARCBALL_MODE_ROTATE;
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        arcballMode = ARCBALL_MODE_SCALE;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        arcballMode = ARCBALL_MODE_TRANSLATE;
    }
     
    // クリックされた位置を取得
    double px, py;
    glfwGetCursorPos(window, &px, &py);

    if (action == GLFW_PRESS) {
        if (!isDragging) {
            isDragging = true;
            oldPos = glm::ivec2(px, py);
            newPos = glm::ivec2(px, py);
        }
    } else {        
        isDragging = false;
        oldPos = glm::ivec2(0, 0);
        newPos = glm::ivec2(0, 0);
        arcballMode = ARCBALL_MODE_NONE;
    }

    if (action == GLFW_PRESS) {
        const int cx = (int)px;
        const int cy = (int)py;  

        // 選択モードでの描画
        selectMode = true;
        paintGL();
        selectMode = false;
        
        // ピクセルの大きさの計算 (Macの場合には必要)
        int renderBufferWidth, renderBufferHeight;
        glfwGetFramebufferSize(window, &renderBufferWidth, &renderBufferHeight);
        int pixelSize = std::max(renderBufferWidth / WIN_WIDTH, renderBufferHeight / WIN_HEIGHT);

        // より適切なやり方
        unsigned char byte[4];
        glReadPixels(cx * pixelSize, (WIN_HEIGHT - cy - 1) * pixelSize, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &byte);
        printf("Mouse position: %d %d\n", cx, cy);
        printf("Select cube type %d\n", (int)byte[0]);
        printf("Select cube id %d\n", (int)byte[1]);

        if ((int)byte[0] == 1) {
            selectedObj = &Cubes[(int)byte[1]];
        } else if ((int)byte[0] == 2) {
            selectedObj = &Cubes[(int)byte[1]];
        } else if ((int)byte[0] == 3) {
            selectedObj = &Cubes[(int)byte[1]];
        }
    }
}

// スクリーン上の位置をアークボール球上の位置に変換する関数
glm::vec3 getVector(double x, double y) {
    glm::vec3 pt( 2.0 * x / WIN_WIDTH  - 1.0,
                 -2.0 * y / WIN_HEIGHT + 1.0, 0.0);

    const double xySquared = pt.x * pt.x + pt.y * pt.y;
    if (xySquared <= 1.0) {
        // 単位円の内側ならz座標を計算
        pt.z = std::sqrt(1.0 - xySquared);
    } else {
        // 外側なら球の外枠上にあると考える
        pt = glm::normalize(pt);
    }

    return pt;
}

void updateRotate() {
    static const double Pi = 4.0 * std::atan(1.0);

    // スクリーン座標をアークボール球上の座標に変換
    const glm::vec3 u = glm::normalize(getVector(newPos.x, newPos.y));
    const glm::vec3 v = glm::normalize(getVector(oldPos.x, oldPos.y));

    // カメラ座標における回転量 (=オブジェクト座標における回転量)
    const double angle = std::acos(std::max(-1.0f, std::min(glm::dot(u, v), 1.0f)));

    // カメラ空間における回転軸
    const glm::vec3 rotAxis = glm::cross(v, u);

    // カメラ座標の情報をワールド座標に変換する行列
    const glm::mat4 c2oMat = glm::inverse(viewMat * modelMat);

    // オブジェクト座標における回転軸
    const glm::vec3 rotAxisObjSpace = glm::vec3(c2oMat * glm::vec4(rotAxis, 0.0f));

    // 回転行列の更新
    if ((char)pressKey == 'W') {
        selectedObj->rotMat = glm::rotate((float)(4.0f * angle), rotAxisObjSpace) * selectedObj->rotMat;
    } else {
        acRotMat = glm::rotate((float)(4.0f * angle), rotAxisObjSpace) * acRotMat;
    }
}

void updateTranslate() {
    // オブジェクト重心のスクリーン座標を求める
    glm::vec4 gravityScreenSpace = (projMat * viewMat * modelMat) * glm::vec4(gravity.x, gravity.y, gravity.z, 1.0f);
    gravityScreenSpace /= gravityScreenSpace.w;

    // スクリーン座標系における移動量
    glm::vec4 newPosScreenSpace(2.0 * newPos.x / WIN_WIDTH, -2.0 * newPos.y / WIN_HEIGHT, gravityScreenSpace.z, 1.0f);
    glm::vec4 oldPosScreenSpace(2.0 * oldPos.x / WIN_WIDTH, -2.0 * oldPos.y / WIN_HEIGHT, gravityScreenSpace.z, 1.0f);

    // スクリーン座標の情報をオブジェクト座標に変換する行列
    const glm::mat4 s2oMat = glm::inverse(projMat * viewMat * modelMat);

    // スクリーン空間の座標をオブジェクト空間に変換
    glm::vec4 newPosObjSpace = s2oMat * newPosScreenSpace;
    glm::vec4 oldPosObjSpace = s2oMat * oldPosScreenSpace;
    newPosObjSpace /= newPosObjSpace.w;
    oldPosObjSpace /= oldPosObjSpace.w;

    // オブジェクト座標系での移動量
    const glm::vec3 transObjSpace = glm::vec3(newPosObjSpace - oldPosObjSpace);

    // オブジェクト空間での平行移動
    // selectedObj->acTransMat = glm::translate(selectedObj->acTransMat, transObjSpace);
    selectedObj->transMat = glm::translate(selectedObj->transMat, transObjSpace);
}

void updateScale() {
    //selectedObj->acScaleMat = glm::scale(glm::vec3(acScale, acScale, acScale));
}

void updateMouse() {
    switch (arcballMode) {
    case ARCBALL_MODE_ROTATE:
        updateRotate();
        break;

    case ARCBALL_MODE_TRANSLATE:
        updateTranslate();
        break;

    case ARCBALL_MODE_SCALE:
        acScale += (float)(oldPos.y - newPos.y) / WIN_HEIGHT;
        updateScale();
        break;
    }
}

void mouseMoveEvent(GLFWwindow *window, double xpos, double ypos) {
    if (isDragging) {
        // マウスの現在位置を更新
        newPos = glm::ivec2(xpos, ypos);

        // マウスがあまり動いていない時は処理をしない
        const double dx = newPos.x - oldPos.x;
        const double dy = newPos.y - oldPos.y;
        const double length = dx * dx + dy * dy;
        if (length < 2.0f * 2.0f) {
            return;
        } else {
            updateMouse();
            oldPos = glm::ivec2(xpos, ypos);
        }
    }
}

void wheelEvent(GLFWwindow *window, double xpos, double ypos) {
    acScale += ypos / 10.0;
    updateScale();
}

int rotateCount = 0;
// 回転のアニメーションのためのアップデート
void animateRotate() {
    if (rotating) {
        if (rotateCount < 9) {
            glm::vec3 dir = rotateDir ? glm::vec3(1) : glm::vec3(-1);
            for (auto itr = selectedCubePlane->cubeIds.begin(); itr != selectedCubePlane->cubeIds.end(); ++itr) {
                Cubes[*itr].rotMat = glm::rotate((float)(10.0f * PI / 180.0f), selectedCubePlane->nv * dir) * Cubes[*itr].rotMat;
            }
            rotateCount++;
        } else {
            updateCubePlane(axis, rotateDir, selectedCubePlane);
            rotateCount = 0;
            rotating = false;
        }
    }
}

int main(int argc, char **argv) {

    srand((unsigned int)time(NULL));

    // OpenGLを初期化する
    if (glfwInit() == GL_FALSE) {
        fprintf(stderr, "Initialization failed!\n");
        return 1;
    }

    // OpenGLのバージョン設定 (Macの場合には必ず必要)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Windowの作成
    GLFWwindow *window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE,
                                          NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Window creation failed!");
        glfwTerminate();
        return 1;
    }

    // OpenGLの描画対象にWindowを追加
    glfwMakeContextCurrent(window);

    // マウスのイベントを処理する関数を登録
    glfwSetMouseButtonCallback(window, mouseEvent);
    glfwSetCursorPosCallback(window, mouseMoveEvent);
    glfwSetScrollCallback(window, wheelEvent);

    // キーボードのイベントを処理する関数を登録
    glfwSetKeyCallback(window, keyboardEvent);

    // OpenGL 3.x/4.xの関数をロードする (glfwMakeContextCurrentの後でないといけない)
    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        fprintf(stderr, "Failed to load OpenGL 3.x/4.x libraries!\n");
        return 1;
    }

    // バージョンを出力する
    printf("Load OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    // ウィンドウのリサイズを扱う関数の登録
    glfwSetWindowSizeCallback(window, resizeGL);

    // OpenGLを初期化
    initializeGL();

    // メインループ
    while (glfwWindowShouldClose(window) == GL_FALSE) {
        // 描画
        paintGL();

        animateRotate();

        // 描画用バッファの切り替え
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
