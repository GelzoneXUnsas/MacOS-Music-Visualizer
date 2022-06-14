
#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "RingBuffer.h"

/** Frequency Spectrum visualizer. Uses basic shaders, and calculates all points
    on the CPU as opposed to the OScilloscope3D which calculates points on the
    GPU.
 */

class Spectrum :    public Component,
                    public OpenGLRenderer,
                    public AsyncUpdater
{
    
public:
    Spectrum (RingBuffer<GLfloat> * ringBuffer)
    :   readBuffer (2, RING_BUFFER_READ_SIZE),
        forwardFFT (fftOrder)
    {
        // Sets the version to 3.2
        openGLContext.setOpenGLVersionRequired (OpenGLContext::OpenGLVersion::openGL3_2);
        setWantsKeyboardFocus(true);
     
        this->ringBuffer = ringBuffer;
        
        // Allocate FFT data
        fftData = new GLfloat [2 * fftSize];
        fftData2 = new GLfloat [2 * fftSize];
        
        // Attach the OpenGL context but do not start [ see start() ]
        openGLContext.setRenderer(this);
        openGLContext.attachTo(*this);
        
        // Setup GUI Overlay Label: Status of Shaders, compiler errors, etc.
        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (Justification::topLeft);
        statusLabel.setFont (Font (4.0f));
    }
    
    ~Spectrum()
    {
        // Turn off OpenGL
        openGLContext.setContinuousRepainting (false);
        openGLContext.detach();
        
        delete [] fftData;
        delete [] fftData2;
        
        // Detach ringBuffer
        ringBuffer = nullptr;
    }
    
    void handleAsyncUpdate() override
    {
        statusLabel.setText (statusText, dontSendNotification);
    }
    
    //==========================================================================
    // Oscilloscope Control Functions
    
    void start()
    {
        openGLContext.setContinuousRepainting (true);
    }
    
    void stop()
    {
        openGLContext.setContinuousRepainting (false);
    }
    
    float rot = 0.0f;
    //==========================================================================
    // OpenGL Callbacks
    
    /** Called before rendering OpenGL, after an OpenGLContext has been associated
        with this OpenGLRenderer (this component is a OpenGLRenderer).
        Sets up GL objects that are needed for rendering.
     */
    void newOpenGLContextCreated() override
    {
        // Setup Sizing Variables
        xFreqWidth = 5.0f;
        yAmpHeight = 2.0f;
        zTimeDepth = 20.0f;
        xFreqResolution = 96;
        zTimeResolution = 64;

        numVertices = xFreqResolution * zTimeResolution;
        
        // Initialize XZ Vertices
        initializeXZVertices();
        
        // Initialize Y Vertices
        initializeYVertices();
        
        // Setup Buffer Objects
        openGLContext.extensions.glGenBuffers (1, &xzVBO); // Vertex Buffer Object
        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, xzVBO);
        openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(GLfloat) * numVertices * 2, xzVertices, GL_STATIC_DRAW);
        
        
        openGLContext.extensions.glGenBuffers (1, &yVBO);
        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, yVBO);
        openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(GLfloat) * numVertices, yVertices, GL_STREAM_DRAW);
        
        openGLContext.extensions.glGenVertexArrays(1, &VAO);
        openGLContext.extensions.glBindVertexArray(VAO);
        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, xzVBO);
        openGLContext.extensions.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, yVBO);
        openGLContext.extensions.glVertexAttribPointer (1, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), NULL);
        
        openGLContext.extensions.glEnableVertexAttribArray (0);
        openGLContext.extensions.glEnableVertexAttribArray (1);

        
        // Setup Shaders
        createShaders();
    }
    
    /** Called when done rendering OpenGL, as an OpenGLContext object is closing.
        Frees any GL objects created during rendering.
     */
    void openGLContextClosing() override
    {
        shader.release();
        uniforms.release();
        
        delete [] xzVertices;
        delete [] yVertices;
    }
    
    
    /** The OpenGL rendering callback.
     */
    void renderOpenGL() override
    {
        jassert (OpenGLHelpers::isContextActive());
        
        // Setup Viewport
        const float renderingScale = (float) openGLContext.getRenderingScale();
        glViewport (0, 0, roundToInt (renderingScale * getWidth()), roundToInt (renderingScale * getHeight()));
        
        // Set background Color
        OpenGLHelpers::clear (Colours::black);
        
        // Enable Alpha Blending
        glEnable (GL_BLEND);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Use Shader Program that's been defined
        shader->use();
        
        
        // Copy data from ring buffer into FFT
        
        ringBuffer->readSamples (readBuffer, RING_BUFFER_READ_SIZE);
        FloatVectorOperations::clear (fftData, RING_BUFFER_READ_SIZE);
        FloatVectorOperations::clear (fftData2, RING_BUFFER_READ_SIZE);
        
        FloatVectorOperations::add (fftData, readBuffer.getReadPointer(0, 0), RING_BUFFER_READ_SIZE);
        FloatVectorOperations::add (fftData2, readBuffer.getReadPointer(1, 0), RING_BUFFER_READ_SIZE);
        
        // Calculate FFTs
        forwardFFT.performFrequencyOnlyForwardTransform (fftData);
        forwardFFT.performFrequencyOnlyForwardTransform (fftData2);
        
        // Find the range of values produced, so we can scale our rendering to
        // show up the detail clearly
        Range<float> maxFFTLevel = FloatVectorOperations::findMinAndMax (fftData, fftSize / 2);
        Range<float> maxFFTLevel2 = FloatVectorOperations::findMinAndMax (fftData2, fftSize / 2);
        
        // Calculate new y values and shift old y values back
        for (int i = numVertices; i >= 0; i--)
        {
            // For the first row of points, render the new height via the FFT
            if (i <= xFreqResolution/2)
            {
                const float skewedProportionY = 1.0f - std::exp (std::log (i / ((float) xFreqResolution/2 - 1.0f)) * 0.2f);
                const int fftDataIndex = jlimit (0, fftSize / 2, (int) (skewedProportionY * fftSize / 2));
                float level = 0.0f;
                float level2 = 0.0f;
                
                if (maxFFTLevel.getEnd() != 0.0f)
                    level = jmap (fftData[fftDataIndex], 0.0f, maxFFTLevel.getEnd(), 0.0f, yAmpHeight);
                if (maxFFTLevel2.getEnd() != 0.0f)
                    level2 = jmap (fftData2[fftDataIndex], 0.0f, maxFFTLevel2.getEnd(), 0.0f, yAmpHeight);
                
                yVertices[xFreqResolution - i + xFreqResolution/2] = level * 1.2f;
                yVertices[i - xFreqResolution/2] = level2 * 1.2f;

            }
            
            else // For the subsequent rows, shift back
            {
                yVertices[i] = yVertices[i - xFreqResolution];
            }
        }
        
        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, yVBO);
        openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(GLfloat) * numVertices, yVertices, GL_STREAM_DRAW);
        
        
        // Setup the Uniforms for use in the Shader
        if (uniforms->projectionMatrix != nullptr)
            uniforms->projectionMatrix->setMatrix4 (getProjectionMatrix().mat, 1, false);
        
        if (uniforms->viewMatrix != nullptr)
        {
            Matrix3D<float> scale;
            scale.mat[0] = 2.0;
            scale.mat[5] = 2.0;
            scale.mat[10] = 2.0;
            Matrix3D<float> finalMatrix = scale * getViewMatrix();
            uniforms->viewMatrix->setMatrix4 (finalMatrix.mat, 1, false);
            
        }

        // Draw the points
        openGLContext.extensions.glBindVertexArray(VAO);
        glDrawArrays (GL_POINTS, 0, numVertices);
        
        
        // Zero Out FFT for next use
        zeromem (fftData, sizeof (GLfloat) * 2 * fftSize);
        zeromem (fftData2, sizeof (GLfloat) * 2 * fftSize);
        
        // Reset the element buffers so child Components draw correctly
//        openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
//        openGLContext.extensions.glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
//        openGLContext.extensions.glBindVertexArray(0);
    }
    
    
    //==========================================================================
    // JUCE Callbacks
    
    void paint (Graphics& g) override {}
    
    void resized () override
    {
        statusLabel.setBounds (getLocalBounds().reduced (4).removeFromTop (75));
    }
    
    
    bool keyStateChanged(bool isKeyDown) override
    {
        if (isKeyDown)
        {
            if (KeyPress::isKeyCurrentlyDown(KeyPress::leftKey))
            {
                rot+=0.01;
            }
            if (KeyPress::isKeyCurrentlyDown(KeyPress::rightKey))
            {
                rot-=0.01;
            }
        }
        return false;
    }

private:
    
    //==========================================================================
    // Mesh Functions
    
    // Initialize the XZ values of vertices
    void initializeXZVertices()
    {
        
        int numFloatsXZ = numVertices * 2;
        
        xzVertices = new GLfloat [numFloatsXZ];
        
        // Variables when setting x and z
        int numFloatsPerRow = xFreqResolution * 2;
        GLfloat xOffset = xFreqWidth / ((GLfloat) xFreqResolution - 1.0f);
        GLfloat zOffset = zTimeDepth / ((GLfloat) zTimeResolution - 1.0f);
        GLfloat xStart = -(xFreqWidth / 2.0f);
        GLfloat zStart = -(zTimeDepth / 2.0f);
        
        // Set all X and Z values
        for (int i = 0; i < numFloatsXZ; i += 2)
        {
            
            int xFreqIndex = (i % (numFloatsPerRow)) / 2;
            int zTimeIndex = floor (i / numFloatsPerRow);
            
            // Set X Vertex
            xzVertices[i] = xStart + xOffset * xFreqIndex;
            xzVertices[i + 1] = zStart + zOffset * zTimeIndex;
        }
    }
    
    // Initialize the Y valies of vertices
    void initializeYVertices()
    {
        // Set all Y values to 0.0
        yVertices = new GLfloat [numVertices];
        memset(yVertices, 0.0f, sizeof(GLfloat) * xFreqResolution * zTimeResolution);
    }
    
    
    //==========================================================================
    // OpenGL Functions
    
    /** Calculates and returns the Projection Matrix.
     */
    Matrix3D<float> getProjectionMatrix() const
    {
        float w = 1.0f / (0.5f + 0.1f);
        float h = w * getLocalBounds().toFloat().getAspectRatio (false);
        return Matrix3D<float>::fromFrustum (-w, w, -h, h, 4.0f, 30.0f);
    }
    
    /** Calculates and returns the View Matrix.
     */
    Matrix3D<float> getViewMatrix() const
    {
        Matrix3D<float> viewMatrix (Vector3D<float> (0.0f, -2.0f, -10.0f));
        Matrix3D<float> rotationMatrix = viewMatrix.rotation ({ 0.0f, rot, 0.0f});
        
        
        return rotationMatrix * viewMatrix;
    }
    
    /** Loads the OpenGL Shaders and sets up the whole ShaderProgram
     */
    void createShaders()
    {
        vertexShader =
        "#version 330 core\n"
        "layout (location = 0) in vec2 xzPos;\n"
        "layout (location = 1) in float yPos;\n"
        // Uniforms
        "uniform mat4 projectionMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "out float height;\n"
        "out vec2 bound;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    height = yPos;"
        "    bound = xzPos;"
        "    gl_Position = projectionMatrix * viewMatrix * vec4(xzPos[0], yPos, xzPos[1], 1.0f);\n"
        "    gl_PointSize = 10;\n"
        "}\n";
   
        
        // Base Shader
        fragmentShader =
        "#version 330 core\n"
        "out vec4 color;\n"
        "in float height;\n"
        "in vec2 bound;\n"
        "void main()\n"
        "{\n"
        "    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;\n"
        "    float heightp = pow(height, 0.4);\n"
        "    if (dot(circCoord, circCoord) > 1.0) {\n"
        "       discard;\n"
        "   }\n"
        "    else if (dot(circCoord, circCoord) <= 1.0 && dot(circCoord, circCoord) > 0.2) {\n"
        "       color = vec4 (1.0*(2-heightp) + 0.08, 0.5*(2-heightp) + 0.01, 0.8*(2-heightp)+ 0.1, 1.0f-abs(bound[1])*0.1);\n"
        "   }\n"
        "    else {\n"
        "       color = vec4 (0.92*(2-heightp) + 0.08, 0.05*(2-heightp) + 0.01, 0.49*(2-heightp)+ 0.1, 1.0f-abs(bound[1])*0.1);\n"
        "   }\n"
        "}\n";
        

        std::unique_ptr<OpenGLShaderProgram> shaderProgramAttempt = std::make_unique<OpenGLShaderProgram> (openGLContext);
        
        if (shaderProgramAttempt->addVertexShader ((vertexShader))
            && shaderProgramAttempt->addFragmentShader ((fragmentShader))
            && shaderProgramAttempt->link())
        {
            uniforms.release();
            shader = std::move (shaderProgramAttempt);
            uniforms.reset (new Uniforms (openGLContext, *shader));
            
            statusText = "GLSL: v" + String (OpenGLShaderProgram::getLanguageVersion(), 2);
        }
        else
        {
            statusText = shaderProgramAttempt->getLastError();
        }
        
        triggerAsyncUpdate();
    }
    
    //==============================================================================
    // This class manages the uniform values that the shaders use.
    struct Uniforms
    {
        Uniforms (OpenGLContext& openGLContext, OpenGLShaderProgram& shaderProgram)
        {
            projectionMatrix.reset (createUniform (openGLContext, shaderProgram, "projectionMatrix"));
            viewMatrix.reset (createUniform (openGLContext, shaderProgram, "viewMatrix"));
        }
        
        std::unique_ptr<OpenGLShaderProgram::Uniform> projectionMatrix, viewMatrix;
        //ScopedPointer<OpenGLShaderProgram::Uniform> lightPosition;
        
    private:
        static OpenGLShaderProgram::Uniform* createUniform (OpenGLContext& openGLContext,
                                                            OpenGLShaderProgram& shaderProgram,
                                                            const char* uniformName)
        {
            if (openGLContext.extensions.glGetUniformLocation (shaderProgram.getProgramID(), uniformName) < 0)
                return nullptr;
            
            return new OpenGLShaderProgram::Uniform (shaderProgram, uniformName);
        }
    };

    // Visualizer Variables
    GLfloat xFreqWidth;
    GLfloat yAmpHeight;
    GLfloat zTimeDepth;
    int xFreqResolution;
    int zTimeResolution;
    
    int numVertices;
    GLfloat * xzVertices;
    GLfloat * yVertices;
    
    
    // OpenGL Variables
    OpenGLContext openGLContext;
    GLuint xzVBO;
    GLuint yVBO;
    GLuint VAO;/*, EBO;*/
    
    std::unique_ptr<OpenGLShaderProgram> shader;
    std::unique_ptr<Uniforms> uniforms;
    
    const char* vertexShader;
    const char* fragmentShader;
    
    // Audio Structures
    RingBuffer<GLfloat> * ringBuffer;
    AudioBuffer<GLfloat> readBuffer;    // Stores data read from ring buffer
    juce::dsp::FFT forwardFFT;
    GLfloat * fftData;
    GLfloat * fftData2;
    
    // This is so that we can initialize fowardFFT in the constructor with the order
    enum
    {
        fftOrder = 20,
        fftSize  = 1 << fftOrder // set 10th bit to one
    };
    
    // Overlay GUI
    String statusText;
    Label statusLabel;
    
    /** DEV NOTE
        If I wanted to optionally have an interchangeable shader system,
        this would be fairly easy to add. Chack JUCE Demo -> OpenGLDemo.cpp for
        an implementation example of this. For now, we'll just allow these
        shader files to be static instead of interchangeable and dynamic.
        String newVertexShader, newFragmentShader;
     */
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrum)
};
