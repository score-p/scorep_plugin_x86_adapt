# Score-P generic X86_Adapt Event Plugin

## Compilation and Installation

### Prerequisites

To compile this plugin, you need:

*   C++14 compiler

*   Score-P

*   The `x86_adapt` kernelmodule and library from [here](https://github.com/tud-zih-energy/x86_adapt)

### Building

1.  Create a build directory

        mkdir build
        cd build

2.  Invoke CMake

        cmake ..

3.  Invoke make

        make

4.  Copy it to a location listed in `LD_LIBRARY_PATH` or add current path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

> *Note:*
>
> If you have `scorep-config` in your `PATH`, it should be found by CMake.

### Installation

1.  Set the installation prefix

        cmake -DCMAKE_INSTALL_PREFIX=/path/to/install .

2.  Invoke make

        make install

> *Note:*
>
> Make sure to add the subfolder `lib` to your `LD_LIBRARY_PATH`.

## Usage

To add a x86_adapt metric to your trace, you have to add `x86_adapt_plugin` to the environment
variable `SCOREP_METRIC_PLUGINS`.

You have to add the list of the metrics you are interested in to the environment variable
`SCOREP_METRIC_X86_ADAPT_PLUGIN`.

> *Note:*
>
> Some metrics still need postprocessing. For details refere to the processor manual.

### Environment variables

*   `SCOREP_METRIC_X86_ADAPT_PLUGIN`

    Defines the list of metrics.

### If anything fails:

1.  Check whether the plugin library can be loaded from the `LD_LIBRARY_PATH`.

2.  Check whether your dataheap server is working properly and you can connect to it.

3.  Create an issue.

## Authors

*   Mario Bielert (mario.bielert at tu-dresden dot de)
