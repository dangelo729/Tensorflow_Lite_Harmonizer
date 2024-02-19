#include "assets/sphere_bitmap.h"
#include "display.h"
#include "knob.h"
#include "button.h"
#include "audio_engine.h"
#include "assets/fellas_bitmap.h"
#include "machine_learning.h"
#include "keyboard.h"

float target = 0;
float mouthHeight = 6;
float sphereFrame = 175;
int moonCenter = 52;
int roundedSphereFrame = 175;
float otherMouthHeight = 6;
unsigned long lastMouthTime = 0;
float mouthWobbleAmount = 1.0; // Maximum deviation in mouth height due to wobble
float mouthWobbleSpeed = 2;    // Speed of mouth wobble oscillation, in Hz
float mouthWobbleTime = 0;
float moonVibratoDepth = 20; // Maximum deviation in frequency due to vibrato, in Hz
float moonVibratoRate = 6;   // Speed of vibrato oscillation, in Hz
float moonVibrato = 0;
float vibratoBuildupStart;          // Time when the note started (in seconds)
float vibratoBuildupDuration = 1.5; // Duration over which vibrato builds up (in seconds)
bool vibratoBuildupActive = false;  // Flag to track if vibrato buildup is ongoing
float targetMoonFreq;
int prevNote;
bool firstPress = true;
bool velocityPressed = false;
bool mlSetup = false;

int8_t input_data[5];
int8_t output_data[3];

float voiceFreq = scaleFreq[2][4];
float voiceFreqMain = 80;
unsigned long voiceTime = 0;
int fellasX[3] = {-5, 22, 90};
float fellasY[3] = {0, 0, 0};
float fellasTargetY[3] = {0, 0, 0};
float fellasSpeed[3] = {0.02, 0.003, 0.009};
unsigned long lastFellasMouthTime[3] = {0, 0, 0};
int fellasMouthDelay[3] = {20, 10, 10};
int fellasMouthHeight[3] = {7, 5, 9};
int fellasMouthTarget[3] = {7, 5, 8};
bool singing;

float formantFrequencies[5][3] = {
    {730.0f, 1090.0f, 2440.0f}, // AH
    {400.0f, 1700.0f, 2600.0f}, // EE

    {400.0f, 800.0f, 2600.0f},  // OH
    {350.0f, 600.0f, 2700.0f},  // OO
    {290.0f, 1870.0f, 2800.0f}, // I

};

float formantQs[5][3]{
    {14.0f * 1.3f, 12.0f * 1.3f, 9.0f * 1.3f},  // AH
    {16.0f * 1.3f, 14.0f * 1.3f, 10.0f * 1.3f}, // EE

    {25.0f * 1.3f, 14.0f * 1.3f, 10.0f * 1.3f}, // OH
    {25.0f * 1.3f, 18.0f * 1.3f, 10.0f * 1.3f}, // OO
    {25.0f * 1.3f, 12.0f * 1.3f, 10.0f * 1.3f}, // I
};

float formantGains[5][3]{
    {1.0f, 0.501f, 0.431f}, // AH
    {1.0f, 0.21f, 0.29f},   // EE

    {1.0f, .316f, 0.3f}, // OH
    {1.0f, 0.1f, 0.14f}, // OO
    {1.0f, .18f, .13f},  // I

};

void changeSphereFrame()
{

  encHandler(); // Check for encoder input

  // Check encoder direction and adjust target
  if (getEncTurn() == -1)
  {
    target += 35;
    if (target > 359)
    {
      target -= 360; // Wrap around to the beginning
    }
  }
  else if (getEncTurn() == 1)
  {
    target -= 35;
    if (target < 0)
    {
      target += 360; // Wrap around to the end
    }
  }

  float diff = target - sphereFrame;

  // Adjust diff for the shortest path
  if (diff > 180)
  {
    diff -= 360;
  }
  else if (diff < -180)
  {
    diff += 360;
  }

  float rate = .006 * diff; // You can now reduce your rate multiplier
  sphereFrame += rate;      // adjust sphereFrame towards target

  // keep sphereFrame in the range of 0-359
  if (sphereFrame > 359)
  {
    sphereFrame -= 360;
  }
  else if (sphereFrame < 0)
  {
    sphereFrame += 360;
  }

  roundedSphereFrame = round(sphereFrame);
  // Make sure roundedSphereFrame stays within the range of 0-359
  if (roundedSphereFrame < 0)
  {
    roundedSphereFrame = 0;
  }
  else if (roundedSphereFrame > 359)
  {
    roundedSphereFrame = 359;
  }

  // use roundedSphereFrame where you need an integer frame index
}

void drawMouth()
{
  int centerX = 66;      // center X position of the mouth
  int centerY = 27;      // Y position of the mouth
  int maxMouthWidth = 6; // Maximum width of the mouth
  float scaleFactor;     // Scaling factor for mouth width
  int mouthWidth;        // Final calculated mouth width
  int mouthX;            // Final calculated mouth X position

  // Normalized value between -1 and 1 representing the "rotation" of the sphere.
  // Assumes that roundedSphereFrame is between 0 and 359.
  float rotation = sin(2 * PI * roundedSphereFrame / 360.0);

  // Scale the width of the mouth based on the rotation. We want the mouth to be
  // at its widest when rotation = 0 (i.e., sphere is facing directly at us).
  scaleFactor = (1 - abs(rotation));
  mouthWidth = round(scaleFactor * maxMouthWidth);

  // Adjust the X position of the mouth based on the rotation. We want the mouth
  // to move from left to right as the sphere turns.
  if (rotation > 0)
  {
    // right
    mouthX = centerX + round(rotation * maxMouthWidth / 0.35);
  }
  else
  {
    // left
    mouthX = centerX + round(rotation * maxMouthWidth / .49);
  }

  // Only draw the mouth if its width is > 0.
  if (mouthWidth > 0)
  {
    if (roundedSphereFrame > 250 || roundedSphereFrame < 105)
    {
      display.drawRoundRect(mouthX - 2, (centerY - (mouthHeight / 2) - 2), mouthWidth + 4, mouthHeight + 4, 10, 0);
      display.fillRoundRect(mouthX, centerY - (mouthHeight / 2), mouthWidth, mouthHeight, 2, 0);
    }
  }
}

void drawOtherMouth()
{
  int otherCenterX = 63;       // center X position of the other mouth
  int otherCenterY = 27;       // Y position of the other mouth
  int otherMaxMouthWidth = 13; // Maximum width of the other mouth
  float otherScaleFactor;      // Scaling factor for other mouth width
  int otherMouthWidth;         // Final calculated other mouth width
  int otherMouthX;             // Final calculated other mouth X position

  // Normalized value between -1 and 1 representing the "rotation" of the sphere.
  // Assumes that roundedSphereFrame is between 0 and 359.
  float otherRotation = sin(2 * PI * roundedSphereFrame / 360.0);

  // Scale the width of the other mouth based on the rotation. We want the other mouth to be
  // at its widest when otherRotation = 0 (i.e., sphere is facing directly away from us).
  otherScaleFactor = (1 - abs(otherRotation));
  otherMouthWidth = round(otherScaleFactor * otherMaxMouthWidth);
  if (otherMouthWidth < 1)
  {
    otherMouthWidth = 1;
  }
  // Adjust the X position of the other mouth based on the rotation. We want the other mouth
  // to move from right to left as the sphere turns.
  if (otherRotation > 0)
  {
    // left
    otherMouthX = otherCenterX - round(otherRotation * otherMaxMouthWidth / 1.09);
  }
  else
  {
    // right
    otherMouthX = otherCenterX - round(otherRotation * otherMaxMouthWidth / 0.85);
  }

  // Only draw the other mouth if its width is > 0.
  if (otherMouthWidth > 0)
  {
    if (roundedSphereFrame > 298 || roundedSphereFrame < 35)
    {
    }
    else
    {
      display.fillRoundRect(otherMouthX - 2, (otherCenterY - (otherMouthHeight / 2) - 2), otherMouthWidth + 4, otherMouthHeight + 4, 10, 1);
      display.fillRoundRect(otherMouthX, otherCenterY - (otherMouthHeight / 2), otherMouthWidth, otherMouthHeight, 7, 0);
    }
  }
}

void openMouth()
{

  if (buttonState[1] == 0 || buttonState[2] == 0 || buttonState[3] == 0 || buttonState[4] == 0)
  {

    if (!vocalEnvelopes[0]->isActive())
    {
      for (int i = 0; i < 4; i++)
      {
        vocalEnvelopes[i]->noteOn();
      }

      singing = true;
    }
    /*
    if (!envelope2.isActive()) {
      envelope2.noteOn();
    }
    if (lowPassFreq < 3500) {
      lowPassFreq += 30;
    }
    */
    if (mouthHeight < 6)
    {

      if (lastMouthTime + 30 < millis())
      {
        mouthHeight++;
        otherMouthHeight++;
        lastMouthTime = millis();
      }
    }
    else
    {
      // If mouth is fully open, apply wobble
      mouthHeight = 6 + mouthWobbleAmount * sin(2 * PI * mouthWobbleSpeed * mouthWobbleTime);
      otherMouthHeight = 6 + mouthWobbleAmount * sin(2 * PI * mouthWobbleSpeed * mouthWobbleTime);
    }

    // Update wobble time
    mouthWobbleTime += 0.003; // Increase this for faster wobble
    if (mouthWobbleTime > 1.0)
    {
      mouthWobbleTime = 0;
    }
  }
  else if (buttonState[1] == 1 || buttonState[2] == 1 || buttonState[3] == 1 || buttonState[4] == 1)
  {

    for (int i = 0; i < 4; i++)
    {
      vocalEnvelopes[i]->noteOff();
    }
    singing = false;
    /*
    envelope2.noteOff();
    if (lowPassFreq > 20) {
      lowPassFreq -= 20;
    }

    */
    if (mouthHeight > 1)
    {

      if (lastMouthTime + 40 < millis())
      {
        mouthHeight--;
        otherMouthHeight--;
        lastMouthTime = millis();
      }
      if (mouthHeight < 1)
      {
        mouthHeight = 1;
        otherMouthHeight = 1;
      }
    }
  }
}

void updateMoonVibrato()
{
  float currentTime = millis() / 1000.0; // Current time in seconds
  if (!vocalEnvelope1.isActive() && vibratoBuildupActive)
  {
    // Reset vibrato buildup
    Serial.println("buhbuh");
    vibratoBuildupActive = false;
    moonVibratoDepth = 0;
  }

  if (velocityPressed)
  {
    velocityPressed = false;
    // I acknowledge that this is a terrible way to do this, but I'm not sure how else to do it. This is where
    //  buttonPressed is determined, and im too deep. THis code randomizes the formants for the fellas. good luck soldier,.
    // wow guess im doing the machine learning here as well. fuck me.

    // Okay potentially ignore above comments, im making structural changes

    for (int i = 0; i < 4; i++)
    {

      int randomFormant = random(0, 3);
      // Serial.println(randomFormant);

      for (int j = 0; j < 3; j++)
      {

        vocalFormants[i][j]->setBandpass(0, formantFrequencies[randomFormant][j], formantQs[randomFormant][j]);
        vocalMixers[i]->gain(j, formantGains[randomFormant][i]);
      }
    }

    // Start vibrato buildup
    vibratoBuildupStart = currentTime;
    vibratoBuildupActive = true;
  }

  if (vibratoBuildupActive)
  {
    // Calculate how far into the buildup we are
    float buildupProgress = (currentTime - vibratoBuildupStart) / vibratoBuildupDuration;
    if (buildupProgress >= 1.0)
    {
      // Buildup complete
      vibratoBuildupActive = false;
      buildupProgress = 1.0;
    }

    // Scale vibrato depth based on buildup progress
    float currentVibratoDepth = moonVibratoDepth * buildupProgress;

    // Calculate vibrato using sine wave based on absolute time
    moonVibrato = sin(2 * PI * moonVibratoRate * currentTime) * currentVibratoDepth;
  }
  else
  {
    // Calculate vibrato normally if not in buildup
    moonVibrato = sin(2 * PI * moonVibratoRate * currentTime) * moonVibratoDepth;
  }
}

void activateVoice()
{

  for (int i = 1; i < 5; i++)
  {
    if (buttonState[i] == 0)
    {
      prevNote = freqToMidi(targetMoonFreq);
      targetMoonFreq = scaleFreq[2][i];
      // Serial.println(freqToMidi(scaleFreq[2][i]));
      //  Adjust voice frequency towards target
      if (voiceFreq < scaleFreq[2][i])
      {
        if (voiceTime + 8 < millis())
        {
          voiceFreq++;
          voiceTime = millis();
        }
      }
      else if (voiceFreq > scaleFreq[2][i])
      {
        if (voiceTime + 8 < millis())
        {
          voiceFreq--;
          voiceTime = millis();
        }
      }

      // If we're at the target frequency, apply vibrato
    }
  }

  // Update vibrato time

  voiceFreqMain = voiceFreq + moonVibrato;

  vocalWaveforms[0]->frequency(voiceFreqMain);

  vocalWaveforms[1]->frequency(midiToFreq(output_data[0] * .5));
  vocalWaveforms[2]->frequency(midiToFreq(output_data[1]) * .5);
  vocalWaveforms[3]->frequency(midiToFreq(output_data[2]));
}

void drawFellasMouths()
{

  if (singing)
  {

    for (int i = 0; i < 3; i++)
    {
      if (lastFellasMouthTime[i] + fellasMouthDelay[i] < millis())
      {

        if (fellasMouthHeight[i] < fellasMouthTarget[i])
        {
          fellasMouthHeight[i]++;
        }

        lastFellasMouthTime[i] = millis();
      }
    }
  }
  else
  {

    for (int i = 0; i < 3; i++)
    {
      if (lastFellasMouthTime[i] + fellasMouthDelay[i] < millis())
      {

        if (fellasMouthHeight[i] > 3)
        {
          fellasMouthHeight[i]--;
        }

        lastFellasMouthTime[i] = millis();
      }
    }
  }
  display.drawRoundRect(fellasX[0] + 12, fellasY[0] + 20, 6, fellasMouthHeight[0], 6, 1);
  display.drawRoundRect(fellasX[1] + 11, fellasY[1] + 20, 9, fellasMouthHeight[1], 6, 1);
  display.drawRoundRect(fellasX[2] + 11, fellasY[2] + 18, 5, fellasMouthHeight[2], 5, 1);
}

void drawFellas()
{

  for (int i = 0; i < 3; i++)
  {
    display.drawBitmap(fellasX[i], fellasY[i], fellas[i], 30, 30, 1);
  }
}

void fellasEmerge()
{

  if (roundedSphereFrame < 245 && roundedSphereFrame > 86)
  {
    fellasTargetY[0] = 30;
    fellasTargetY[1] = 30;
    fellasTargetY[2] = 30;
  }
  else
  {
    fellasTargetY[0] = 2;
    fellasTargetY[1] = 4;
    fellasTargetY[2] = 5;
  }

  for (int i = 0; i < 3; i++)
  {
    float dif = fellasTargetY[i] - fellasY[i];
    fellasY[i] += dif * fellasSpeed[i];
  }
}

void adjustMixerLevels()
{
  float fellasGain; // Gain levels for the channels

  // For channel 3, we want the gain to be at its maximum when the sphere is halfway turned (i.e., roundedSphereFrame = 180).
  // We'll use a cosine function and shift the phase by 180 degrees to achieve this.
  fellasGain = 0.5 * (cos(2 * PI * roundedSphereFrame / 360.0) + 1);

  // Apply the calculated gain levels to the mixer channels.
  for (int i = 1; i < 4; i++)
  {
    vocalSummingMixer1.gain(i, fellasGain * .6);
  }
}

void fellasML()
{

  for (int i = 1; i < NUM_BUTTONS; i++)
  {
    if (buttonPressed[i])
    {
      velocityPressed = true;
      if (firstPress)
      {
        Serial.println("first press");

        output_data[0] = freqToMidi(targetMoonFreq * 1.5);
        output_data[1] = freqToMidi(targetMoonFreq * 1.25);
        output_data[2] = freqToMidi(targetMoonFreq * .5);
        prevNote = freqToMidi(targetMoonFreq);

        vocalWaveforms[1]->frequency(midiToFreq(output_data[0]));
        vocalWaveforms[2]->frequency(midiToFreq(output_data[1]));
        vocalWaveforms[3]->frequency(midiToFreq(output_data[2]));
      }
      /*
            for (int i = 1; i < 4; i++)
            {
              if (firstPress)
              {
                input_data[i] = output_data[i - 1];
              }
              else
              {
                input_data[i] = output->data.int8[i - 1];
              }
            }


         */

      input_data[1] = output_data[0];
      input_data[2] = output_data[1];
      input_data[3] = output_data[2];

      input_data[0] = prevNote + 12;
      input_data[4] = freqToMidi(targetMoonFreq) + 12;

      Serial.println("input:");
      for (int i = 0; i < 5; i++)
      {
        Serial.println(input_data[i]);
      }

      for (int i = 0; i < 5; i++)
      {
        input->data.int8[i] = input_data[i];
      }

      // Run inference.
      TfLiteStatus invoke_status = interpreter->Invoke();
      Serial.println("ml invoke");
      if (invoke_status != kTfLiteOk)
      {
        TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed");
        return;
      }

      // Assuming the model outputs a 3-value tensor.

      for (int i = 0; i < 3; i++)
      {
        output_data[i] = (output->data.int8[i]);
      }
      output_data[2] += 3;

      
      Serial.println("output:");
      for (int i = 0; i < 3; i++)
      {
        Serial.println(output_data[i]);
      }

      
      if (firstPress)
      {
        firstPress = false;
      }
      buttonPressed[i] = false;
    }
  }
}

void moonManLoop()
{

  fellasEmerge();
  drawFellas();
  drawFellasMouths();
  changeSphereFrame();
  openMouth();
  adjustMixerLevels();
  activateVoice();
  fellasML();
  updateMoonVibrato();
  display.drawBitmap(moonCenter, 0, sphere_frame[roundedSphereFrame], 32, 32, 1);
  drawMouth();
  drawOtherMouth();
}

void setupMoonManAudio()
{

  if (!mlSetup)
  {
    setupML();
    mlSetup = true;
    Serial.println("ml okay");
  }
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    buttonPressed[i] = false;
  }

  vocalWaveforms[0]->begin(.6, 110, WAVEFORM_SAWTOOTH);
  vocalWaveforms[1]->begin(.6, 110, WAVEFORM_SAWTOOTH);
  vocalWaveforms[2]->begin(.6, 110, WAVEFORM_SAWTOOTH);
  vocalWaveforms[3]->begin(.6, 110, WAVEFORM_SAWTOOTH);

  for (int i = 0; i < 3; i++)
  {

    vocalFormants[0][i]->setBandpass(0, formantFrequencies[0][i], formantQs[0][i]);
    vocalMixers[0]->gain(i, formantGains[0][i]);
  }

  for (int i = 0; i < 4; i++)
  {
    vocalEnvelopes[i]->attack(60);
    vocalEnvelopes[i]->decay(100);
    vocalEnvelopes[i]->sustain(0.5);
    vocalEnvelopes[i]->release(30);
    vibratoMixers[i]->gain(1, 1);
  }
  vocalSummingMixer1.gain(0, 1);
  vocalSummingMixer1.gain(1, .7);
  vocalSummingMixer1.gain(2, .7);
  vocalSummingMixer1.gain(3, .7);

  summingMixer1.gain(0, 0);
  summingMixer1.gain(1, 1);
  summingMixer1.gain(2, 0);
  summingMixer1.gain(3, 0);
  mainMixer.gain(1, 1);
\
  roundedSphereFrame = 175;
}

void endMoonManAudio()
{
  for (int i = 0; i < 4; i++)
  {
    vocalWaveforms[i]->amplitude(0.0);
  }
  Serial.println("moon end");
}
