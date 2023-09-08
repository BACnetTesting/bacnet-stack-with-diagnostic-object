A "new" port of Steve's BACnet Stack to Microsoft Visual Studio C++
===================================================================

I am the original author of the /ports/win32/Microsoft Visual Studio 2015 (or before). Others came after me and one of us
made the /ports/win32/Microsoft Visual Studio 2019... etc.


I think it is time to take a new approach

- Currently this compiles on (the free!) Microsoft Visual Studio Community 2022. The intent is to updgrade and maintain for the latest at all times.
- No more "win32" - the stack runs just fine as a 32- or 64-bit application - so just "win"
- No more "Microsoft Visual Studion 2015" - just "win"
- One Microsoft Solution, multiple Microsoft Projects, as follows...
- One Project for the stack itself "bacnet-stack"
- A [sub-]Project for each of the example applications, e.g. "server"
- Each sub-project references the "bacnet-stack" project (i.e. Treats "bacnet-stack" as a library)
- Use of Microsoft's Property Sheets for a single place to set solution-wide settings (macros, include paths)



To use
------

- Fire up Microsoft Visual Studio (or double-click on "bacnet-stack.sln")
- Choose your project to run ("server")
- Choose your platform (x86 or x64)
- In the properties sheet, on the debugger settings
	- for program arguments, put in a parameter for Device Object ID and a second for Device Object Name . e.g. 260002 BITS_Demo_Server
	- for environment, put in your chosen adapter (IP address). You do have multiple adapters, don't you?' e.g. BACNET_IFACE=127.0.0.1
- Choose "Debug/Start Debugging" (or just "F5")

- On another machine, fire up your BACnet Browser and discover the app



Notes
-----

BACNET_STACK_EXPORT is defined as BACNET_STACK_STATIC_DEFINE in the "bacnet-stack" property sheet for the solution
the "regular" as opposed the "routed" NPDU handler is selected in each of the application projects ( e.g. "server" )

History
-------

2023-09-06 Edward Hague		-	Created this new Port



Todo
----

Ensure MingW32 still compiles \ports\win
so far, only 'server' app compiles and tested
so far, only 32-bit (x86) compiles and tested
Only the BACnet/IP and MS/TP datalinks have been copied across from ports\win32 to ports\win. Complete the rest.
When all win32 options have been moved and tested, delete the ports\win32 direcory
There are LOTS of compiler warnings - eliminate them
Compile as C++ code (even the C-modules)
CPPCheck
lint







