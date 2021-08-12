#include "renderer/Shader.h"
#include "utils/CMath.h"

#include <unordered_map>


namespace Minecraft
{
	namespace NShader
	{
		// Internal Structures
		struct ShaderVariable
		{
			std::string name;
			uint32 hash;
			GLint varLocation;
			uint32 shaderProgramId;
		};

		// Internal Variables
		static std::vector<ShaderVariable> mAllShaderVariables = std::vector<ShaderVariable>(10);

		// Forward Declarations
		static GLint GetVariableLocation(const Shader& shader, const char* varName);
		static GLenum ShaderTypeFromString(const std::string& type);
		static std::string ReadFile(const char* filepath);

		Shader createShader()
		{
			return {
				UINT32_MAX,
				UINT32_MAX,
				std::filesystem::path()
			};
		}

		Shader createShader(const std::filesystem::path& resourceName)
		{
			// TODO: Should compilation happen on creation?
			return compile(resourceName);
		}

		Shader compile(const std::filesystem::path& filepath)
		{
			g_logger_info("Compiling shader: %s", filepath.string().c_str());
			std::string fileSource = ReadFile(filepath.string().c_str());

			std::unordered_map<GLenum, std::string> shaderSources;

			const char* typeToken = "#type";
			size_t typeTokenLength = strlen(typeToken);
			size_t pos = fileSource.find(typeToken, 0);
			while (pos != std::string::npos)
			{
				size_t eol = fileSource.find_first_of("\r\n", pos);
				g_logger_assert(eol != std::string::npos, "Syntax error");
				size_t begin = pos + typeTokenLength + 1;
				std::string type = fileSource.substr(begin, eol - begin);
				g_logger_assert(ShaderTypeFromString(type), "Invalid shader type specified.");

				size_t nextLinePos = fileSource.find_first_not_of("\r\n", eol);
				pos = fileSource.find(typeToken, nextLinePos);
				shaderSources[ShaderTypeFromString(type)] = fileSource.substr(nextLinePos, pos - (nextLinePos == std::string::npos ? fileSource.size() - 1 : nextLinePos));
			}

			GLuint program = glCreateProgram();
			g_logger_assert(shaderSources.size() <= 2, "Shader source must be less than 2.");
			std::array<GLenum, 2> glShaderIDs;
			int glShaderIDIndex = 0;

			for (auto& kv : shaderSources)
			{
				GLenum shaderType = kv.first;
				const std::string& source = kv.second;

				// Create an empty vertex shader handle
				GLuint shader = glCreateShader(shaderType);

				// Send the vertex shader source code to GL
				// Note that std::string's .c_str is NULL character terminated.
				const GLchar* sourceCStr = source.c_str();
				glShaderSource(shader, 1, &sourceCStr, 0);

				// Compile the vertex shader
				glCompileShader(shader);

				GLint isCompiled = 0;
				glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
				if (isCompiled == GL_FALSE)
				{
					GLint maxLength = 0;
					glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

					// The maxLength includes the NULL character
					std::vector<GLchar> infoLog(maxLength);
					glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

					// We don't need the shader anymore.
					glDeleteShader(shader);

					g_logger_error("%s", infoLog.data());
					g_logger_assert(false, "Shader compilation failed!");
					return createShader();
				}

				glAttachShader(program, shader);
				glShaderIDs[glShaderIDIndex++] = shader;
			}

			// Link our program
			glLinkProgram(program);

			// Note the different functions here: glGetProgram* instead of glGetShader*.
			GLint isLinked = 0;
			glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
			if (isLinked == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

				// The maxLength includes the NULL character
				std::vector<GLchar> infoLog(maxLength);
				glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

				// We don't need the program anymore.
				glDeleteProgram(program);
				// Don't leak shaders either.
				for (auto id : glShaderIDs)
					glDeleteShader(id);

				g_logger_error("%s", infoLog.data());
				g_logger_assert(false, "Shader linking failed!");
				return NShader::createShader();
			}

			const uint32 startIndex = (uint32)mAllShaderVariables.size();

			// Get all the active vertex attributes and store them in our map of uniform variable locations
			int numUniforms;
			glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

			int maxCharLength;
			glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxCharLength);
			if (numUniforms > 0 && maxCharLength > 0)
			{
				char* charBuffer = (char*)g_memory_allocate(sizeof(char) * maxCharLength);

				for (int i = 0; i < numUniforms; i++)
				{
					int length, size;
					GLenum type;
					glGetActiveUniform(program, i, maxCharLength, &length, &size, &type, charBuffer);
					GLint varLocation = glGetUniformLocation(program, charBuffer);
					mAllShaderVariables.push_back({
						std::string(charBuffer),
						CMath::hashString(charBuffer),
						varLocation,
						program
					});
				}

				g_memory_free(charBuffer);
			}

			// Always detach shaders after a successful link.
			for (auto id : glShaderIDs)
				glDetachShader(program, id);

			return Shader{
				program,
				startIndex,
				filepath
			};
		}

		void destroy(Shader& shader)
		{
			glDeleteProgram(shader.programId);
		}

		void bind(const Shader& shader)
		{
			glUseProgram(shader.programId);
		}

		void unbind(const Shader& shader)
		{
			glUseProgram(0);
		}

		void uploadVec4(const Shader& shader, const char* varName, const glm::vec4& vec4)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform4f(varLocation, vec4.x, vec4.y, vec4.z, vec4.w);
		}

		void uploadVec3(const Shader& shader, const char* varName, const glm::vec3& vec3)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform3f(varLocation, vec3.x, vec3.y, vec3.z);
		}

		void uploadVec2(const Shader& shader, const char* varName, const glm::vec2& vec2)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform2f(varLocation, vec2.x, vec2.y);
		}

		void uploadIVec2(const Shader& shader, const char* varName, const glm::ivec2& vec2)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform2i(varLocation, vec2.x, vec2.y);
		}

		void uploadFloat(const Shader& shader, const char* varName, float value)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform1f(varLocation, value);
		}

		void uploadInt(const Shader& shader, const char* varName, int value)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform1i(varLocation, value);
		}

		void uploadUInt(const Shader& shader, const char* varName, uint32 value)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform1ui(varLocation, value);
		}

		void uploadMat4(const Shader& shader, const char* varName, const glm::mat4& mat4)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniformMatrix4fv(varLocation, 1, GL_FALSE, glm::value_ptr(mat4));
		}

		void uploadMat3(const Shader& shader, const char* varName, const glm::mat3& mat3)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniformMatrix3fv(varLocation, 1, GL_FALSE, glm::value_ptr(mat3));
		}

		void uploadIntArray(const Shader& shader, const char* varName, int length, const int* array)
		{
			int varLocation = GetVariableLocation(shader, varName);
			glUniform1iv(varLocation, length, array);
		}

		bool isNull(const Shader& shader)
		{
			return shader.programId == UINT32_MAX;
		}

		void clearAllShaderVariables()
		{
			mAllShaderVariables.clear();
		}

		// Private functions
		static GLint GetVariableLocation(const Shader& shader, const char* varName)
		{
			uint32 hash = CMath::hashString(varName);

			for (int i = shader.startIndex; i < mAllShaderVariables.size(); i++)
			{
				const ShaderVariable& shaderVar = mAllShaderVariables[i];
				if (shaderVar.shaderProgramId != shader.programId)
				{
					g_logger_warning("Could not find shader variable '%s' for shader '%s'", varName, shader.filepath.string().c_str());
					break;
				}

				if (shaderVar.hash == hash && shaderVar.name == varName)
				{
					return shaderVar.varLocation;
				}
			}

			return -1;
		}

		static GLenum ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")
				return GL_VERTEX_SHADER;
			else if (type == "fragment" || type == "pixel")
				return GL_FRAGMENT_SHADER;

			g_logger_assert(false, "Unkown shader type.");
			return 0;
		}

		static std::string ReadFile(const char* filepath)
		{
			std::string result;
			std::ifstream in(filepath, std::ios::in | std::ios::binary);
			if (in)
			{
				in.seekg(0, std::ios::end);
				result.resize(in.tellg());
				in.seekg(0, std::ios::beg);
				in.read(&result[0], result.size());
				in.close();
			}
			else
			{
				g_logger_error("Could not open file: '%s'", filepath);
			}

			return result;
		}
	}
}
