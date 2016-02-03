# OPC UA -> MQTT Gateway client

This client software is made for transferring OPC UA node value data to a target MQTT broker.

### Dependencies

- Compiler that supports C++11
- Qt 5.5
- FreeOpcUa library
-- Boost libraries
- Mosquitto library
-- C-Ares
-- PThread
-- OpenSSL

I personally used Qt 5.5 & MinGW32 4.9.2 while developing this and I only provide compiled library files for MinGW32 in this repository. Compiling & linking on linux shouldn't be an issue but you will probably struggle if you want to compile and link some of those libraries under Windows with MSVC.

### Repository structure

**client_qt_project**: This folder contains the source code of the client & Qt Creator project files.
**images**: Contains images related to the gateway client project.
**libraries**: This folder contains pre-compiled Windows library files for FreeOpcUa/Mosquitto & include headers.

### UML Diagram, clientside relations

![UML Diagram, simplified](images/client_hierarchy_simple.png)