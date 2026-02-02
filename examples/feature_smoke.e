bring stdr
bring math

class Point {
    x = int
    y = int

    fun init(?this, x = int, y = int) (( -- )) {
        this.x = x
        this.y = y
    }

    fun sum(this) (( int )) {
        this.x + this.y
    }
}

fun feature_smoke() (( -- )) {
    let p = new Point(2, 3)
    let s = p.sum()
    writef(@"sum={}\n", s)

    let f = |x = int| x + 1
    let v = f(10)

    let m = match v: 11 => 1, n => n + 1
    writef(@"m={}\n", m)

    let total = 0.5 + 1.23
    writef(@"total={}\n", total)

    let s = math.sin(math.PI / 2.0)
    writef(@"sin(pi/2)={}\n", s)

    for (let ?i = 0; i < 3; i = i + 1) {
        writef(@"i={}\n", i)
    }

    for (ch in @"hi") {
        writef(@"ch={}\n", ch)
    }
}

entry () (( -- )) {
    feature_smoke()
}
