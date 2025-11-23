#pragma once


#include <GL/glew.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>


inline std::string loadFileToString(const char* path) {
	std::ifstream in(path, std::ios::in);
	if (!in) {
		std::cerr << "Failed to open file: " << path << "\n";
		return std::string();
	}
	std::ostringstream ss; ss << in.rdbuf();
	return ss.str();
}


class Shader {
public:
	GLuint ID = 0;
	Shader() {}
	bool compileFromFiles(const char* vsPath, const char* fsPath) {
		std::string vsSrc = loadFileToString(vsPath);
		std::string fsSrc = loadFileToString(fsPath);
		if (vsSrc.empty() || fsSrc.empty()) return false;
		const char* vsrc = vsSrc.c_str();
		const char* fsrc = fsSrc.c_str();
		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vsrc, NULL);
		glCompileShader(vs);
		GLint ok; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
		if (!ok) { char buf[1024]; glGetShaderInfoLog(vs, 1024, NULL, buf); std::cerr << "VS compile error:\n" << buf << "\n"; return false; }


		GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &fsrc, NULL);
		glCompileShader(fs);
		glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
		if (!ok) { char buf[1024]; glGetShaderInfoLog(fs, 1024, NULL, buf); std::cerr << "FS compile error:\n" << buf << "\n"; return false; }


		ID = glCreateProgram();
		glAttachShader(ID, vs); glAttachShader(ID, fs);
		glLinkProgram(ID);
		glGetProgramiv(ID, GL_LINK_STATUS, &ok);
		if (!ok) { char buf[1024]; glGetProgramInfoLog(ID, 1024, NULL, buf); std::cerr << "Program link error:\n" << buf << "\n"; return false; }


		glDeleteShader(vs); glDeleteShader(fs);
		return true;
	}
	void use() { if (ID) glUseProgram(ID); }
	GLint getUniformLocation(const char* name) { return glGetUniformLocation(ID, name); }
};