# Juce Standalone Generator

## Steps for standalone generation

- check blueprints, there are some examples.
- ./generate-juce-standalone.py --mode standalone --target-dir /path/to/juce-projects yourblueprint
- generated files are in /path/to/juce-projects/yourblueprint
- to setup the project run the init-project.sh (chmod +x first)

## Steps for example generation

- ./generate-juce-standalone.py --mode localexample yourblueprint
- generated files are in ./examples/yourblueprint
- add the folder generated to CMakeLists.txt


