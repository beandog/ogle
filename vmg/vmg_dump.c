/* Ogle - A video player
 * Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include <dvdread/nav_types.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#define BLOCK_SIZE 2048
typedef struct
{
  uint32_t bit_position;
  uint8_t bytes [BLOCK_SIZE];
} buffer_t;


int debug = 8;
char *program_name;


void parse_vmg_data (FILE *in)
{
  pci_t pci;
  dsi_t dsi;
  uint8_t substream;
  buffer_t buffer;
  buffer.bit_position = BLOCK_SIZE * 8;
  
  while(1) {
    int left = BLOCK_SIZE - buffer.bit_position/8;
    
    if(buffer.bit_position % 8 != 0)
      abort();
    
    memmove(&buffer.bytes[0], &buffer.bytes[buffer.bit_position/8], left);
    if(fread(&buffer.bytes[left], BLOCK_SIZE - left, 1, in) != 1) {
      perror("reading nav data");
      exit(1);
    }
    
    substream = buffer.bytes[0];
    buffer.bit_position = 8;
    
    if(substream == PS2_PCI_SUBSTREAM_ID) {
      navRead_PCI(&pci, &buffer.bytes[1]);
      buffer.bit_position += 8 * (PCI_BYTES - 1);
      navPrint_PCI(&pci);
    }
    else if(substream == PS2_DSI_SUBSTREAM_ID) {
      navRead_DSI(&dsi, &buffer.bytes[1]);
      buffer.bit_position += 8 * (DSI_BYTES - 1);
      navPrint_DSI(&dsi);
    }
    else {
      fprintf (stdout, "ps2 packet of unknown substream 0x%02x", substream);
      exit(1);
    }
  }
}



void usage()
{
  fprintf(stderr, "Usage: %s  [-d <debug_level>] file\n", 
          program_name);
}

int main(int argc, char *argv[])
{
  int c;
  FILE *infile;
  program_name = argv[0];
  
  /* Parse command line options */
  while ((c = getopt(argc, argv, "d:h?")) != EOF) {
    switch (c) {
    case 'd':
      debug = atoi(optarg);
      break;
    case 'h':
    case '?':
      usage();
      return 1;
    }
  }

  if(argc - optind != 1){
    usage();
    return 1;
  }

  infile = fopen(argv[optind], "rb");
  if(!infile) {
    fprintf (stderr, "error opening file\n");
    exit(1);
  }
  parse_vmg_data(infile);
  
  exit(0);
}
