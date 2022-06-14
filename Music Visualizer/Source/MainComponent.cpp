#include "../JuceLibraryCode/JuceHeader.h"
#include "Oscilloscope2D.h"
#include "Spectrum.h"
#include "RingBuffer.h"



/** The MainContentComponent is the component that holds all the buttons and
    visualizers. This component fills the entire window.
*/
class MainContentComponent   :  public AudioAppComponent,
                                public ChangeListener,
                                public Button::Listener
{
public:
    //==============================================================================
    MainContentComponent()
    {
        audioFileModeEnabled = false;
        setWantsKeyboardFocus(true);
        
        // Setup Audio
        audioTransportState = AudioTransportState::Stopped;
        formatManager.registerBasicFormats();
        audioTransportSource.addChangeListener (this);
        setAudioChannels (2, 2); // Initially Stereo Input to Stereo Output
        
        // Setup GUI
        addAndMakeVisible (&openFileButton);
        openFileButton.setButtonText ("Open File");
        openFileButton.setColour(TextButton::buttonColourId, Colours::midnightblue);
        openFileButton.addListener (this);
        
        addAndMakeVisible (&playButton);
        playButton.setButtonText ("Play");
        playButton.addListener (this);
        playButton.setColour (TextButton::buttonColourId, Colours::green);
        playButton.setEnabled (false);

        addAndMakeVisible (&stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.addListener (this);
        stopButton.setColour (TextButton::buttonColourId, Colours::red);
        stopButton.setEnabled (false);
        
        addAndMakeVisible (&oscilloscope2DButton);
        oscilloscope2DButton.setButtonText ("Oscilloscope");
        oscilloscope2DButton.setColour (TextButton::buttonColourId, Colour (0xFF0C4B95));
        oscilloscope2DButton.addListener (this);
        oscilloscope2DButton.setToggleState (false, NotificationType::dontSendNotification);
        
        addAndMakeVisible(&spectrumButton);
        spectrumButton.setButtonText ("Spectrum");
        spectrumButton.setColour (TextButton::buttonColourId, Colour (0xFF0C4B95));
        spectrumButton.addListener (this);
        spectrumButton.setToggleState (false, NotificationType::dontSendNotification);
        
        
        setSize (1200, 900); // Set Component Size
    }

    ~MainContentComponent()
    {
        shutdownAudio();
    }
    
    //==============================================================================
    // Audio Callbacks
    
    /** Called before rendering Audio. 
    */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        // Setup Audio Source
        audioTransportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
        
        // Setup Ring Buffer of GLfloat's for the visualizer to use
        // Uses two channels
        ringBuffer = new RingBuffer<GLfloat> (2, samplesPerBlockExpected * 10);
        
        
        // Allocate all Visualizers
        
        oscilloscope2D = new Oscilloscope2D (ringBuffer);
        addChildComponent (oscilloscope2D);
        
        spectrum = new Spectrum (ringBuffer);
        addChildComponent (spectrum);
    }
    
    /** Called after rendering Audio. 
    */
    void releaseResources() override
    {
        // Delete all visualizer allocations
        if (oscilloscope2D != nullptr)
        {
            oscilloscope2D->stop();
            removeChildComponent (oscilloscope2D);
            delete oscilloscope2D;
        }

        if (spectrum != nullptr)
        {
            spectrum->stop();
            removeChildComponent (spectrum);
            delete spectrum;
        }
        
        audioTransportSource.releaseResources();
        delete ringBuffer;
    }
    
    /** The audio rendering callback.
    */
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        // If no mode is enabled, do not mess with audio
        if (!audioFileModeEnabled)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }
        
        if (audioFileModeEnabled)
            audioTransportSource.getNextAudioBlock (bufferToFill);
        
        // Write to Ring Buffer
        ringBuffer->writeSamples (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
    }
    
    
    //==============================================================================
    // GUI Callbacks
    
    /** Paints UI elements and various graphics on the screen. NOT OpenGL.
        This will draw on top of the OpenGL background.
     */
    void paint (Graphics& g) override
    {
        g.setColour(Colour (0xFF252831)); // Set background color (below any GL Visualizers)
    }

    /** Resizes various JUCE Components (UI elements, etc) placed on the screen. NOT OpenGL.
    */
    void resized() override
    {
        const int w = getWidth();
        const int h = getHeight();
        
        // Button Dimenstions
        const int bWidth = (w - 30) / 2;
        const int bHeight = 20;
        const int bMargin = 10;

        openFileButton.setBounds (bMargin, bMargin, bWidth * 2, bHeight);
        
        playButton.setBounds (bMargin, 40, bWidth, 20);
        stopButton.setBounds (bMargin, 70, bWidth, 20);
        
        oscilloscope2DButton.setBounds (bWidth + 2 * bMargin, bMargin + 30, bWidth, bHeight);
        spectrumButton.setBounds (bWidth + 2 * bMargin, 70, bWidth, bHeight);
        
        
        if (oscilloscope2D != nullptr)
            oscilloscope2D->setBounds (0, 100, w, h - 100);
        if (spectrum != nullptr)
            spectrum->setBounds (0, 100, w, h - 100);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &audioTransportSource)
        {
            if (audioTransportSource.isPlaying())
                changeAudioTransportState (Playing);
            else if ((audioTransportState == Stopping) || (audioTransportState == Playing))
                changeAudioTransportState (Stopped);
            else if (audioTransportState == Pausing)
                changeAudioTransportState (Paused);
        }
    }

    void buttonClicked (Button* button) override
    {
        if (button == &openFileButton)  openFileButtonClicked();
        else if (button == &playButton)  playButtonClicked();
        else if (button == &stopButton)  stopButtonClicked();
        else if (button == &oscilloscope2DButton)
        {
            bool buttonToggleState = !button->getToggleState();
            button->setToggleState (buttonToggleState, NotificationType::dontSendNotification);
            spectrumButton.setToggleState (false, NotificationType::dontSendNotification);

            oscilloscope2D->setVisible(buttonToggleState);
            spectrum->setVisible(false);
            
            oscilloscope2D->start();
            spectrum->stop();
            resized();
        }
        
        else if (button == &spectrumButton)
        {
            bool buttonToggleState = !button->getToggleState();
            button->setToggleState (buttonToggleState, NotificationType::dontSendNotification);
            oscilloscope2DButton.setToggleState (false, NotificationType::dontSendNotification);
            
            oscilloscope2D->setVisible(false);
            spectrum->setVisible(buttonToggleState);
            
            spectrum->start();
            oscilloscope2D->stop();
            resized();
        }
    }
    
private:
    //==============================================================================
    // PRIVATE MEMBERS
    
    /** Describes one of the states of the audio transport.
    */
    enum AudioTransportState
    {
        Stopped,
        Starting,
        Playing,
        Pausing,
        Paused,
        Stopping
    };
    
    /** Changes audio transport state.
    */
    void changeAudioTransportState (AudioTransportState newState)
    {
        if (audioTransportState != newState)
        {
            audioTransportState = newState;
            
            switch (audioTransportState)
            {
                case Stopped:
                    playButton.setButtonText ("Play");
                    stopButton.setButtonText ("Stop");
                    stopButton.setEnabled (false);
                    audioTransportSource.setPosition (0.0);
                    break;
                    
                case Starting:
                    audioTransportSource.start();
                    break;
                    
                case Playing:
                    playButton.setButtonText ("Pause");
                    stopButton.setButtonText ("Stop");
                    stopButton.setEnabled (true);
                    break;
                    
                case Pausing:
                    audioTransportSource.stop();
                    break;
                    
                case Paused:
                    playButton.setButtonText ("Resume");
                    stopButton.setButtonText ("Return to Start");
                    break;
                    
                case Stopping:
                    audioTransportSource.stop();
                    break;
            }
        }
    }
    
    /** Triggered when the openButton is clicked. It opens an audio file selected by the user.
    */
    void openFileButtonClicked()
    {
        FileChooser chooser ("Select a audio file to play...", File(), "*.mp3, *.wav");
        
        if (chooser.browseForFileToOpen())
        {
            File file (chooser.getResult());
            AudioFormatReader* reader = formatManager.createReaderFor (file);
            
            if (reader != nullptr)
            {
                audioReaderSource.reset (new AudioFormatReaderSource (reader, true));
                audioTransportSource.setSource (audioReaderSource.get(), 0, nullptr, reader->sampleRate);
                playButton.setEnabled (true);
                audioFileModeEnabled = true;
            }
        }
    }
    
    

    void playButtonClicked()
    {
        if ((audioTransportState == Stopped) || (audioTransportState == Paused))
            changeAudioTransportState (Starting);
        else if (audioTransportState == Playing)
            changeAudioTransportState (Pausing);
    }
    
    void stopButtonClicked()
    {
        if (audioTransportState == Paused)
            changeAudioTransportState (Stopped);
        else
            changeAudioTransportState (Stopping);
    }

    
    //==============================================================================
    // PRIVATE MEMBER VARIABLES

    // App State
    bool audioFileModeEnabled;
    
    // GUI Buttons
    TextButton openFileButton;
    TextButton playButton;
    TextButton stopButton;
    
    TextButton oscilloscope2DButton;
    TextButton spectrumButton;
    
    // Audio File Reading Variables
    AudioFormatManager formatManager;
    std::unique_ptr<AudioFormatReaderSource> audioReaderSource;
    AudioTransportSource audioTransportSource;
    AudioTransportState audioTransportState;
    
    // Audio & GL Audio Buffer
    RingBuffer<float> * ringBuffer;
    
    // Visualizers
    Oscilloscope2D * oscilloscope2D;
    Spectrum * spectrum;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()    { return new MainContentComponent(); }
