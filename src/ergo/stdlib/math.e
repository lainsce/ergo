-- Ergo Standard Library: math.e

let PI = 3.141592653589793

-- Sine function (compiler intrinsic)
fun sin(x = float) (( float )) { }

-- Cosine function (compiler intrinsic)
fun cos(x = float) (( float )) { }

-- Tangent function (compiler intrinsic)
fun tan(x = float) (( float )) { }

-- Square root function (compiler intrinsic)
fun sqrt(x = float) (( float )) { }

-- Absolute value
fun abs(x = float) (( float )) {
  if x < 0.0 {
    return -x
  } else {
    return x
  }
}

-- Minimum of two floats
fun min(a = float, b = float) (( float )) {
  if a < b {
    return a
  } else {
    return b
  }
}

-- Maximum of two floats
fun max(a = float, b = float) (( float )) {
  if a > b {
    return a
  } else {
    return b
  }
}
