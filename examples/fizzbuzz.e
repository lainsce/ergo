bring stdr

-- Ergo Example: FizzBuzz

fun fizzbuzz(n = int) (( -- )) {
    for (let ?i = 1; i <= n; i = i + 1) {
        if i % 15 == 0 {
            write(@"FizzBuzz\n")
        } elif i % 3 == 0 {
            write(@"Fizz\n")
        } elif i % 5 == 0 {
            write(@"Buzz\n")
        } else {
            writef(@"{}\n", i)
        }
    }
}

entry () (( -- )) {
    write(@"== FizzBuzz Example ==\n")
    fizzbuzz(20)
    write(@"== Done ==\n")
}
