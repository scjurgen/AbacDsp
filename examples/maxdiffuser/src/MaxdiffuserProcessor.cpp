/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MaxdiffuserProcessor.h"

#include "MaxdiffuserConstants.h"
#include "MaxdiffuserEditor.h"


juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    GuiConstants::setPreset(AppSettings::loadTheme());
    GuiConstants::instance().init.WindowWidth = Constants::InitJuce::WindowWidth;
    GuiConstants::instance().init.WindowHeight = Constants::InitJuce::WindowHeight;
    GuiConstants::instance().init.TimerHertz = Constants::InitJuce::TimerHertz;
    return new AudioPluginAudioProcessorEditor(*this, m_parameters);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
