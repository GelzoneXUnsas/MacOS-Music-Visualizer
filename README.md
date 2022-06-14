<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li>
          <a href="#overview">Overview</a>
          <ul>
            <li><a href="#juce-implementation">JUCE Implementation</a></li>
            <li><a href="#oscilloscope">Oscilloscope</a></li>
            <li><a href="#spectrum">Spectrum</a></li>
          </ul>
        </li>
    <li><a href="#references">References</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

# Music Visualizer

## About The Project
A 3D audio visualizer that reads in audio files, shows visualizations rendered with OpenGL. The visualizations will be rendered in real-time, and fade to black when it’s far from the viewport/camera.<br /><br />
[![Visualizer Preview](https://github.com/GelzoneXUnsas/MacOS-Music-Visualizer/blob/main/Music%20Visualizer/HTML/src/Spectrum.png?raw=true)](https://drive.google.com/file/d/11BHjdlza270u0R0LCiln8Ry4DRV38nZ6/view?usp=sharing)
  * Click on the image above for Demo

## Overview
#### JUCE Implementation
After getting the audio sample per block from the audio file reader based on the sample rate, the ring buffer of GLfloat type can be used for the visualizer. I have set up the size of the buffer to be 10 times bigger than the expected block size for real-time use.
```c++
// Setup Audio Source
audioTransportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
// Setup Ring Buffer of GLfloat's for the visualizer to use
// Uses two channels
ringBuffer = new RingBuffer (2, samplesPerBlockExpected * 10);
```
#### Oscilloscope
  ##### Definition
  An oscilloscope uses a two-axis graph to give a visual representation of a waveform over time, with the horizontal axis representing time and the vertical axis representing the amplitude. In music production, oscilloscopes are commonly used to assist dynamics processing and sound synthesis.

  ##### Calculation
  For the oscilloscope calculation, I sum up the 2 channels in the ring buffer and further pass the sample data into the shader as a uniform variable to perform amplitude calculation which then maps to positions.
  
  Rendering the oscilloscope:
Not in action             |  In action
:-------------------------:|:-------------------------:
![](https://github.com/GelzoneXUnsas/MacOS-Music-Visualizer/blob/main/Music%20Visualizer/HTML/src/osc_not_running.png?raw=true)  |  ![](https://github.com/GelzoneXUnsas/MacOS-Music-Visualizer/blob/main/Music%20Visualizer/HTML/src/osc_running.png?raw=true)
  
#### Spectrum
  ##### Definition
  A sound spectrum displays the different frequencies present in a sound. is a representation of a sound – usually a short sample of a sound – in terms of the amount of vibration at each individual frequency. It is usually presented as a graph of either power or pressure as a function of frequency.

  ##### Calculation
  For the spectrum calculation, Instead of summing up the 2 channels in the ring buffer, I keep them separately for the FFT calculation to show left and right channels individually on either half of the spectrum.
```c++
// Get the data from the ring buffer<br>
FloatVectorOperations::add (fftData, readBuffer.getReadPointer(0, 0), RING_BUFFER_READ_SIZE);
FloatVectorOperations::add (fftData2, readBuffer.getReadPointer(1, 0), RING_BUFFER_READ_SIZE);
// Calculate FFTs
forwardFFT.performFrequencyOnlyForwardTransform (fftData);
forwardFFT.performFrequencyOnlyForwardTransform (fftData2);
// Find the range of values produced for excluding samples
Range maxFFTLevel = FloatVectorOperations::findMinAndMax (fftData, fftSize / 2);
Range maxFFTLevel2 = FloatVectorOperations::findMinAndMax (fftData2, fftSize / 2);
```
  Rendering the spectrum on GL_POINTS for simplity, plan to change to a mesh for future implementation:
Not in action             |  In action
:-------------------------:|:-------------------------:
![](https://github.com/GelzoneXUnsas/MacOS-Music-Visualizer/blob/main/Music%20Visualizer/HTML/src/spec_not_running.png?raw=true)  |  ![](https://github.com/GelzoneXUnsas/MacOS-Music-Visualizer/blob/main/Music%20Visualizer/HTML/src/spec_running.png?raw=true)

### References
[Templete Code by TimArt](https://github.com/TimArt/3DAudioVisualizers)<br />
[OpenGL - JUCE](https://docs.juce.com/master/tutorial_open_gl_application.html)<br />
[Audio player - JUCE](https://docs.juce.com/master/tutorial_playing_sound_files.html)<br />
[What is a sound spectrum](https://newt.phys.unsw.edu.au/jw/sound.spectrum.html)<br />
[What is an oscilloscope](https://www.musicradar.com/tuition/tech/what-is-an-oscilloscope-601111#:~:text=An%20oscilloscope%20uses%20a%20two,dynamics%20processing%20and%20sound%20synthesis.)<br />

### Contact
Lucas Li - yli76@calpoly.edu<br />
