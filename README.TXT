PROLAND 4.0 - http://proland.inrialpes.fr

A C++/OpenGL library for the real-time realistic rendering of 3D natural scenes

*******************************************************************************
* DISCLAIMER                                                                  *
*******************************************************************************

Proland is a RESEARCH PROTOTYPE, not a commercial software library. Its main
purpose is to foster research on real-time and realistic rendering algorithms.
It provides a framework and several predefined algorithms allowing researchers
to develop new ideas, without having to redevelop a full rendering engine to
show their results. In return, the new algorithms developed with Proland must
be licensed with the GPL (dual licensing with a commercial license is possible
too, see http://proland.inrialpes.fr/contact.html).

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************
* REQUIREMENTS                                                                *
*******************************************************************************

Proland requires a graphics card compatible with OpenGL 3.x. Proland has only
been tested on Windows and Linux, with NVidia graphics cards and specific 
OpenGL driver versions. IT MAY NOT WORK WITH ANY OTHER SYSTEM CONFIGURATION.

*******************************************************************************
* CONTENT                                                                     *
*******************************************************************************

This directory contains the following files:
- demo: a demo using the Proland library and its plugins (see below).
- doc: the Proland library documentation (for developpers).
- examples: small examples showing how to use each plugin (see below).
- include: the C++ header files of the library (for developpers).

DEMO --------------------------------------------------------------------------

The Proland demo is a "Google Earth"-like application using the Proland
library. It can display various scenes, including small procedural terrains,
a procedural planet, or the Earth with various features. 

The procedural scenes can be run directly (the first run precomputes some
data, so you need to launch the example again to actually run it). However
the Earth scenes need some terrain data. You need to download it by following
the instructions on http://proland.inrialpes.fr/download.html. You must then
update the demo/init.bat file with the directory where you downloaded the data
(for optimal performance, store this data on a SSD drive if possible).

To select a particular scene, launch the corresponding .bat file in demo/. The
user controls are the same for all the scenes, and are presented below.

EXAMPLES ----------------------------------------------------------------------

To run an example, simply launch the corresponding .bat file. Some examples
precompute some data on their first run, in which case you need to launch the
.bat file a second time to actually run the example itself. The user controls
are the same for all the examples, and are presented below.

USER CONTROLS -----------------------------------------------------------------

Drag mouse with left button:
  change the point the camera is looking at

Drag mouse with left button and CTRL key:
  rotate camera around the point it is looking at

Drag mouse with right button:
  rotate Sun around the Earth

Mousewheel (or PageUp / PageDown):
  move camera away / closer to the point it is looking at
