#ifndef FFB_ASM_H_INCLUDED
#define FFB_ASM_H_INCLUDED

void yuv_plane_to_yuyv_packed(char *y, char *u, char *v,
			      int *dst, int width);

#endif
