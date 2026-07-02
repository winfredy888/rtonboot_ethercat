#!/bin/bash

for file in *.cpp; do
  mv "$file" "${file%.cpp}.cxx"
done
