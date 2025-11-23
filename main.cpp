// main.cpp - minimal app that loads a model via Assimp and renders with the paint shader
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Shader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"
#include <string>
#include <cstdint>

void loadModel(const char* path);
void createBuffers();
GLuint loadCubemapFaces(const std::vector<std::string>& faces);

// adjust these paths if you put files elsewhere
const char* VERT_PATH = "shaders/car_vert.glsl";
const char* FRAG_PATH = "shaders/car_frag.glsl";
const char* MODEL_PATH = "assets/head.OBJ";

float cameraDist = 4.0f;
float cameraAzimuth = 0.0f;
float cameraElevation = 0.0f;
bool autoRotate = true;  // pornește cu rotație automată

Shader shader;
GLuint envCubemap = 0;
GLuint VAO = 0, VBO = 0, EBO = 0;
int windowW = 1280, windowH = 720;


struct Vertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
std::vector<Vertex> vertices;
std::vector<unsigned int> indices;



void display() {
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    shader.use();

    // === MODEL + ROTAȚIE ===
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(6.0f));
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1, 0)); // întoarce fața spre cameră

    // rotație automată (poate fi oprită cu SPAȚIU)
    if (autoRotate) {
        cameraAzimuth = (float)glutGet(GLUT_ELAPSED_TIME) / 80.0f;
    }

    // === CAMERA LIBERĂ (WASD + mouse + săgeți + SPAȚIU pauză) ===
    float x = cameraDist * cos(glm::radians(cameraElevation)) * sin(glm::radians(cameraAzimuth));
    float y = cameraDist * sin(glm::radians(cameraElevation));
    float z = cameraDist * cos(glm::radians(cameraElevation)) * cos(glm::radians(cameraAzimuth));

    glm::mat4 view = glm::lookAt(
        glm::vec3(x, y + 0.5f, z),      // poziția camerei
        glm::vec3(0.0f, 0.5f, 0.0f),     // privește în centru
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)windowW / windowH, 0.1f, 100.0f);
    glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

    // Trimite la shader
    glUniformMatrix4fv(shader.getUniformLocation("uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(shader.getUniformLocation("uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(shader.getUniformLocation("uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix3fv(shader.getUniformLocation("uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMat));

    glm::vec3 camPos(x, y + 0.5f, z);
    glUniform3fv(shader.getUniformLocation("uCameraPos"), 1, glm::value_ptr(camPos));

    //  paint perfect
    glUniform3f(shader.getUniformLocation("uLightPos"), 8.0f, 10.0f, 8.0f);
    glUniform3f(shader.getUniformLocation("uLightColor"), 7.0f, 7.0f, 7.0f);
    glUniform3f(shader.getUniformLocation("uBaseColor"), 0.18f, 0.16f, 0.15f);
    glUniform1f(shader.getUniformLocation("uMetallic"), 1.0f);
    glUniform1f(shader.getUniformLocation("uRoughness"), 0.04f);
    glUniform1f(shader.getUniformLocation("uClearCoat"), 1.0f);
    float t = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glUniform1f(shader.getUniformLocation("uTime"), t);

    // Desenează modelul
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glutSwapBuffers();
    glutPostRedisplay();
}

// ================================================================
// 1. LOAD MODEL cu Assimp (funcțional pentru .obj)
void loadModel(const char* path)
{
    Assimp::Importer* importer = new Assimp::Importer();  // <--- pe heap
    const aiScene* scene = importer->ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_ValidateDataStructure);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp error: " << importer->GetErrorString() << "\n";
        delete importer;
        return;
    }

    vertices.clear();
    indices.clear();
    unsigned int indexOffset = 0;

    for (unsigned int m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex v = {};
            v.pos = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            v.normal = mesh->mNormals ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0, 1, 0);

            // NOTE: Some imported meshes (or corrupt/odd exporters) expose a non-null
            // mTextureCoords[0] pointer that is not safe to index. Dereferencing such
            // pointers causes access violations in the loader. The safe and simple
            // approach below avoids any direct indexing of mTextureCoords and uses a
            // default UV. If you need real UVs, re-export the model with valid UVs
            // or perform a stronger validation step for the UV pointer before reading.
            v.uv = glm::vec2(0.0f, 0.0f);

            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j] + indexOffset);
        }
        indexOffset += mesh->mNumVertices;
    }

    std::cout << "FULL MODEL LOADED: " << scene->mNumMeshes << " meshes, "
        << vertices.size() << " vertices, " << indices.size() / 3 << " triangles\n";
    importer->FreeScene();  // important!
    delete importer;        // eliberează memoria
}

// ================================================================
// 2. CREATE BUFFERS (VAO/VBO/EBO)
void createBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// ================================================================
// 3. LOAD CUBEMAP (6 imagini)
GLuint loadCubemapFaces(const std::vector<std::string>& faces)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int w, h, channels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &channels, 0);
        if (data)
        {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cerr << "Cubemap failed: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void initResources() {
	glewExperimental = GL_TRUE; glewInit();
	if (!shader.compileFromFiles(VERT_PATH, FRAG_PATH)) std::cerr << "Shader compile/link failed" << std::endl;


	loadModel(MODEL_PATH);
	if (vertices.empty()) { std::cerr << "No vertices loaded\n"; }
	createBuffers();


    // Cubemap SUPER simplu și rapid – doar cer albastru + lumină (funcționează PERFECT pentru car paint)
   // std::vector<std::string> faces = {
   //     "assets/envposx.jpg",   // right
   //     "assets/envnegx.jpg",   // left  
   ///     "assets/envposy.jpg",   // top (cer)
   //     "assets/envnegy.jpg",   // bottom
    //    "assets/envposz.jpg",   // front
   //     "assets/envnegz.jpg"    // back
   // };
   // envCubemap = loadCubemapFaces(faces);
    envCubemap = 0;   // important!
}

void mouseMotion(int x, int y) {
    if (!autoRotate) {
        static int lastX = x, lastY = y;
        cameraAzimuth += (x - lastX) * 0.5f;
        cameraElevation += (y - lastY) * 0.5f;
        cameraElevation = glm::clamp(cameraElevation, -89.0f, 89.0f);
        lastX = x; lastY = y;
        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (key == ' ') autoRotate = !autoRotate;  // SPAȚIU = pauză/pornește rotația
    if (key == 'w') cameraDist -= 0.2f;
    if (key == 's') cameraDist += 0.2f;
    if (cameraDist < 1.0f) cameraDist = 1.0f;
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_LEFT) cameraAzimuth -= 5.0f;
    if (key == GLUT_KEY_RIGHT) cameraAzimuth += 5.0f;
    if (key == GLUT_KEY_UP) cameraElevation += 5.0f;
    if (key == GLUT_KEY_DOWN) cameraElevation -= 5.0f;
    cameraElevation = glm::clamp(cameraElevation, -89.0f, 89.0f);
    glutPostRedisplay();
}

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(windowW, windowH);
	glutCreateWindow("Human Shader");
	GLenum err = glewInit(); if (err != GLEW_OK) { std::cerr << "GLEW init failed\n"; return -1; }
	initResources();
	glutDisplayFunc(display);
    glutMotionFunc(mouseMotion);           // click + miști
    glutPassiveMotionFunc(mouseMotion);    
    glutMouseFunc([](int button, int state, int x, int y) {
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
            autoRotate = false;            // oprire rotatie automată când se apass click
        }
        });                               
    glutKeyboardFunc(keyboard);       // W/S = zoom, SPAȚIU = pauză rotație
    glutSpecialFunc(specialKeys);     // săgeți = mișcare cameră
	glutMainLoop();
	return 0;
}