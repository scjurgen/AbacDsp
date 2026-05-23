/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "PlaingainProcessor.h"
#include "PlaingainEditor.h"


juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    GuiConstants::setPreset(CLutPreset);
    return new AudioPluginAudioProcessorEditor(*this, m_parameters);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
