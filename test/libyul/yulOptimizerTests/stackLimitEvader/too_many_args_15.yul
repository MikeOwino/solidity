{
    {
        mstore(0x40, memoryguard(128))
        sstore(g(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29, 30), 0)
    }
    function g(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23, b24, b25, b26, b27, b28, b29, b30) -> v {
	sstore(0, b14)
	sstore(1, b15)
	sstore(2, b16)
	sstore(3, b17)
	sstore(4, b18)
	sstore(5, b19)
	sstore(6, b29)
	sstore(7, b30)
        v := b30
        sstore(b1, b30)
    }

}
// ====
// bytecodeFormat: legacy
// ----
// step: stackLimitEvader
//
// {
//     {
//         mstore(0x40, memoryguard(0x0300))
//         sstore(g(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30), 0)
//     }
//     function g(b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23, b24, b25, b26, b27, b28, b29, b30) -> v
//     {
//         mstore(0x02e0, b1)
//         mstore(0x02c0, b2)
//         mstore(0x02a0, b3)
//         mstore(0x0280, b4)
//         mstore(0x0260, b5)
//         mstore(0x0240, b6)
//         mstore(0x0220, b7)
//         mstore(0x0200, b8)
//         mstore(0x01e0, b9)
//         mstore(0x01c0, b10)
//         mstore(0x01a0, b11)
//         mstore(0x0180, b12)
//         mstore(0x0160, b13)
//         mstore(0x0140, b14)
//         mstore(0x0120, b15)
//         mstore(0x0100, b17)
//         mstore(0xe0, b18)
//         mstore(0xc0, b19)
//         mstore(0xa0, b29)
//         mstore(0x80, b30)
//         sstore(0, mload(0x0140))
//         sstore(1, mload(0x0120))
//         sstore(2, b16)
//         sstore(3, mload(0x0100))
//         sstore(4, mload(0xe0))
//         sstore(5, mload(0xc0))
//         sstore(6, mload(0xa0))
//         sstore(7, mload(0x80))
//         v := mload(0x80)
//         sstore(mload(0x02e0), mload(0x80))
//     }
// }
