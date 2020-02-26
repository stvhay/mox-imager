; Set DDR3 to 800MHz
; Pre-init
WRITE: 0xC0014008 0x00404500
WRITE: 0xC0002000 0x00010000
WRITE: 0xC0002004 0x00000000
; DDR MAC init
WRITE: 0xC0000340 0x00000303
WRITE: 0xC0000344 0x0200000A
WRITE: 0xC0000310 0x00200000
WRITE: 0xC0000304 0x00000400
WRITE: 0xC0000308 0x00000000
WRITE: 0xC0000044 0x00030200
WRITE: 0xC00002c0 0x00006000
WRITE: 0xC00002c4 0x00100020
WRITE: 0xC0000058 0x0000143F
WRITE: 0xC0000048 0x00000001
WRITE: 0xC0000180 0x00010200
WRITE: 0xC0000050 0x000001FF
WRITE: 0xC000004c 0x00000000
WRITE: 0xC0000054 0x00000480
WRITE: 0xC0000300 0x0000080B
WRITE: 0xC0000380 0x00061A80
WRITE: 0xC0000384 0x00027100
WRITE: 0xC0000388 0x00000050
WRITE: 0xC000038C 0x00000200
WRITE: 0xC0000390 0x00400100
WRITE: 0xC0000394 0x011803CF
WRITE: 0xC0000398 0x01200200
WRITE: 0xC000039C 0x00000808
WRITE: 0xC00003A0 0x01040514
WRITE: 0xC00003A4 0x00000001
WRITE: 0xC00003A8 0x00000C04
WRITE: 0xC00003AC 0x20270B1C
WRITE: 0xC00003B0 0x0B0C060B
WRITE: 0xC00003B4 0x04000600
WRITE: 0xC00003B8 0x00000600
WRITE: 0xC0000200 0x000D0001
WRITE: 0xC0000204 0x00000000
WRITE: 0xC0000220 0x04020632
WRITE: 0xC00003c4 0x00000000
WRITE: 0xC00003dc 0x00081239
WRITE: 0xC00002c8 0x00000000
WRITE: 0xC0000064 0x00000006
; DDR PHY init
WRITE: 0xC0001010 0x00300108
WRITE: 0xC0001028 0x00000000
WRITE: 0xC0001040 0x00000400
WRITE: 0xC00010c0 0x80000000
WRITE: 0xC00010d0 0x00000000
WRITE: 0xC00010e0 0x00011FF0
WRITE: 0xC0001014 0x00080200
WRITE: 0xC0001038 0x00000002
WRITE: 0xC000103c 0x00000010
WRITE: 0xC0001180 0x0000020A
WRITE: 0xC0001184 0x0000020A
WRITE: 0xC0001188 0x0000020A
WRITE: 0xC000118c 0x0000020A
WRITE: 0xC0001190 0x0000020A
WRITE: 0xC0001194 0x0000020A
WRITE: 0xC0001198 0x0000020A
WRITE: 0xC000119c 0x0000020A
WRITE: 0xC00011a0 0x0000020A
WRITE: 0xC0001050 0x1F130000
WRITE: 0xC0001054 0x20190000
WRITE: 0xC0001074 0x20200000
WRITE: 0xC0001058 0x08080000
WRITE: 0xC000105c 0x08080000
WRITE: 0xC0001060 0x08080000
WRITE: 0xC0001064 0x08080000
WRITE: 0xC0001068 0x08080000
WRITE: 0xC000106c 0x08080000
WRITE: 0xC0001070 0x08080000

;init read fifo pointer and OFF spec parameter
WRITE: 0xC0001000 0x00044041
WRITE: 0xC00003bc 0x02020404

;phyinit_sequence_sync2(1, 3, 2, 0)
WRITE: 0xC0001014 0x00080200
OR_VAL: 0xC0001038 0x00000003
WRITE: 0xC000103C 0x00000020
WRITE: 0xC0001020 0x80000000
WRITE: 0xC0001020 0x20000000
WRITE: 0xC0001020 0x40000000
DELAY: 0x0000000A
WRITE: 0xC0001020 0x80000000
DELAY: 0x0000000A

;DDRPHY Driver/Receiver & DQS internal Pullup/Pulldown settings
WRITE: 0xC0001004 0xC4477889
WRITE: 0xC0001008 0x05500FFA
WRITE: 0xC000100c 0x0521DF77

;clear Terminal for DDR3 (<400MHz)
AND_VAL: 0xC0001004 0xF00FFFFF

;DDRPHY pads POWERDOWN disable
WRITE: 0xC0001030 0x03800000
WRITE: 0xC0001034 0x00000000
WRITE: 0xC0001090 0x00000000
WRITE: 0xC0001094 0x00000000

;initC2Dofset
WRITE: 0xC00010E0 0x00010001
WRITE: 0xC00010D0 0x000C0300
WRITE: 0xC00010D4 0x00300C03
WRITE: 0xC00010D8 0x00300C03
WRITE: 0xC00010DC 0x00300C03
WRITE: 0xC00010E0 0x00010002
WRITE: 0xC00010D0 0x000C0300
WRITE: 0xC00010D4 0x00300C03
WRITE: 0xC00010D8 0x00300C03
WRITE: 0xC00010DC 0x00300C03
WRITE: 0xC00010E0 0x00010004
WRITE: 0xC00010D0 0x000C0300
WRITE: 0xC00010D4 0x00300C03
WRITE: 0xC00010D8 0x00300C03
WRITE: 0xC00010DC 0x00300C03
WRITE: 0xC00010E0 0x00010008
WRITE: 0xC00010D0 0x000C0300
WRITE: 0xC00010D4 0x00300C03
WRITE: 0xC00010D8 0x003294A5
WRITE: 0xC00010DC 0x00300C03
WRITE: 0xC00010E0 0x00010010
WRITE: 0xC00010D0 0x000C0300
WRITE: 0xC00010D4 0x00300C03
WRITE: 0xC00010D8 0x003294A5
WRITE: 0xC00010DC 0x00300C03

; Trigger DDR init for channel 0
WRITE: 0xC0000020 0x11000001
WAIT_FOR_BIT_SET: 0xC0000008 0x00000001 0x00001000