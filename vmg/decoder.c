/* Ogle - A video player
 * Copyright (C) 2000, 2001 Martin Norb�ck, H�kan Hjort
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

#include <dvdread/ifo_types.h> // vm_cmd_t
#include "vmcmd.h"
#include "decoder.h"

#ifndef bool
typedef int bool;
#endif

typedef struct
{
  uint8_t bits[8];
  uint8_t examined[8];
} cmd_t;

// Fix theses two.. pass as parameters instead.
static cmd_t cmd;
static registers_t *state;

/* Get count bits of command from byte and bit position. */
static uint32_t bits(int byte, int bit, int count)
{
  uint32_t val = 0;
  int bit_mask;
  
  while(count--) {
    if(bit > 7) {
      bit = 0;
      byte++;
    }
    bit_mask = 0x01 << (7 - bit);
    val <<= 1;
    if(cmd.bits[byte] & bit_mask)
      val |= 1;
    cmd.examined[byte] |= bit_mask;
    bit++;
  }
  return val;
}


/* Eval register code, can either be system or general register.
   SXXX_XXXX, where S is 1 if it is system register. */
static uint16_t eval_reg(uint8_t reg)
{
  if(reg & 0x80) {
    // assert(reg & 0x7f < 24);
    return state->SPRM[reg & 0x1f]; // FIXME max 24 not 32
  } else {
    // assert(reg < 16);
    return state->GPRM[reg & 0x0f];
  }
}

/* Eval register or immediate data.
   AAAA_AAAA BBBB_BBBB, if immediate use all 16 bits for data else use
   lower eight bits for the system or general purpose register. */
static uint16_t eval_reg_or_data(int imm, int byte)
{
  if(imm) { // immediate
    return bits(byte, 0, 16);
  } else {
    return eval_reg(bits(byte + 1, 0, 8));
  }
}

/* Eval register or immediate data.
   xBBB_BBBB, if immediate use all 7 bits for data else use
   lower four bits for the general purpose register number. */
/* Evaluates gprm or data depending on bit, data is in byte n */
uint16_t eval_reg_or_data_2(int imm, int byte)
{
  if(imm) /* immediate */
    return bits(byte, 1, 7);
  else
    return state->GPRM[bits(byte, 4, 4)];
}


/* Compare data using operation, return result from comparison. 
   Helper function for the different if functions. */
static bool eval_compare(uint8_t operation, uint16_t data1, uint16_t data2)
{
  switch(operation) {
    case 1:
      return data1 & data2;
    case 2:
      return data1 == data2;
    case 3:
      return data1 != data2;
    case 4:
      return data1 >= data2;
    case 5:
      return data1 >  data2;
    case 6:
      return data1 <= data2;
    case 7:
      return data1 <  data2;
  }
  fprintf(stderr,"eval_compare: Invalid comparison code\n");
  return 0;
}


/* Evaluate if version 1.
 * PPP0**** 0CCC**** ******** AAAAAAAA ******** BBBBBBBB ******** ******** 
 * PPP0**** 1CCC**** ******** AAAAAAAA DDDDDDDD DDDDDDDD ******** ********
 * where PPP is 000, 001 or 101, 110
 * "if (r[A] `C` r[B]) then .." resp "if (r[A] `C` D) then .." */
static bool eval_if_version_1(void)
{
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(3, 0, 8)), 
                            eval_reg_or_data(bits(1, 0, 1), 4));
  }
  return 1;
}

/* Evaluate if version 2.
 * PPPQ**** 0CCC**** ******** ******** ******** ******** AAAAAAAA BBBBBBBB
 * where PPP is 001 & Q=1 or PPP is 010 & Q=*
 * "if(r[A] `C` r[B]) then .." */
static bool eval_if_version_2(void)
{
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(6, 0, 8)), eval_reg(bits(7, 0, 8)));
  }
  return 1;
}

/* Evaluate if version 3.
 * PPP***** 0CCC**** AAAAAAAA ******** ******** ******** ******** BBBBBBBB
 * PPP***** 1CCC**** AAAAAAAA ******** ******** ******** DDDDDDDD DDDDDDDD
 * where PPP is 011
 * "if (r[A] `C` r[B]) then .." resp. "if (r[A] `C` D) then .." */
static bool eval_if_version_3(void)
{
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(2, 0, 8)), 
                            eval_reg_or_data(bits(1, 0, 1), 6));
  }
  return 1;
}

/* Evaluate if version 4.
 * PPP***** 0CCCAAAA ******** ******** ******** BBBBBBBB ******** ********
 * PPP***** 1CCCAAAA ******** ******** DDDDDDDD DDDDDDDD ******** ********
 * where PPP is 100
 * "if (g[A] `C` r[B]) then .." resp "if (g[A] `C` D) then .." */
static bool eval_if_version_4(void)
{
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(1, 4, 4)), 
                            eval_reg_or_data(bits(1, 0, 1), 4));
  }
  return 1;
}

/* Evaluate if version 5.
 * PPP1**** 0CCC**** ******** ******** AAAAAAAA BBBBBBBB ******** ********
 * where PPP is 101, 110
 * "if (g[A] `C` r[B]) .." */
static bool eval_if_version_5(void)
{
  uint8_t op = bits(1, 1, 3);
  if(op) {
    return eval_compare(op, eval_reg(bits(4, 0, 8)), eval_reg(bits(5, 0, 8)));
  }
  return 1;
}

/* Evaluate special instruction.... returns the new row/line number,
   0 if no new row and 256 if Break. */
static int eval_special_instruction(bool cond)
{
  int line, level;
  
  switch(bits(1, 4, 4)) {
    case 0: // NOP
      line = 0;
      return cond ? line : 0;
    case 1: // Goto line
      line = bits(7, 0, 8);
      return cond ? line : 0;
    case 2: // Break
      // max number of rows < 256, so we will end this set
      line = 256;
      return cond ? 256 : 0;
    case 3: // Set temporary parental level and goto
      line = bits(7, 0, 8); 
      level = bits(6, 4, 4);
      if(cond) {
	// This always succeeds now, if we want real parental protection
	// we need to ask the user and have passwords and stuff.
	state->SPRM[13] = level;
      }
      return cond ? line : 0;
  }
  return 0;
}

/* Evaluate link by subinstruction.
   Return 1 if link, or 0 if no link
   Actual link instruction is in return_values parameter */
static bool eval_link_subins(bool cond, link_t *return_values)
{
  uint16_t button = bits(6, 0, 6);
  uint8_t  linkop = bits(7, 3, 5);
  
  if(linkop > 0x10)
    return 0;    // Unknown Link by Sub-Instruction command

  // Assumes that the link_cmd_t enum has the same values as the LinkSIns codes
  return_values->command = linkop;
  return_values->data1 = button;
  if(cond && return_values->data1)
    state->SPRM[8] = return_values->data1 << 10;
  if(linkop == LinkNoLink) /* Obviously LinkNoLink isn't a link ;) */
    return 0;
  else
    return cond;
}


/* Evaluate link instruction.
   Return 1 if link, or 0 if no link
   Actual link instruction is in return_values parameter */
static bool eval_link_instruction(bool cond, link_t *return_values)
{
  uint8_t op = bits(1, 4, 4);
  
  switch(op) {
    case 1:
	return eval_link_subins(cond, return_values);
    case 4:
	return_values->command = LinkPGCN;
	return_values->data1   = bits(6, 1, 15);
	return cond;
    case 5:
	return_values->command = LinkPTTN;
	return_values->data1 = bits(6, 6, 10);
	return_values->data2 = bits(6, 0, 6);
	if(cond && return_values->data2)
	  state->SPRM[8] = return_values->data2 << 10;
	return cond;
    case 6:
	return_values->command = LinkPGN;
	return_values->data1 = bits(7, 1, 7);
	return_values->data2 = bits(6, 0, 6);
	if(cond && return_values->data2)
	  state->SPRM[8] = return_values->data2 << 10;
	return cond;
    case 7:
	return_values->command = LinkCN;
	return_values->data1 = bits(7, 0, 8);
	return_values->data2 = bits(6, 0, 6);
	if(cond && return_values->data2)
	  state->SPRM[8] = return_values->data2 << 10;
	return cond;
  }
  return 0;
}


/* Evaluate a jump instruction.
   returns 1 if jump or 0 if no jump
   actual jump instruction is in return_values parameter */
static bool eval_jump_instruction(bool cond, link_t *return_values)
{
  
  switch(bits(1, 4, 4)) {
    case 1:
      return_values->command = Exit;
      return cond;
    case 2:
      return_values->command = JumpTT;
      return_values->data1 = bits(5, 1, 7);
      return cond;
    case 3:
      return_values->command = JumpVTS_TT;
      return_values->data1 = bits(5, 1, 7);
      return cond;
    case 5:
      return_values->command = JumpVTS_PTT;
      return_values->data1 = bits(5, 1, 7);
      return_values->data2 = bits(2, 6, 10);
      return cond;
    case 6:
      switch(bits(5,0,2)) {
        case 0:
          return_values->command = JumpSS_FP;
          return cond;
        case 1:
          return_values->command = JumpSS_VMGM_MENU;
          return_values->data1 =  bits(5, 4, 4);
          return cond;
        case 2:
          return_values->command = JumpSS_VTSM;
          return_values->data1 =  bits(4, 0, 8);
          return_values->data2 =  bits(3, 0, 8);
          return_values->data3 =  bits(5, 4, 4);
          return cond;
        case 3:
          return_values->command = JumpSS_VMGM_PGC;
          return_values->data1 =  bits(2, 1, 15);
          return cond;
        }
      break;
    case 8:
      switch(bits(5,0,2)) {
        case 0:
          return_values->command = CallSS_FP;
          return_values->data1 = bits(4, 0, 8);
          return cond;
        case 1:
          return_values->command = CallSS_VMGM_MENU;
          return_values->data1 = bits(5, 4, 4);
          return_values->data2 = bits(4, 0 ,8);
          return cond;
        case 2:
          return_values->command = CallSS_VTSM;
          return_values->data1 = bits(5, 4, 4);
          return_values->data2 = bits(4, 0, 8);
          return cond;
        case 3:
          return_values->command = CallSS_VMGM_PGC;
          return_values->data1 = bits(2, 1, 15);
          return_values->data2 = bits(4, 0, 8);
          return cond;
      }
      break;
  }
  return 0;
}

/* Evaluate a set sytem register instruction 
   May contain a link so return the same as eval_link */
static bool eval_system_set(int cond, link_t *return_values)
{
  int i;
  uint16_t data, data2;
  
  switch(bits(0, 4, 4)) {
    case 1: // Set system reg 1 &| 2 &| 3 (Audio, Subp. Angle)
      for(i = 1; i <= 3; i++) {
        if(bits(2 + i, 0, 1)) {
          data = eval_reg_or_data_2(bits(0, 3, 1), 2 + i);
          if(cond) {
            state->SPRM[i] = data;
          }
        }
      }
      break;
    case 2: // Set system reg 9 & 10 (Navigation timer, Title PGC number)
      data = eval_reg_or_data(bits(0, 3, 1), 2);
      data2 = bits(5, 0, 8); // ?? size
      if(cond) {
	state->SPRM[9] = data; // time
	state->SPRM[10] = data2; // pgcN
      }
      break;
    case 3: // Mode: Counter / Register + Set
      data = eval_reg_or_data(bits(0, 3, 1), 2);
      data2 = bits(5, 4, 4);
      if(bits(5, 0, 1)) {
	fprintf(stderr, "Detected SetGPRMMD Counter!! This is unsupported.\n");
	//exit(-1);
      } else {
	;
      }
      if(cond) {
	state->GPRM[data2] = data;
      }
      break;
    case 6: // Set system reg 8 (Highlighted button)
      data = eval_reg_or_data(bits(0, 3, 1), 4); // Not system reg!!
      if(cond) {
	/* We check that it's in range 1..36 */
	data &= 0xfc00; // Mask out the correct bits
	if(data < 0x0400 || data > 0x9000) {
	  //abort(0); // FixMe
	  data = state->SPRM[8]; // Keep the last value instead
	}
	state->SPRM[8] = data;
      }
      break;
  }
  if(bits(1, 4, 4)) {
    return eval_link_instruction(cond, return_values);
  }
  return 0;
}


/* Evaluate set operation
   Sets the register given to the value indicated by op and data.
   For the swap case the contents of reg is stored in reg2.
*/
static void eval_set_op(int op, int reg, int reg2, int data)
{
  /* General parameters and System Parameters (GPRMS and SPRMS)
   * are treated as 16-bit unsigned integers.
   * in case of overflow, 0xFFFF must be assigned
   * in case of underflow, 0x0 must be assigned
   */
  const int shortmax = 0xffff;
  
  int tmp;
  switch(op) {
    case 1: /* = (assignment) */
      state->GPRM[reg] = data;
      break;
    case 2: /* Swap */
      tmp = state->GPRM[reg];
      state->GPRM[reg] = state->GPRM[reg2];
      state->GPRM[reg2] = tmp;
      break;
    case 3: /* += (addition) */
      tmp = state->GPRM[reg] + data;
      if(tmp >= shortmax) tmp = shortmax;
      state->GPRM[reg] = (uint16_t)tmp;
      break;
    case 4: /* -= (subtraction) */
      tmp = state->GPRM[reg] - data;
      if(tmp < 0) tmp = 0;
      state->GPRM[reg] = (uint16_t)tmp;      
      break;
    case 5: /* *= (multiplication) */
      tmp = state->GPRM[reg] * data;
      if(tmp >= shortmax) tmp = shortmax;
      state->GPRM[reg] = (uint16_t)tmp;
      break;
    case 6: /* /= (division) */
      if(data != 0)
	state->GPRM[reg] /= data;
      else
	state->GPRM[reg] = 65535;
      break;
    case 7: /* %= (modulo) */
      if(data != 0)
	state->GPRM[reg] %= data;
      else
	state->GPRM[reg] = 0; /* really an error I guess */
      break;
      break;
    case 8: /* SPECIAL CASE - RANDOM! */
      if(data != 0)
	/* TODO rand() might only return 0-32768  */
	state->GPRM[reg] = (rand() % data) + 1;
      else
	state->GPRM[reg] = 0; /* really an error I guess */
      break;
    case 9: /* &= (bitwise and) */
      state->GPRM[reg] &= data;
      break;
    case 10: /* |= (bitwise or) */
      state->GPRM[reg] |= data;
      break;
    case 11: /* ^= (bitwise xor) */
      state->GPRM[reg] ^= data;
      break;
  }
}

/* Evaluate set instruction, combined with either Link or Compare. 
 * PPP0SSSS ******** ******** 0AAAAAAA ******** BBBBBBBB ******** ********
 * PPP1SSSS ******** ******** 0AAAAAAA DDDDDDDD DDDDDDDD ******** ********
 * where PPP is 011
 * "r[A] `S` r[B]" resp "r[A] `S` D"
 */
static void eval_set_version_1(int cond)
{
  uint8_t  op   = bits(0, 4, 4);
  uint8_t  reg  = bits(3, 0, 8); // GPRM
  uint8_t  reg2 = bits(5, 4, 4); // GPRM or SPRM
  uint16_t data = eval_reg_or_data(bits(0, 3, 1), 4); // or Imm

  if(cond) {
    eval_set_op(op, reg, reg2, data);
  }
}


/* Evaluate set instruction for Set Compare Link.
 * PPP0SSSS ****AAAA ******** BBBBBBBB ******** ******** ******** ********
 * PPP1SSSS ****AAAA DDDDDDDD DDDDDDDD ******** ******** ******** ********
 * when PPP is 101
 * "g[A] `S` r[B]" resp "g[A] `S` D"
 */
static void eval_set_version_2(int cond)
{
  uint8_t  op   = bits(0, 4, 4);
  uint8_t  reg  = bits(1, 4, 4); // GPRM
  uint8_t  reg2 = bits(3, 0, 8); // GPRM or SPRM
  uint16_t data = eval_reg_or_data(bits(0, 3, 1), 2); // or Imm

  if(cond) {
    eval_set_op(op, reg, reg2, data);
  }
}

/* Evaluate set instruction for, Compare & Set-Link, Compare-Set & Link. 
 * PPP0SSSS ****AAAA BBBBBBBB ******** ******** ******** ******** ********
 * PPP1SSSS 0***AAAA DDDDDDDD DDDDDDDD ******** ******** ******** ********
 * when PPP is 100 or 110
 * "g[A] `S` r[B]" resp "g[A] `S` D"
 */
static void eval_set_version_3(int cond)
{
  uint8_t  op   = bits(0, 4, 4);
  uint8_t  reg  = bits(1, 4, 4); // GPRM
  uint8_t  reg2 = bits(2, 0, 8); // GPRM or SPRM
  uint16_t data;
  if(bits(0, 3, 1)) { // immediate
    data = bits(2, 0, 16);
  } else {
    data = eval_reg(reg2);
  }

  if(cond) {
    eval_set_op(op, reg, reg2, data);
  }
}


/* Evaluate a command
   returns row number of goto, 0 if no goto, -1 if link.
   Link command in return_values */
static int eval_command(uint8_t *bytes, link_t *return_values)
{
  int i, extra_bits;
  int cond, res = 0;

  for(i = 0; i < 8; i++) {
    cmd.bits[i] = bytes[i];
    cmd.examined[i] = 0;
  }
  memset(return_values, 0, sizeof(link_t));

  switch(bits(0, 0, 3)) { /* three first bits */
    case 0: // Special instructions
      cond = eval_if_version_1();
      res = eval_special_instruction(cond);
      if(res == -1) {
	fprintf(stderr, "Unknown Instruction!\n");
	//exit(0);
      }
      break;
    case 1: // Link/jump instructions
      if(bits(0, 3, 1)) {
        cond = eval_if_version_2();
        res = eval_jump_instruction(cond, return_values);
      } else {
        cond = eval_if_version_1();
        res = eval_link_instruction(cond, return_values);
      }
      if(res)
	res = -1;
      break;
    case 2: // System set instructions
      cond = eval_if_version_2();
      res = eval_system_set(cond, return_values);
      if(res)
	res = -1;
      break;
    case 3: // Set instructions, either Compare or Link may be used
      cond = eval_if_version_3();
      eval_set_version_1(cond);
      if(bits(1, 4, 4)) {
	res = eval_link_instruction(cond, return_values);
      }
      if(res)
	res = -1;
      break;
    case 4: // Set, Compare -> Link Sub-Instruction
      eval_set_version_2(/*True*/ 1);
      cond = eval_if_version_4();
      res = eval_link_subins(cond, return_values);
      if(res)
	res = -1;
      break;
    case 5: // Compare -> (Set and Link Sub-Instruction)
      if(bits(0, 3, 1))
	cond = eval_if_version_5();
      else
	cond = eval_if_version_1();
      eval_set_version_3(cond);
      res = eval_link_subins(cond, return_values);
      if(res)
	res = -1;
      break;
    case 6: // Compare -> Set, always Link Sub-Instruction
      if(bits(0, 3, 1))
	cond = eval_if_version_5();
      else
	cond = eval_if_version_1();
      eval_set_version_3(cond);
      res = eval_link_subins(/*True*/ 1, return_values);
      if(res)
	res = -1;
      break;
  }
  // Check if there are bits not yet examined

  extra_bits = 0;
  for(i = 0; i < 8; i++)
    if(cmd.bits [i] & ~cmd.examined [i]) {
      extra_bits = 1;
      break;
    }
  if(extra_bits) {
    fprintf(stderr, "[WARNING, unknown bits:");
    for(i = 0; i < 8; i++)
      fprintf(stderr, " %02x", cmd.bits [i] & ~cmd.examined [i]);
    fprintf(stderr, "]\n");
  }

  return res;
}

/* Evaluate a set of commands in the given register set (which is
 * modified */
int vmEval_CMD(vm_cmd_t commands[], int num_commands, 
	       registers_t *registers, link_t *return_values)
{
  int i = 0;
  int total = 0;
  
  state = registers; // TODO FIXME

#ifdef TRACE
  // DEBUG
  if(1) {
    int i;
    fprintf(stderr, "   #   ");
    for(i = 0; i < 24; i++)
    fprintf(stderr, " %2d |", i);
    fprintf(stderr, "\nSPRMS: ");
    for(i = 0; i < 24; i++)
      fprintf(stderr, "%04x|", state->SPRM[i]);
    fprintf(stderr, "\nGPRMS: ");
    for(i = 0; i < 16; i++)
      fprintf(stderr, "%04x|", state->GPRM[i]);
    fprintf(stderr, "\n");
  }
  if(1) {
    int i;
    for(i = 0; i < num_commands; i++)
      vmPrint_CMD(i, &commands[i]);
    fprintf(stderr, "--------------------------------------------\n");
  } // end DEBUG
#endif
  
  while(i < num_commands && total < 100000) {
    int line;
    
    if(0) vmPrint_CMD(i, &commands[i]);
    line = eval_command(&commands[i].bytes[0], return_values);
    
    if (line < 0) { // Link command
#ifdef TRACE
      fprintf(stderr, "eval: Doing Link/Jump/Call\n");
#endif
      return 1;
    }
    
    if (line > 0) // Goto command
      i = line - 1;
    else // Just continue on the next line
      i++;

    total++;
  }
  
  memset(return_values, 0, sizeof(link_t));
  return 0;
}


static char *linkcmd2str(link_cmd_t cmd)
{
  switch(cmd) {
  case LinkNoLink:
    return "LinkNoLink";
  case LinkTopC:
    return "LinkTopC";
  case LinkNextC:
    return "LinkNextC";
  case LinkPrevC:
    return "LinkPrevC";
  case LinkTopPG:
    return "LinkTopPG";
  case LinkNextPG:
    return "LinkNextPG";
  case LinkPrevPG:
    return "LinkPrevPG";
  case LinkTopPGC:
    return "LinkTopPGC";
  case LinkNextPGC:
    return "LinkNextPGC";
  case LinkPrevPGC:
    return "LinkPrevPGC";
  case LinkGoUpPGC:
    return "LinkGoUpPGC";
  case LinkTailPGC:
    return "LinkTailPGC";
  case LinkRSM:
    return "LinkRSM";
  case LinkPGCN:
    return "LinkPGCN";
  case LinkPTTN:
    return "LinkPTTN";
  case LinkPGN:
    return "LinkPGN";
  case LinkCN:
    return "LinkCN";
  case Exit:
    return "Exit";
  case JumpTT:
    return "JumpTT";
  case JumpVTS_TT:
    return "JumpVTS_TT";
  case JumpVTS_PTT:
    return "JumpVTS_PTT";
  case JumpSS_FP:
    return "JumpSS_FP";
  case JumpSS_VMGM_MENU:
    return "JumpSS_VMGM_MENU";
  case JumpSS_VTSM:
    return "JumpSS_VTSM";
  case JumpSS_VMGM_PGC:
    return "JumpSS_VMGM_PGC";
  case CallSS_FP:
    return "CallSS_FP";
  case CallSS_VMGM_MENU:
    return "CallSS_VMGM_MENU";
  case CallSS_VTSM:
    return "CallSS_VTSM";
  case CallSS_VMGM_PGC:
    return "CallSS_VMGM_PGC";
  case PlayThis:
    return "PlayThis";
  }
  return "*** (bug)";
}

void vmPrint_LINK(link_t value)
{
  char *cmd = linkcmd2str(value.command);
    
  switch(value.command) {
  case LinkNoLink:
  case LinkTopC:
  case LinkNextC:
  case LinkPrevC:
  case LinkTopPG:
  case LinkNextPG:
  case LinkPrevPG:
  case LinkTopPGC:
  case LinkNextPGC:
  case LinkPrevPGC:
  case LinkGoUpPGC:
  case LinkTailPGC:
  case LinkRSM:
    fprintf(stderr, "%s (button %d)\n", cmd, value.data1);
    break;
  case LinkPGCN:
  case JumpTT:
  case JumpVTS_TT:
  case JumpSS_VMGM_MENU: // == 2 -> Title Menu
  case JumpSS_VMGM_PGC:
    fprintf(stderr, "%s %d\n", cmd, value.data1);
    break;
  case LinkPTTN:
  case LinkPGN:
  case LinkCN:
    fprintf(stderr, "%s %d (button %d)\n", cmd, value.data1, value.data2);
    break;
  case Exit:
  case JumpSS_FP:
  case PlayThis: // Humm.. should we have this at all..
    fprintf(stderr, "%s\n", cmd);
    break;
  case JumpVTS_PTT:
    fprintf(stderr, "%s %d:%d\n", cmd, value.data1, value.data2);
    break;
  case JumpSS_VTSM:
    fprintf(stderr, "%s vts %d title %d menu %d\n", 
	    cmd, value.data1, value.data2, value.data3);
    break;
  case CallSS_FP:
    fprintf(stderr, "%s resume cell %d\n", cmd, value.data1);
    break;
  case CallSS_VMGM_MENU: // == 2 -> Title Menu
  case CallSS_VTSM:
    fprintf(stderr, "%s %d resume cell %d\n", cmd, value.data1, value.data2);
    break;
  case CallSS_VMGM_PGC:
    fprintf(stderr, "%s %d resume cell %d\n", cmd, value.data1, value.data2);
    break;
  }
}
