# Copyright (c) 2010,2021 Joseph Koshy
# All rights reserved.

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.

# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.





# In the following definitions, `_' is an M4 macro that is meant to be
# expanded later.  Its intended usage is:
#
#   `_(ELF_SYMBOL, VALUE, OPTIONAL-DESCRIPTION)'
#
# These (deferred) macros are then grouped together into named collections.
#
# At the point of use, `_' would be defined to expand to the desired
# replacement text.
#
#   # File: example_expansion.m4
#
#   define(`_',`case $2: return ("$1");')
#   include(`elfconstants.m4')
#
#   const char *name_of_capability(int capability)
#   {
#       switch(capability) {
#       DEFINE_CAPABILITIES();
#       default:
#           return (NULL);
#       }
#   }   


# The `__' macro is used to record comments.
#
# Provide a default definition for the macro that ignores its arguments.


#
# Types of capabilities.
# 


#
# Flags used with dynamic linking entries.
#


#
# Dynamic linking entry types.
#




#
# Flags used in the executable header (field: e_flags).
#

























#
# Offsets in the `ei_ident[]` field of an ELF executable header.
#


#
# The ELF class of an object.
#


#
# Endianness of data in an ELF object.
#



#
# The magic numbers used in the initial four bytes of an ELF object.
#
# These numbers are: 0x7F, 'E', 'L' and 'F'.


# Additional ELFMAG related constants.


#
# ELF OS ABI field.
#


# OS ABI aliases.


#
# ELF Machine types: (EM_*).
#




#
# ELF file types: (ET_*).
#


# ELF file format version numbers.


#
# Flags for section groups.
#


#
# Flags / mask for .gnu.versym sections.
#


#
# Flags used by program header table entries.
#


#
# Types of program header table entries.
#




#
# Section flags.
#


#
# Special section indices.
#


#
# Section types.
#


# Aliases for section types.


#
# Symbol binding information.
#


#
# Symbol types
#


# Additional symbol type related constants.


#
# Symbol binding.
#


#
# Symbol visibility.
#


#
# Symbol flags.
#


#
# Version dependencies.
#


#
# Version flags.
#


#
# Version needs
#


#
# Versioning numbers.
#


#
# Relocation types.
#








#
# Relocation definitions from the ARM ELF ABI, version "ARM IHI
# 0044E" released on 30th November 2012.
#




































# These are the symbols used in the Sun ``Linkers and Loaders
# Guide'', Document No: 817-1984-17.  See the X86_64 relocations
# list above for the spellings used in the ELF specification.




# Obsolete relocation types.


# Alternate spellings for relocation types.




#
# Note tags
#


# Aliases for the ABI tag.


#
# Option kinds.
#


#
# ODK_EXCEPTIONS info field masks.
#


#
# ODK_PAD info field masks.
#


#
# ODK_HWPATCH info field masks and ODK_HWAND/ODK_HWOR
# info field and hwp_flags[12] masks.
#


#
# ODK_IDENT/ODK_GP_GROUP info field masks.
#


# MIPS ABI related constants.

