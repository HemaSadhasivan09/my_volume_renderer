#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <exception>
#include <algorithm>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "shader.hpp"
#include "util/util.hpp"
#include "configraw.hpp"
#include "transferfunc.hpp"

//-----------------------------------------------------------------------------
// type definitions
//-----------------------------------------------------------------------------
enum class Mode : int
{
    line_of_sight = 0,
    maximum_intensity_projection,
    isosurface,
    transfer_function
};
enum class Gradient : int
{
    central_differences = 0,
    sobel_operators
};

//-----------------------------------------------------------------------------
// global parameters
//-----------------------------------------------------------------------------
unsigned int win_w = 1600;
unsigned int win_h = 900;

unsigned int tf_func_img_w = 384;
unsigned int tf_func_img_h = 96;

unsigned int tf_color_img_w = 384;
unsigned int tf_color_img_h = 16;

float fovY = 90.f;
float zNear = 0.000001f;
float zFar = 30.f;

glm::vec3 camPos = glm::vec3(1.2f, 0.75f, 1.f);

#define REQUIRED_OGL_VERSION_MAJOR 3
#define REQUIRED_OGL_VERSION_MINOR 3

#define MAX_FILEPATH_LENGTH 200

#define DEFAULT_VOLUME_JSON_FILE "/mnt/data/steffen/jet.json"

#define CONTROL_POINT_LIST false

//-----------------------------------------------------------------------------
// gui parameters
//-----------------------------------------------------------------------------
int gui_mode = static_cast<int>(Mode::line_of_sight);

char gui_volume_desc_file[MAX_FILEPATH_LENGTH]; // init from program options

int gui_timestep = 0;

float gui_step_size = 0.25f;

int gui_grad_method = static_cast<int>(Gradient::sobel_operators);

float gui_brightness = 1.f;

float gui_cam_zoom_speed = 0.1f;
float gui_cam_rot_speed = 0.2f;

float gui_isovalue = 0.1f;
bool gui_iso_denoise = true;
float gui_iso_denoise_r = 0.1f;

float gui_light_dir[3] = {0.3f, 1.f, -0.3f};
float gui_ambient[3] = {0.2f, 0.2f, 0.2f};
float gui_diffuse[3] = {1.f, 1.f, 1.f};
float gui_specular[3] = {1.f, 1.f, 1.f};
float gui_k_amb = 0.2f;
float gui_k_diff = 0.3f;
float gui_k_spec = 0.5f;
float gui_k_exp = 10.0f;

bool gui_show_histogram_window = true;
bool gui_hist_semilog = false;
int gui_num_bins = 255;
int gui_y_limit = 100000;
float gui_x_min = 0.f;
float gui_x_max = 255.f;

bool gui_show_tf_window = true;

bool gui_show_demo_window = false;
bool gui_frame = true;
bool gui_wireframe = false;
bool gui_invert_colors = false;
bool gui_invert_alpha = false;

bool gui_slice_volume = false;
float gui_slice_plane_normal[3] = {0.f, 0.f, 1.f};
float gui_slice_plane_base[3] = {0.f, 0.f, 0.f};

float gui_tf_cp_pos = 0.f;
float gui_tf_cp_color_rgb[3] = {0.f, 0.f, 0.0f};
float gui_tf_cp_color_a = 0.f;

//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void cursor_position_cb(GLFWwindow *window, double xpos, double ypos);
void mouse_button_cb(GLFWwindow* window, int button, int action, int mods);
void scroll_cb(GLFWwindow* window, double xoffset, double yoffset);
void key_cb(GLFWwindow* window, int key, int scancode , int action, int mods);
void char_cb(GLFWwindow* window, unsigned int c);
void framebuffer_size_cb(GLFWwindow* window, int width, int height);
void error_cb(int error, const char* description);
GLFWwindow* createWindow(
    unsigned int win_w, unsigned int win_h, const char* title);
void applyProgramOptions(int argc, char *argv[]);
void createDefaultFBO(
    GLuint &fbo,
    GLuint texIDs[],
    unsigned int win_w,
    unsigned int win_h);
void initializeGl3w();
void initializeImGui(GLFWwindow* window);
static GLuint createFrameVAO(
    const float vertices[4 * 8],
    const unsigned int indices[2 * 12],
    const float texCoords[3 * 8]);
static GLuint createVolumeVAO(
    const float vertices[4 * 8],
    const unsigned int indices[3 * 2 * 6],
    const float texCoords[3 * 8]);
static GLuint createQuadVAO(
        const float vertices[2 * 4],
        const unsigned int indices[4],
        const float texCoords[2 * 4]);
static void showHelpMarker(const char* desc);
static void showSettingsWindow(
    cr::VolumeConfig &vConf,
    void *&volumeData,
    GLuint &volumeTex,
    glm::mat4 &modelMX,
    std::vector<util::bin_t> *&histogramBins);
static void showHistogramWindow(
    std::vector<util::bin_t> *&histogramBins,
    cr::VolumeConfig vConf,
    void* volumeData);
static void showTransferFunctionWindow(
    tf::TransferFuncRGBA1D &transferFunction,
    Shader &shaderTfColor,
    Shader &shaderTfFunc,
    Shader &shaderTfPoint,
    GLuint tfColorFBO,
    GLuint *tfColorTexIDs,
    GLuint tfFuncFBO,
    GLuint *tfFuncTexIDs,
    GLuint quadVAO);
std::vector<util::bin_t> *loadHistogramBins(
    cr::VolumeConfig vConf, void* values, size_t numBins, float min, float max);
GLuint loadScalarVolumeTex(cr::VolumeConfig vConf, void* volumeData);
static void reloadStuff(
    cr::VolumeConfig vConf,
    void *&volumeData,
    GLuint &volumeTex,
    unsigned int timestep,
    glm::mat4 &modelMX,
    std::vector<util::bin_t> *&histogramBins,
    size_t num_bins,
    float x_min,
    float x_max);
static void drawTfColor(
    tf::TransferFuncRGBA1D &transferFunc,
    Shader &shaderTfColor,
    GLuint fboID,
    GLuint quadVAO,
    unsigned int width,
    unsigned int height);
static void drawTfFunc(
    tf::TransferFuncRGBA1D &transferFunc,
    Shader &shaderTfFunc,
    Shader &shaderTfPoint,
    GLuint fboID,
    GLuint quadVAO,
    unsigned int width,
    unsigned int height);
//-----------------------------------------------------------------------------
// internals
//-----------------------------------------------------------------------------
static bool _flag_reload_shaders = false;
static bool _flag_show_menues = true;

// default fbo for storing different rendering results and helpers
static GLuint _defaultFBO = 0;
static GLuint _defaultTexIDs[2] = { 0, 0 };

// for picking of control points in transfer function editor
static float _selected_cp_pos = 0.f;
static GLuint _selected_cp_fbo = 0.f;
static ImVec2 _tf_screen_pos = ImVec2();

//-----------------------------------------------------------------------------
// main program
//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    applyProgramOptions(argc, argv);

    GLFWwindow* window = createWindow(win_w, win_h, "MVR");

    initializeGl3w();
    initializeImGui(window);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    glPointSize(13.f);

    Shader shaderQuad("src/shader/quad.vert", "src/shader/quad.frag");
    Shader shaderFrame("src/shader/frame.vert", "src/shader/frame.frag");
    Shader shaderVolume("src/shader/volume.vert", "src/shader/volume.frag");
    Shader shaderTfColor("src/shader/tfColor.vert", "src/shader/tfColor.frag");
    Shader shaderTfFunc("src/shader/tfFunc.vert", "src/shader/tfFunc.frag");
    Shader shaderTfPoint("src/shader/tfPoint.vert", "src/shader/tfPoint.frag");

    // bounding box and volume geometry
    // --------------------------------
    float verticesCube[] = {
        -0.5f,  -0.5f,  -0.5f,  1.f,
        0.5f,   -0.5f,  -0.5f,  1.f,
        0.5f,   0.5f,   -0.5f,  1.f,
        -0.5f,  0.5f,   -0.5f,  1.f,
        -0.5f,  -0.5f,  0.5f,   1.f,
        0.5f,   -0.5f,  0.5f,   1.f,
        0.5f,   0.5f,   0.5f,   1.f,
        -0.5f,  0.5f,   0.5f,   1.f
    };
    unsigned int frameIndices[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        4, 5,
        5, 6,
        6, 7,
        7, 4,
        0, 4,
        1, 5,
        2, 6,
        3, 7
    };
    unsigned int volumeIndices[] = {
        2, 1, 0,
        0, 3, 2,
        6, 5, 1,
        1, 2, 6,
        7, 4, 5,
        5, 6, 7,
        3, 0, 4,
        4, 7, 3,
        3, 7, 6,
        6, 2, 3,
        4, 0, 1,
        1, 5, 4
    };
    // bounding box
    glm::vec4 bbMin = glm::vec4(
        verticesCube[0], verticesCube[1],verticesCube[2], verticesCube[3]);
    glm::vec4 bbMax = glm::vec4(
        verticesCube[24], verticesCube[25],verticesCube[26], verticesCube[27]);

    float texCoordsCube[] = {
        0.f, 0.f, 1.f,
        1.f, 0.f, 1.f,
        1.f, 1.f, 1.f,
        0.f, 1.f, 1.f,
        0.f, 0.f, 0.f,
        1.f, 0.f, 0.f,
        1.f, 1.f, 0.f,
        0.f, 1.f, 0.f
    };

    // quad for rendering to textures
    // ------------------------------
    float verticesQuad[] = {
        0.f,    0.f,
        1.f,    0.f,
        1.f,    1.f,
        0.f,    1.f
    };
    unsigned int quadIndices[] = {
        0, 1, 2, 3
    };

    // ------------------------------------------------------------------------
    // vertex array objects
    // ------------------------------------------------------------------------
    GLuint frameVAO = 0, volumeVAO = 0, quadVAO = 0;

    frameVAO = createFrameVAO(verticesCube, frameIndices, texCoordsCube);
    volumeVAO = createVolumeVAO(verticesCube, volumeIndices, texCoordsCube);
    quadVAO = createQuadVAO(verticesQuad, quadIndices, verticesQuad);

    // ------------------------------------------------------------------------
    // framebuffer objects
    // ------------------------------------------------------------------------
    createDefaultFBO(_defaultFBO, _defaultTexIDs, win_w, win_h);

    GLuint tfColorFBO = 0;
    GLuint tfColorTexIDs[1] = { 0 };
    {
        GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
        GLint  internalFormats[1] = { GL_RGBA };
        GLenum formats[1] = { GL_RGBA };
        GLenum datatypes[1] = { GL_FLOAT };
        GLint  filters[1] = { GL_LINEAR };

        tfColorFBO = util::createFrameBufferObject(
                tf_color_img_w,
                tf_color_img_h,
                tfColorTexIDs,
                1,
                attachments,
                internalFormats,
                formats,
                datatypes,
                filters);
    }

    GLuint tfFuncFBO = 0;
    GLuint tfFuncTexIDs[2] = {0, 0};

    {
        GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        GLint  internalFormats[2] = { GL_RGBA, GL_RG32F};
        GLenum formats[2] = { GL_RGBA, GL_RG};
        GLenum datatypes[2] = { GL_FLOAT, GL_FLOAT };
        GLint  filters[2] = { GL_LINEAR, GL_NEAREST };

        tfFuncFBO = util::createFrameBufferObject(
                tf_func_img_w,
                tf_func_img_h,
                tfFuncTexIDs,
                2,
                attachments,
                internalFormats,
                formats,
                datatypes,
                filters);
    }
    _selected_cp_fbo = tfFuncFBO;

    // ------------------------------------------------------------------------
    // volume
    // initialize model, view and projection matrices
    // ------------------------------------------------------------------------
    glm::mat4 modelMX = glm::mat4(1.f);

    glm::vec3 right = glm::normalize(
        glm::cross(-camPos, glm::vec3(0.f, 1.f, 0.f)));
    glm::vec3 up = glm::normalize(glm::cross(right, -camPos));
    glm::mat4 viewMX = glm::lookAt(camPos, glm::vec3(0.f), up);

    glm::mat4 projMX = glm::perspective(
        glm::radians(fovY),
        static_cast<float>(win_w)/static_cast<float>(win_h),
        0.1f,
        50.0f);

    // Diagonal of a voxel
    float voxel_diag = 1.f;

    // load the data, create a model matrix, bins and a texture
    std::vector<util::bin_t> *histogramBins = nullptr;
    void *volumeData = nullptr;
    GLuint volumeTex = 0;

    cr::VolumeConfig vConf = cr::VolumeConfig(gui_volume_desc_file);
    if(vConf.isValid())
    {
        reloadStuff(
            vConf,
            volumeData,
            volumeTex,
            gui_timestep,
            modelMX,
            histogramBins,
            gui_num_bins,
            gui_x_min,
            gui_x_max);
    }
    else
    {
        modelMX = glm::mat4(1.f);
        histogramBins = nullptr;
        volumeData = nullptr;
        volumeTex = 0;
    }

    // ------------------------------------------------------------------------
    // misc. stuff
    // ------------------------------------------------------------------------
    // create a transfer function object
    tf::TransferFuncRGBA1D transferFunction = tf::TransferFuncRGBA1D();

    // temporary variables
    glm::vec3 tempVec3 = glm::vec3(0.f);

    // projection matrix for the window filling quad
    glm::mat4 projMXquad = glm::ortho(0.f, 1.f, 0.f, 1.f);
    // ------------------------------------------------------------------------
    // render loop
    // ------------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

                // check if shader programs shall be reloaded
        if (_flag_reload_shaders)
        {
            _flag_reload_shaders = false;
            std::cout << "reloading shaders..." << std::endl;

            glDeleteProgram(shaderQuad.ID);
            glDeleteProgram(shaderFrame.ID);
            glDeleteProgram(shaderVolume.ID);
            glDeleteProgram(shaderTfColor.ID);
            glDeleteProgram(shaderTfFunc.ID);
            glDeleteProgram(shaderTfPoint.ID);

            shaderQuad = Shader(
                "src/shader/quad.vert", "src/shader/quad.frag");
            shaderFrame = Shader(
                "src/shader/frame.vert", "src/shader/frame.frag");
            shaderVolume = Shader(
                "src/shader/volume.vert", "src/shader/volume.frag");
            shaderTfColor = Shader(
                "src/shader/tfColor.vert", "src/shader/tfColor.frag");
            shaderTfFunc = Shader(
                "src/shader/tfFunc.vert", "src/shader/tfFunc.frag");
            shaderTfPoint = Shader(
                "src/shader/tfPoint.vert", "src/shader/tfPoint.frag");
        }
        // --------------------------------------------------------------------
        // draw the volume, frame and menues into a frame buffer object
        // --------------------------------------------------------------------

        // activate the framebuffer object as current framebuffer
        glViewport(0, 0, win_w, win_h);
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);

        // select color attachments as render targets
        static const GLenum buf[2] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, buf);

        // clear old buffer content
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // update model, view and projection matrix
        right = glm::normalize(glm::cross(-camPos, glm::vec3(0.f, 1.f, 0.f)));
        up = glm::normalize(glm::cross(right, -camPos));
        viewMX = glm::lookAt(camPos, glm::vec3(0.f), up);
        projMX = glm::perspective(
            glm::radians(fovY),
            static_cast<float>(win_w)/static_cast<float>(win_h),
            zNear,
            zFar);

        // calculate the diagonal of a voxel
        voxel_diag = glm::length(glm::vec3(
                (modelMX *
                    glm::vec4(
                        1.f / static_cast<float>(vConf.getVolumeDim()[0]),
                        1.f / static_cast<float>(vConf.getVolumeDim()[1]),
                        1.f / static_cast<float>(vConf.getVolumeDim()[2]),
                        1.f)).xyz));

        // apply gui settings
        if(gui_wireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // draw the bounding frame
        if(gui_frame)
        {
            shaderFrame.use();
            shaderFrame.setMat4("pvmMX", projMX * viewMX * modelMX);
            glBindVertexArray(frameVAO);
            glDrawElements(GL_LINES, 2*12, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        // draw the volume
        shaderVolume.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, volumeTex);
        shaderVolume.setInt("volumeTex", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, transferFunction.getTexture());
        shaderVolume.setInt("transferfunctionTex", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _defaultTexIDs[1]);
        shaderVolume.setInt("stateIn", 2);

        shaderVolume.setMat4("modelMX", modelMX);
        shaderVolume.setMat4("pvmMX", projMX * viewMX * modelMX);
        shaderVolume.setVec3("eyePos", camPos);
        shaderVolume.setVec3("bbMin", (modelMX * bbMin).xyz);
        shaderVolume.setVec3("bbMax", (modelMX * bbMax).xyz);
        shaderVolume.setInt("mode", gui_mode);
        shaderVolume.setInt("gradMethod", gui_grad_method);
        shaderVolume.setFloat("stepSize", voxel_diag * gui_step_size);
        shaderVolume.setFloat("stepSizeVoxel", gui_step_size);
        shaderVolume.setFloat("brightness", gui_brightness);
        shaderVolume.setFloat("isovalue", gui_isovalue);
        shaderVolume.setBool("isoDenoise", gui_iso_denoise);
        shaderVolume.setFloat("isoDenoiseR", voxel_diag * gui_iso_denoise_r);
        tempVec3 = glm::normalize(
                glm::vec3(
                    gui_light_dir[0], gui_light_dir[1], gui_light_dir[2]));
        shaderVolume.setVec3(
            "lightDir", tempVec3[0], tempVec3[1], tempVec3[2]);
        shaderVolume.setVec3(
            "ambient", gui_ambient[0], gui_ambient[1], gui_ambient[2]);
        shaderVolume.setVec3(
            "diffuse", gui_diffuse[0], gui_diffuse[1], gui_diffuse[2]);
        shaderVolume.setVec3(
            "specular", gui_specular[0], gui_specular[1], gui_specular[2]);
        shaderVolume.setFloat("kAmb", gui_k_amb);
        shaderVolume.setFloat("kDiff", gui_k_diff);
        shaderVolume.setFloat("kSpec", gui_k_spec);
        shaderVolume.setFloat("kExp", gui_k_exp);
        shaderVolume.setBool("invertColors", gui_invert_colors);
        shaderVolume.setBool("invertAlpha", gui_invert_alpha);
        shaderVolume.setBool("sliceVolume", gui_slice_volume);
        tempVec3 = glm::normalize(
                glm::vec3(
                    gui_slice_plane_normal[0],
                    gui_slice_plane_normal[1],
                    gui_slice_plane_normal[2]));
        shaderVolume.setVec3(
            "slicePlaneNormal", tempVec3[0], tempVec3[1], tempVec3[2]);
        tempVec3 = (modelMX *
                glm::vec4(
                    gui_slice_plane_base[0] / 2.f,
                    gui_slice_plane_base[1] / 2.f,
                    gui_slice_plane_base[2] / 2.f,
                    1.f)).xyz;
        shaderVolume.setVec3(
            "slicePlaneBase", tempVec3[0], tempVec3[1], tempVec3[2]);

        glBindVertexArray(volumeVAO);
        glDrawElements(GL_TRIANGLES, 3*2*6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // draw ImGui windows
        if(_flag_show_menues)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            showSettingsWindow(
                vConf,
                volumeData,
                volumeTex,
                modelMX,
                histogramBins);
            if(gui_show_demo_window)
                ImGui::ShowDemoWindow(&gui_show_demo_window);
            if(gui_mode == static_cast<int>(Mode::transfer_function))
            {
                if(gui_show_tf_window)
                    showTransferFunctionWindow(
                        transferFunction,
                        shaderTfColor,
                        shaderTfFunc,
                        shaderTfPoint,
                        tfColorFBO,
                        tfColorTexIDs,
                        tfFuncFBO,
                        tfFuncTexIDs,
                        quadVAO);
                if(gui_show_histogram_window)
                    showHistogramWindow(
                        histogramBins,
                        vConf,
                        volumeData);
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        // --------------------------------------------------------------------
        // show the rendering result as window filling quad in the default
        // framebuffer
        // --------------------------------------------------------------------
        glViewport(0, 0, win_w, win_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        shaderQuad.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _defaultTexIDs[0]);
        shaderQuad.setInt("tex", 0);

        shaderQuad.setMat4("projMX", projMXquad);

        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteFramebuffers(1, &_defaultFBO);
    glDeleteTextures(2, _defaultTexIDs);
    glDeleteFramebuffers(1, &tfColorFBO);
    glDeleteTextures(1, tfColorTexIDs);
    glDeleteFramebuffers(1, &tfFuncFBO);
    glDeleteTextures(2, tfFuncTexIDs);

    glDeleteTextures(1, &volumeTex);

    glDeleteProgram(shaderQuad.ID);
    glDeleteProgram(shaderVolume.ID);
    glDeleteProgram(shaderFrame.ID);
    glDeleteProgram(shaderTfColor.ID);
    glDeleteProgram(shaderTfFunc.ID);
    glDeleteProgram(shaderTfPoint.ID);

    glDeleteVertexArrays(1, &frameVAO);
    glDeleteVertexArrays(1, &volumeVAO);
    glDeleteVertexArrays(1, &quadVAO);

    glfwDestroyWindow(window);
    glfwTerminate();

    if (volumeData != nullptr) cr::deleteVolumeData(vConf, volumeData);
    if (histogramBins != nullptr) delete histogramBins;

    return 0;
}

//-----------------------------------------------------------------------------
// GLFW callbacks and input processing
//-----------------------------------------------------------------------------
void cursor_position_cb(GLFWwindow *window, double xpos, double ypos)
{
    static double xpos_old = 0.0;
    static double ypos_old = 0.0;
    double dx, dy;

    dx = xpos - xpos_old; xpos_old = xpos;
    dy = ypos - ypos_old; ypos_old = ypos;

    if (GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE))
    {
        glm::vec3 polar = util::cartesianToPolar<glm::vec3>(camPos);
        float half_pi = glm::half_pi<float>();

        polar.y += glm::radians(dx) * gui_cam_rot_speed;
        polar.z += glm::radians(dy) * gui_cam_rot_speed;
        if (polar.z <= -0.999f * half_pi)
            polar.z = -0.999f * half_pi;
        else if (polar.z >= 0.999f * half_pi)
            polar.z = 0.999f * half_pi;

        camPos = util::polarToCartesian<glm::vec3>(polar);
    }
}

void mouse_button_cb(GLFWwindow* window, int button, int action, int mods)
{
    double mouseX = 0.0, mouseY = 0.0;
    glm::vec2 temp = glm::vec2(0.f);
    GLint prevFBO = 0;

    if ((button == GLFW_MOUSE_BUTTON_LEFT) && (action == GLFW_RELEASE))
    {
        if (    (gui_mode == static_cast<int>(Mode::transfer_function)) &&
                gui_show_tf_window  )
        {
            // the user might try to select a control point in the transfer
            // function editor
            glfwGetCursorPos(window, &mouseX, &mouseY);

            // store the previously bound framebuffer
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

            // bind FBO-object and the color-attachment which contains
            // the unique picking ID
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _selected_cp_fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT1);

            // send all commands to the GPU and wait until everything is drawn
            glFlush();
            glFinish();

            glReadPixels(
                static_cast<GLint>(mouseX - _tf_screen_pos.x),
                static_cast<GLint>(mouseY - _tf_screen_pos.y),
                1,
                1,
                GL_RG,
                GL_FLOAT,
                glm::value_ptr(temp));

            if (temp.g > 0.f)
                _selected_cp_pos = temp.r;

            glBindFramebuffer(GL_READ_FRAMEBUFFER, prevFBO);
        }
    }
    // chain ImGui callback
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

void scroll_cb(GLFWwindow *window, double xoffset, double yoffset)

{

    if ((GLFW_PRESS == glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) ||
        (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL)))
    {
        // y scrolling changes the distance of the camera from the origin
        camPos +=
            static_cast<float>(-yoffset) *
            gui_cam_zoom_speed *
            camPos;
    }

    // chain ImGui callback
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

void key_cb(GLFWwindow* window, int key, int scancode , int action, int mods)
{
    if((key == GLFW_KEY_ESCAPE) && (action == GLFW_PRESS))
        glfwSetWindowShouldClose(window, true);

    if((key == GLFW_KEY_F5) && (action == GLFW_PRESS))
        _flag_reload_shaders = true;

    if((key == GLFW_KEY_F10) && (action == GLFW_PRESS))
        _flag_show_menues = !_flag_show_menues;

    // chain ImGui callback
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

void char_cb(GLFWwindow* window, unsigned int c)
{
    // chain ImGui callback
    ImGui_ImplGlfw_CharCallback(window, c);
}

void framebuffer_size_cb(
    __attribute__((unused)) GLFWwindow* window,
    int width,
    int height)
{
    win_w = width;
    win_h = height;

    glDeleteFramebuffers(1, &_defaultFBO);
    glDeleteTextures(2, _defaultTexIDs);

    createDefaultFBO(_defaultFBO, _defaultTexIDs, win_w, win_h);
}

void error_cb(int error, const char* description)
{
    std::cerr << "Glfw error " << error << ": " << description << std::endl;
}

//-----------------------------------------------------------------------------
// subroutines
//-----------------------------------------------------------------------------
void applyProgramOptions(int argc, char *argv[])
{
    // Declare the supporded options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("volume", po::value<std::string>(), "volume description file")
    ;

    try
    {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            exit(EXIT_SUCCESS);
        }

        std::string desc_file = "";
        if (vm.count("volume"))
            desc_file = vm["volume"].as<std::string>();
        else
            desc_file = DEFAULT_VOLUME_JSON_FILE;

        cr::VolumeConfig tempConf = cr::VolumeConfig(desc_file);
        if(tempConf.isValid())
            strncpy(
                gui_volume_desc_file, desc_file.c_str(), MAX_FILEPATH_LENGTH);
        else
        {
            std::cout << "Invalid volume description!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    catch(std::exception &e)
    {
        std::cout << "Invalid program options!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void createDefaultFBO(
    GLuint &fbo,
    GLuint texIDs[],
    unsigned int win_w,
    unsigned int win_h)
{
    GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    GLint  internalFormats[2] = { GL_RGBA, GL_RGBA32UI };
    GLenum formats[2] = { GL_RGBA, GL_RGBA_INTEGER };
    GLenum datatypes[2] = { GL_FLOAT, GL_UNSIGNED_INT };
    GLint  filters[2] = { GL_LINEAR, GL_NEAREST };

    fbo = util::createFrameBufferObject(
            win_w,
            win_h,
            texIDs,
            2,
            attachments,
            internalFormats,
            formats,
            datatypes,
            filters);
}

GLFWwindow* createWindow(
    unsigned int win_w, unsigned int win_h, const char* title)
{
    glfwSetErrorCallback(error_cb);
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        win_w, win_h, title, nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // install callbacks
    glfwSetMouseButtonCallback(window, mouse_button_cb);
    glfwSetScrollCallback(window, scroll_cb);
    glfwSetKeyCallback(window, key_cb);
    glfwSetCharCallback(window, char_cb);
    glfwSetCursorPosCallback(window, cursor_position_cb);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);

    return window;
}

void initializeGl3w()
{
    if (gl3wInit())
    {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    if (!gl3wIsSupported(
            REQUIRED_OGL_VERSION_MAJOR, REQUIRED_OGL_VERSION_MINOR))
    {
        std::cerr << "OpenGL " << REQUIRED_OGL_VERSION_MAJOR << "." <<
            REQUIRED_OGL_VERSION_MINOR << " not supported" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << ", GLSL " <<
        glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void initializeImGui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init();

    ImGui::StyleColorsDark();
}

static GLuint createFrameVAO(
        const float vertices[4 * 8],
        const unsigned int indices[2 * 12],
        const float texCoords[3 * 8])
{
    GLuint frameVAO = 0;
    GLuint ebo = 0;
    GLuint vbo[2] = {0, 0};

    // create buffers
    glGenVertexArrays(1, &frameVAO);
    glGenBuffers(2, vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(frameVAO);

    // vertex coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(
        GL_ARRAY_BUFFER, 32 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    // vertex indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        24 * sizeof(unsigned int),
        indices,
        GL_STATIC_DRAW);

    // texture coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(
        GL_ARRAY_BUFFER, 24 * sizeof(float), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);

    // unbind vao and delete buffers that are not need anymore
    glBindVertexArray(0);
    glDeleteBuffers(2, vbo);
    glDeleteBuffers(1, &ebo);

    return frameVAO;
}

static GLuint createVolumeVAO(
        const float vertices[4 * 8],
        const unsigned int indices[3 * 2 * 6],
        const float texCoords[3 * 8])
{
    GLuint volumeVAO = 0;
    GLuint ebo = 0;
    GLuint vbo[2] = {0, 0};

    // create buffers
    glGenVertexArrays(1, &volumeVAO);
    glGenBuffers(2, vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(volumeVAO);

    // vertex coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(
        GL_ARRAY_BUFFER, 32 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    // vertex indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        36 * sizeof(unsigned int),
        indices,
        GL_STATIC_DRAW);

    // texture coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(
        GL_ARRAY_BUFFER, 24 * sizeof(float), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);

    // unbind vao and delete buffers that are not need anymore
    glBindVertexArray(0);
    glDeleteBuffers(2, vbo);
    glDeleteBuffers(1, &ebo);

    return volumeVAO;
}
static GLuint createQuadVAO(
        const float vertices[2 * 4],
        const unsigned int indices[4],
        const float texCoords[2 * 4])
{
    GLuint quadVAO = 0;
    GLuint ebo = 0;
    GLuint vbo[2] = {0, 0};

    // create buffers
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(2, vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(quadVAO);

    // vertex coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(
        GL_ARRAY_BUFFER, 8 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    // vertex indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        4 * sizeof(unsigned int),
        indices,
        GL_STATIC_DRAW);

    // texture coordinates
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(
        GL_ARRAY_BUFFER, 8 * sizeof(float), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);

    // unbind vao and delete buffers that are not need anymore
    glBindVertexArray(0);
    glDeleteBuffers(2, vbo);
    glDeleteBuffers(1, &ebo);

    return quadVAO;
}

// from imgui_demo.cpp
static void showHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void showSettingsWindow(
    cr::VolumeConfig &vConf,
    void *&volumeData,
    GLuint &volumeTex,
    glm::mat4 &modelMX,
    std::vector<util::bin_t> *&histogramBins)
{
    cr::VolumeConfig tempConf;

    ImGui::Begin("Settings");
    {
        if(ImGui::InputText(
                "volume",
                gui_volume_desc_file,
                MAX_FILEPATH_LENGTH,
                ImGuiInputTextFlags_CharsNoBlank |
                    ImGuiInputTextFlags_EnterReturnsTrue))
        {
            tempConf = cr::VolumeConfig(gui_volume_desc_file);
            if(tempConf.isValid())
            {
                vConf = tempConf;
                reloadStuff(
                    vConf,
                    volumeData,
                    volumeTex,
                    gui_timestep,
                    modelMX,
                    histogramBins,
                    gui_num_bins,
                    gui_x_min,
                    gui_x_max);
            }
        }
        ImGui::SameLine();
        showHelpMarker("Path to the volume description file");
        ImGui::Text("Mode");
        ImGui::RadioButton(
            "line of sight",
            &gui_mode,
            static_cast<int>(Mode::line_of_sight));
        ImGui::RadioButton(
            "maximum intensity projection",
            &gui_mode,
            static_cast<int>(Mode::maximum_intensity_projection));
        ImGui::RadioButton(
            "isosurface",
            &gui_mode,
            static_cast<int>(Mode::isosurface));
        ImGui::RadioButton(
            "transfer function",
            &gui_mode,
            static_cast<int>(Mode::transfer_function));

        ImGui::Spacing();

        if(ImGui::InputInt(
            "timestep",
            &gui_timestep,
            1,
            1,
            ImGuiInputTextFlags_CharsNoBlank |
                ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (gui_timestep < 0) gui_timestep = 0;
            else if(gui_timestep >
                    static_cast<int>(vConf.getNumTimesteps() - 1))
                gui_timestep = vConf.getNumTimesteps() - 1;

            reloadStuff(
                vConf,
                volumeData,
                volumeTex,
                gui_timestep,
                modelMX,
                histogramBins,
                gui_num_bins,
                gui_x_min,
                gui_x_max);
        }

        ImGui::Spacing();

        ImGui::SliderFloat(
            "step size", &gui_step_size, 0.05f, 2.f, "%.3f");

        ImGui::Spacing();

        ImGui::InputFloat("brightness", &gui_brightness, 0.01f, 0.1f);

        ImGui::Spacing();

        ImGui::Text("Gradient Calculation Method:");
        ImGui::RadioButton(
            "central differences",
            &gui_grad_method,
            static_cast<int>(Gradient::central_differences));
        ImGui::RadioButton(
            "sobel operators",
            &gui_grad_method,
            static_cast<int>(Gradient::sobel_operators));

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Isosurface"))
        {
            ImGui::SliderFloat(
                "isovalue", &gui_isovalue, 0.f, 1.f, "%.3f");
            ImGui::Checkbox("denoise", &gui_iso_denoise);
            ImGui::SliderFloat(
                "denoise radius", &gui_iso_denoise_r, 0.001f, 5.f, "%.3f");
            if (ImGui::TreeNode("Lighting"))
            {
                ImGui::SliderFloat3(
                    "light direction", gui_light_dir, -1.f, 1.f);
                ImGui::ColorEdit3("ambient", gui_ambient);
                ImGui::ColorEdit3("diffuse", gui_diffuse);
                ImGui::ColorEdit3("specular", gui_specular);

                ImGui::Spacing();

                ImGui::SliderFloat("k_amb", &gui_k_amb, 0.f, 1.f);
                ImGui::SliderFloat("k_diff", &gui_k_diff, 0.f, 1.f);
                ImGui::SliderFloat("k_spec", &gui_k_spec, 0.f, 1.f);
                ImGui::SliderFloat("k_exp", &gui_k_exp, 0.f, 50.f);
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("Transfer Function"))
        {
            ImGui::Checkbox("show histogram", &gui_show_histogram_window);
            ImGui::SameLine();
            showHelpMarker(
                "Only visible in transfer function mode.");

            ImGui::Spacing();

            ImGui::Checkbox(
                "show transfer function editor", &gui_show_tf_window);
            ImGui::SameLine();
            showHelpMarker(
                "Only visible in transfer function mode.");
        }

        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::InputFloat(
                "camera zoom speed",
                &gui_cam_zoom_speed,
                0.01f,
                0.1f,
                "%.3f");
            ImGui::SameLine();
            showHelpMarker(
                "Scroll up or down while holding CTRL to zoom.");
            ImGui::InputFloat(
                "camera rotation speed",
                &gui_cam_rot_speed,
                0.01f,
                0.1f,
                "%.3f");
            ImGui::SameLine();
            showHelpMarker(
                "Hold the middle mouse button and move the mouse to pan "
                "the camera");
           glm::vec3 polar = util::cartesianToPolar<glm::vec3>(camPos);
            ImGui::Text("phi: %.3f", polar.y);
            ImGui::Text("theta: %.3f", polar.z);
            ImGui::Text("radius: %.3f", polar.x);
            ImGui::Text(
                "Camera position: x=%.3f, y=%.3f, z=%.3f",
                camPos.x, camPos.y, camPos.z);
        }

        if (ImGui::CollapsingHeader("General"))
        {
            ImGui::Checkbox("draw frame", &gui_frame); ImGui::SameLine();
            ImGui::Checkbox("wireframe", &gui_wireframe);
            ImGui::Checkbox(
                "show ImGui demo window", &gui_show_demo_window);
            ImGui::Checkbox("invert colors", &gui_invert_colors);
            ImGui::Checkbox("invert alpha", &gui_invert_alpha);

            ImGui::Spacing();

            ImGui::Checkbox("Slice volume", &gui_slice_volume);
            ImGui::SliderFloat3(
                "slicing plane normal", gui_slice_plane_normal, -1.f, 1.f);
            ImGui::SliderFloat3(
                "slicing plane base", gui_slice_plane_base, -1.f, 1.f);
        }

        ImGui::Separator();

        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    }
    ImGui::End();
}

static void showHistogramWindow(
    std::vector<util::bin_t> *&histogramBins,
    cr::VolumeConfig vConf,
    void* volumeData)
{
    float values[(*histogramBins).size()];

    for(size_t i = 0; i < (*histogramBins).size(); i++)
    {
        if (gui_hist_semilog)
            values[i] = log10(std::get<2>((*histogramBins)[i]));
        else
            values[i] = static_cast<float>(std::get<2>((*histogramBins)[i]));
    }

    ImGui::Begin("Histogram", &gui_show_histogram_window);
    ImGui::PushItemWidth(-1);
    ImGui::PlotHistogram(
        "",
        values,
        (*histogramBins).size(),
        0,
        nullptr,
        0.f,
        gui_hist_semilog ? static_cast<int>(log10(gui_y_limit)) : gui_y_limit,
        ImVec2(0, 160));
    ImGui::PopItemWidth();
    ImGui::InputInt("y limit", &gui_y_limit, 1, 100);
    ImGui::SameLine();
    ImGui::Checkbox("semi-logarithmic", &gui_hist_semilog);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::DragFloatRange2(
            "interval",
            &gui_x_min,
            &gui_x_max,
            1.f,
            0.f,
            0.f,
            "Min: %.1f",
            "Max: %.1f");
    ImGui::InputInt("number of bins", &gui_num_bins);
    if(ImGui::Button("Regenerate Histogram"))
    {
        delete histogramBins;
        histogramBins = loadHistogramBins(
            vConf, volumeData, gui_num_bins, gui_x_min, gui_x_max);
    }
    ImGui::End();
}

/**
 * \brief Shows and handles the ImGui Window for the transfer function editor
*/
static void showTransferFunctionWindow(
        tf::TransferFuncRGBA1D &transferFunction,
        Shader &shaderTfColor,
        Shader &shaderTfFunc,
        Shader &shaderTfPoint,
        GLuint tfColorFBO,
        GLuint *tfColorTexIDs,
        GLuint tfFuncFBO,
        GLuint *tfFuncTexIDs,
        GLuint quadVAO)
{
    glm::vec4 tempVec4 = glm::vec4(0.f);
    tf::ControlPointRGBA1D cp = tf::ControlPointRGBA1D();
    static tf::controlPointSet1D::iterator cpSelected;
    tf::controlPointSet1D::iterator cpIterator;
    std::pair<tf::controlPointSet1D::iterator, bool> ret;

    // render the transfer function
    drawTfColor(
        transferFunction,
        shaderTfColor,
        tfColorFBO,
        quadVAO,
        tf_color_img_w,
        tf_color_img_h);
    drawTfFunc(
        transferFunction,
        shaderTfFunc,
        shaderTfPoint,
        tfFuncFBO,
        quadVAO,
        tf_func_img_w,
        tf_func_img_h);

    // draw the imgui elements
    ImGui::Begin("Transfer Function Editor", &gui_show_tf_window);

    ImGui::DragFloatRange2(
            "interval",
            &gui_x_min,
            &gui_x_max,
            1.f,
            0.f,
            0.f,
            "Min: %.1f",
            "Max: %.1f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    _tf_screen_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        reinterpret_cast<ImTextureID>(tfFuncTexIDs[0]),
        ImVec2(tf_func_img_w, tf_func_img_h));

    ImGui::Spacing();

    ImGui::Image(
        reinterpret_cast<ImTextureID>(tfColorTexIDs[0]),
        ImVec2(tf_color_img_w, tf_color_img_h));


    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Edit selected control point:");

    // get attributes of the selected control point
    cp = tf::ControlPointRGBA1D(_selected_cp_pos);
    cpIterator = transferFunction.getControlPoints()->find(cp);
    if (transferFunction.getControlPoints()->cend() != cpIterator)
    {
        cpSelected = cpIterator;
    }
    cp = *cpSelected;

    ImGui::SliderFloat("position##edit", &(cp.pos), gui_x_min, gui_x_max);
    ImGui::SliderFloat("slope##edit", &(cp.fderiv), -10.f, 10.f);
    ImGui::ColorEdit3("assigned color##edit", glm::value_ptr(cp.color));
    ImGui::SliderFloat("alpha##edit", &(cp.color.a), 0.f, 1.f);

    if (cp != (*cpSelected))
    {
        ret = transferFunction.updateControlPoint(cpIterator, cp);
        if (ret.second == true)
        {
            transferFunction.updateTexture(
                    transferFunction.getControlPoints()->begin()->pos,
                    transferFunction.getControlPoints()->rbegin()->pos);
            cpSelected = ret.first;
            _selected_cp_pos = cpSelected->pos;
        }
    }
    if (ImGui::Button("remove"))
    {
        if(transferFunction.getControlPoints()->size() > 1)
        {
            transferFunction.removeControlPoint(cpSelected);
            cpSelected = transferFunction.getControlPoints()->begin();
            transferFunction.updateTexture(
                std::max(
                    gui_x_min,
                    transferFunction.getControlPoints()->begin()->pos),
                std::min(
                    gui_x_max,
                    transferFunction.getControlPoints()->rbegin()->pos));
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Add new control point:");
    ImGui::SliderFloat("position", &gui_tf_cp_pos, gui_x_min, gui_x_max);
    ImGui::ColorEdit3("assigned color", gui_tf_cp_color_rgb);
    ImGui::SliderFloat("alpha", &gui_tf_cp_color_a, 0.f, 1.f);
    if(ImGui::Button("add"))
    {
        tempVec4 = glm::vec4(
                gui_tf_cp_color_rgb[0],
                gui_tf_cp_color_rgb[1],
                gui_tf_cp_color_rgb[2],
                gui_tf_cp_color_a);

        ret = transferFunction.insertControlPoint(gui_tf_cp_pos, tempVec4);
        if(ret.second == true)
        {
            transferFunction.updateTexture(
                std::max(
                    gui_x_min,
                    transferFunction.getControlPoints()->begin()->pos),
                std::min(
                    gui_x_max,
                    transferFunction.getControlPoints()->rbegin()->pos));
        }
    }

    // dynamic list of control points
    if(CONTROL_POINT_LIST)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int idx = 0;
        for (
                auto i = transferFunction.getControlPoints()->cbegin();
                i != transferFunction.getControlPoints()->cend();
                ++i)
        {

            cp = *i;

            ImGui::SliderFloat(
                (std::string("position##") + std::to_string(idx)).c_str(),
                &(cp.pos),
                gui_x_min,
                gui_x_max);
            ImGui::SliderFloat(
                (std::string("slope##") + std::to_string(idx)).c_str(),
                &(cp.fderiv),
                -1.f,
                1.f);
            ImGui::ColorEdit3(
                (std::string("assigned color##") + std::to_string(idx)).c_str(),
                glm::value_ptr(cp.color));
            ImGui::SliderFloat(
                (std::string("alpha##") + std::to_string(idx)).c_str(),
                &(cp.color.a),
                0.f,
                1.f);
            ImGui::Spacing();

            if (cp != *i)
            {
                transferFunction.updateControlPoint(i, cp);
                transferFunction.updateTexture(
                    transferFunction.getControlPoints()->begin()->pos,
                    transferFunction.getControlPoints()->rbegin()->pos);

            }

            ++idx;
        }
    }

    ImGui::End();
}

/**
 * \brief Bins the volume data according to its data type
 *
 * \param vConf configuration object for the dataset
 * \param values pointer to the volume data
 * \param numBins number of histogram bins
 * \param min lower histogram x axis limit
 * \param max upper histogram x axis limit
 *
 * \returns an vector of bin objects
 *
 * Note: vector of bins has to be deleted by the calling function
*/
std::vector<util::bin_t > *loadHistogramBins(
    cr::VolumeConfig vConf, void* values, size_t numBins, float min, float max)
{
    std::vector<util::bin_t> *bins = nullptr;

    switch(vConf.getVoxelType())
    {
        case cr::Datatype::unsigned_byte:
            bins = util::binData<unsigned_byte_t>(
                numBins,
                static_cast<unsigned_byte_t>(min),
                static_cast<unsigned_byte_t>(max),
                reinterpret_cast<unsigned_byte_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::signed_byte:
            bins = util::binData<signed_byte_t>(
                numBins,
                static_cast<signed_byte_t>(min),
                static_cast<signed_byte_t>(max),
                reinterpret_cast<signed_byte_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::unsigned_halfword:
            bins = util::binData<unsigned_halfword_t>(
                numBins,
                static_cast<unsigned_halfword_t>(min),
                static_cast<unsigned_halfword_t>(max),
                reinterpret_cast<unsigned_halfword_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::signed_halfword:
            bins = util::binData<signed_halfword_t>(
                numBins,
                static_cast<signed_halfword_t>(min),
                static_cast<signed_halfword_t>(max),
                reinterpret_cast<signed_halfword_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::unsigned_word:
            bins = util::binData<unsigned_word_t>(
                numBins,
                static_cast<unsigned_word_t>(min),
                static_cast<unsigned_word_t>(max),
                reinterpret_cast<unsigned_word_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::signed_word:
            bins = util::binData<signed_word_t>(
                numBins,
                static_cast<signed_word_t>(min),
                static_cast<signed_word_t>(max),
                reinterpret_cast<signed_word_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::unsigned_longword:
            bins = util::binData<unsigned_longword_t>(
                numBins,
                static_cast<unsigned_longword_t>(min),
                static_cast<unsigned_longword_t>(max),
                reinterpret_cast<unsigned_longword_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::signed_longword:
            bins = util::binData<signed_longword_t>(
                numBins,
                static_cast<signed_longword_t>(min),
                static_cast<signed_longword_t>(max),
                reinterpret_cast<signed_longword_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::single_precision_float:
            bins = util::binData<single_precision_float_t>(
                numBins,
                static_cast<single_precision_float_t>(min),
                static_cast<single_precision_float_t>(max),
                reinterpret_cast<single_precision_float_t*>(values),
                vConf.getVoxelCount());
            break;

        case cr::Datatype::double_precision_float:
            bins = util::binData<double_precision_float_t>(
                numBins,
                static_cast<double_precision_float_t>(min),
                static_cast<double_precision_float_t>(max),
                reinterpret_cast<double_precision_float_t*>(values),
                vConf.getVoxelCount());
            break;

        default:
            break;
    }

    return bins;
}

GLuint loadScalarVolumeTex(cr::VolumeConfig vConf, void* volumeData)
{
    GLuint tex = 0;

    switch(vConf.getVoxelType())
    {
        case cr::Datatype::unsigned_byte:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_UNSIGNED_BYTE,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::signed_byte:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_BYTE,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::unsigned_halfword:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_UNSIGNED_SHORT,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::signed_halfword:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_SHORT,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::unsigned_word:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_UNSIGNED_INT,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::signed_word:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_INT,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::single_precision_float:
            tex = util::create3dTexFromScalar(
                volumeData,
                GL_FLOAT,
                vConf.getVolumeDim()[0],
                vConf.getVolumeDim()[1],
                vConf.getVolumeDim()[2]);
            break;

        case cr::Datatype::double_precision_float:
        case cr::Datatype::unsigned_longword:
        case cr::Datatype::signed_longword:
        default:
            break;
    }

    return tex;
}

/**
 * \brief updates the volume data, texture, histogram information...
*/
static void reloadStuff(
    cr::VolumeConfig vConf,
    void *&volumeData,
    GLuint &volumeTex,
    unsigned int timestep,
    glm::mat4 &modelMX,
    std::vector<util::bin_t> *&histogramBins,
    size_t num_bins,
    float x_min,
    float x_max)
{
    glDeleteTextures(1, &volumeTex);
    if (volumeData != nullptr) cr::deleteVolumeData(vConf, volumeData);
    if (histogramBins != nullptr) delete histogramBins;

    volumeData = cr::loadScalarVolumeDataTimestep(vConf, timestep, false);
    modelMX = glm::scale(
        glm::mat4(1.f),
        glm::normalize(glm::vec3(
            static_cast<float>(vConf.getVolumeDim()[0]),
            static_cast<float>(vConf.getVolumeDim()[1]),
            static_cast<float>(vConf.getVolumeDim()[2]))));
    histogramBins = loadHistogramBins(
        vConf, volumeData, num_bins, x_min, x_max);
    volumeTex = loadScalarVolumeTex(vConf, volumeData);
}

/**
 * \brief draws the color resulting from the transfer function to an fbo object
 */
static void drawTfColor(
    tf::TransferFuncRGBA1D &transferFunc,
    Shader &shaderTfColor,
    GLuint fboID,
    GLuint quadVAO,
    unsigned int width,
    unsigned int height)
{
    GLint prevFBO = 0;

    if (!glIsFramebuffer(fboID))
        return;

    // store the previously bound framebuffer
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    // activate the framebuffer object as current framebuffer
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    // select color attachments as render targets
    static const GLenum buf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buf);

    // clear old buffer content
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set up the projection matrix
    static const glm::mat4 projMX = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

    // draw the resulting transfer function colors
    shaderTfColor.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transferFunc.getTexture());
    shaderTfColor.setInt("transferTex", 0);
    shaderTfColor.setMat4("projMX", projMX);
    shaderTfColor.setFloat("x_min", gui_x_min);
    shaderTfColor.setFloat("x_max", gui_x_max);
    shaderTfColor.setFloat("tf_interval_lower",
            transferFunc.getControlPoints()->begin()->pos);
    shaderTfColor.setFloat("tf_interval_upper",
            transferFunc.getControlPoints()->rbegin()->pos);

    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // reset framebuffer to the one previously bound
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}
/**
 * \brief draws the alpha value from the transfer function to an fbo object
 */
static void drawTfFunc(
    tf::TransferFuncRGBA1D &transferFunc,
    Shader &shaderTfFunc,
    Shader &shaderTfPoint,
    GLuint fboID,
    GLuint quadVAO,
    unsigned int width,
    unsigned int height)
{
    bool enableBlend = false;
    GLint prevFBO = 0;

    if (!glIsFramebuffer(fboID))
        return;

    // disable alpha blending to prevent discarding of fragments
    if (glIsEnabled(GL_BLEND))
    {
        glDisable(GL_BLEND);
        enableBlend = true;
    }

    // store the previously bound framebuffer
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    // activate the framebuffer object as current framebuffer
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    // select color attachments as render targets
    static const GLenum buf[2] =
        { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, buf);

    // clear old buffer content
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set up the projection matrix
    static const glm::mat4 projMX = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

    // draw the line plot of the transfer function alpha value
    shaderTfFunc.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, transferFunc.getTexture());
    shaderTfFunc.setInt("transferTex", 0);
    shaderTfFunc.setMat4("projMX", projMX);
    shaderTfFunc.setFloat("x_min", gui_x_min);
    shaderTfFunc.setFloat("x_max", gui_x_max);
    shaderTfFunc.setFloat("tf_interval_lower",
            transferFunc.getControlPoints()->begin()->pos);
    shaderTfFunc.setFloat("tf_interval_upper",
            transferFunc.getControlPoints()->rbegin()->pos);
    shaderTfFunc.setInt("width", tf_func_img_w);
    shaderTfFunc.setInt("height", tf_func_img_h);

    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, 0);

    // draw the control points
    glClear(GL_DEPTH_BUFFER_BIT);
    shaderTfPoint.use();

    shaderTfPoint.setMat4("projMX", projMX);
    shaderTfPoint.setFloat("x_min", gui_x_min);
    shaderTfPoint.setFloat("x_max", gui_x_max);

    for (
            auto i = transferFunc.getControlPoints()->cbegin();
            i != transferFunc.getControlPoints()->cend();
            ++i)
    {
        shaderTfPoint.setFloat("pos", i->pos);
        shaderTfPoint.setVec4("color", i->color);

        glDrawArrays(GL_POINTS, 0, 1);
    }

    glBindVertexArray(0);

    if (enableBlend)
        glEnable(GL_BLEND);

    // reset framebuffer to the one previously bound
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

}
