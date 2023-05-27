/*
converts a planar yuv image to packed yuyv mode and writes it
to the yuv buffer on the ffb2(+) gfx card

as -xarch=v8plusa ffb_asm.s

*/
	
	.section	".text"


	.global		yuv_plane_to_yuyv_packed

	.align		4


yuv_plane_to_yuyv_packed:
/* arg0 y, arg1 u, arg2 v, arg3 dst, arg4 width, arg5 stride */



/* arg0 y, arg1 u, arg2 v, arg3 dst, arg4 width */

	add	%o0, %o4, %l0
	add	%o3, 2048, %l1	
	add	%l1, 2048, %l1	

!	ld	[%o0+256], %l7
!	ld	[%o1+256], %l4
!	ld	[%o2+256], %l6
!	ld	[%l0+256], %l5
	
loop1:	
	ldd	[%o1], %f8
	ldd	[%o1+8], %f10

		
	ldd	[%o2], %f12
	ldd	[%o2+8], %f14

		
	ldd	[%o0], %f0
	ldd	[%o0+8], %f2
	ldd	[%o0+16], %f4
	ldd	[%o0+24], %f6

	
	ldd	[%l0], %f24
	ldd	[%l0+8], %f26
	ldd	[%l0+16], %f28
	ldd	[%l0+24], %f30
	



	add	%o1, 16, %o1
	add	%o2, 16, %o2

	fpmerge %f8, %f12, %f16
	fpmerge %f9, %f13, %f18

	add	%o0, 32, %o0
	add	%l0, 32, %l0
	subcc	%o4, 16, %o4
	

	fpmerge %f10, %f14, %f20
	fpmerge %f11, %f15, %f22
	fpmerge %f0, %f16, %f32
	fpmerge %f1, %f17, %f34
	fpmerge %f2, %f18, %f36
	fpmerge %f3, %f19, %f38
	
	
	fpmerge %f4, %f20, %f40
	fpmerge %f5, %f21, %f42
	fpmerge %f6, %f22, %f44
	fpmerge %f7, %f23, %f46


!	stda	%f32, [%o3] 0xf0
	
	fpmerge	%f24, %f16, %f48
	fpmerge	%f25, %f17, %f50
	fpmerge	%f26, %f18, %f52
	fpmerge	%f27, %f19, %f54

	fpmerge	%f28, %f20, %f56
	fpmerge	%f29, %f21, %f58
	fpmerge	%f30, %f22, %f60
	fpmerge	%f31, %f23, %f62


	
	std	%f32, [%o3]
	std	%f34, [%o3+8]
	std	%f36, [%o3+16]
	std	%f38, [%o3+24]

	std	%f48, [%l1]	
	std	%f50, [%l1+8]
	std	%f52, [%l1+16]
	std	%f54, [%l1+24]
	
	be	end
	subcc	%o4, 16, %o4
	


	std	%f40, [%o3+32]
	std	%f42, [%o3+40]
	std	%f44, [%o3+48]
	std	%f46, [%o3+56]
	
	std	%f56, [%l1+32]
	std	%f58, [%l1+40]
	std	%f60, [%l1+48]
	std	%f62, [%l1+56]
	

!	stda	%f48, [%l1] 0xf0 	

	add     %l1, 64, %l1
	bne	loop1
	add     %o3, 64, %o3
	
end:	

	retl
	nop

	
	.type	yuv_plane_to_yuyv_packed,#function
	.size	yuv_plane_to_yuyv_packed,.-yuv_plane_to_yuyv_packed

	/*
	32 pixels
	without instructions:
		10 M Cycle	14 M Instr
		1.48M IU_stall	600 FP_stall

	with instructions:
		55 M (44M)Cycle	23 M (19M)Instr
		1.3M IU_stall	1.1M FP_stall	

	with double line:
		51 M (40M)Cycle	13 M (10M)Instr
		0.8M (0.65M)IU_stall	0.5M (0.4M)FP_stall	
		     (2.2M) DC_rd	     (0.4M)DC_rd_miss
		     (5M) IC_ref	     (17k) IC_miss
		     (2M) EC_ref	     (50k) EC_misses
		     (50k) EC_rd_miss	     (2k) EC_ic_miss
		     (2.2M) DC_wr            (2.1M) DC_wr_miss
		     (17M) Rstall_storeQ  
		9-10k (7k)br_miss_taken	 13-14k (14k)br_miss_untaken
		      (296k) br_taken     (314k) br_untaken  
		(300k) dispatch0_ic_miss  (29k)dispatch0_mispred
		(12k) dispatch0_br_tgt	  (10k) EC_wb
		(30k) mc_reads_0	 (5k) mc_writes_0
		(0) mc_reads_1		(0) mc_writes_1
		(26k) mc_reads_2	(5k) mc_writes_2
		(0) mc_reads_3		(0) mc_writes_3
		(450k) mc_stalls_0	(0) mc_stalls_1
		(450k) mc_stalls_2	(0) mc_stalls_3
					(10M) Re_EC_miss
		(0) EC_write_hit_RTO    (15M) Re_DC_miss	
		(2.6M) FA_pipe_completion (72) FM_pipe_completion
	
		with double line block store:
		51 M (40M)Cycle	10 M (8M)Instr
		0.8M (0.65M)IU_stall	0.5M (0.4M)FP_stall	
		     (2.2M) DC_rd	     (0.4M)DC_rd_miss
		     (5M) IC_ref	     (17k) IC_miss
		     (2M) EC_ref	     (50k) EC_misses
		     (50k) EC_rd_miss	     (2k) EC_ic_miss
		     (2.2M) DC_wr            (2.1M) DC_wr_miss
		     (9M) Rstall_storeQ  

	
	with block store:
		55 M (43M) Cycle	21 M (17M)Instr
		1.3M (1.0M)IU_stall	1.0M (0.8M)FP_stall	

	16 pixels
	without instructions:
		16 M Cycle	24 M Instr
		2 M IU_stall	600 FP_stall

	with instructions:
		54 M Cycle	33 M Instr
		1.5M IU_stall	1.1M FP_stall	


	8 pixels
	without instructions:
		28 M Cycle	46 M Instr
		3.4 M IU_stall	600 FP_stall

	with instructions:
		56 M (45M) Cycle	56 M (45M) Instr
		2.1M (1.7M)IU_stall	4.9 M (3.9M)FP_stall	
		
	*/

