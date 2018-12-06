#pragma once

#include <memory>
#include <array>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "util/util.hpp"
#include "shader.hpp"
#include "configraw.hpp"
#include "transferfunc.hpp"


namespace mvr
{
    /**
     * Basic modes for converting the volume data into color and opacity.
     */
    enum class Mode : int
    {
        line_of_sight = 0,
        maximum_intensity_projection,
        isosurface,
        transfer_function
    };

    /**
     * Gradient calculation method selection for all gradient based operations
     */
    enum class Gradient : int
    {
        central_differences = 0,
        sobel_operators
    };

    /**
     * Selection setting for which output shall be shown. Can be used for
     * debugging purposes.
     */
    enum class Output : int
    {
        volume_rendering = 0,
        random_number_generator,
        volume_data_slice
    };

    /**
     * Selection how mapped volume shall be projected onto the screen.
     */
    enum class Projection : int
    {
        perspective = 0,
        orthographic
    };

    /**
     * \brief volume renderer for dynamic 3D scalar data
     *
     * Class providing volume renderer functionality in form of an interactive
     * gui program or as batch interface.
     */
    class Renderer
    {
        public:

        Renderer();
        ~Renderer();

        int Initialize();
        int run();
        int setConfig();
        int renderImage();

        //---------------------------------------------------------------------
        // class-wide constants and default values
        //---------------------------------------------------------------------
        static const int REQUIRED_OGL_VERSION_MAJOR = 3;
        static const int REQUIRED_OGL_VERSION_MINOR = 3;

        static const size_t MAX_FILEPATH_LENGTH = 200;

        static const std::string DEFAULT_VOLUME_FILE;

        static const glm::vec3 DEFAULT_CAMERA_POSITION;
        static const glm::vec3 DEFAULT_CAMERA_LOOKAT;

        private:
        //---------------------------------------------------------------------
        // render settings and interactive program parameters
        //---------------------------------------------------------------------
        // window and gui size
        std::array<unsigned int, 2> m_windowDimensions;
        std::array<unsigned int, 2> m_renderingDimensions;
        std::array<unsigned int, 2> m_tfFuncWidgetDimensions;
        std::array<unsigned int, 2> m_tfColorWidgetDimensions;

        // output mode
        Mode m_renderMode;
        Output m_outputSelect;

        // detailled mode and output settings
        bool m_showVolumeFrame;
        bool m_showWireframe;
        bool m_showDemoWindow;
        bool m_showTfWindow;
        bool m_showHistogramWindow;
        bool m_semilogHistogram;
        int m_binNumberHistogram;
        int m_yLimitHistogramMax;
        float m_xLimitsMin;
        float m_xLimitsMax;
        bool m_invertColors;
        bool m_invertAlpha;
        std::array<float, 3> m_clearColor;

        // data selection
        std::string m_volumeDescriptionFile;
        unsigned int m_timestep;
        float m_outputDataZSlice;

        // ray casting
        float m_stepSize;
        Gradient m_gradientMethod;

        // camera settings
        float m_fovY;
        float m_zNear;
        float m_zFar;
        glm::vec3 m_cameraPosition;
        glm::vec3 m_cameraLookAt;
        float m_cameraZoomSpeed;
        float m_cameraRotationSpeed;
        float m_cameraTranslationSpeed;
        Projection m_projection;

        // isosurface mode
        float m_isovalue;
        bool m_isovalueDenoising;
        float m_isovalueDenoisingRadius;

        // lighting
        float m_brightness;
        std::array<float, 3> m_lightDirection;
        std::array<float, 3> m_ambientColor;
        std::array<float, 3> m_diffuseColor;
        std::array<float, 3> m_specularColor;
        float m_ambientFactor;
        float m_diffuseFactor;
        float m_specularFactor;
        float m_specularExponent;

        // slicing plane functionality
        bool m_slicingPlane;
        std::array<float, 3> m_slicingPlaneNormal;
        std::array<float, 3> m_slicingPlaneBase;

        // ambient occlussion functionality
        bool m_ambientOcclusion;
        float m_ambientOcclusionRadius;
        float m_ambientOcclusionProportion;
        int m_ambientOcclusionNumSamples;

        // temporary control points for transfer function
        //float gui_tf_cp_pos = 0.f;
        //float gui_tf_cp_color_rgb[3] = {0.f, 0.f, 0.0f};
        //float gui_tf_cp_color_a = 0.f;

        //---------------------------------------------------------------------
        // internals
        //---------------------------------------------------------------------
        // window and context creation
        bool m_isInitialized;
        GLFWwindow* m_window;

        // shader and rendering targets
        Shader m_shaderQuad;
        Shader m_shaderFrame;
        Shader m_shaderVolume;
        Shader m_shaderTfColor;
        Shader m_shaderTfFunc;
        Shader m_shaderTfPoint;

        std::array<util::FramebufferObject, 2> m_framebuffers;
        //std::array<std::array<util::texture::Texture2D, 2>, 2>
        //    m_framebufferTextures;
        // util::FramebufferObject m_tfColorWidgetFBO;
        // util::FramebufferObject m_tfFuncWidgetFBO;

        // geometry
        util::geometry::CubeFrame m_volumeFrame;
        util::geometry::Cube m_volumeCube;
        util::geometry::Quad m_windowQuad;
        glm::vec4 m_boundingBoxMin;
        glm::vec4 m_boundingBoxMax;

        // transformation matrices
        glm::mat4 m_volumeModelMx;
        glm::mat4 m_volumeViewMx;
        glm::mat4 m_volumeProjMx;
        glm::mat4 m_quadProjMx;

        // volume data
        std::vector<util::bin_t> m_histogramBins;
        tf::TransferFuncRGBA1D m_transferFunction;
        std::shared_ptr<cr::VolumeDataBase> m_volumeData;
        util::texture::Texture3D m_volumeTex;

        // miscellaneous
        util::texture::Texture2D m_randomSeedTex;
        float m_voxelDiagonal;
        bool m_showMenues;

        /*
        static bool _flag_show_menues = true;

        // flag for seeding the random generator in the fragment shader
        static GLuint _rngTex = 0;

        // for picking of control points in transfer function editor
        static float _selected_cp_pos = 0.f;
        static ImVec2 _tf_screen_pos = ImVec2();*/

        //---------------------------------------------------------------------
        // subroutines
        //---------------------------------------------------------------------
        void drawVolume(const util::texture::Texture2D& stateInTexture);
        void drawSettingsWindow();
        void drawHistogramWindow();
        void drawTransferFunctionWindow();
        void drawTfColor();
        void drawTfFunc();

        /**
         * \brief updates the volume data, texture, histogram information...
        */
        void loadVolume(cr::VolumeConfig volumeConfig);

        //---------------------------------------------------------------------
        // helper functions
        //---------------------------------------------------------------------
        int initializeGl3w();
        int initializeImGui();

        GLFWwindow* createWindow(
            unsigned int width, unsigned int height, const char* title);

        void resizeRendering(int width, int height);

        void createHelpMarker(const char* desc);

        //---------------------------------------------------------------------
        // glfw callback functions
        //---------------------------------------------------------------------
        void cursorPosition_cb(
                GLFWwindow *window,
                double xpos,
                double ypos);

        void mouseButton_cb(
                GLFWwindow* window,
                int button,
                int action,
                int mods);

        void scroll_cb(
                GLFWwindow* window,
                double xoffset,
                double yoffset);

        void key_cb(
                GLFWwindow* window,
                int key,
                int scancode,
                int action,
                int mods);

        void char_cb(
                GLFWwindow* window,
                unsigned int c);

        void framebufferSize_cb(
                GLFWwindow* window,
                int width,
                int height);

        void error_cb(
                int error,
                const char* description);

    };

    const std::string Renderer::DEFAULT_VOLUME_FILE =
        std::string("example/bucky.json");

    const glm::vec3 Renderer::DEFAULT_CAMERA_POSITION =
        glm::vec3(1.2f, 0.75f, 1.f);

    const glm::vec3 Renderer::DEFAULT_CAMERA_LOOKAT =
        glm::vec3(0.f);

}
