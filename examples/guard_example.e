bring stdr

-- Ergo Example: Optionals and Safe Operations
-- Example: Optionals and Safe Division in Ergo

fun describe_number(n = int) (( -- )) {
    if n != null {
        writef(@"number={}\n", n)
    } else {
        write(@"number=(none)\n")
    }
}

fun safe_divide(a = int, b = int) (( -- )) {
    let result = if b != 0 {
        a / b
    } else {
        null
    }
    let output = if result != null {
        result.to_string()
    } else {
        @"(div by zero)"
    }
    writef(@"{} / {} = {}\n", a, b, output)
}

entry () (( -- )) {
    write(@"-- guard_example.ergo --\n")

    let maybe_num = 42
    describe_number(maybe_num)

    let maybe_num2 = null
    describe_number(maybe_num2)

    safe_divide(10, 2)
    safe_divide(10, 0)

    write(@"-- done --\n")
}
