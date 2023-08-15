# foglio
foglio is a solution for arranging and manipulating multiple planes that are playing videos or rendering GLSL shaders.

It is based on the [NAP Framework](https://github.com/napframework/nap). A platform for visualization.

The advantages compared to doing this in a game engine such as Unity or a software vision mixer like OBS are:
  - low overhead
  - freedom of extending functionality in C++ and lower level control of graphics API
  - run time resource management

It is still in its early stages but features so far are:
  - a separate window to set variables and have an overview
  - defining arbitrary shapes using images as a mask
  - deforming planes to align them to physical objects when using for example a projector

# Demo
https://github.com/jkukatzki/foglio/assets/77100166/042294dd-37f2-46c5-b853-674a5a679c6c

# Build
You can download a built Windows binary release or build it yourself by downloading the NAP Framework release and cloning the foglio repository into the apps folder and running the build script.
To generate a Visual Studio solution run the regenerate script.
Note that macOS targets are not actively supported anymore.
For more information refer to the [NAP Documentation](https://docs.nap.tech/).
