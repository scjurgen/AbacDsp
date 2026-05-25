/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "DelayProcessor.h"
#include "DelayEditor.h"

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
  GuiConstants::setPreset(CLutPreset);
  return new AudioPluginAudioProcessorEditor(*this, m_parameters);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new AudioPluginAudioProcessor();
}
