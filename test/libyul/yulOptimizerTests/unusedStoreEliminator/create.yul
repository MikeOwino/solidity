{
    let x := 5
    sstore(x, 10)
    pop(create(0, 0, 0))
    sstore(x, 20)
}
// ====
// bytecodeFormat: legacy
// ----
// step: unusedStoreEliminator
//
// {
//     {
//         let x := 5
//         sstore(x, 10)
//         pop(create(0, 0, 0))
//         sstore(x, 20)
//     }
// }
