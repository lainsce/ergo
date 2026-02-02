bring stdr
bring math

-- Ergo Example: Namespaced Imports and Array Types

fun sum(arr = [num]) (( num )) {
    let ?total = 0
    for (let ?i = 0; i < len(arr); i = i + 1) {
        total = total + arr[i]
    }
    total
}

entry () (( -- )) {
    let nums = [1, 2, 3]
    writef(@"sum={}\n", sum(nums))

    let x = math.PI / 2.0
    let y = math.sin(x)
    writef(@"sin(pi/2)={}\n", y)

    stdr.write(@"-- done --\n")
}
