           
                      How to build tkdnd
                  --------------------------

This directory contains the windows implementation of the tkdnd
extension. These files need Visual C++ 5.0 or later in order to
compile. The needed project files can be found at directories:
 * VC-5.0  -  for VC++ version 5.0 and
 * VC-6.0  -  for VC++ version 6.0
tkdnd should build "out of the box" with these project files.
(Note that VC-5.0 project files may have minor errors, since they are updated
manually to reflect changes in the version 6.0 project files. Please, if you
locate such problems report them as bugs in the Sourceforge tkdnd project.)

If you fail building tkdnd using the project files or you want
to use a different building environment (i.e. cygwin32), you
should follow these main directions:

The needed files are:
  ./tkOleDND.cpp
  ./OleDND.cpp
  ./OleDND.h
  ./tkOleDND_TEnumFormatEtc.cpp
  ./tkOleDND_TEnumFormatEtc.h
  ./tkShape.cpp
 ../generic/tkDND.c
 ../generic/tkDND.h
 ../generic/tkDNDBind.c

The only point you should pay attention is that the files
../src/tkDND.c and ../generic/tkDNDBind.c must be compiled as C++ source files,
and not as C source files. Under VC++ this can be done with the /TP switch.
If you fail in forcing your compiler to treat these files as C++ sources,
you can safely rename them to ../src/tkDND.cpp and ../generic/tkDNDBind.cpp.
If these files sre compiled as C files, then you will not be able to link the
shared object (dll), as different naming conversions will be used.

The additional include directories should be "..\generic" and "..\win".
The compilation defines should include "USE_TCL_STUBS", "USE_TK_STUBS" and
"DND_ENABLE_DROP_TARGET_HELPER" if on a recent windows version. If tkdnd fails
to compile due to missing header files, remove
"DND_ENABLE_DROP_TARGET_HELPER". This define is enabled by default.
Finally "VERSION" should be defined also, with value "1.0". But since it is
difficult to define it from the graphical interface it can be avoided.

The libraries that tkdnd should be link to are:
tclstub84.lib tkstub84.lib kernel32.lib user32.lib gdi32.lib winspool.lib
comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib
The extected name of the output dll should be "libtkdndVERSION.dll". For
example, if VERSION = "1.0", the dll should be "libtkdnd10.dll" (note the
missing "." between 1 and 0).

As of version 8.4a1, tk does not export properly the function Tk_GetHWND
through its stub table. So, if you plan to use stubs, you should
also link against tk84.lib along with tkstub84.lib. Note of course that
although stubs are enabled, the built dll will have a dependency on
the specific tk dll linked against. This problem is fixed with tk8.4a2.


George Petasis, National Centre for Scientific Research "Demokritos",
Aghia Paraskevi, Athens, Greece.
e-mail: petasis@iit.demokritos.gr
              and 
Laurent Riesterer, Rennes, France.
e-mail: (laurent.riesterer@free.fr)
