-- Ergo Standard Library: stdr.e

-- Print formatted output (compiler intrinsic)
fun writef(fmt = string) (( -- )) { }

-- Read formatted input (compiler intrinsic)
fun readf(fmt = string) (( -- )) { }

-- Print a value (compiler intrinsic)
fun write(x = any) (( -- )) { }

-- Check if a value is null
fun is_null(x = any) (( bool )) { }

-- Get the length of an array or string
fun len(x = any) (( int )) { }
