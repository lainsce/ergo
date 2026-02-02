bring stdr

-- Ergo Example: Null and Boolean Logic

fun null_and_bool_demo () (( -- )) {
    write(@"-- Null and Boolean Logic Demo --")

    let ?flag = true
    writef(@"flag initially: {}\n", flag)

    flag = null
    writef(@"flag after null assignment: {}\n", flag)

    if flag {
        write(@"This should not print (flag is null)\n")
    } else {
        write(@"flag is null or false\n")
    }

    let ?value = null
    if value {
        write(@"value is truthy\n")
    } else {
        write(@"value is null or false\n")
    }

    value = 42
    if value {
        writef(@"value is now truthy: {}\n", value)
    } else {
        write(@"value is still null or false\n")
    }

    let t = true
    let f = false
    if t && !f {
        write(@"t && !f is true\n")
    } else {
        write(@"t && !f is fals\n")
    }

    if t || f {
        write(@"t || f is true\n")
    } else {
        write(@"t || f is false\n")
    }
}

entry () (( -- )) {
    null_and_bool_demo()
}
