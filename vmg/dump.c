/* Ogle - A video player
 * Copyright (C) 2001 Martin Norbäck
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
#include <string.h>
#include <inttypes.h>

#include "vmcmd.h"
#include "decoder.h"

int 
main(int argc, char *argv[])
{
  vm_cmd_t cmd;
  int i;
  registers_t state;
  link_t return_values;

  memset(&state, 0, sizeof(registers_t));

  while(!feof(stdin)) {
    fprintf(stderr, "   #   ");
    for(i = 0; i < 24; i++)
      fprintf(stderr, " %2d |", i);
    fprintf(stderr, "\nSRPMS: ");
    for(i = 0; i < 24; i++)
      fprintf(stderr, "%04x|", state.SPRM[i]);
    fprintf(stderr, "\nGRPMS: ");
    for(i = 0; i < 16; i++)
      fprintf(stderr, "%04x|", state.GPRM[i]);
    fprintf(stderr," \n");

    for(i = 0; i < 8; i++) {
      unsigned int res;
      if(feof(stdin))
	exit(0);
      scanf("%x", &res);
      cmd.bytes[i] = res;
    }
    
    vmEval_CMD(&cmd, 1, &state, &return_values);
    
    fprintf(stderr, "mnemonic:    ");
    vmPrint_mnemonic(&cmd);
    fprintf(stderr, "\n");
  }
  
  exit(0);
}

