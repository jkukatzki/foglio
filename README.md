# foglio
foglio is a solution for arranging and manipulating multiple planes that are playing videos or rendering GLSL shaders.

It is based on the [NAP Framework](https://github.com/napframework/nap). A platform for visualization.

The advantages compared to doing this in a game engine such as Unity or a software vision mixer like OBS are:
  - low overhead
  - freedom of extending functionality in C++

It is still in its early stages but features so far are:
  - a separate window to set variables and have an overview
  - defining arbitrary shapes using images as a mask
  - deforming planes to align them to physical objects when using for example a projector

# Demo
https://github.com/jkukatzki/foglio/assets/77100166/042294dd-37f2-46c5-b853-674a5a679c6c

# How to build
Run the build.bat file to create a release executable.
To generate a Visual Studio solution run the generate.bat file.
For more information refer to the [NAP Framework documentation](https://docs.nap.tech/).

