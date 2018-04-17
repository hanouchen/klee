#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
klee_add_component(kleeDebugger
  linenoise.c
  Command.cpp
  Prompt.cpp
  KleeDebugger.cpp
  Breakpoint.cpp
)
