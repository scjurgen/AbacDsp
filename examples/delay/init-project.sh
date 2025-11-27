#!/usr/bin/env bash

cd project

git init

git submodule add https://github.com/juce-framework/JUCE.git 3rdparty/JUCE
git submodule add https://github.com/google/googletest.git 3rdparty/googletest
git submodule add https://github.com/scjurgen/AbacDsp.git 3rdparty/AbacDsp

