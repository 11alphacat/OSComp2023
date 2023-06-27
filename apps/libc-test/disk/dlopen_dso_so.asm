
./libc-test/disk/dlopen_dso.so:     file format elf64-littleriscv


Disassembly of section .text:

0000000000000358 <f-0x90>:
 358:	00002517          	auipc	a0,0x2
 35c:	cb850513          	addi	a0,a0,-840 # 2010 <i+0x8>
 360:	00002797          	auipc	a5,0x2
 364:	cb078793          	addi	a5,a5,-848 # 2010 <i+0x8>
 368:	00a78963          	beq	a5,a0,37a <f-0x6e>
 36c:	00002317          	auipc	t1,0x2
 370:	cd433303          	ld	t1,-812(t1) # 2040 <_ITM_deregisterTMCloneTable>
 374:	00030363          	beqz	t1,37a <f-0x6e>
 378:	8302                	jr	t1
 37a:	8082                	ret
 37c:	00002517          	auipc	a0,0x2
 380:	c9450513          	addi	a0,a0,-876 # 2010 <i+0x8>
 384:	00002797          	auipc	a5,0x2
 388:	c8c78793          	addi	a5,a5,-884 # 2010 <i+0x8>
 38c:	8f89                	sub	a5,a5,a0
 38e:	4037d713          	srai	a4,a5,0x3
 392:	03f7d593          	srli	a1,a5,0x3f
 396:	95ba                	add	a1,a1,a4
 398:	8585                	srai	a1,a1,0x1
 39a:	c981                	beqz	a1,3aa <f-0x3e>
 39c:	00002317          	auipc	t1,0x2
 3a0:	c9c33303          	ld	t1,-868(t1) # 2038 <_ITM_registerTMCloneTable>
 3a4:	00030363          	beqz	t1,3aa <f-0x3e>
 3a8:	8302                	jr	t1
 3aa:	8082                	ret
 3ac:	1141                	addi	sp,sp,-16
 3ae:	e022                	sd	s0,0(sp)
 3b0:	00002417          	auipc	s0,0x2
 3b4:	c9840413          	addi	s0,s0,-872 # 2048 <i+0x40>
 3b8:	00044783          	lbu	a5,0(s0)
 3bc:	e406                	sd	ra,8(sp)
 3be:	e385                	bnez	a5,3de <f-0xa>
 3c0:	00002797          	auipc	a5,0x2
 3c4:	c687b783          	ld	a5,-920(a5) # 2028 <__cxa_finalize>
 3c8:	c791                	beqz	a5,3d4 <f-0x14>
 3ca:	00002517          	auipc	a0,0x2
 3ce:	c3653503          	ld	a0,-970(a0) # 2000 <f+0x1c18>
 3d2:	9782                	jalr	a5
 3d4:	f85ff0ef          	jal	ra,358 <f-0x90>
 3d8:	4785                	li	a5,1
 3da:	00f40023          	sb	a5,0(s0)
 3de:	60a2                	ld	ra,8(sp)
 3e0:	6402                	ld	s0,0(sp)
 3e2:	0141                	addi	sp,sp,16
 3e4:	8082                	ret
 3e6:	bf59                	j	37c <f-0x6c>

00000000000003e8 <f>:
 3e8:	1141                	addi	sp,sp,-16
 3ea:	e422                	sd	s0,8(sp)
 3ec:	0800                	addi	s0,sp,16
 3ee:	00002797          	auipc	a5,0x2
 3f2:	c427b783          	ld	a5,-958(a5) # 2030 <i+0x28>
 3f6:	439c                	lw	a5,0(a5)
 3f8:	2785                	addiw	a5,a5,1
 3fa:	0007871b          	sext.w	a4,a5
 3fe:	00002797          	auipc	a5,0x2
 402:	c327b783          	ld	a5,-974(a5) # 2030 <i+0x28>
 406:	c398                	sw	a4,0(a5)
 408:	0001                	nop
 40a:	6422                	ld	s0,8(sp)
 40c:	0141                	addi	sp,sp,16
 40e:	8082                	ret
