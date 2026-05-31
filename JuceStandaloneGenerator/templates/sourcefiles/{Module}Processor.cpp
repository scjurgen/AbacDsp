/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "/*MODULE_UPPER*/Editor.h"
#include "/*MODULE_UPPER*/Processor.h"


juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    GuiConstants::setPreset(AppSettings::loadTheme());
    return new AudioPluginAudioProcessorEditor(*this, m_parameters);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
