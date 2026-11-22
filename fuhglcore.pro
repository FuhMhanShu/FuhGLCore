# FuhGLCore — standalone qmake project (static library), alternative to CMake.
# Build:  qmake6 fuhglcore.pro && make        -> libfuhglcore.a
# Consumers should not use this file directly; include fuhglcore.pri instead
# (see README.md "Build" and docs/DEVELOPMENT.md).

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt          # FuhGLCore itself does not use Qt
TARGET = fuhglcore
VERSION = 0.1.0

FUHGLCORE_DIR = $$PWD
include(fuhglcore.pri)
