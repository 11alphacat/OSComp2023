
./libc-test/disk/dlopen_dso.so:     file format elf64-littleriscv


Disassembly of section .text:

0000000000000390 <f-0x8a>:
 390:	00002517          	auipc	a0,0x2
 394:	c8050513          	addi	a0,a0,-896 # 2010 <i+0x8>
 398:	00002797          	auipc	a5,0x2
 39c:	c7878793          	addi	a5,a5,-904 # 2010 <i+0x8>
 3a0:	00a78863          	beq	a5,a0,3b0 <f-0x6a>
 3a4:	00002797          	auipc	a5,0x2
 3a8:	c9c7b783          	ld	a5,-868(a5) # 2040 <_ITM_deregisterTMCloneTable>
 3ac:	c391                	beqz	a5,3b0 <f-0x6a>
 3ae:	8782                	jr	a5
 3b0:	8082                	ret
 3b2:	00002517          	auipc	a0,0x2
 3b6:	c5e50513          	addi	a0,a0,-930 # 2010 <i+0x8>
 3ba:	00002597          	auipc	a1,0x2
 3be:	c5658593          	addi	a1,a1,-938 # 2010 <i+0x8>
 3c2:	8d89                	sub	a1,a1,a0
 3c4:	4035d793          	srai	a5,a1,0x3
 3c8:	91fd                	srli	a1,a1,0x3f
 3ca:	95be                	add	a1,a1,a5
 3cc:	8585                	srai	a1,a1,0x1
 3ce:	c599                	beqz	a1,3dc <f-0x3e>
 3d0:	00002797          	auipc	a5,0x2
 3d4:	c687b783          	ld	a5,-920(a5) # 2038 <_ITM_registerTMCloneTable>
 3d8:	c391                	beqz	a5,3dc <f-0x3e>
 3da:	8782                	jr	a5
 3dc:	8082                	ret
 3de:	1141                	addi	sp,sp,-16
 3e0:	e022                	sd	s0,0(sp)
 3e2:	00002417          	auipc	s0,0x2
 3e6:	c6640413          	addi	s0,s0,-922 # 2048 <i+0x40>
 3ea:	00044783          	lbu	a5,0(s0)
 3ee:	e406                	sd	ra,8(sp)
 3f0:	e385                	bnez	a5,410 <f-0xa>
 3f2:	00002797          	auipc	a5,0x2
 3f6:	c367b783          	ld	a5,-970(a5) # 2028 <__cxa_finalize>
 3fa:	c791                	beqz	a5,406 <f-0x14>
 3fc:	00002517          	auipc	a0,0x2
 400:	c0453503          	ld	a0,-1020(a0) # 2000 <f+0x1be6>
 404:	9782                	jalr	a5
 406:	f8bff0ef          	jal	ra,390 <f-0x8a>
 40a:	4785                	li	a5,1
 40c:	00f40023          	sb	a5,0(s0)
 410:	60a2                	ld	ra,8(sp)
 412:	6402                	ld	s0,0(sp)
 414:	0141                	addi	sp,sp,16
 416:	8082                	ret
 418:	bf69                	j	3b2 <f-0x68>

000000000000041a <f>:
 41a:	1141                	addi	sp,sp,-16
 41c:	e422                	sd	s0,8(sp)
 41e:	0800                	addi	s0,sp,16
 420:	00002797          	auipc	a5,0x2
 424:	c107b783          	ld	a5,-1008(a5) # 2030 <i+0x28>
 428:	439c                	lw	a5,0(a5)
 42a:	2785                	addiw	a5,a5,1
 42c:	0007871b          	sext.w	a4,a5
 430:	00002797          	auipc	a5,0x2
 434:	c007b783          	ld	a5,-1024(a5) # 2030 <i+0x28>
 438:	c398                	sw	a4,0(a5)
 43a:	0001                	nop
 43c:	6422                	ld	s0,8(sp)
 43e:	0141                	addi	sp,sp,16
 440:	8082                	ret
