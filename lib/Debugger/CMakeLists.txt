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
  BreakpointCommand.cpp
  ExecutionCommand.cpp
  DebugSymbolTable.cpp
  DebugUtil.cpp
  HelpCommand.cpp
  InfoCommand.cpp
  StateCommand.cpp
  PrintCommand.cpp
  SetCommand.cpp
  Prompt.cpp
  KleeDebugger.cpp
  Breakpoint.cpp
)
