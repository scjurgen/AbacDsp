# TODO

## UI

Goal: Integrate Themes: 
- use memory JuceStandaloneGenerator workflow (feedback_juce_generator)
- apply the changes in the JuceStandaloneGenerator/templates/sourcefiles and eventually in blueprints/guisandbox.json
- for this refactoring we will use JuceStandaloneGenerator/blueprints/guisandbox.json  (```./generate-juce-standalone.py guisandbox```)
- add a menu with settings containing Theme Settings and Audio Settings.
- the themes are in GuiConstants.h.

Step 1: user wants to change theme, selects theme, needs to restart the application so the theme will be set. 
Step 2: the theme will be applied directly and implies more refactoring
Step 3: clean up GuiConstants.h, there are color names that are not anymore reflecting the intent (e.g. dark, grey, bg -> background, gd -> gradient). Analyse and propose changes
Step 4: Audiosettings should be also customized with the theme colors

## DSP Library


## Project Generator

- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Save presets
- Midi only
- 5.1

-- 

Metronome: set colours, more prominent, 8th rhythms should be on 1/4 speed
