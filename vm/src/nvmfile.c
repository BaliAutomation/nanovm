//
//  NanoVM, a tiny java VM for the Atmel AVR family
//  Copyright (C) 2005 by Till Harbaum <Till@Harbaum.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

//
//  nvmfile.c
//
//  routines to store and access the NanoVM internal nvm file
//  format as generated by NanoVMTool
//

#include "types.h"
#include "debug.h"
#include "config.h"
#include "error.h"

#include "nvmfile.h"
#include "vm.h"
#include "eeprom.h"
#include "nvmfeatures.h"

#ifdef NVM_USE_FLASH_PROGRAM
# include <avr/io.h>
# include <avr/pgmspace.h>
#endif

#ifdef UNIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void nvmfile_load(char *filename, bool_t quiet) {
  FILE *file;
  u16_t size;
  u08_t *buffer;

  file = fopen(filename, "rb");
  if(!file) {
    printf("Unable to open file %s\n", filename);
    exit(-1);
  }

  // get file size
  fseek(file, 0l, SEEK_END);
  size = ftell(file);
  fseek(file, 0l, SEEK_SET);

  buffer = malloc(size);

  if(!quiet)
    printf("Loading %s, size %d\n", filename, size);

  if(fread(buffer, 1l, size, file) != size) {
    perror("fread()");
    exit(-1);
  }

  DEBUG_HEXDUMP(buffer, size);

  // store in uvm buffer
  nvmfile_store(0, buffer, size);

  fclose(file);
}
#endif // UNIX

// buffer for file itself is in eeprom

#ifdef NVM_USE_FLASH_PROGRAM
static u08_t nvmfile[CODESIZE] PROGMEM =
#include "nvmdefault.h"
#else
static u08_t EEPROM nvmfile[CODESIZE] =
#include "nvmdefault.h"
#endif

u08_t nvmfile_constant_count;

void *nvmfile_get_base(void) {
  return nvmfile;
}

#ifdef NVM_USE_FLASH_PROGRAM

void nvmfile_read(void *dst, void *src, u16_t len) {
  src = NVMFILE_ADDR(src);  // remove marker (if present)
  memcpy_P(dst, (PGM_P)src, len);
}

u08_t nvmfile_read08(void *addr) {
  u08_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  memcpy_P((u08_t*)&val, (PGM_P)addr, sizeof(val));
  return val;
}

u16_t nvmfile_read16(void *addr) {
  u16_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  memcpy_P((u08_t*)&val, (PGM_P)addr, sizeof(val));
  return val;
}

u32_t nvmfile_read32(void *addr) {
  u32_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  memcpy_P((u08_t*)&val, (PGM_P)addr, sizeof(val));
  return val;
}

void nvmfile_write08(void *addr, u08_t data) {
  //no write
}

void nvmfile_write_initialize(void) {

}

void nvmfile_write_finalize(void) {

}

#else // NVM_USE_FLASH_PROGRAM

void nvmfile_read(void *dst, void *src, u16_t len) {
  src = NVMFILE_ADDR(src);  // remove marker (if present)
  eeprom_read_block(dst, (eeprom_addr_t)src, len);
}

u08_t nvmfile_read08(void *addr) {
  u08_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  eeprom_read_block((u08_t*)&val, (eeprom_addr_t)addr, sizeof(val));
  return val;
}

u16_t nvmfile_read16(void *addr) {
  u16_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  eeprom_read_block((u08_t*)&val, (eeprom_addr_t)addr, sizeof(val));
  return val;
}

u32_t nvmfile_read32(void *addr) {
  u32_t val;
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  eeprom_read_block((u08_t*)&val, (eeprom_addr_t)addr, sizeof(val));
  return val;
}

void nvmfile_write08(void *addr, u08_t data) {
  addr = NVMFILE_ADDR(addr);  // remove marker (if present)
  eeprom_write_byte((eeprom_addr_t)addr, data);
}

#endif // NVM_USE_FLASH_PROGRAM

bool_t nvmfile_init(void) {
  u32_t features = nvmfile_read32(&((nvm_header_t*)nvmfile)->magic_feature);
  DEBUGF("NVM_MAGIC_FEAUTURE[file] = %x\n", features);
  DEBUGF("NVM_MAGIC_FEAUTURE[vm] = %x\n", NVM_MAGIC_FEAUTURE);

  if ((features&NVM_MAGIC_FEAUTURE)!=(features|NVMFILE_MAGIC)) {
    error(ERROR_NVMFILE_MAGIC);
    return FALSE;
  }

  if(nvmfile_read08(&((nvm_header_t*)nvmfile)->version) != NVMFILE_VERSION) {
    error(ERROR_NVMFILE_VERSION);
    return FALSE;
  }

  u16_t t = nvmfile_read16(&((nvm_header_t*)nvmfile)->string_offset);
  t      -= nvmfile_read16(&((nvm_header_t*)nvmfile)->constant_offset);
  nvmfile_constant_count = t/4;

  return TRUE;
}

void nvmfile_store(u16_t index, u08_t *buffer, u16_t size) {
#ifdef DEBUG
  // this check is not required in real life, since the code
  // limit is verified by the upload tool and by the compiler for
  // the default code
  if(size > CODESIZE) {
    DEBUGF("Code size exceeds buffer size (%d > %d)\n",
	   size, CODESIZE);
    for(;;);
  }
#endif

  eeprom_write_block(buffer, (eeprom_addr_t)(nvmfile + index), size);
}

nvm_method_hdr_t *nvmfile_get_method_hdr(u16_t index) {
  // get pointer to method header
  nvm_method_hdr_t *hdrs =
    ((nvm_method_hdr_t*)(nvmfile +
	 nvmfile_read16(&((nvm_header_t*)nvmfile)->method_offset)))+index;

  return(hdrs);
}

u32_t nvmfile_get_constant(u08_t index) {
  if (index<nvmfile_constant_count)
  {
    u16_t addr = nvmfile_read16(&((nvm_header_t*)nvmfile)->constant_offset);
    u32_t result = nvmfile_read32(nvmfile+addr+4*index);
    DEBUGF("  constant = 0x%08x\n", result);
    return result;
  }
  // it's a string!

  DEBUGF("  constant string index = %i\n", index);
  nvm_ref_t res = NVM_TYPE_CONST | (index-nvmfile_constant_count);
  return res;
}

void nvmfile_call_main(void) {
  u08_t i;

  for(i=0;i<nvmfile_read08(&((nvm_header_t*)nvmfile)->methods);i++) {
    // is this a clinit method?
    if(nvmfile_read08(&nvmfile_get_method_hdr(i)->flags) & FLAG_CLINIT) {
      DEBUGF("calling clinit %d\n", i);
      vm_run(i);
    }
  }

  // determine method description address and code
  vm_run(nvmfile_read16(&((nvm_header_t*)nvmfile)->main));
}

void *nvmfile_get_addr(u16_t ref) {
  // get pointer to string
  u16_t *refs =
    (u16_t*)(nvmfile +
	     nvmfile_read16(&((nvm_header_t*)nvmfile)->string_offset));

  return((u08_t*)refs + nvmfile_read16(refs+ref));
}

u08_t nvmfile_get_class_fields(u08_t index) {
  return nvmfile_read08(&((nvm_header_t*)nvmfile)->class_hdr[index].fields);
}

u08_t nvmfile_get_static_fields(void) {
  return nvmfile_read08(&((nvm_header_t*)nvmfile)->static_fields);
}

#ifdef NVM_USE_INHERITANCE
u08_t nvmfile_get_method_by_fixed_class_and_id(u08_t class, u08_t id) {
  u08_t i;
  nvm_method_hdr_t mhdr, *mhdr_ptr;

  DEBUGF("Searching for class "DBG8", method "DBG8"\n", class, id);

  for(i=0;i<nvmfile_read08(&((nvm_header_t*)nvmfile)->methods);i++) {
    DEBUGF("Method %d ", i);
    // load new method header into ram
    mhdr_ptr = nvmfile_get_method_hdr(i);
    nvmfile_read(&mhdr, mhdr_ptr, sizeof(nvm_method_hdr_t));
    DEBUGF("id = #"DBG16"\n", mhdr.id);

    if(((mhdr.id >> 8) == class) && ((mhdr.id & 0xff) == id)) {
      DEBUGF("Match!\n");
      return i;
    }
  }

  DEBUGF("No matching method in this class\n");
  return 0xff;
}

u08_t nvmfile_get_method_by_class_and_id(u08_t class, u08_t id) {
  u08_t mref;

  for(;;) {
    if((mref = nvmfile_get_method_by_fixed_class_and_id(class, id)) != 0xff)
      return mref;

    DEBUGF("Getting super class of %d ", class);
    class = nvmfile_read08(&((nvm_header_t*)nvmfile)->class_hdr[class].super);
    DEBUGF("-> %d\n", class);
  }

  return 0;
}
#endif