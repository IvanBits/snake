#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// --- Librería Nuklear y Backend ---
#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

// Backend de GLFW3/OpenGL4 para Nuklear
#define NK_GLFW_GL4_IMPLEMENTATION
#include "nuklear_glfw_gl4.h"

// --- Constantes y Estructuras del Juego ---
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define GRID_WIDTH  40
#define GRID_HEIGHT 30
#define INITIAL_LENGTH 4
#define MAX_SNAKE_LENGTH (GRID_WIDTH * GRID_HEIGHT)

#define CELL_SIZE_PX_WIDTH  (WINDOW_WIDTH / GRID_WIDTH)
#define CELL_SIZE_PX_HEIGHT (WINDOW_HEIGHT / GRID_HEIGHT)

typedef struct { int x; int y; } Point;
typedef enum { UP, DOWN, LEFT, RIGHT } Direction;
typedef enum { MENU, PLAYING, GAME_OVER } GameState;

typedef struct {
    Point body[MAX_SNAKE_LENGTH];
    int length;
    Direction current_direction;
    Point food;
    GameState state;
    int score;
    double move_timer;
    const double move_interval; 
} SnakeGame;

SnakeGame game = { .move_interval = 0.15 };

// --- Variables Globales de Renderizado (OpenGL) ---
GLuint shaderProgram;
GLuint VBO, VAO;
GLfloat projection[16];

// --- Funciones de Lógica del Juego ---

void generate_food(SnakeGame *g) {
    int valid_pos = 0;
    while (!valid_pos) {
        g->food.x = rand() % GRID_WIDTH;
        g->food.y = rand() % GRID_HEIGHT;

        valid_pos = 1;
        for (int i = 0; i < g->length; i++) {
            if (g->food.x == g->body[i].x && g->food.y == g->body[i].y) {
                valid_pos = 0; 
                break;
            }
        }
    }
}

void init_game_state(SnakeGame *g) {
    g->length = INITIAL_LENGTH;
    g->current_direction = RIGHT;
    g->state = PLAYING; 
    g->score = 0;
    g->move_timer = 0.0;

    // Cabeza inicial
    g->body[0].x = GRID_WIDTH / 4;
    g->body[0].y = GRID_HEIGHT / 2;

    // Resto del cuerpo
    for (int i = 1; i < g->length; i++) {
        g->body[i].x = g->body[i-1].x - 1;
        g->body[i].y = g->body[i-1].y;
    }

    generate_food(g);
}

void update_game(SnakeGame *g, double delta_time) {
    if (g->state != PLAYING) return;

    g->move_timer += delta_time;

    if (g->move_timer < g->move_interval) {
        return;
    }
    g->move_timer = 0.0; 

    // 1. Mover el cuerpo
    for (int i = g->length - 1; i > 0; i--) {
        g->body[i] = g->body[i - 1];
    }

    // 2. Mover la cabeza
    Point new_head = g->body[0];
    switch (g->current_direction) {
        case UP:    new_head.y--; break;
        case DOWN:  new_head.y++; break;
        case LEFT:  new_head.x--; break;
        case RIGHT: new_head.x++; break;
    }
    g->body[0] = new_head;

    // 3. Comprobar Colisión con la Comida
    if (g->body[0].x == g->food.x && g->body[0].y == g->food.y) {
        if (g->length < MAX_SNAKE_LENGTH) {
            g->length++;
            g->score += 10;
        }
        generate_food(g);
    }

    // 4. Comprobar Colisión de "Game Over"
    // a) Choque con las paredes
    if (g->body[0].x < 0 || g->body[0].x >= GRID_WIDTH ||
        g->body[0].y < 0 || g->body[0].y >= GRID_HEIGHT) {
        g->state = GAME_OVER;
        return;
    }

    // b) Choque consigo misma
    for (int i = 1; i < g->length; i++) {
        if (g->body[0].x == g->body[i].x && g->body[0].y == g->body[i].y) {
            g->state = GAME_OVER;
            return;
        }
    }
}

// --- Callbacks de GLFW ---

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    if (game.state == PLAYING) {
        switch (key) {
            case GLFW_KEY_UP:
                if (game.current_direction != DOWN) game.current_direction = UP;
                break;
            case GLFW_KEY_DOWN:
                if (game.current_direction != UP) game.current_direction = DOWN;
                break;
            case GLFW_KEY_LEFT:
                if (game.current_direction != RIGHT) game.current_direction = LEFT;
                break;
            case GLFW_KEY_RIGHT:
                if (game.current_direction != LEFT) game.current_direction = RIGHT;
                break;
        }
    }
}

// --- Funciones de Renderizado (OpenGL) ---

// Shaders GLSL
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "uniform mat4 projection;\n"
    "uniform vec2 offset;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * vec4(aPos.x + offset.x, aPos.y + offset.y, 0.0, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 uColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = uColor;\n"
    "}\0";

// Función auxiliar para compilar shaders
GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Verificar errores de compilación
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "ERROR: Shader compilation failed\n%s\n", infoLog);
    }
    
    return shader;
}

void setup_rendering() {
    // 1. Compilación y enlace de shaders
    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // Verificar errores de enlace
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "ERROR: Shader linking failed\n%s\n", infoLog);
    }
    
    // Limpiar shaders (ya están en el programa)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 2. Configuración del VAO/VBO para un solo cuadrado
    float vertices[] = {
        0.0f, 0.0f, // Inferior izquierda
        CELL_SIZE_PX_WIDTH, 0.0f, // Inferior derecha
        CELL_SIZE_PX_WIDTH, CELL_SIZE_PX_HEIGHT, // Superior derecha
        0.0f, CELL_SIZE_PX_HEIGHT  // Superior izquierda
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 3. Configurar la matriz de proyección ortográfica
    float L = 0.0f, R = (float)WINDOW_WIDTH;
    float B = (float)WINDOW_HEIGHT, T = 0.0f;
    float N = -1.0f, F = 1.0f;

    projection[0] = 2.0f / (R - L);   projection[4] = 0.0f;             projection[8] = 0.0f;            projection[12] = -(R + L) / (R - L);
    projection[1] = 0.0f;             projection[5] = 2.0f / (T - B);   projection[9] = 0.0f;            projection[13] = -(T + B) / (T - B);
    projection[2] = 0.0f;             projection[6] = 0.0f;             projection[10] = -2.0f / (F - N);  projection[14] = -(F + N) / (F - N);
    projection[3] = 0.0f;             projection[7] = 0.0f;             projection[11] = 0.0f;           projection[15] = 1.0f;

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void draw_cell(int grid_x, int grid_y, float r, float g, float b) {
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    // Color y Offset
    glUniform4f(glGetUniformLocation(shaderProgram, "uColor"), r, g, b, 1.0f);
    float offset_x_px = (float)grid_x * CELL_SIZE_PX_WIDTH;
    float offset_y_px = (float)grid_y * CELL_SIZE_PX_HEIGHT;
    glUniform2f(glGetUniformLocation(shaderProgram, "offset"), offset_x_px, offset_y_px);

    // Dibujar
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void render_game_elements(SnakeGame *g) {
    // Colores
    float snake_r = 0.0f, snake_g = 0.7f, snake_b = 0.0f; // Verde
    float food_r = 1.0f, food_g = 0.0f, food_b = 0.0f;    // Rojo

    // 1. Dibujar Comida
    if (g->state == PLAYING) {
        draw_cell(g->food.x, g->food.y, food_r, food_g, food_b);
    }

    // 2. Dibujar Culebra
    if (g->state == PLAYING || g->state == GAME_OVER) {
        for (int i = 0; i < g->length; i++) {
            // Un color diferente para la cabeza
            if (i == 0) draw_cell(g->body[i].x, g->body[i].y, 0.0f, 1.0f, 0.0f); 
            else draw_cell(g->body[i].x, g->body[i].y, snake_r, snake_g, snake_b);
        }
    }
}

// --- Funciones de Interfaz (Nuklear) ---

void render_gui(GLFWwindow *window, struct nk_context *ctx, SnakeGame *g) {
    // 1. Procesar input de Nuklear
    nk_glfw3_new_frame();

    // 2. Ventana de estado/menú
    if (nk_begin(ctx, "Snake Status", nk_rect(WINDOW_WIDTH - 210, 10, 200, 150),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE))
    {
        nk_layout_row_dynamic(ctx, 25, 1);
        
        char score_text[50];
        sprintf(score_text, "Score: %d", g->score);
        nk_label(ctx, score_text, NK_TEXT_LEFT);

        if (g->state == GAME_OVER) {
            nk_label(ctx, "GAME OVER!", NK_TEXT_CENTERED);
        } else if (g->state == MENU) {
            nk_label(ctx, "Press START to begin", NK_TEXT_CENTERED);
        }

        nk_layout_row_dynamic(ctx, 30, 1);
        if (g->state != PLAYING) {
            if (nk_button_label(ctx, "START Game")) {
                init_game_state(g);
                g->state = PLAYING;
            }
        }
    }
    nk_end(ctx);

    // 3. Renderizar Nuklear
    nk_glfw3_render(NK_ANTI_ALIASING_ON);
}

void cleanup_rendering() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}

// --- Función Principal ---

int main(int argc, char** argv) {
    srand(time(NULL));
    init_game_state(&game);    
    game.state = MENU;
    struct nk_context *ctx;
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Snake Nuklear/GLFW", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Inicialización de GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // Configuración de OpenGL
    setup_rendering();

    // Inicialización de Nuklear
    ctx = nk_glfw3_init(window, NK_GLFW3_INSTALL_CALLBACKS, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
    struct nk_font_atlas *atlas;
    nk_glfw3_font_stash_begin(&atlas);
    struct nk_font *default_font = nk_font_atlas_add_default(atlas, 13, 0);
    nk_glfw3_font_stash_end();
    nk_style_set_font(ctx, &default_font->handle);

    // Configurar callbacks
    glfwSetKeyCallback(window, key_callback);

    // Bucle Principal
    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        double delta_time = current_time - last_time;
        last_time = current_time;

        glfwPollEvents();

        // 1. Lógica del Juego
        update_game(&game, delta_time);

        // 2. Limpiar pantalla
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 3. Renderizado de Elementos del Juego
        render_game_elements(&game);
        
        // 4. Renderizado de la GUI (Nuklear)
        render_gui(window, ctx, &game);

        // 5. Intercambiar buffers
        glfwSwapBuffers(window);
    }

    // Limpieza
    cleanup_rendering();
    nk_glfw3_shutdown();
    glfwTerminate();
    return 0;
}