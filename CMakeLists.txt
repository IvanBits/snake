cmake_minimum_required(VERSION 3.10)

project(SnakeNuklear C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Definiciones del preprocesador ---
add_compile_definitions(GLEW_STATIC)

# --- Encontrar Dependencias ---

# Política para preferir GLVND sobre la biblioteca legacy de GL.
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

# Busca el paquete de OpenGL
find_package(OpenGL REQUIRED)

# Busca el paquete de GLEW
find_package(GLEW REQUIRED)

# Busca el paquete de GLFW3
# SOLUCIÓN: Usar 'glfw3' y enlazar con 'glfw' (el target moderno)
find_package(glfw3 REQUIRED)

# --- Definir el Ejecutable ---

add_executable(snake_game main.c)

# --- Incluir Directorios ---

target_include_directories(snake_game PUBLIC
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

# --- Enlazar Bibliotecas ---

# SOLUCIÓN PRINCIPAL: Usar 'glfw' en lugar de ${glfw3_LIBRARIES}
target_link_libraries(snake_game
    PUBLIC
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    glfw  # <-- Cambio crítico: usar el target 'glfw' directamente
    m     # Para la biblioteca matemática (math.h)
)